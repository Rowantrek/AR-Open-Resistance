[ComponentEditorProps(category: "OpenResistance/Operations", description: "Server force-pool executor for infantry, motorized, mechanized and armoured operations. Airborne profiles await a flight adapter.")]
class AROR_OperationExecutorComponentClass : SCR_BaseGameModeComponentClass
{
}

class AROR_OperationExecutorComponent : SCR_BaseGameModeComponent
{
	[Attribute("0", desc: "Allow explicit airborne response selections through the installed REAPER adapter, without replacing ground or combined-arms responses")]
	protected bool m_bAllowAirborneResponses;

	AROR_ResponseSelection GetResponsePreview(AROR_EFactionRole role, int type, int readiness, int crackdown)
	{
		if (type == AROR_EResponseType.AIR_GROUND && (!m_bAllowJointResponses || !m_bQRFTacticalCommander))
		{
			AROR_ResponseSelection unavailable = new AROR_ResponseSelection();
			unavailable.m_sReason = "Joint response coordination is disabled";
			return unavailable;
		}
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		string faction;
		if (roles)
			faction = roles.GetFactionKey(role);
		return AROR_ResponsePolicy.Select(m_aForceProfiles, m_aGroundQRFRecipes, role, faction, type, readiness, crackdown, m_bAllowAirborneResponses);
	}

	//! Every machine reads the same authored catalogue. Debug selections ignore campaign
	//! equipment/finance gates, but retain faction, actor-capacity and launch safety checks.
	void GetMarsEntries(AROR_EFactionRole role, int tier, notnull array<ref AROR_MarsEntry> entries)
	{
		entries.Clear();
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles || !m_aForceProfiles || tier < 1 || tier > 5)
			return;
		string faction = roles.GetFactionKey(role);
		if (m_aGroundQRFRecipes)
		{
			foreach (AROR_GroundQRFRecipe recipe : m_aGroundQRFRecipes)
			{
				array<AROR_ForceProfile> elements = {};
				if (!recipe || (recipe.m_eResponseType == AROR_EResponseType.AIR_GROUND && (!m_bAllowJointResponses || !m_bAllowAirborneResponses || !m_bQRFTacticalCommander)) || !recipe.Resolve(m_aForceProfiles, role, faction, elements, recipe.m_iMaximumReadinessLevel))
					continue;
				AROR_MarsEntry ground = new AROR_MarsEntry();
				ground.m_sKey = "recipe:" + recipe.m_sId;
				ground.m_sLabel = recipe.m_sId;
				ground.m_sRecipeId = recipe.m_sId;
				ground.m_iTier = recipe.m_iLevel;
				entries.Insert(ground);
			}
		}
		if (!m_aForceProfiles)
			return;
		foreach (AROR_ForceProfile profile : m_aForceProfiles)
		{
			if (!profile || !profile.m_bEnabled || !profile.IsValid() || profile.m_eRole != role || profile.m_sFactionKey != faction)
				continue;
			if (profile.m_iQRFWeight <= 0 || (!profile.HasNativeExecutor() && !(m_bAllowAirborneResponses && profile.m_eType == AROR_EForceType.AIRBORNE)))
				continue;
			AROR_MarsEntry entry = new AROR_MarsEntry();
			entry.m_sKey = "profile:" + profile.m_sId;
			entry.m_sLabel = profile.m_sId;
			entry.m_sProfileId = profile.m_sId;
			entry.m_iTier = tier;
			entry.m_bAirborne = profile.m_eType == AROR_EForceType.AIRBORNE;
			entries.Insert(entry);
		}
	}

	bool HasMarsOrigin_S(AROR_EFactionRole role, string targetId)
	{
		return Replication.IsServer() && m_bExecuteOperations && HasFriendlyQRFOrigin(role, targetId, true);
	}

	//! Explain the existing origin gates without changing campaign ownership or safety rules.
	string MarsOriginFailure_S(AROR_EFactionRole role, string targetId)
	{
		if (!Replication.IsServer())
			return "Server origin check required";
		if (!m_bExecuteOperations)
			return "Operation execution is disabled; check server configuration/validation logs";
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations || !locations.GetLocationById(targetId))
			return "Target location is unavailable";
		AROR_LocationComponent target = locations.GetLocationById(targetId);
		array<AROR_LocationComponent> sites = {};
		locations.GetLocations(sites);
		string details = "";
		foreach (AROR_LocationComponent site : sites)
		{
			if (!site || site == target || site.GetControllingRole() != role || !AROR_QRFOriginPolicy.IsOriginType(site.GetLocationType()))
				continue;
			string reason = "unavailable";
			if (!IsLaunchPositionSafe(site.GetCenter(), target.GetCenter()))
				reason = "launch safety/player clearance";
			else if (AROR_RecaptureComponent.IsContested(site.GetId()))
				reason = "contested";
			else if (m_mOriginLaunchedAt.Contains(site.GetId()) && m_iExecutorClock - m_mOriginLaunchedAt.Get(site.GetId()) < m_iOriginLaunchCooldown)
				reason = "launch cooldown";
			else if (IsGarrisonDepleted(site))
				reason = "garrison depleted";
			else if (m_aRoadlessOrigins.Contains(site.GetId()))
				reason = "no reachable road";
			details += site.GetName() + ": " + reason + "; ";
		}
		if (details.IsEmpty())
			return "That army owns no eligible military launch territory";
		Print("[AROR MARS] Origin rejection: " + details, LogLevel.WARNING);
		return "No safe launch origin. " + details;
	}
	// Shared tactical state belongs to this class: base methods cannot see later modded layers.
	[Attribute("0", desc: "Enable shared QRF tactical coordination and finite staged waves for both enemy roles")]
	protected bool m_bQRFTacticalCommander;
	[Attribute("1", desc: "Keep later-wave elements as reservations without actors until released")]
	protected bool m_bQRFTwoWaves;
	[Attribute("3", desc: "Minimum tier eligible for two waves (requires at least four elements)", params: "1 5 1")]
	protected int m_iQRFSplitTier;
	[Attribute("60", desc: "Seconds after first dispatch before reserve release; losses can shorten this to 15 seconds", params: "15 180 1")]
	protected int m_iQRFReinforcementDelay;
	[Attribute("300", desc: "Cancel unspawned reserves after this many seconds from first dispatch", params: "60 480 1")]
	protected int m_iQRFReserveWait;
	[Attribute("6", desc: "Maximum live native ground packages before constructing more tactical QRF elements", params: "4 8 1")]
	protected int m_iQRFMaxLiveGround;
	[Attribute("6", desc: "Maximum live aircraft before constructing more QRF transport/gunship pairs", params: "6 10 1")]
	protected int m_iQRFMaxLiveAircraft;
	[Attribute("600", desc: "Seconds of active tactical tasking before ordered withdrawal", params: "180 720 1")]
	protected int m_iQRFTacticalDuration;
	[Attribute("900", desc: "Fixed town-security duration after the first ground element physically investigates the town; zero restores legacy timed/casualty withdrawal", params: "0 1800 30")]
	protected int m_iQRFTownHoldDuration;
	[Attribute("15", desc: "Minimum seconds between tactical contact retasks and routine order updates", params: "10 60 1")]
	protected int m_iQRFOrderInterval;
	[Attribute("12", desc: "Maximum reachable-road queries per tactical vehicle approach; tries alternate wedges/radii before failing", params: "4 16 1")]
	protected int m_iQRFApproachAttempts;
	[Attribute("150", desc: "Maximum road-launch distance from the friendly QRF origin, metres. A bounded 36-query search expands to this radius while preserving collision, slope, water, player and package separation checks.", params: "75 300 25")]
	protected float m_fQRFRoadLaunchRadius;
	[Attribute("20", desc: "Stopped vehicle arrival tolerance around the selected QRF delivery road point, metres", params: "10 40 1")]
	protected float m_fQRFVehicleArrivalTolerance;
	[Attribute("100", desc: "Maximum distance from town objective for ground QRF delivery planning and physical unloading, metres", params: "50 150 5")]
	protected float m_fQRFObjectiveDeliveryRadius;
	[Attribute("300", desc: "If no road stop fits the objective area, allow infantry to dismount at a validated nearby road and walk to the objective. Security hold still requires physical objective entry. Zero disables this fallback.", params: "0 350 10")]
	protected float m_fQRFMaximumRoadDismount;
	[Attribute("0", desc: "Require a currently friendly outpost, base or airport for QRF dispatch. When enabled, configured role coordinates cannot spawn forces from lost/enemy territory.")]
	protected bool m_bQRFRequireFriendlyOrigin;
	[Attribute("600", desc: "Maximum seconds for ground QRF ingress, including bounded stalled-route recovery", params: "120 600 1")]
	protected int m_iQRFIngressTimeout;
	[Attribute("90", desc: "Seconds without at least 20 m of net progress toward the objective before a moving ground response tries a bounded alternate road/foot approach", params: "60 300 5")]
	protected int m_iQRFDetourTimeout;
	[Attribute("30", desc: "Maximum seconds for animated passenger exits after stopping for a tactical drop", params: "15 90 1")]
	protected int m_iQRFExitTimeout;
	[Attribute("2", desc: "Additional animated-exit deadline windows for fixed security tasks before this package withdraws; zero disables retries", params: "0 5 1")]
	protected int m_iQRFExitRetryLimit;
	[Attribute("10", desc: "Seconds to allow normal ground passenger exits before native cleared-position recovery for stuck AI passengers; zero disables recovery", params: "0 20 1")]
	protected int m_iQRFExitRecoveryDelay;
	[Attribute("180", desc: "Maximum seconds for an infantry investigation/patrol leg, scaled to walking distance", params: "60 300 15")]
	protected int m_iQRFInfantryLegTimeout;
	[Attribute("1.5", desc: "Nominal infantry walking speed for leg time estimates, metres per second", params: "0.5 3 0.1")]
	protected float m_fQRFInfantryPlanningSpeed;
	[Attribute("100", desc: "Living ground personnel must return within this radius of the friendly origin before recovery, metres", params: "20 200 5")]
	protected float m_fQRFRecoveryRadius;
	[Attribute("125", desc: "Surviving ground vehicles must return within this radius of the friendly origin before recovery, metres", params: "20 250 5")]
	protected float m_fQRFVehicleRecoveryRadius;
	[Attribute("200", desc: "Preferred vehicle dismount standoff with fresh hostile reports; road snapping may offset this by up to 75 m", params: "100 350 1")]
	protected float m_fQRFThreatDismount;
	[Attribute("0.4", desc: "Living fraction at or below which the QRF withdraws when no reserve is available", params: "0.1 0.7 0.05")]
	protected float m_fQRFWithdrawFraction;
	[Attribute("0.5", desc: "Individual package living fraction triggering local withdrawal", params: "0.1 0.8 0.05")]
	protected float m_fQRFLocalWithdrawFraction;
	[Attribute("120", desc: "Minimum terminal ground casualty retention before distance-based cleanup", params: "30 600 1")]
	protected int m_iQRFGroundRetention;
	[Attribute("1200", desc: "Every owned ground member/vehicle must be this far from all players before tactical removal", params: "500 2500 1")]
	protected float m_fQRFGroundCleanupDistance;
	protected int m_iLastQRFConstructionTick = -1;
	protected int m_iQRFConstructionIndex = -1;
	protected string m_sQRFConstructionOperation;

	[Attribute("0", desc: "Enable only after configuring infantry groups, waypoints and safe launch positions")]
	protected bool m_bExecuteOperations;
	[Attribute("", UIWidgets.ComboBox, desc: "Infantry unit types for responses without a force profile, in order (the first leads). Empty uses a fire team.", enums: ParamEnumArray.FromEnum(AROR_EUnitType))]
	protected ref array<AROR_EUnitType> m_aLegacyInfantryUnits;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Attack waypoint prefab, e.g. a search-and-destroy waypoint", params: "et")]
	protected ResourceName m_sAttackWaypointPrefab;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "QRF waypoint prefab, e.g. a defend waypoint", params: "et")]
	protected ResourceName m_sQRFWaypointPrefab;
	[Attribute("0 0 0", UIWidgets.Coords, desc: "Occupier infantry launch position; choose navigable land in the test world")]
	protected vector m_vOccupiersSpawnPosition;
	[Attribute("0 0 0", UIWidgets.Coords, desc: "Invader infantry launch position; choose navigable land in the test world")]
	protected vector m_vInvadersSpawnPosition;
	[Attribute("4", desc: "Maximum concurrent executor groups across both roles")]
	protected int m_iMaxGroups;
	[Attribute("32", desc: "Maximum reserved infantry prefab slots across pending and active groups")]
	protected int m_iMaxMembers;
	[Attribute("150", desc: "Minimum launch distance from the objective centre in metres")]
	protected float m_fMinTargetDistance;
	[Attribute("100", desc: "Minimum launch distance from player-controlled characters in metres")]
	protected float m_fMinPlayerDistance;
	[Attribute("600", desc: "Preferred minimum distance from a launch site to its target, metres, so a response travels instead of appearing beside the fight. Closer sites are used only when none qualifies.", params: "0 3000 50", category: "Launch origins")]
	protected float m_fMinOriginDistance;
	[Attribute("600", desc: "Seconds before the same base can launch another response or attack", params: "0 3600 30", category: "Launch origins")]
	protected int m_iOriginLaunchCooldown;
	[Attribute("0.5", desc: "A base whose garrison has lost more than this share of its infantry and crew cannot launch", params: "0 1 0.05", category: "Launch origins")]
	protected float m_fOriginMaxGarrisonLoss;
	[Attribute("2", desc: "Route-cost multiplier for the part of the straight line to the target that crosses water", params: "1 10 0.5", category: "Launch origins")]
	protected float m_fOriginWaterPenalty;
	protected ref map<string, int> m_mOriginLaunchedAt = new map<string, int>();
	//! Bases where a road launch found no reachable road at all; skipped as launch sites for the session
	protected ref array<string> m_aRoadlessOrigins = {};
	//! Metres every player must be from a package that never boarded before it is cleared away
	protected static const float UNDISPATCHED_CLEANUP_DISTANCE = 300;
	[Attribute("500", desc: "Attacks: distance of each package's road rally point from the target, metres", params: "200 1500 50", category: "Attacks")]
	protected float m_fAttackStagingDistance;
	[Attribute("180", desc: "Attacks: seconds after the first package starts staging before the assault goes regardless", params: "30 600 10", category: "Attacks")]
	protected int m_iAttackStagingTimeout;
	[Attribute("6", desc: "Support per minute a response shifts to its army while holding a lost town and outnumbering all hostile military forces there (percentage points). Zero disables.", params: "0 30 1")]
	protected float m_fQRFTownSupportPerMinute;
	protected static const int TACTICAL_CHECK_SECONDS = 5;	//!< Commander contact/state check interval
	[Attribute("50", desc: "Waypoint completion radius and arrival reporting distance in metres")]
	protected float m_fArrivalRadius;
	[Attribute("5", desc: "Seconds between wake requests if the engine evicts an active operation group")]
	protected int m_iWakeRetryInterval;
	[Attribute("10", desc: "Seconds of incomplete initial ground boarding before retries may interrupt a stuck native get-in queue for this package's unseated AI", params: "5 60 1")]
	protected int m_iQRFSeatRecoveryDelay;
	[Attribute("", desc: "Optional force pool. Empty retains the existing infantry-only configuration.")]
	protected ref array<ref AROR_ForceProfile> m_aForceProfiles;
	[Attribute("", desc: "Optional explicit ground QRF recipe ID; empty restores existing profile/level selection. Same ID can have one entry per role.")]
	protected string m_sGroundQRFRecipeId;
	[Attribute("", desc: "Optional Invader-specific ground recipe ID; overrides the shared ground recipe for this role only. Does not enable Invader dispatch.")]
	protected string m_sInvadersGroundQRFRecipeId;

	protected string GetGroundQRFRecipeId(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.INVADERS && !m_sInvadersGroundQRFRecipeId.IsEmpty())
			return m_sInvadersGroundQRFRecipeId;
		return m_sGroundQRFRecipeId;
	}
	[Attribute("", desc: "Authored ground QRF recipes resolving existing faction-specific force profiles")]
	protected ref array<ref AROR_GroundQRFRecipe> m_aGroundQRFRecipes;
	protected ref AROR_QRFAvailabilityProvider m_QRFAvailabilityProvider;

	//! Registration does not enable Invader dispatch or manufacture stock.
	void RegisterQRFAvailabilityProvider_S(AROR_QRFAvailabilityProvider provider)
	{
		if (Replication.IsServer())
			m_QRFAvailabilityProvider = provider;
	}

	//! Read-only planning seam for future territory/strategy integration. Never spawns.
	AROR_QRFSelectionDecision PreviewInvaderQRF_S(string targetId, AROR_QRFSelectionContext context)
	{
		AROR_QRFSelectionDecision rejected = new AROR_QRFSelectionDecision();
		rejected.m_sReason = "Server campaign/executor not ready for planning";
		if (!Replication.IsServer() || !m_bExecuteOperations || !m_Operations)
			return rejected;
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!commanders || !commanders.IsCampaignActive() || !locations)
			return rejected;
		AROR_Commander commander = commanders.GetCommander(AROR_EFactionRole.INVADERS);
		if (!commander)
			return rejected;
		AROR_LocationComponent target = locations.GetLocationById(targetId);
		rejected.m_sReason = "Target is not a registered Invader-controlled location";
		if (!target || target.GetControllingRole() != AROR_EFactionRole.INVADERS)
			return rejected;
		rejected.m_sReason = "No military availability provider registered; no resources assumed";
		if (!m_QRFAvailabilityProvider)
			return rejected;
		int now = m_Operations.GetClockSeconds();
		AROR_QRFAvailabilitySnapshot snapshot = m_QRFAvailabilityProvider.CaptureAvailability_S(AROR_EFactionRole.INVADERS, commander.GetFactionKey(), targetId, now);
		rejected.m_sReason = "Availability provider returned no usable snapshot";
		if (!snapshot || !snapshot.IsValidAt(now))
			return rejected;
		AROR_LocationComponent origin = locations.GetLocationById(snapshot.m_sOriginLocationId);
		rejected.m_sReason = "Resource origin is not an eligible friendly outpost";
		if (!origin || origin == target || !AROR_QRFOriginPolicy.IsOriginType(origin.GetLocationType()))
			return rejected;
		if (origin.GetControllingRole() != AROR_EFactionRole.INVADERS || !IsLaunchPositionSafe(origin.GetCenter(), target.GetCenter()))
			return rejected;
		AROR_QRFAvailabilitySnapshot limited = snapshot.WithRuntimeCapacity(m_iMaxGroups - GetReservedGroups(), m_iMaxMembers - GetReservedMembers(), m_iMaxVehicles - GetReservedVehicles());
		return AROR_QRFSelectionPolicy.Select(m_aGroundQRFRecipes, m_aForceProfiles, AROR_EFactionRole.INVADERS, commander.GetFactionKey(), context, limited, now);
	}
	[Attribute("4", desc: "Maximum reserved ground vehicles across both roles")]
	protected int m_iMaxVehicles;
	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty base group used for separate vehicle crews", params: "et")]
	protected ResourceName m_sCrewGroupPrefab;
	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, desc: "Vehicle approach waypoint", params: "et")]
	protected ResourceName m_sMoveWaypointPrefab;
	[Attribute("", desc: "Workbench only: force this profile ID for its matching role instead of weighted selection. Empty uses random selection.")]
	protected string m_sWorkbenchForceProfileId;
	[Attribute("", desc: "Response levels 1-5 with independent attack/QRF weights. Empty creates five equally weighted levels.")]
	protected ref array<ref AROR_ResponseLevel> m_aResponseLevels;
	[Attribute("0", desc: "Workbench only: force level 1-5; zero uses weighted random selection")]
	protected int m_iWorkbenchResponseLevel;
	[Attribute("25", desc: "Metres between package launch positions in a multi-package response")]
	protected float m_fPackageSpacing;
	[Attribute("1", desc: "Mounted QRFs investigate reported contact then sweep separate town sectors before deployment. No live player tracking.")]
	protected bool m_bQRFVehicleSearch;
	[Attribute("180", desc: "Maximum seconds for a mounted town search", params: "1 900 1")]
	protected int m_iQRFSearchDuration;
	[Attribute("6", desc: "Maximum road stops per mounted QRF package", params: "1 12 1")]
	protected int m_iQRFSearchStops;
	[Attribute("8", desc: "Seconds to pause at each reached search stop", params: "0 60 1")]
	protected int m_iQRFSearchDwell;
	[Attribute("60", desc: "Seconds before skipping a stalled search leg", params: "10 300 1")]
	protected int m_iQRFSearchLegTimeout;
	[Attribute("120", desc: "Maximum age in seconds of attacker-position intel when the QRF starts searching", params: "1 900 1")]
	protected int m_iQRFContactMaxAge;
	[Attribute("150", desc: "Metres beyond the location radius allowed for investigating reported contact", params: "0 500 10")]
	protected float m_fQRFSearchMargin;
	[Attribute("50", desc: "Minimum metres between road stops assigned to any packages of the same QRF", params: "10 150 5")]
	protected float m_fQRFSearchStopSpacing;
	[Attribute("0", desc: "Temporary diagnostic: report the first same-faction damage event on each operation-owned character/vehicle. Does not change combat behavior.")]
	protected bool m_bLogFriendlyDamage;

	protected AROR_OperationManagerComponent m_Operations;
	protected int m_iExecutorClock;
	//! Trusted server spotting adapter. Never called remotely and never sent as a raw client list.
	void GetQRFSpottingAssets_S(array<IEntity> vehicles, array<int> kinds, array<int> sides)
	{
		vehicles.Clear();
		kinds.Clear();
		sides.Clear();
		if (!Replication.IsServer() || !m_Operations)
			return;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return;
		Faction resistance = roles.GetFaction(AROR_EFactionRole.RESISTANCE);
		if (!resistance)
			return;
		foreach (AROR_OperationForce force : m_aForces)
		{
			AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
			AROR_Operation operation = m_Operations.GetOperation(force.m_sOperationId);
			if (!ground || !ground.m_Profile || force.m_bReservation || !operation || !operation.UsesTacticalResponse())
				continue;
			Faction campaignFaction = roles.GetFaction(operation.GetRole());
			if (!campaignFaction || !resistance.IsFactionEnemy(campaignFaction))
				continue;
			if (ground.m_Vehicle)
			{
				vehicles.Insert(ground.m_Vehicle);
				kinds.Insert(ground.m_Profile.GetSpottingKind());
				sides.Insert(operation.GetRole());
			}
			AROR_ReaperAirForce air = AROR_ReaperAirForce.Cast(force);
			if (air && air.m_Escort && air.m_Escort.m_Vehicle)
			{
				vehicles.Insert(air.m_Escort.m_Vehicle);
				kinds.Insert(4);
				sides.Insert(operation.GetRole());
			}
		}
	}
	protected ref array<ref AROR_OperationForce> m_aForces = {};
	protected ref map<string, string> m_mWaitReasons = new map<string, string>();

	//! Log changed blockers only; requests are retried each second without flooding the console.
	protected void ReportSpawnWait(AROR_Operation operation, string reason)
	{
		string previous;
		if (m_mWaitReasons.Find(operation.GetId(), previous) && previous == reason)
			return;
		m_mWaitReasons.Set(operation.GetId(), reason);
		Print(string.Format("[AROR] Executor waiting for %1: %2", operation.GetId(), reason), LogLevel.WARNING);
	}

	protected AROR_OperationForce FindForce(string operationId)
	{
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (force.m_sOperationId == operationId)
				return force;
		}
		return null;
	}

	protected void ReleaseForce(AROR_OperationForce force)
	{
		if (!force)
			return;
		AROR_QRFDeferredForce pending = AROR_QRFDeferredForce.Cast(force);
		if (pending && pending.m_Funding && pending.m_Profile && pending.m_iReservedMembers > 0)
		{
			pending.m_Funding.m_iQuote = Math.Max(0, pending.m_Funding.m_iQuote - pending.m_Profile.ReplacementQuote());
			// The deferred object can still appear in an existing monitor snapshot; release once.
			pending.m_iReservedMembers = 0;
			pending.m_bOwnsPackageQuote = false;
		}
		AROR_ReaperAirForce pendingAir = AROR_ReaperAirForce.Cast(force);
		if (pendingAir && pendingAir.m_bConstructionPending && pendingAir.m_Funding && pendingAir.m_Profile)
		{
			pendingAir.m_Funding.m_iQuote = Math.Max(0, pendingAir.m_Funding.m_iQuote - pendingAir.m_Profile.ReplacementQuote());
			pendingAir.m_bConstructionPending = false;
			pendingAir.m_iReservedMembers = 0;
			pendingAir.m_bOwnsPackageQuote = false;
		}
		// A failed construction attempt may have consumed its virtual reservation without creating actors.
		// Release that package quote now; do not hold unbuilt collateral for the surviving operation.
		AROR_GroundOperationForce emptyGround = AROR_GroundOperationForce.Cast(force);
		if (emptyGround && emptyGround.m_Funding && emptyGround.m_Profile && emptyGround.m_iReservedMembers > 0)
		{
			if (!emptyGround.m_Vehicle && !emptyGround.m_Group && !emptyGround.m_CrewGroup && (!pendingAir || !pendingAir.m_Escort))
			{
				array<IEntity> emptyOwnedMembers = {};
				emptyGround.GetOwnedMembers(emptyOwnedMembers);
				if (emptyOwnedMembers.IsEmpty())
				{
					emptyGround.m_Funding.m_iQuote = Math.Max(0, emptyGround.m_Funding.m_iQuote - emptyGround.m_Profile.ReplacementQuote());
					emptyGround.m_iReservedMembers = 0;
					emptyGround.m_bOwnsPackageQuote = false;
				}
			}
		}
		force.Cleanup_S();
		m_aForces.RemoveItem(force);
	}

	protected AROR_OperationForce FindPackage(string operationId, int packageIndex)
	{
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (!force.m_bReservation && force.m_sOperationId == operationId && force.m_iPackageIndex == packageIndex)
				return force;
		}
		return null;
	}

	//! Keep materialized tactical actors and casualties until normal player-distance retention permits removal.
	//! Empty reservations still release immediately; world unload uses direct cleanup.
	protected bool ShouldRetainTacticalForce(AROR_OperationForce force)
	{
		if (!force || !force.m_bTacticalControlled || force.m_bReservation || AROR_ReaperAirForce.Cast(force))
			return false;
		if (force.m_bOrdersIssued || force.m_Group)
			return true;
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
		if (ground && (ground.m_Vehicle || ground.m_CrewGroup))
			return true;
		array<IEntity> members = {};
		force.GetOwnedMembers(members);
		return !members.IsEmpty();
	}

	protected void ReleaseOperationForces(string operationId)
	{
		array<ref AROR_OperationForce> snapshot = {};
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (force.m_sOperationId == operationId)
				snapshot.Insert(force);
		}
		foreach (AROR_OperationForce packageForce : snapshot)
		{
			if (ShouldRetainTacticalForce(packageForce))
				RetainTacticalForce(packageForce);
			else
				ReleaseForce(packageForce);
		}
		if (snapshot.Count() > 0)
			Print(string.Format("[AROR] Operation %1 terminal handling completed for %2 force records; tactical retention and player preservation rules apply", operationId, snapshot.Count()), LogLevel.NORMAL);
	}

	protected void ReportPackageProgress(AROR_OperationForce force, string progress)
	{
		if (force.m_sLastProgress == progress)
			return;
		force.m_sLastProgress = progress;
		Print(string.Format("[AROR] QRF/attack %1 package=%2: %3", force.m_sOperationId, force.m_iPackageIndex + 1, progress), LogLevel.NORMAL);
	}

	protected void TryActivateOperation(AROR_Operation operation)
	{
		if (!operation || operation.IsTerminal())
			return;
		AROR_QRFTacticalCommander tactical = operation.m_TacticalCommander;
		if (operation.GetState() != AROR_EOperationState.SPAWNING && !tactical)
			return;
		if (tactical && tactical.m_bWithdraw)
			return;
		int packages;
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (force.m_sOperationId != operation.GetId())
				continue;
			if (force.m_bTacticalRetained)
				continue;
			if (tactical && tactical.IsReserve(force.m_iPackageIndex) && !force.m_bReady)
				continue;
			if (force.m_bReservation || !force.m_bReady)
				return;
			packages++;
		}
		if (packages == 0)
			return;
		int required = operation.GetPackageCount();
		if (tactical)
			required = tactical.m_iFirstWaveCount;
		if (operation.GetState() == AROR_EOperationState.SPAWNING)
		{
			if ((!operation.GetRecipeId().IsEmpty() || tactical) && packages != required)
				return;
		}
		foreach (AROR_OperationForce ready : m_aForces)
		{
			if (ready.m_sOperationId != operation.GetId() || ready.m_bOrdersIssued || ready.m_bReservation || !ready.m_bReady || ready.m_bTacticalRetained)
				continue;
			if (!DispatchReadyPackage(ready, operation))
			{
				// Active reinforcement losses degrade one package; required lead losses still abort SPAWNING.
				FailPackage(ready, operation, "Ready package could not receive deployment orders");
				return;
			}
		}
		if (operation.GetState() == AROR_EOperationState.SPAWNING)
		{
			// Estimated before activation so the dispatch headline can carry it
			operation.m_iEtaSeconds = EstimateArrival(operation);
			operation.m_iEtaSetAt = m_Operations.GetClockSeconds();
			m_Operations.MarkActive_S(operation.GetId());
			if (tactical)
				tactical.MarkDispatched(m_iExecutorClock);
		}
	}

	//! Rough seconds until the first lead package reaches the objective, or -1. Each package is timed from where
	//! it is now (vehicle, or group for infantry and helicopters), falling back to the launch site.
	protected int EstimateArrival(AROR_Operation operation)
	{
		AROR_QRFTacticalCommander tactical = operation.m_TacticalCommander;
		vector objective;
		if (tactical)
		{
			objective = tactical.m_vTown;
		}
		else
		{
			AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
			AROR_LocationComponent target;
			if (locations)
				target = locations.GetLocationById(operation.GetTargetLocationId());
			if (!target)
				return -1;
			objective = target.GetCenter();
		}
		int best = -1;
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (force.m_sOperationId != operation.GetId() || force.m_bReservation || force.m_bTacticalRetained)
				continue;
			if (tactical && tactical.IsReserve(force.m_iPackageIndex))
				continue;
			AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
			IEntity mover;
			if (ground && ground.m_Vehicle)
				mover = ground.m_Vehicle;
			else
				mover = force.m_Group;
			if (!mover)
				continue;
			int eta = AROR_OperationEta.Estimate(vector.DistanceXZ(mover.GetOrigin(), objective), AROR_ReaperAirForce.Cast(force) != null);
			if (eta >= 0 && (best < 0 || eta < best))
				best = eta;
		}
		if (best < 0 && tactical)
			best = AROR_OperationEta.Estimate(vector.DistanceXZ(tactical.m_vOrigin, objective), false);
		return best;
	}

	//! Dispatch one prepared package after the shared readiness barrier.
	protected bool DispatchReadyPackage(AROR_OperationForce ready, AROR_Operation operation)
	{
		AROR_QRFTacticalCommander tactical = operation.m_TacticalCommander;
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(ready);
		if (ground)
		{
			if (!ground.m_Vehicle || !ground.m_CrewGroup || !ground.m_Group)
				return false;
			if (ground.m_CrewGroup.GetPlayerCount() > 0 || ground.m_Group.GetPlayerCount() > 0)
				return false;
			if (ground.CountCrew() < 1 || !ground.IsMountComplete())
				return false;
			DamageManagerComponent dispatchDamage = DamageManagerComponent.Cast(ground.m_Vehicle.FindComponent(DamageManagerComponent));
			if (dispatchDamage && dispatchDamage.IsDestroyed())
				return false;
			if (!ground.m_Waypoint || ((ground.m_Profile.m_eType != AROR_EForceType.ARMOUR || ground.m_Profile.m_bFillPassengerSeats) && !ground.m_CrewWaypoint))
				return false;
			if ((ground.m_Profile.m_eType == AROR_EForceType.ARMOUR && !ground.m_Profile.m_bFillPassengerSeats))
				ground.m_CrewGroup.AddWaypoint(ground.m_Waypoint);
			else
				ground.m_CrewGroup.AddWaypoint(ground.m_CrewWaypoint);
		}
		else
		{
			if (!ready.m_Group || !ready.m_Waypoint || ready.m_Group.GetPlayerCount() > 0)
				return false;
			ready.m_Group.AddWaypoint(ready.m_Waypoint);
		}
		ready.m_bOrdersIssued = true;
		ready.m_iTacticalLegDeadline = m_iExecutorClock + m_iQRFIngressTimeout;
		if (ground && tactical && tactical.m_bAssault && !tactical.m_bAssaultReleased)
			BeginStaging(ground, tactical, operation);
		return true;
	}

	//! One lost element reduces a multi-package response; the final lost element fails it.
	protected void FailPackage(AROR_OperationForce force, AROR_Operation operation, string reason)
	{
		if (operation.GetState() == AROR_EOperationState.SPAWNING && (!operation.GetRecipeId().IsEmpty() || operation.m_TacticalCommander))
		{
			FailOperation(operation, "Required composite element lost before dispatch: " + reason);
			return;
		}
		Print(string.Format("[AROR] Operation %1 lost package %2: %3", operation.GetId(), force.m_iPackageIndex + 1, reason), LogLevel.NORMAL);
		if (operation.m_TacticalCommander)
			operation.m_TacticalCommander.MarkLost(force.m_iPackageIndex);
		if (ShouldRetainTacticalForce(force))
			RetainTacticalForce(force);
		else
			ReleaseForce(force);
		foreach (AROR_OperationForce survivor : m_aForces)
		{
			if (survivor.m_sOperationId == operation.GetId() && !survivor.m_bTacticalRetained && (!survivor.m_bReservation || AROR_QRFDeferredForce.Cast(survivor)))
			{
				TryActivateOperation(operation);
				return;
			}
		}
		FailOperation(operation, reason);
	}

	protected void FailOperation(AROR_Operation operation, string reason)
	{
		if (operation.GetState() == AROR_EOperationState.ACTIVE && operation.m_TacticalCommander && m_sQRFConstructionOperation == operation.GetId())
		{
			AROR_OperationForce partial = FindPackage(operation.GetId(), m_iQRFConstructionIndex);
			if (partial)
			{
				if (ShouldRetainTacticalForce(partial))
					RetainTacticalForce(partial);
				else
					ReleaseForce(partial);
			}
			operation.m_TacticalCommander.MarkLost(m_iQRFConstructionIndex);
			Print(string.Format("[AROR TACTICAL] %1 reserve construction failed: %2; existing force continues", operation.GetId(), reason), LogLevel.WARNING);
			return;
		}
		m_Operations.ResolveOperation_S(operation.GetId(), AROR_EOperationState.FAILED, reason);
	}
	protected void FailGroundPlanning(AROR_Operation operation, string reason)
	{
		if (operation.GetState() == AROR_EOperationState.SPAWNING && m_Operations.ReportQRFPlanningFailure_S(operation.GetId(), reason))
			return;
		FailOperation(operation, reason);
	}

	protected bool IsLaunchPositionSafe(vector position, vector target)
	{
		if (GetGame().GetWorld().GetSurfaceY(position[0], position[2]) <= 0 || vector.DistanceXZ(position, target) < m_fMinTargetDistance)
			return false;
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (player && vector.DistanceXZ(position, player.GetOrigin()) < m_fMinPlayerDistance)
				return false;
		}
		return true;
	}

	protected IEntity SpawnResource(Resource resource, vector position)
	{
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		spawnParams.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
	}

	protected int GetReservedGroups()
	{
		int count;
		foreach (AROR_OperationForce force : m_aForces)
			count += force.m_iReservedGroups;
		return count;
	}

	protected int GetReservedMembers()
	{
		int count;
		foreach (AROR_OperationForce force : m_aForces)
			count += force.m_iReservedMembers;
		return count;
	}

	protected int GetReservedVehicles()
	{
		int count;
		foreach (AROR_OperationForce force : m_aForces)
			count += force.m_iReservedVehicles;
		return count;
	}

	protected void ConvertReservation(AROR_OperationForce reservation, AROR_ForceProfile profile)
	{
		if (!reservation || !profile)
			return;
		reservation.m_bOwnsPackageQuote = false;
		reservation.m_iReservedGroups -= profile.GetGroupBudget();
		reservation.m_iReservedMembers -= profile.m_iPersonnelBudget;
		if (profile.m_eType != AROR_EForceType.INFANTRY)
			reservation.m_iReservedVehicles--;
	}

	protected AROR_ForceProfile SelectProfile(AROR_Operation operation, AROR_Commander commander)
	{
		if (!operation.m_sRequestedProfileId.IsEmpty())
		{
			foreach (AROR_ForceProfile requested : m_aForceProfiles)
			{
				if (!requested || requested.m_sId != operation.m_sRequestedProfileId)
					continue;
				if (operation.m_bUseResponseMetrics && !AROR_ResponsePolicy.IsProfileUnlocked(requested, operation.m_iPoolReadinessLevel))
					continue;
				int weight = AROR_ForcePool.GetEligibleWeight(requested, commander.GetRole(), commander.GetFactionKey(), operation.GetType(), m_iMaxGroups - GetReservedGroups(), m_iMaxMembers - GetReservedMembers(), m_iMaxVehicles - GetReservedVehicles(), operation.m_iRequestedTier);
				if (weight > 0 && operation.BindForceProfile_S(requested.m_sId, operation.m_iRequestedTier))
					return requested;
			}
			ReportSpawnWait(operation, "Selected MARS profile cannot fit remaining capacity");
			return null;
		}
		if (!operation.GetForceProfileId().IsEmpty())
		{
			foreach (AROR_ForceProfile bound : m_aForceProfiles)
			{
				if (bound && bound.m_sId == operation.GetForceProfileId() && AROR_ForcePool.GetEligibleWeight(bound, commander.GetRole(), commander.GetFactionKey(), operation.GetType(), m_iMaxGroups - GetReservedGroups(), m_iMaxMembers - GetReservedMembers(), m_iMaxVehicles - GetReservedVehicles(), operation.GetResponseLevel()) > 0)
					return bound;
			}
			return null;
		}
		array<ref AROR_ForceProfile> candidates = {};
#ifdef WORKBENCH
		if (!m_sWorkbenchForceProfileId.IsEmpty())
		{
			foreach (AROR_ForceProfile forced : m_aForceProfiles)
			{
				if (forced && forced.m_sId == m_sWorkbenchForceProfileId && forced.m_eRole == commander.GetRole())
				{
					candidates.Insert(forced);
					break;
				}
			}
		}
#endif
		if (candidates.Count() == 0)
		{
			foreach (AROR_ForceProfile candidate : m_aForceProfiles)
				candidates.Insert(candidate);
		}
		int freeGroups = m_iMaxGroups - GetReservedGroups();
		int freeMembers = m_iMaxMembers - GetReservedMembers();
		int freeVehicles = m_iMaxVehicles - GetReservedVehicles();
		int level;
		int levelTotal;
		foreach (AROR_ResponseLevel response : m_aResponseLevels)
			levelTotal += AROR_ResponseScale.GetEligibleWeight(response, candidates, commander.GetRole(), commander.GetFactionKey(), operation.GetType(), freeGroups, freeMembers, freeVehicles);
		if (levelTotal <= 0)
		{
			ReportSpawnWait(operation, string.Format("No eligible response level: role=%1 faction=%2 candidates=%3 free groups=%4 members=%5 vehicles=%6; check enabled profiles, role/faction, weights and budgets", commander.GetRole(), commander.GetFactionKey(), candidates.Count(), freeGroups, freeMembers, freeVehicles));
			return null;
		}
		level = AROR_ResponseScale.PickLevel(m_aResponseLevels, candidates, commander.GetRole(), commander.GetFactionKey(), operation.GetType(), freeGroups, freeMembers, freeVehicles, Math.RandomInt(0, levelTotal));
#ifdef WORKBENCH
		if (m_iWorkbenchResponseLevel > 0)
		{
			level = 0;
			foreach (AROR_ResponseLevel forcedLevel : m_aResponseLevels)
			{
				if (forcedLevel.m_iLevel == m_iWorkbenchResponseLevel && AROR_ResponseScale.GetEligibleWeight(forcedLevel, candidates, commander.GetRole(), commander.GetFactionKey(), operation.GetType(), freeGroups, freeMembers, freeVehicles) > 0)
					level = forcedLevel.m_iLevel;
			}
		}
#endif
		if (level < 1)
		{
			ReportSpawnWait(operation, "Selected/Workbench-forced response level is not eligible for this role and available capacity");
			return null;
		}
		int total;
		foreach (AROR_ForceProfile profile : candidates)
			total += AROR_ForcePool.GetEligibleWeight(profile, commander.GetRole(), commander.GetFactionKey(), operation.GetType(), freeGroups, freeMembers, freeVehicles, level);
		if (total <= 0)
			return null;
		AROR_ForceProfile selected = AROR_ForcePool.Pick(candidates, commander.GetRole(), commander.GetFactionKey(), operation.GetType(), freeGroups, freeMembers, freeVehicles, Math.RandomInt(0, total), level);
		if (!selected || !operation.BindForceProfile_S(selected.m_sId, level))
			return null;
		Print(string.Format("[AROR] Operation %1 selected force profile %2 (type=%3), response level=%4, packages=%4", operation.GetId(), selected.m_sId, selected.m_eType, level), LogLevel.NORMAL);
		return selected;
	}

	//! Shared by ground and air QRFs. Existing role coordinates remain a scenario fallback.
	protected vector ResolveQRFOrigin(AROR_Operation operation, AROR_LocationComponent target, vector fallback, bool logOrigin = true)
	{
		if (!target || !operation.UsesTacticalResponse())
			return fallback;
		AROR_LocationManagerComponent manager = AROR_LocationManagerComponent.GetInstance();
		if (!manager)
			return fallback;
		AROR_LocationComponent nearest = BestOrigin(operation.GetRole(), target, operation);
		if (nearest)
		{
			if (logOrigin)
				Print(string.Format("[AROR] QRF %1 origin outpost=%2 distance=%3m routeCost=%4", operation.GetId(), nearest.GetId(), vector.DistanceXZ(nearest.GetCenter(), target.GetCenter()), OriginCost(nearest.GetCenter(), target.GetCenter())), LogLevel.NORMAL);
			return nearest.GetCenter();
		}
		if (logOrigin)
			Print(string.Format("[AROR] QRF %1 has no eligible friendly outpost; requireFriendlyOrigin=%2", operation.GetId(), m_bQRFRequireFriendlyOrigin), LogLevel.WARNING);
		if (m_bQRFRequireFriendlyOrigin || operation.m_bDebugRequest || operation.GetType() == AROR_EOperationType.ATTACK)
			return vector.Zero;
		return fallback;
	}

	//! A missing flight adapter must fail without deploying ground-only remnants.
	protected void StartJointResponse(AROR_Operation operation, AROR_GroundQRFRecipe recipe, array<AROR_ForceProfile> elements)
	{
		FailOperation(operation, "Joint response requires the airborne adapter");
	}

	protected void StartGroundQRFRecipe(AROR_Operation operation)
	{
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!commanders)
		{
			ReportSpawnWait(operation, "Commander manager unavailable for ground recipe");
			return;
		}
		AROR_Commander commander = commanders.GetCommander(operation.GetRole());
		if (!commander || commander.IsPlayerDirected())
		{
			ReportSpawnWait(operation, string.Format("no AI commander for role %1", operation.GetRole()));
			return;
		}
		if (!operation.m_bHasOriginOverride && !HasFriendlyQRFOrigin(operation.GetRole(), operation.GetTargetLocationId()))
		{
			FailGroundPlanning(operation, "No safe friendly military origin remains for this capture response");
			return;
		}
		AROR_GroundQRFRecipe recipe;
		if (!operation.m_sRequestedRecipeId.IsEmpty())
		{
			foreach (AROR_GroundQRFRecipe requested : m_aGroundQRFRecipes)
			{
				if (requested && requested.m_sId == operation.m_sRequestedRecipeId && requested.m_eRole == operation.GetRole())
					recipe = requested;
			}
		}
		else if (m_Operations.IsQRFStrategyEnabled())
			recipe = SelectStrategicGroundRecipe(operation, commander);
		foreach (AROR_GroundQRFRecipe candidate : m_aGroundQRFRecipes)
		{
			if (operation.m_sRequestedRecipeId.IsEmpty() && !m_Operations.IsQRFStrategyEnabled() && candidate && candidate.m_sId == GetGroundQRFRecipeId(operation.GetRole()) && candidate.m_eRole == operation.GetRole())
				recipe = candidate;
		}
		if (!recipe && m_Operations.IsQRFStrategyEnabled())
		{
			ReportSpawnWait(operation, "No affordable, capacity-eligible ground recipe at or below the bound desired tier");
			return;
		}
		array<AROR_ForceProfile> elements = {};
		int poolWar = 5;
		if (!operation.m_bDebugRequest && m_Operations.IsQRFStrategyEnabled())
			poolWar = QRFPoolReadinessLevel(operation);
		if (operation.m_bDebugRequest && recipe)
			poolWar = recipe.m_iMaximumReadinessLevel;
		if (operation.m_bUseResponseMetrics)
			poolWar = operation.m_iPoolReadinessLevel;
		if (!recipe || !recipe.Resolve(m_aForceProfiles, operation.GetRole(), commander.GetFactionKey(), elements, poolWar))
		{
			FailOperation(operation, "Ground QRF recipe missing, disabled, unsupported or faction-incompatible");
			return;
		}
		if (recipe.m_eResponseType == AROR_EResponseType.AIR_GROUND)
		{
			StartJointResponse(operation, recipe, elements);
			return;
		}
		int groups;
		int members;
		int vehicles;
		foreach (AROR_ForceProfile element : elements)
		{
			groups += element.GetGroupBudget();
			members += element.m_iPersonnelBudget;
			if (element.m_eType != AROR_EForceType.INFANTRY)
				vehicles++;
		}
		bool fits = true;
		if (GetReservedGroups() + groups > m_iMaxGroups)
			fits = false;
		if (GetReservedMembers() + members > m_iMaxMembers)
			fits = false;
		if (GetReservedVehicles() + vehicles > m_iMaxVehicles)
			fits = false;
		if (!fits)
		{
			ReportSpawnWait(operation, "Entire ground QRF recipe cannot fit remaining executor capacity");
			return;
		}
		if (!ReserveQRFFunding(operation, elements, recipe.m_iLevel))
			return;
		if (!operation.BindGroundRecipe_S(recipe.m_sId, recipe.m_iLevel, elements.Count()))
			return;
		if (m_bQRFTacticalCommander)
		{
			QueueTacticalGroundQRF(operation, elements);
			return;
		}
		AROR_OperationForce reservation = new AROR_OperationForce();
		reservation.m_sOperationId = operation.GetId();
		reservation.m_bReservation = true;
		reservation.m_iPackageIndex = -1;
		reservation.m_iReservedGroups = groups;
		reservation.m_iReservedMembers = members;
		reservation.m_iReservedVehicles = vehicles;
		m_aForces.Insert(reservation);
		Print(string.Format("[AROR] Composite QRF %1 recipe=%2 tier=%3 elements=%4 reserved groups=%5 personnel=%6 vehicles=%7", operation.GetId(), recipe.m_sId, recipe.m_iLevel, elements.Count(), groups, members, vehicles), LogLevel.NORMAL);
		for (int index = 0; index < elements.Count(); index++)
		{
			if (operation.IsTerminal())
				break;
			AROR_ForceProfile profile = elements[index];
			if (profile.m_eType == AROR_EForceType.INFANTRY)
				StartInfantryForce(operation, profile, index, reservation);
			else
				StartGroundForce(operation, commander, profile, index, reservation);
		}
		m_aForces.RemoveItem(reservation);
		TryActivateOperation(operation);
	}

	protected void StartForce(AROR_Operation operation)
	{
		if (!operation || operation.GetState() != AROR_EOperationState.SPAWNING || FindForce(operation.GetId()))
			return;
		if (!GetGame().GetAIWorld())
		{
			FailOperation(operation, "No AIWorld in this scenario; add the terrain's AIWorld prefab before deploying AI");
			return;
		}
		if (!operation.m_TacticalCommander && (operation.m_bDebugRequest || operation.GetType() == AROR_EOperationType.ATTACK) && !HasMarsOrigin_S(operation.GetRole(), operation.GetTargetLocationId()))
		{
			FailOperation(operation, "No safe owned military launch territory remains");
			return;
		}
		bool explicitProfile = !operation.m_sRequestedProfileId.IsEmpty();
		if (operation.UsesTacticalResponse() && !explicitProfile && (m_Operations.IsQRFStrategyEnabled() || !GetGroundQRFRecipeId(operation.GetRole()).IsEmpty() || !operation.m_sRequestedRecipeId.IsEmpty()))
		{
			StartGroundQRFRecipe(operation);
			return;
		}
		if (m_aForceProfiles.Count() == 0)
		{
			StartInfantryForce(operation);
			return;
		}
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!commanders)
		{
			ReportSpawnWait(operation, "Commander manager unavailable");
			return;
		}
		AROR_Commander commander = commanders.GetCommander(operation.GetRole());
		if (!commander || commander.IsPlayerDirected())
			return;
		AROR_ForceProfile profile = SelectProfile(operation, commander);
		if (!profile)
			return;
		int packages = operation.GetResponseLevel();
		if (m_bQRFTacticalCommander && operation.UsesTacticalResponse())
		{
			array<AROR_ForceProfile> elements = {};
			for (int index = 0; index < packages; index++)
				elements.Insert(profile);
			QueueTacticalGroundQRF(operation, elements);
			return;
		}
		AROR_OperationForce reservation = new AROR_OperationForce();
		reservation.m_sOperationId = operation.GetId();
		reservation.m_bReservation = true;
		reservation.m_iPackageIndex = -1;
		reservation.m_iReservedGroups = profile.GetGroupBudget() * packages;
		reservation.m_iReservedMembers = profile.m_iPersonnelBudget * packages;
		if (profile.m_eType != AROR_EForceType.INFANTRY)
			reservation.m_iReservedVehicles = packages;
		m_aForces.Insert(reservation);
		for (int packageIndex = 0; packageIndex < packages; packageIndex++)
		{
			if (operation.IsTerminal())
				break;
			if (profile.m_eType == AROR_EForceType.INFANTRY)
				StartInfantryForce(operation, profile, packageIndex, reservation);
			else
				StartGroundForce(operation, commander, profile, packageIndex, reservation);
		}
		m_aForces.RemoveItem(reservation);
		TryActivateOperation(operation);
	}

	//! Fills a group with one member per unit type of the role: the role's base character, dressed from its faction
	//! preset loadout at the role's current Readiness as it spawns. Returns the slot count.
	protected int FillUnits(SCR_AIGroup group, AROR_EFactionRole role, notnull array<AROR_EUnitType> types)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!group || !presets)
			return 0;
		return presets.FillGroup_S(group, role, types);
	}

	protected SCR_AIGroup SpawnEmptyGroup(ResourceName prefab, vector position)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;
		SCR_AIGroup.IgnoreSpawning(true);
		IEntity entity = SpawnResource(resource, position);
		SCR_AIGroup.IgnoreSpawning(false);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group && entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		return group;
	}

	protected AIWaypoint SpawnWaypoint(ResourceName prefab, vector position)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;
		IEntity entity = SpawnResource(resource, position);
		AIWaypoint waypoint = AIWaypoint.Cast(entity);
		if (!waypoint && entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		return waypoint;
	}

	protected void ConfigureGroup(SCR_AIGroup group, AROR_Operation operation)
	{
		group.SetCanDeleteIfNoPlayer(false);
		group.SetPrivate(true);
		group.SetPrivacyChangeable(false);
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		if (operation.UsesTacticalResponse())
			group.SetImportance(SCR_EAISpawnImportance.HIGH);
		else
			group.SetImportance(SCR_EAISpawnImportance.NORMAL);
	}

	protected void StartGroundForce(AROR_Operation operation, AROR_Commander commander, AROR_ForceProfile profile, int packageIndex, AROR_OperationForce reservation)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent target;
		if (locations)
			target = locations.GetLocationById(operation.GetTargetLocationId());
		vector launchPosition = m_vOccupiersSpawnPosition;
		if (operation.GetRole() == AROR_EFactionRole.INVADERS)
			launchPosition = m_vInvadersSpawnPosition;
		if (operation.m_TacticalCommander)
			launchPosition = TacticalLaunchPoint(operation.m_TacticalCommander, packageIndex);
		else
		{
			launchPosition = ResolveQRFOrigin(operation, target, launchPosition, packageIndex == 0);
			launchPosition[0] = launchPosition[0] + packageIndex * m_fPackageSpacing;
		}
		if (!target || (!operation.m_TacticalCommander && !IsLaunchPositionSafe(launchPosition, target.GetCenter())))
		{
			FailGroundPlanning(operation, "Ground-force target or launch position invalid");
			return;
		}
		if (operation.m_TacticalCommander)
		{
			vector roadLaunch;
			if (!FindGroundRoadLaunch(operation.m_TacticalCommander, packageIndex, roadLaunch) || !IsLaunchPositionSafe(roadLaunch, target.GetCenter()))
			{
				FailGroundPlanning(operation, "No clear separated road launch near the selected QRF origin");
				return;
			}
			launchPosition = roadLaunch;
		}
		// Reserve the whole package before queuing any member; failures release through terminal events.
		AROR_GroundOperationForce force = new AROR_GroundOperationForce();
		force.m_sOperationId = operation.GetId();
		force.m_iPackageIndex = packageIndex;
		force.m_sFactionKey = commander.GetFactionKey();
		force.m_Profile = profile;
		ConfigureForceFunding(force, operation, profile);
		force.m_bTacticalControlled = operation.m_TacticalCommander != null;
		force.m_sAssetFactionKey = profile.m_sAssetFactionKey;
		force.m_bLogFriendlyDamage = m_bLogFriendlyDamage;
		force.m_iReservedGroups = profile.GetGroupBudget();
		force.m_iReservedMembers = profile.m_iPersonnelBudget;
		force.m_iReservedVehicles = 1;
		// Plan delivery before spawning any hull/group or converting the virtual reservation.
		// A road search failure must not leave empty retained vehicles at the origin.
		vector objectivePosition = target.GetCenter();
		if (operation.m_TacticalCommander)
			objectivePosition = operation.m_TacticalCommander.m_vTown;
		vector approach = objectivePosition;
		if ((profile.m_eType != AROR_EForceType.ARMOUR || profile.m_bFillPassengerSeats))
		{
			float distance = vector.DistanceXZ(launchPosition, target.GetCenter());
			if (distance <= profile.m_fDismountDistance + 20 && !operation.m_TacticalCommander)
			{
				FailGroundPlanning(operation, "Launch position is too close for profile dismount distance");
				return;
			}
			if (operation.m_TacticalCommander)
			{
				float standoff = Math.Min(profile.m_fDismountDistance, m_fQRFObjectiveDeliveryRadius * 0.6);
				standoff = Math.Min(standoff, Math.Max(30, distance - 40));
				if (!FindTacticalApproach(force, operation.m_TacticalCommander, launchPosition, standoff, approach, operation.m_iPlanningRetries * 3))
				{
					FailGroundPlanning(operation, "Bounded tactical approach search exhausted; no reachable separated road point");
					return;
				}
			}
			else
			{
				approach[0] = approach[0] + (launchPosition[0] - approach[0]) / distance * profile.m_fDismountDistance;
				approach[2] = approach[2] + (launchPosition[2] - approach[2]) / distance * profile.m_fDismountDistance;
			}
		}
		force.m_vApproachPosition = approach;
		ConvertReservation(reservation, profile);
		m_aForces.Insert(force);
		Resource vehicleResource = Resource.Load(profile.m_sVehicle);
		if (vehicleResource && vehicleResource.IsValid())
			force.m_Vehicle = SpawnGroundVehicle(vehicleResource, launchPosition, target.GetCenter());
		if (!force.m_Vehicle)
		{
			FailOperation(operation, "Ground-force vehicle resource failed");
			return;
		}
		// Assets and campaign factions are distinct; accept only the profile's declared source.
		if (!force.InitializeSpawnFaction_S(force.m_Vehicle))
		{
			FailOperation(operation, "Ground-force vehicle faction mismatch");
			return;
		}
		force.TrackDamage_S(force.m_Vehicle);
		vector crewPosition = launchPosition;
		crewPosition[0] = crewPosition[0] + 5;
		force.m_CrewGroup = SpawnEmptyGroup(m_sCrewGroupPrefab, crewPosition);
		if (!force.m_CrewGroup)
		{
			FailOperation(operation, "Ground-force crew group could not be created");
			return;
		}
		force.m_CrewGroup.GetOnAgentAdded().Insert(force.OnAgentAdded);
		if (!force.InitializeSpawnGroupFaction_S(force.m_CrewGroup))
		{
			FailOperation(operation, "Crew group cannot adopt the configured campaign faction");
			return;
		}
		array<AROR_EUnitType> crewTypes = {};
		for (int c = 0; c < profile.m_iCrewCount; c++)
			crewTypes.Insert(profile.m_eCrewType);
		if (FillUnits(force.m_CrewGroup, profile.m_eRole, crewTypes) != profile.m_iCrewCount)
		{
			FailOperation(operation, "Crew has no character in the role's faction presets");
			return;
		}
		if ((profile.m_eType == AROR_EForceType.ARMOUR && !profile.m_bFillPassengerSeats))
			force.m_Group = force.m_CrewGroup;
		else
		{
			vector infantryPosition = launchPosition;
			infantryPosition[0] = infantryPosition[0] - 8;
			force.m_Group = SpawnEmptyGroup(m_sCrewGroupPrefab, infantryPosition);
			if (force.m_Group)
			{
				force.m_Group.GetOnAgentAdded().Insert(force.OnAgentAdded);
				profile.GetInfantryUnits(force.m_aInfantryTypes);
				FillUnits(force.m_Group, profile.m_eRole, force.m_aInfantryTypes);
			}
		}
		if (!force.m_Group || !force.m_Group.m_aUnitPrefabSlots || force.m_Group.m_aUnitPrefabSlots.Count() < 1)
		{
			FailOperation(operation, "Ground-force infantry group is invalid");
			return;
		}
		if (!force.InitializeSpawnGroupFaction_S(force.m_Group))
		{
			FailOperation(operation, "Infantry group cannot adopt the configured campaign faction");
			return;
		}
		if (!force.PreparePassengerRoster_S())
		{
			FailOperation(operation, "Native passenger capacity or required crew exceeds the reserved full-load profile");
			return;
		}
		int requested = force.m_Group.m_aUnitPrefabSlots.Count();
		int totalRequested = requested;
		if (force.m_Group != force.m_CrewGroup)
			totalRequested += profile.m_iCrewCount;
		if (totalRequested > profile.m_iPersonnelBudget)
		{
			FailOperation(operation, "Ground-force members exceed profile reservation");
			return;
		}
		ResourceName objectiveWaypoint = m_sAttackWaypointPrefab;
		if (operation.UsesTacticalResponse())
			objectiveWaypoint = m_sQRFWaypointPrefab;
		force.m_Waypoint = SpawnWaypoint(objectiveWaypoint, objectivePosition);
		if ((profile.m_eType != AROR_EForceType.ARMOUR || profile.m_bFillPassengerSeats))
			force.m_CrewWaypoint = SpawnWaypoint(m_sMoveWaypointPrefab, approach);
		force.m_vApproachPosition = approach;
		if (!force.m_Waypoint || ((profile.m_eType != AROR_EForceType.ARMOUR || profile.m_bFillPassengerSeats) && !force.m_CrewWaypoint))
		{
			FailOperation(operation, "Ground-force waypoint resource failed");
			return;
		}
		force.m_Waypoint.SetCompletionRadius(m_fArrivalRadius);
		if (force.m_CrewWaypoint)
			force.m_CrewWaypoint.SetCompletionRadius(10);
		ConfigureGroup(force.m_CrewGroup, operation);
		if (force.m_Group != force.m_CrewGroup)
		{
			ConfigureGroup(force.m_Group, operation);
			force.m_Group.RequestSpawn(requested);
		}
		force.m_CrewGroup.RequestSpawn(profile.m_iCrewCount);
		Print(string.Format("[AROR] Ground package queued: %1 profile=%2 personnel=%3", operation.GetId(), profile.m_sId, totalRequested), LogLevel.NORMAL);
	}

	protected void StartInfantryForce(AROR_Operation operation, AROR_ForceProfile profile = null, int packageIndex = 0, AROR_OperationForce reservation = null)
	{
		if (FindPackage(operation.GetId(), packageIndex))
			return;
		// A busy executor leaves requests pending; the manager's spawn deadline still applies.
		if (!profile && GetReservedGroups() >= m_iMaxGroups)
		{
			ReportSpawnWait(operation, "Legacy infantry group budget is full");
			return;
		}
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!commanders || !locations)
			return;
		AROR_Commander commander = commanders.GetCommander(operation.GetRole());
		AROR_LocationComponent target = locations.GetLocationById(operation.GetTargetLocationId());
		if (!commander || commander.IsPlayerDirected() || !target)
		{
			FailOperation(operation, "Executor has no enemy commander or target");
			return;
		}

		// Infantry spawn into a generic group as the role's base characters, dressed from the faction preset
		ResourceName groupPrefab = m_sCrewGroupPrefab;
		vector launchPosition = m_vOccupiersSpawnPosition;
		if (operation.GetRole() == AROR_EFactionRole.INVADERS)
			launchPosition = m_vInvadersSpawnPosition;
		array<AROR_EUnitType> infantryTypes = {};
		if (profile)
			profile.GetInfantryUnits(infantryTypes);
		else if (m_aLegacyInfantryUnits && !m_aLegacyInfantryUnits.IsEmpty())
			infantryTypes.Copy(m_aLegacyInfantryUnits);
		else
			AROR_ForceProfile.GetDefaultFireTeam(infantryTypes);
		if (operation.m_TacticalCommander)
			launchPosition = TacticalLaunchPoint(operation.m_TacticalCommander, packageIndex);
		else
		{
			launchPosition = ResolveQRFOrigin(operation, target, launchPosition, packageIndex == 0);
			launchPosition[0] = launchPosition[0] + packageIndex * m_fPackageSpacing;
		}
		ResourceName waypointPrefab = m_sAttackWaypointPrefab;
		if (operation.UsesTacticalResponse())
			waypointPrefab = m_sQRFWaypointPrefab;
		if (groupPrefab.IsEmpty() || waypointPrefab.IsEmpty() || !IsLaunchPositionSafe(launchPosition, target.GetCenter()))
		{
			FailOperation(operation, "Executor group/waypoint missing or launch position unsafe");
			return;
		}
		Resource groupResource = Resource.Load(groupPrefab);
		Resource waypointResource = Resource.Load(waypointPrefab);
		if (!groupResource || !groupResource.IsValid() || !waypointResource || !waypointResource.IsValid())
		{
			FailOperation(operation, "Executor resource could not be loaded");
			return;
		}

		// Create a cheap empty group; all members must go through the engine's shared queue.
		SCR_AIGroup.IgnoreSpawning(true);
		IEntity groupEntity = SpawnResource(groupResource, launchPosition);
		SCR_AIGroup.IgnoreSpawning(false);
		SCR_AIGroup group = SCR_AIGroup.Cast(groupEntity);
		if (!group)
		{
			if (groupEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(groupEntity);
			FailOperation(operation, "Configured infantry prefab is not an SCR_AIGroup");
			return;
		}

		AROR_OperationForce force = new AROR_OperationForce();
		force.m_sOperationId = operation.GetId();
		force.m_iPackageIndex = packageIndex;
		force.m_sFactionKey = commander.GetFactionKey();
		force.m_Group = group;
		ConfigureForceFunding(force, operation, profile);
		if (profile)
			force.m_sAssetFactionKey = profile.m_sAssetFactionKey;
		force.m_bLogFriendlyDamage = m_bLogFriendlyDamage;
		group.GetOnAgentAdded().Insert(force.OnAgentAdded);
		force.m_bTacticalControlled = operation.m_TacticalCommander != null;
		force.m_iReservedMembers = FillUnits(group, operation.GetRole(), infantryTypes);
		int requestedMembers = force.m_iReservedMembers;
		if (profile && requestedMembers <= profile.m_iPersonnelBudget)
			force.m_iReservedMembers = profile.m_iPersonnelBudget;
		ConvertReservation(reservation, profile);
		m_aForces.Insert(force);
		if (!force.InitializeSpawnGroupFaction_S(group))
		{
			FailOperation(operation, "Infantry group cannot adopt the configured campaign faction");
			return;
		}
		int reserved = GetReservedMembers();
		if (requestedMembers < 1 || reserved > m_iMaxMembers || (profile && requestedMembers > profile.m_iPersonnelBudget))
		{
			FailOperation(operation, "Infantry group has no member slots or exceeds executor member budget");
			return;
		}
		if (group.GetFaction() && group.GetFaction().GetFactionKey() != force.m_sFactionKey)
		{
			FailOperation(operation, "Group prefab faction does not match commander role");
			return;
		}

		IEntity waypointEntity = SpawnResource(waypointResource, target.GetCenter());
		force.m_Waypoint = AIWaypoint.Cast(waypointEntity);
		if (!force.m_Waypoint)
		{
			if (waypointEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
			FailOperation(operation, "Configured waypoint prefab is not an AIWaypoint");
			return;
		}
		force.m_Waypoint.SetCompletionRadius(m_fArrivalRadius);
		group.SetPrivate(true);
		group.SetPrivacyChangeable(false);
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		if (operation.UsesTacticalResponse())
			group.SetImportance(SCR_EAISpawnImportance.HIGH);
		else
			group.SetImportance(SCR_EAISpawnImportance.NORMAL);
		group.RequestSpawn(requestedMembers);
		Print(string.Format("[AROR] Force queued for %1: faction=%2, slots=%3", operation.GetId(), force.m_sFactionKey, force.m_iReservedMembers), LogLevel.NORMAL);
	}

	protected void OnOperationChanged(AROR_Operation operation)
	{
		if (!Replication.IsServer() || !operation)
			return;
		if (operation.IsTerminal())
		{
			m_mWaitReasons.Remove(operation.GetId());
			ReleaseOperationForces(operation.GetId());
		}
		else if (operation.GetState() == AROR_EOperationState.SPAWNING)
			StartForce(operation);
	}

	protected void MonitorForce(AROR_OperationForce force)
	{
		if (!force || !m_aForces.Contains(force))
			return;
		if (force.m_bTacticalRetained)
		{
			MonitorTacticalRetention(force);
			return;
		}
		AROR_QRFDeferredForce deferred = AROR_QRFDeferredForce.Cast(force);
		if (deferred)
		{
			MonitorDeferredQRF(deferred);
			return;
		}
		if (force.m_bReservation)
			return;
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
		if (ground)
			MonitorGroundForce(ground);
		else
			MonitorInfantryForce(force);
	}

	protected void MonitorGroundForce(AROR_GroundOperationForce force)
	{
		AROR_Operation operation = m_Operations.GetOperation(force.m_sOperationId);
		if (!operation || operation.IsTerminal())
		{
			ReleaseForce(force);
			return;
		}
		if (!force.m_bOrdersIssued && operation.m_TacticalCommander && operation.m_TacticalCommander.m_bWithdraw)
		{
			FailPackage(force, operation, "Unissued ground package cancelled during withdrawal");
			return;
		}
		if (!force.m_Group || (!force.m_CrewGroup && !force.m_bDismounted))
		{
			FailPackage(force, operation, "Ground-force element wiped out or removed before delivery (vehicle destroyed, all members killed, or deleted)");
			return;
		}
		int infantry = force.CountInfantry();
		int crew = force.CountCrew();
		if (!force.HasCorrectMemberFactions())
		{
			FailOperation(operation, "Ground-force personnel faction mismatch");
			return;
		}
		bool vehicleLost = !force.m_Vehicle;
		if (force.m_Vehicle)
		{
			DamageManagerComponent damage = DamageManagerComponent.Cast(force.m_Vehicle.FindComponent(DamageManagerComponent));
			if (damage && damage.IsDestroyed())
				vehicleLost = true;
		}
		if (vehicleLost && !force.m_bDismounted)
		{
			if (force.m_bTacticalControlled && force.HasDismountedInfantry())
				force.m_bDismounted = true;
			else
			{
				FailPackage(force, operation, "Operation vehicle destroyed before delivery or during armoured attack");
				return;
			}
		}
		if (!vehicleLost)
		{
			FactionAffiliationComponent vehicleFaction = FactionAffiliationComponent.Cast(force.m_Vehicle.FindComponent(FactionAffiliationComponent));
			if (!ValidateGroundVehicleFaction(force, operation, vehicleFaction))
				return;
		}
		// Retain reservations and wake engine-dormant groups rather than declaring combat losses.
		bool dormant;
		force.m_iWakeRetryTicks--;
		array<SCR_AIGroup> groups = {force.m_Group};
		if (force.m_CrewGroup && force.m_CrewGroup != force.m_Group && !force.m_bDismounted)
			groups.Insert(force.m_CrewGroup);
		foreach (SCR_AIGroup group : groups)
		{
			if (!group.IsDormant())
				continue;
			dormant = true;
			bool sawGroupMembers = force.m_bSawInfantry;
			if (group == force.m_CrewGroup)
				sawGroupMembers = force.m_bSawCrew;
			// A newly queued group has no members yet. Zero is a defeat only after
			// this particular element has actually materialized, not another package's group.
			if (sawGroupMembers && group.GetDormantAliveCount() == 0)
			{
				FailPackage(force, operation, "Ground-force element eliminated");
				return;
			}
			if (force.m_iWakeRetryTicks <= 0)
				group.RequestSpawn();
		}
		if (dormant)
		{
			ReportPackageProgress(force, "Waiting for engine-dormant groups to materialize");
			if (force.m_iWakeRetryTicks <= 0)
				force.m_iWakeRetryTicks = m_iWakeRetryInterval;
			return;
		}
		if (!force.m_bMounted)
		{
			if (crew < 1 || infantry < 1 || !force.m_Group.IsExpandComplete() || !force.m_CrewGroup.IsExpandComplete())
			{
				ReportPackageProgress(force, string.Format("Spawning members: crew=%1 infantry=%2 crew complete=%3 infantry complete=%4", crew, infantry, force.m_CrewGroup.IsExpandComplete(), force.m_Group.IsExpandComplete()));
				return;
			}
			if (!force.m_Group.GetFaction() || !force.m_CrewGroup.GetFaction() || force.m_Group.GetFaction().GetFactionKey() != force.m_sFactionKey || force.m_CrewGroup.GetFaction().GetFactionKey() != force.m_sFactionKey)
			{
				FailOperation(operation, "Ground-force group faction mismatch");
				return;
			}
			if (!force.m_bMountRequested && !force.Mount_S())
			{
				FailOperation(operation, "Vehicle is not AI-drivable or crew/passenger seating failed");
				return;
			}
			if (!force.IsMountComplete())
			{
				ReportPackageProgress(force, "Waiting for actual driver, crew and passenger seat occupancy after direct placement");
				// Seat teleports can be dropped silently; re-request the missing seats like garrison crews do
				force.m_iMountWaitTicks++;
				if (Math.Mod(force.m_iMountWaitTicks, m_iWakeRetryInterval) == 0)
				{
					// Still refused after the recovery delay twice over: spawn the missing people straight into their seats
					if (force.m_iMountWaitTicks >= m_iQRFSeatRecoveryDelay * 2)
					{
						int replaced = force.ReplaceUnseated_S();
						if (replaced > 0)
							Print(string.Format("[AROR] %1 package=%2 spawned %3 replacement(s) straight into their seats after %4 s of refused seating", operation.GetId(), force.m_iPackageIndex + 1, replaced, force.m_iMountWaitTicks), LogLevel.WARNING);
					}
					Print(string.Format("[AROR] %1 package=%2 seating retry after %3 s: %4", operation.GetId(), force.m_iPackageIndex + 1, force.m_iMountWaitTicks, force.ReseatMissing_S(force.m_iMountWaitTicks >= m_iQRFSeatRecoveryDelay)), LogLevel.WARNING);
				}
				return;
			}
			force.m_bMounted = true;
			force.m_bReady = true;
			int initialStrength = infantry + crew;
			if ((force.m_Profile.m_eType == AROR_EForceType.ARMOUR && !force.m_Profile.m_bFillPassengerSeats))
				initialStrength = crew;
			RecordTacticalStrength(force, operation, initialStrength);
			FactionAffiliationComponent deployedFaction = FactionAffiliationComponent.Cast(force.m_Vehicle.FindComponent(FactionAffiliationComponent));
			Print(string.Format("[AROR] Deployment faction verified: operation=%1 package=%2 expected=%3 vehicle=%4 crewGroup=%5 infantryGroup=%6 members=%7", operation.GetId(), force.m_iPackageIndex + 1, force.m_sFactionKey, deployedFaction.GetAffiliatedFactionKey(), force.m_CrewGroup.GetFaction().GetFactionKey(), force.m_Group.GetFaction().GetFactionKey(), force.CountLivingMembers()), LogLevel.NORMAL);
			ReportPackageProgress(force, "Crew and infantry placed directly into seats (no boarding pathfinding); waiting for all packages before issuing vehicle orders");
			TryActivateOperation(operation);
			return;
		}
		if (infantry == 0 || (crew == 0 && !force.m_bDismounted))
		{
			FailPackage(force, operation, "Ground-force required infantry or crew eliminated");
			return;
		}
		if (operation.GetState() != AROR_EOperationState.ACTIVE)
			return;
		if (!force.m_bOrdersIssued)
		{
			TryActivateOperation(operation);
			return;
		}
		if (UpdateTacticalGround(force, operation, infantry, crew))
			return;
		if (UpdateTownSearch(force, operation))
			return;
		if ((force.m_Profile.m_eType != AROR_EForceType.ARMOUR || force.m_Profile.m_bFillPassengerSeats) && !force.m_bDismounted)
		{
			if (!force.ArePassengersOut() && !force.m_CrewGroup.GetCurrentWaypoint() && vector.DistanceXZ(force.m_Vehicle.GetOrigin(), force.m_vApproachPosition) <= 30)
			{
				if (force.RequestDismount_S())
					ReportPackageProgress(force, "Vehicle reached dismount point; passengers exiting");
				else
					ReportPackageProgress(force, "At dismount point; waiting for usable passenger exits");
			}
			else if (!force.m_bDismountRequested)
				ReportPackageProgress(force, "Vehicle orders active; travelling to dismount point");
			if (force.m_bDismountRequested && force.ArePassengersOut())
			{
				force.m_bDismounted = true;
				force.m_Group.AddWaypoint(force.m_Waypoint);
				if (force.m_Profile.m_eType == AROR_EForceType.MECHANIZED)
				{
					if (force.m_CrewWaypoint)
						SCR_EntityHelper.DeleteEntityAndChildren(force.m_CrewWaypoint);
					ResourceName supportWaypoint = m_sAttackWaypointPrefab;
					if (operation.UsesTacticalResponse())
						supportWaypoint = m_sQRFWaypointPrefab;
					force.m_CrewWaypoint = SpawnWaypoint(supportWaypoint, force.m_Waypoint.GetOrigin());
					if (!force.m_CrewWaypoint)
					{
						FailOperation(operation, "Mechanized support waypoint could not be created");
						return;
					}
					force.m_CrewGroup.AddWaypoint(force.m_CrewWaypoint);
				}
				Print(string.Format("[AROR] Operation %1 infantry dismounted; objective orders issued", operation.GetId()), LogLevel.NORMAL);
				ReportPackageProgress(force, "Passengers clear of vehicle; infantry moving to defended location");
			}
		}
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent target;
		if (locations)
			target = locations.GetLocationById(operation.GetTargetLocationId());
		IEntity leader = force.m_Group.GetLeaderEntity();
		if (!force.m_bArrivalLogged && target && leader && vector.DistanceXZ(leader.GetOrigin(), target.GetCenter()) <= m_fArrivalRadius)
		{
			force.m_bArrivalLogged = true;
			Print(string.Format("[AROR] Force %1 reached objective; awaiting territory/combat outcome", operation.GetId()), LogLevel.NORMAL);
		}
	}

	//! Owns only QRF search orders; attacks and the post-search deployment policy keep their orders.
	protected bool UpdateTownSearch(AROR_GroundOperationForce force, AROR_Operation operation)
	{
		if (!m_bQRFVehicleSearch || !operation.UsesTacticalResponse() || force.m_bDismounted || force.m_bTownSearchFinished)
			return false;
		if (!force.m_bTownSearchAttempted)
		{
			AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
			if (!locations)
				return false;
			AROR_LocationComponent location = locations.GetLocationById(operation.GetTargetLocationId());
			if (!location || !force.m_Vehicle)
				return false;
			AIWaypoint previousWaypoint = force.m_CrewWaypoint;
			if ((force.m_Profile.m_eType == AROR_EForceType.ARMOUR && !force.m_Profile.m_bFillPassengerSeats))
			{
				previousWaypoint = force.m_Waypoint;
			}
			// Split search assignments on entering the vicinity, before the common dismount point.
			if (vector.DistanceXZ(force.m_Vehicle.GetOrigin(), location.GetCenter()) > location.GetRadius() + m_fQRFSearchMargin)
				return false;
			AROR_QRFSearchPlan plan = m_Operations.PrepareQRFSearch_S(operation.GetId(), m_iQRFContactMaxAge);
			if (!plan)
				return false;
			force.m_bTownSearchAttempted = true;
			force.m_TownSearch = new AROR_VehicleTownSearch();
			force.m_TownSearch.Configure_S(plan, force.m_iPackageIndex, operation.GetPackageCount(), m_fQRFSearchMargin, m_fQRFSearchStopSpacing);
			if (!force.m_TownSearch.Start_S(force.m_CrewGroup, force.m_Vehicle, location, previousWaypoint, m_sMoveWaypointPrefab, operation.GetId(), m_iExecutorClock, m_iQRFSearchDuration, m_iQRFSearchStops, m_iQRFSearchDwell, m_iQRFSearchLegTimeout))
			{
				force.m_TownSearch.Cleanup_S();
				force.m_TownSearch = null;
				force.m_bTownSearchFinished = true;
				Print(string.Format("[AROR] Mounted search skipped for %1: no reachable road stop or move waypoint; keeping deployment orders", operation.GetId()), LogLevel.WARNING);
				return false;
			}
			ReportPackageProgress(force, "Investigating reported contact then patrolling assigned town sector; native AI handles combat");
			return true;
		}
		if (force.m_TownSearch && force.m_TownSearch.Tick_S(m_iExecutorClock))
			return true;
		force.m_bTownSearchFinished = true;
		if ((force.m_Profile.m_eType == AROR_EForceType.ARMOUR && !force.m_Profile.m_bFillPassengerSeats))
			force.m_CrewGroup.AddWaypoint(force.m_Waypoint);
		else
			force.m_CrewGroup.AddWaypoint(force.m_CrewWaypoint);
		ReportPackageProgress(force, "Mounted search ended; returning to deployment waypoint");
		return true;
	}

	protected void MonitorInfantryForce(AROR_OperationForce force)
	{
		AROR_Operation operation = m_Operations.GetOperation(force.m_sOperationId);
		if (!operation || operation.IsTerminal())
		{
			ReleaseForce(force);
			return;
		}
		if (!force.m_Group)
		{
			FailPackage(force, operation, "Operation group was deleted");
			return;
		}
		int living = force.CountLivingMembers();
		if (!force.HasCorrectMemberFactions())
		{
			FailOperation(operation, "Spawned infantry faction does not match commander role");
			return;
		}
		if (force.m_Group.IsDormant())
		{
			if (force.m_bSawMembers && force.m_Group.GetDormantAliveCount() == 0)
			{
				FailPackage(force, operation, "Operation infantry eliminated");
				return;
			}
			// Engine budget eviction is not a combat defeat. Retain the operation and request wake-up.
			force.m_iWakeRetryTicks--;
			if (force.m_iWakeRetryTicks <= 0 && force.m_Group.GetDormantAliveCount() > 0)
			{
				force.m_iWakeRetryTicks = m_iWakeRetryInterval;
				force.m_Group.RequestSpawn();
			}
			return;
		}
		force.m_iWakeRetryTicks = 0;
		if (force.m_bSawMembers && living == 0)
		{
			FailPackage(force, operation, "Operation infantry eliminated");
			return;
		}
		bool waitingForDispatch = operation.GetState() == AROR_EOperationState.SPAWNING;
		if (operation.m_TacticalCommander && !force.m_bReady)
			waitingForDispatch = true;
		if (waitingForDispatch && living > 0 && force.m_Group.IsExpandComplete())
		{
			if (!force.m_Group.GetFaction() || force.m_Group.GetFaction().GetFactionKey() != force.m_sFactionKey)
				FailOperation(operation, "Spawned group has no matching faction");
			else
			{
				force.m_bReady = true;
				RecordTacticalStrength(force, operation, living);
				TryActivateOperation(operation);
			}
			return;
		}
		if (operation.GetState() == AROR_EOperationState.ACTIVE && operation.m_TacticalCommander)
		{
			if (!force.m_bOrdersIssued)
			{
				TryActivateOperation(operation);
				return;
			}
			RecordTacticalStrength(force, operation, living);
			bool retreat = operation.m_TacticalCommander.m_bWithdraw || force.m_bTacticalWithdrawing;
			if (living <= force.m_iTacticalInitialStrength * m_fQRFLocalWithdrawFraction)
				retreat = true;
			if (retreat)
				TacticalWithdrawal(force, operation);
			else
				TacticalInfantryOrders(force, operation);
			return;
		}
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent target;
		if (locations)
			target = locations.GetLocationById(operation.GetTargetLocationId());
		IEntity leader = force.m_Group.GetLeaderEntity();
		if (operation.GetState() == AROR_EOperationState.ACTIVE && !force.m_bArrivalLogged && target && leader && vector.DistanceXZ(leader.GetOrigin(), target.GetCenter()) <= m_fArrivalRadius)
		{
			force.m_bArrivalLogged = true;
			Print(string.Format("[AROR] Force %1 reached objective; awaiting territory/combat outcome", operation.GetId()), LogLevel.NORMAL);
		}
	}

	protected void Tick()
	{
		if (!Replication.IsServer())
			return;
		m_iExecutorClock++;
		if (!m_Operations)
		{
			m_Operations = AROR_OperationManagerComponent.GetInstance();
			if (m_Operations)
			{
				m_Operations.GetOnOperationChanged().Insert(OnOperationChanged);
				Print("[AROR] Operation executor connected to scheduler", LogLevel.NORMAL);
			}
		}
		if (!m_Operations)
			return;
		TickQRFTacticalCommanders();
		array<AROR_OperationForce> forces = {};
		foreach (AROR_OperationForce force : m_aForces)
			forces.Insert(force);
		foreach (AROR_OperationForce tracked : forces)
			MonitorForce(tracked);
		// Recover dispatches emitted before subscription, and retry capacity-blocked requests.
		array<AROR_Operation> operations = {};
		m_Operations.GetOperations(operations);
		foreach (AROR_Operation qrf : operations)
		{
			if (qrf.GetState() == AROR_EOperationState.SPAWNING && qrf.GetType() == AROR_EOperationType.QRF)
				StartForce(qrf);
		}
		foreach (AROR_Operation attack : operations)
		{
			if (attack.GetState() == AROR_EOperationState.SPAWNING && attack.GetType() == AROR_EOperationType.ATTACK)
				StartForce(attack);
		}
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;
		if (!m_bExecuteOperations)
		{
			Print("[AROR] Operation executor disabled: enable m_bExecuteOperations on the Chicken game-mode instance", LogLevel.WARNING);
			return;
		}
		if (m_iQRFSeatRecoveryDelay < 5 || m_iQRFSeatRecoveryDelay > 60)
		{
			Print("[AROR] Initial boarding recovery delay must be between 5 and 60 seconds", LogLevel.ERROR);
			return;
		}
		// Attribute containers are populated from the prefab/world. Declaration initializers
		// overwrite that source data; create only missing containers after deserialization.
		if (!m_aForceProfiles)
			m_aForceProfiles = {};
		if (!m_aResponseLevels)
			m_aResponseLevels = {};
		if (!m_aGroundQRFRecipes)
			m_aGroundQRFRecipes = {};
		if (m_bQRFTacticalCommander && !TacticalSettingsValid())
		{
			Print("[AROR] Invalid tactical commander settings; executor disabled", LogLevel.ERROR);
			return;
		}
		if (m_bQRFVehicleSearch && (m_iQRFSearchDuration < 1 || m_iQRFSearchDuration > 900 || m_iQRFSearchStops < 1 || m_iQRFSearchStops > 12 || m_iQRFSearchDwell < 0 || m_iQRFSearchDwell > 60 || m_iQRFSearchLegTimeout < 10 || m_iQRFSearchLegTimeout > 300 || m_iQRFContactMaxAge < 1 || m_iQRFContactMaxAge > 900 || m_fQRFSearchMargin < 0 || m_fQRFSearchMargin > 500 || m_fQRFSearchStopSpacing < 10 || m_fQRFSearchStopSpacing > 150))
		{
			Print("[AROR] Invalid mounted QRF search settings; executor disabled", LogLevel.ERROR);
			return;
		}
		if (m_aResponseLevels.Count() == 0)
		{
			for (int level = 1; level <= 5; level++)
			{
				AROR_ResponseLevel response = new AROR_ResponseLevel();
				response.m_iLevel = level;
				response.m_iAttackWeight = 20;
				response.m_iQRFWeight = 20;
				m_aResponseLevels.Insert(response);
			}
		}
		if (owner.FindComponent(AROR_OperationExecutorComponent) != this)
		{
			Print("[AROR] Duplicate operation executor on the game mode; this one is disabled", LogLevel.ERROR);
			return;
		}
		if (m_iMaxGroups < 1 || m_iMaxMembers < 1 || m_iMaxVehicles < 0 || m_iWakeRetryInterval < 1 || m_fPackageSpacing < 10 || m_iWorkbenchResponseLevel < 0 || m_iWorkbenchResponseLevel > 5 || m_fMinTargetDistance < 0 || m_fMinPlayerDistance < 0 || m_fArrivalRadius <= 0)
		{
			Print("[AROR] Invalid operation executor budgets, distances or Workbench level; executor disabled", LogLevel.ERROR);
			return;
		}
		if (!ValidatePool())
		{
			Print("[AROR] Invalid operation executor force pool (see the reason above); executor disabled, no QRFs or attacks can spawn", LogLevel.ERROR);
			return;
		}
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);
		Print("[AROR] Operation force executor enabled", LogLevel.NORMAL);
		if (m_bQRFVehicleSearch)
			Print("[AROR] QRF search policy: coordinated contact investigation and sector patrol", LogLevel.NORMAL);
		Print(string.Format("[AROR] Executor configuration: profiles=%1 levels=%2 budgets groups=%3 members=%4 vehicles=%5", m_aForceProfiles.Count(), m_aResponseLevels.Count(), m_iMaxGroups, m_iMaxMembers, m_iMaxVehicles), LogLevel.NORMAL);
	}

	//! Every rejection names its reason: one failure disables all QRFs, so the log must say which
	protected bool ValidatePool()
	{
		if (m_aGroundQRFRecipes.Count() > 64)
			return PoolError(string.Format("%1 ground recipes; the limit is 64", m_aGroundQRFRecipes.Count()));
		array<string> recipeKeys = {};
		foreach (AROR_GroundQRFRecipe recipe : m_aGroundQRFRecipes)
		{
			if (!recipe)
				return PoolError("empty ground recipe entry");
			if (!recipe.IsValid())
				return PoolError(string.Format("recipe '%1' is invalid (ID, faction, level 1-5, readiness range or 1-8 profile IDs)", recipe.m_sId));
			string recipeKey = string.Format("%1:%2", recipe.m_sId, recipe.m_eRole);
			if (recipeKeys.Contains(recipeKey))
				return PoolError(string.Format("recipe '%1' is defined twice for role %2", recipe.m_sId, recipe.m_eRole));
			recipeKeys.Insert(recipeKey);
			array<AROR_ForceProfile> resolved = {};
			if (!recipe.Resolve(m_aForceProfiles, recipe.m_eRole, recipe.m_sFactionKey, resolved, recipe.m_iMinimumReadinessLevel))
				return PoolError(string.Format("recipe '%1' does not resolve at readiness %2: %3", recipe.m_sId, recipe.m_iMinimumReadinessLevel, DescribeRecipeProfiles(recipe)));
		}
		if (m_aForceProfiles.Count() > 64)
			return PoolError(string.Format("%1 force profiles; the limit is 64", m_aForceProfiles.Count()));
		if (m_aResponseLevels.Count() > 5)
			return PoolError(string.Format("%1 response levels; the limit is 5", m_aResponseLevels.Count()));
		array<int> levels = {};
		foreach (AROR_ResponseLevel response : m_aResponseLevels)
		{
			if (!response || !response.IsValid() || levels.Contains(response.m_iLevel))
				return PoolError("response levels must be valid, 1-5 and unique");
			levels.Insert(response.m_iLevel);
		}
		array<string> ids = {};
		foreach (AROR_ForceProfile profile : m_aForceProfiles)
		{
			if (!profile || !profile.IsValid() || ids.Contains(profile.m_sId))
			{
				string profileId = "<empty>";
				if (profile)
					profileId = profile.m_sId;
				return PoolError(string.Format("force profile '%1' is invalid or its ID is not unique", profileId));
			}
			ids.Insert(profile.m_sId);
			if (!profile.HasNativeExecutor())
				Print(string.Format("[AROR] Profile %1 excluded from native pool: airborne requires a separate flight executor", profile.m_sId), LogLevel.WARNING);
		}
#ifdef WORKBENCH
		if ((!m_sWorkbenchForceProfileId.IsEmpty() && !ids.Contains(m_sWorkbenchForceProfileId)) || (m_iWorkbenchResponseLevel > 0 && !levels.Contains(m_iWorkbenchResponseLevel)))
			return PoolError("Workbench forced profile/level is absent from configuration");
#endif
		return true;
	}

	protected bool PoolError(string reason)
	{
		Print("[AROR] Executor force pool rejected: " + reason, LogLevel.ERROR);
		return false;
	}

	//! Why each of a recipe's profiles can or cannot be used, for the pool error
	protected string DescribeRecipeProfiles(AROR_GroundQRFRecipe recipe)
	{
		string result;
		foreach (string id : recipe.m_aProfileIds)
		{
			AROR_ForceProfile found;
			foreach (AROR_ForceProfile profile : m_aForceProfiles)
			{
				if (profile && profile.m_sId == id)
					found = profile;
			}
			string state = "ok";
			if (!found)
				state = "missing";
			else if (!found.IsValid())
				state = "invalid profile";
			else if (!found.m_bEnabled)
				state = "disabled";
			else if (found.m_iMinimumReadinessLevel > recipe.m_iMinimumReadinessLevel)
				state = string.Format("unlocks at readiness %1", found.m_iMinimumReadinessLevel);
			else if (found.m_eRole != recipe.m_eRole || found.m_sFactionKey != recipe.m_sFactionKey)
				state = string.Format("role/faction %1/%2", found.m_eRole, found.m_sFactionKey);
			else if (found.GetWeight(AROR_EOperationType.QRF) <= 0)
				state = "zero QRF weight";
			else if (found.m_eType == AROR_EForceType.AIRBORNE && recipe.m_eResponseType != AROR_EResponseType.AIR_GROUND)
				state = "airborne in a non-joint recipe";
			result += string.Format("%1=%2 ", id, state);
		}
		return result;
	}

	override void OnDelete(IEntity owner)
	{
#ifdef WORKBENCH
		AROR_QRFRouteAudit.Stop_S();
#endif
		GetGame().GetCallqueue().Remove(Tick);
		if (m_Operations)
			m_Operations.GetOnOperationChanged().Remove(OnOperationChanged);
		foreach (AROR_OperationForce force : m_aForces)
			force.Cleanup_S();
		m_aForces.Clear();
		super.OnDelete(owner);
	}

	// Shared native ground/air tactical policy. Backend-only actions stay in the REAPER layer.
	//! Freeze equipment availability separately from crackdown-driven response intensity.
	protected int QRFPoolReadinessLevel(AROR_Operation operation)
	{
		if (operation.m_iPoolReadinessLevel < 1)
			operation.m_iPoolReadinessLevel = m_Operations.GetReadinessLevel(operation.GetRole());
		return operation.m_iPoolReadinessLevel;
	}
	//! Choose once before binding: downgrade intensity for funds/capacity within this equipment pool.
	protected AROR_GroundQRFRecipe SelectStrategicGroundRecipe(AROR_Operation operation, AROR_Commander commander)
	{
		int desired = operation.m_iRequestedTier;
		if (desired < 1)
		{
			desired = m_Operations.GetCrackdownLevel(operation.GetRole());
			operation.m_iRequestedTier = desired;
		}
		int readinessLevel = QRFPoolReadinessLevel(operation);
		int airBucket = -1;
		if (operation.m_bResponseMixTracked)
			airBucket = operation.m_iResponseMixBucket;
		AROR_GroundQRFRecipe paid = FindStrategicGroundRecipe(operation.GetRole(), commander.GetFactionKey(), desired, false, readinessLevel, airBucket);
		if (paid)
			return paid;
		if (operation.m_bCaptureThreatRecorded && m_Operations.AllowsMinimumCaptureQRF())
			return FindStrategicGroundRecipe(operation.GetRole(), commander.GetFactionKey(), 1, true, readinessLevel);
		return null;
	}
	//! Advisory queue preflight; ReserveQRFFunding remains the atomic admission boundary.
	//! Why CanFundQRFThreat_S refuses, for the capture-response wait log: the launch bases by rejection reason,
	//! or the missing recipe
	string ThreatWaitReason_S(AROR_EFactionRole role, int desired, bool captured, string targetId)
	{
		if (!Replication.IsServer() || !m_Operations || !AROR_EconomyComponent.GetInstance())
			return "operations or economy unavailable";
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent target;
		if (locations)
			target = locations.GetLocationById(targetId);
		if (!target)
			return "target unavailable";
		if (!HasFriendlyQRFOrigin(role, targetId))
		{
			array<AROR_LocationComponent> sites = {};
			locations.GetLocations(sites);
			int ownedBases;
			int playerBlocked;
			int contested;
			int cooldown;
			int depleted;
			int roadless;
			foreach (AROR_LocationComponent site : sites)
			{
				if (!site || site == target || site.GetControllingRole() != role || !AROR_QRFOriginPolicy.IsOriginType(site.GetLocationType()))
					continue;
				ownedBases++;
				if (m_aRoadlessOrigins.Contains(site.GetId()))
					roadless++;
				else if (!IsLaunchPositionSafe(site.GetCenter(), target.GetCenter()))
					playerBlocked++;
				else if (AROR_RecaptureComponent.IsContested(site.GetId()))
					contested++;
				else if (m_mOriginLaunchedAt.Contains(site.GetId()) && m_iExecutorClock - m_mOriginLaunchedAt.Get(site.GetId()) < m_iOriginLaunchCooldown)
					cooldown++;
				else if (IsGarrisonDepleted(site))
					depleted++;
			}
			return string.Format("no usable launch base: owned=%1 playersNearOrUnsafe=%2 contested=%3 launchCooldown=%4 garrisonDepleted=%5 noRoad=%6", ownedBases, playerBlocked, contested, cooldown, depleted, roadless);
		}
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return "faction roles unavailable";
		return string.Format("no eligible ground recipe at tier %1 for %2 (readiness gates or recipe settings)", desired, roles.GetFactionKey(role));
	}

	bool CanFundQRFThreat_S(AROR_EFactionRole role, int desired, bool captured = false, string targetId = "")
	{
		if (!Replication.IsServer() || !m_Operations)
			return false;
		if (!AROR_EconomyComponent.GetInstance())
			return false;
		if (!HasFriendlyQRFOrigin(role, targetId))
			return false;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return false;
		if (FindStrategicGroundRecipe(role, roles.GetFactionKey(role), desired))
			return true;
		return captured && m_Operations.AllowsMinimumCaptureQRF() && FindStrategicGroundRecipe(role, roles.GetFactionKey(role), 1, true) != null;
	}
	protected bool HasFriendlyQRFOrigin(AROR_EFactionRole role, string targetId, bool alwaysRequire = false)
	{
		if (!m_bQRFRequireFriendlyOrigin && !alwaysRequire)
			return true;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return false;
		AROR_LocationComponent target = locations.GetLocationById(targetId);
		if (!target)
			return false;
		return BestOrigin(role, target, null) != null;
	}

	//------------------------------------------------------------------------------------------------
	// Launch origins
	//------------------------------------------------------------------------------------------------
	//! The role's best launch site for a target: the lowest route cost among usable sites at least
	//! m_fMinOriginDistance away, else the lowest among closer usable sites (so responses never simply stop)
	protected AROR_LocationComponent BestOrigin(AROR_EFactionRole role, AROR_LocationComponent target, AROR_Operation operation)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations || !target)
			return null;
		array<AROR_LocationComponent> sites = {};
		locations.GetLocations(sites);
		AROR_LocationComponent best;
		float bestCost = float.MAX;
		AROR_LocationComponent close;
		float closeCost = float.MAX;
		foreach (AROR_LocationComponent site : sites)
		{
			if (!IsUsableOrigin(site, role, target, operation))
				continue;
			float cost = OriginCost(site.GetCenter(), target.GetCenter());
			if (vector.DistanceXZ(site.GetCenter(), target.GetCenter()) >= m_fMinOriginDistance)
			{
				if (cost < bestCost)
				{
					best = site;
					bestCost = cost;
				}
			}
			else if (cost < closeCost)
			{
				close = site;
				closeCost = cost;
			}
		}
		if (best)
			return best;
		return close;
	}

	//------------------------------------------------------------------------------------------------
	//! A site the role can launch from right now: its own military site, safe to spawn at, not being fought over,
	//! not launched from recently, and with enough of its garrison left
	protected bool IsUsableOrigin(AROR_LocationComponent site, AROR_EFactionRole role, AROR_LocationComponent target, AROR_Operation operation)
	{
		if (!site || site == target || site.GetControllingRole() != role || !AROR_QRFOriginPolicy.IsOriginType(site.GetLocationType()))
			return false;
		if (operation && operation.m_sRejectedOrigins.Contains("|" + site.GetId() + "|"))
			return false;
		if (m_aRoadlessOrigins.Contains(site.GetId()))
			return false;
		if (!IsLaunchPositionSafe(site.GetCenter(), target.GetCenter()) || AROR_RecaptureComponent.IsContested(site.GetId()))
			return false;
		if (m_mOriginLaunchedAt.Contains(site.GetId()) && m_iExecutorClock - m_mOriginLaunchedAt.Get(site.GetId()) < m_iOriginLaunchCooldown)
			return false;
		return !IsGarrisonDepleted(site);
	}

	//------------------------------------------------------------------------------------------------
	//! A base that has lost too much of its garrison has nobody left to send
	protected bool IsGarrisonDepleted(AROR_LocationComponent site)
	{
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(site.GetOwner().FindComponent(AROR_GarrisonComponent));
		if (!garrison || !garrison.HasRoster())
			return false;
		array<ref AROR_GarrisonSlot> slots = {};
		garrison.GetSlots(slots);
		int people;
		int lost;
		foreach (AROR_GarrisonSlot slot : slots)
		{
			// Infantry and crew slots: the people a base could send
			if (slot.m_iKind != 0 && slot.m_iKind != 4)
				continue;
			people++;
			if (slot.m_bLost)
				lost++;
		}
		return people > 0 && lost > people * m_fOriginMaxGarrisonLoss;
	}

	//------------------------------------------------------------------------------------------------
	//! Straight-line distance, with the share of the line over water costed higher: a base across a bay is a long
	//! drive around it, not a short hop
	protected float OriginCost(vector from, vector to)
	{
		BaseWorld world = GetGame().GetWorld();
		int samples = 12;
		int wet;
		for (int i = 1; i < samples; i++)
		{
			vector point = from + (to - from) * (i / (samples * 1.0));
			if (world.GetSurfaceY(point[0], point[2]) <= world.GetOceanHeight(point[0], point[2]))
				wet++;
		}
		float waterShare = wet / (samples - 1.0);
		return vector.DistanceXZ(from, to) * (1 + (m_fOriginWaterPenalty - 1) * waterShare);
	}
	[Attribute("0", desc: "Allow authored joint airborne/ground responses in campaign selection and MARS. Requires airborne execution and tactical coordination.")]
	protected bool m_bAllowJointResponses;

	protected AROR_GroundQRFRecipe FindStrategicGroundRecipe(AROR_EFactionRole role, string faction, int desired, bool minimumCapture = false, int readinessLevel = 0, int airBucket = -1)
	{
		AROR_GroundQRFRecipe best;
		if (readinessLevel == 0)
			readinessLevel = m_Operations.GetReadinessLevel(role);
		int available = m_Operations.GetQRFFinance().Available(role);
		if (!minimumCapture && m_Operations.GetQRFFinance().Debt(role) > 0)
			return null;
		foreach (AROR_GroundQRFRecipe recipe : m_aGroundQRFRecipes)
		{
			bool joint = recipe && recipe.m_eResponseType == AROR_EResponseType.AIR_GROUND;
			if ((airBucket == 0 && joint) || (airBucket == 1 && !joint))
				continue;
			if (joint && (!m_bAllowJointResponses || !m_bAllowAirborneResponses || !m_bQRFTacticalCommander || minimumCapture))
				continue;
			if (!AROR_ResponsePolicy.IsRecipeEligible(recipe, readinessLevel, desired) || (best && (recipe.m_iLevel < best.m_iLevel || (recipe.m_iLevel == best.m_iLevel && (!joint || best.m_eResponseType == AROR_EResponseType.AIR_GROUND)))))
				continue;
			if (minimumCapture && recipe.m_iLevel != 1)
				continue;
			array<AROR_ForceProfile> elements = {};
			if (!recipe.Resolve(m_aForceProfiles, role, faction, elements, readinessLevel))
				continue;
			int quote = m_Operations.QRFOverhead(recipe.m_iLevel);
			int groups;
			int personnel;
			int vehicles;
			foreach (AROR_ForceProfile profile : elements)
			{
				quote += profile.ReplacementQuote();
				groups += profile.GetGroupBudget();
				personnel += profile.m_iPersonnelBudget;
				vehicles += profile.GetVehicleBudget();
			}
			if ((!minimumCapture && quote > available) || groups + GetReservedGroups() > m_iMaxGroups)
				continue;
			if (personnel + GetReservedMembers() > m_iMaxMembers || vehicles + GetReservedVehicles() > m_iMaxVehicles)
				continue;
			best = recipe;
		}
		return best;
	}
	protected bool ReserveQRFFunding(AROR_Operation operation, array<AROR_ForceProfile> elements, int tier)
	{
		if (operation.m_bDebugRequest || !m_Operations.IsQRFStrategyEnabled() || !operation.UsesTacticalResponse())
			return true;
		if (operation.m_Funding)
			return true;
		int quote;
		bool groundOnly = true;
		foreach (AROR_ForceProfile profile : elements)
		{
			quote += profile.ReplacementQuote();
			if (profile.m_eType == AROR_EForceType.AIRBORNE)
				groundOnly = false;
		}
		operation.m_Funding = m_Operations.GetQRFFinance().Reserve_S(operation, quote, m_Operations.QRFOverhead(tier));
		if (!operation.m_Funding && groundOnly && tier == 1 && operation.m_bCaptureThreatRecorded && m_Operations.AllowsMinimumCaptureQRF())
			operation.m_Funding = m_Operations.GetQRFFinance().ReserveMinimumCapture_S(operation, quote, m_Operations.QRFOverhead(tier), tier);
		if (!operation.m_Funding)
			ReportSpawnWait(operation, "Faction funds cannot cover full response collateral and deployment overhead");
		return operation.m_Funding != null;
	}
	protected void ConfigureForceFunding(AROR_OperationForce force, AROR_Operation operation, AROR_ForceProfile profile)
	{
		force.m_Funding = operation.m_Funding;
		if (!profile)
			return;
		force.m_iPackageQuote = profile.ReplacementQuote();
		force.m_bOwnsPackageQuote = true;
		force.m_iPersonnelLossValue = profile.m_iPersonnelLossValue;
		force.m_iVehicleLossValue = profile.VehicleLossValue();
	}

	protected bool TacticalSettingsValid()
	{
		if (m_fAttackStagingDistance < 200 || m_fAttackStagingDistance > 1500 || m_iAttackStagingTimeout < 30 || m_iAttackStagingTimeout > 600)
			return false;
		if (m_fQRFTownSupportPerMinute < 0 || m_fQRFTownSupportPerMinute > 30)
			return false;
		if (m_iOriginLaunchCooldown < 0 || m_iOriginLaunchCooldown > 3600 || m_fOriginMaxGarrisonLoss < 0 || m_fOriginMaxGarrisonLoss > 1)
			return false;
		if (m_fMinOriginDistance < 0 || m_fMinOriginDistance > 3000 || m_fOriginWaterPenalty < 1 || m_fOriginWaterPenalty > 10)
			return false;
		if (m_iQRFSplitTier < 1 || m_iQRFSplitTier > 5)
			return false;
		if (m_iQRFReinforcementDelay < 15 || m_iQRFReinforcementDelay > 180)
			return false;
		if (m_iQRFReserveWait < 60 || m_iQRFReserveWait > 480)
			return false;
		if (m_iQRFReserveWait <= m_iQRFReinforcementDelay)
			return false;
		if (m_iQRFMaxLiveGround < 4 || m_iQRFMaxLiveGround > 8)
			return false;
		if (m_iQRFMaxLiveAircraft < 6 || m_iQRFMaxLiveAircraft > 10)
			return false;
		if (m_iQRFTacticalDuration < 180 || m_iQRFTacticalDuration > 720)
			return false;
		if (m_iQRFTownHoldDuration < 0 || m_iQRFTownHoldDuration > 1800)
			return false;
		if (m_iQRFOrderInterval < 10 || m_iQRFOrderInterval > 60)
			return false;
		if (m_iQRFApproachAttempts < 4 || m_iQRFApproachAttempts > 16)
			return false;
		if (m_fQRFRoadLaunchRadius < 75 || m_fQRFRoadLaunchRadius > 300)
			return false;
		if (m_fQRFVehicleArrivalTolerance < 10 || m_fQRFVehicleArrivalTolerance > 40)
			return false;
		if (m_fQRFObjectiveDeliveryRadius < 50 || m_fQRFObjectiveDeliveryRadius > 150)
			return false;
		if (m_fQRFMaximumRoadDismount < 0 || m_fQRFMaximumRoadDismount > 350)
			return false;
		if (m_iQRFIngressTimeout < 120 || m_iQRFIngressTimeout > 600)
			return false;
		if (m_iQRFDetourTimeout < 60 || m_iQRFDetourTimeout > 300)
			return false;
		if (m_iQRFExitTimeout < 15 || m_iQRFExitTimeout > 90)
			return false;
		if (m_iQRFExitRetryLimit < 0 || m_iQRFExitRetryLimit > 5)
			return false;
		if (m_iQRFExitRecoveryDelay < 0 || m_iQRFExitRecoveryDelay > 20 || m_iQRFExitRecoveryDelay >= m_iQRFExitTimeout)
			return false;
		if (m_iQRFInfantryLegTimeout < 60 || m_iQRFInfantryLegTimeout > 300)
			return false;
		if (m_fQRFInfantryPlanningSpeed < 0.5 || m_fQRFInfantryPlanningSpeed > 3)
			return false;
		if (m_fQRFRecoveryRadius < 20 || m_fQRFRecoveryRadius > 200 || m_fQRFVehicleRecoveryRadius < 20 || m_fQRFVehicleRecoveryRadius > 250)
			return false;
		if (m_fQRFThreatDismount < 100 || m_fQRFThreatDismount > 350)
			return false;
		if (m_fQRFWithdrawFraction < 0.1 || m_fQRFWithdrawFraction > 0.7)
			return false;
		if (m_fQRFLocalWithdrawFraction < 0.1 || m_fQRFLocalWithdrawFraction > 0.8)
			return false;
		if (m_iQRFGroundRetention < 30 || m_iQRFGroundRetention > 600)
			return false;
		return m_fQRFGroundCleanupDistance >= 500 && m_fQRFGroundCleanupDistance <= 2500;
	}

	protected void CreateQRFTacticalCommander(AROR_Operation operation, int packages, vector origin)
	{
		if (!m_bQRFTacticalCommander || !operation.UsesTacticalResponse() || operation.m_TacticalCommander)
			return;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent target;
		if (locations)
			target = locations.GetLocationById(operation.GetTargetLocationId());
		if (!target)
			return;
		AROR_QRFTacticalCommander commander = new AROR_QRFTacticalCommander();
		commander.Initialize(operation.GetResponseLevel(), packages, m_bQRFTwoWaves, m_iQRFSplitTier, m_iQRFReinforcementDelay, m_iQRFReserveWait, m_iQRFTacticalDuration, m_iQRFOrderInterval, m_fQRFWithdrawFraction);
		commander.m_iTownHoldSeconds = m_iQRFTownHoldDuration;
		commander.m_fSecurityRadius = m_fQRFObjectiveDeliveryRadius;
		commander.m_vTown = QRFObjectivePoint(target);
		commander.m_fTownRadius = target.GetRadius();
		commander.m_fMargin = m_fQRFSearchMargin;
		commander.m_fThreatStandoff = m_fQRFThreatDismount;
		commander.m_vOrigin = origin;
		commander.m_eRole = operation.GetRole();
		commander.m_bAssault = operation.GetType() == AROR_EOperationType.ATTACK;
		array<AROR_LocationComponent> outposts = {};
		locations.GetLocations(outposts);
		foreach (AROR_LocationComponent outpost : outposts)
		{
			if (outpost && AROR_QRFOriginPolicy.IsOriginType(outpost.GetLocationType()) && outpost.GetControllingRole() == operation.GetRole() && vector.DistanceXZ(origin, outpost.GetCenter()) < 1)
				commander.m_sOriginLocationId = outpost.GetId();
		}
		// The base is committed: it cannot launch again until its cooldown passes
		if (!commander.m_sOriginLocationId.IsEmpty())
			m_mOriginLaunchedAt.Set(commander.m_sOriginLocationId, m_iExecutorClock);
		commander.RecordTargetControl(target.GetControllingRole());
		operation.m_TacticalCommander = commander;
		Print(string.Format("[AROR TACTICAL] %1 role=%2 tier=%3 first wave=%4 reserve=%5; entire force reserved, one construction per tick", operation.GetId(), operation.GetRole(), operation.GetResponseLevel(), commander.m_iFirstWaveCount, packages - commander.m_iFirstWaveCount), LogLevel.NORMAL);
	}
	//! Military tactical objectives follow the actual defence flag, while stable
	//! location IDs and canonical control/radii remain owned by Locations.
	protected vector QRFObjectivePoint(AROR_LocationComponent target)
	{
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(target.GetOwner().FindComponent(AROR_GarrisonComponent));
		if (garrison && garrison.IsEligible())
		{
			vector flag = garrison.GetDefenceCenter();
			if (target.IsPositionInside(flag))
				return flag;
		}
		return target.GetCenter();
	}

	protected void QueueTacticalGroundQRF(AROR_Operation operation, array<AROR_ForceProfile> elements)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent target;
		if (locations)
			target = locations.GetLocationById(operation.GetTargetLocationId());
		if (!target)
		{
			FailOperation(operation, "Tactical QRF target unavailable");
			return;
		}
		vector origin = m_vOccupiersSpawnPosition;
		if (operation.GetRole() == AROR_EFactionRole.INVADERS)
			origin = m_vInvadersSpawnPosition;
		origin = ResolveQRFOrigin(operation, target, origin);
		CreateQRFTacticalCommander(operation, elements.Count(), origin);
		if (!operation.m_TacticalCommander)
		{
			FailOperation(operation, "Cannot create QRF tactical plan without target");
			return;
		}
		operation.m_TacticalCommander.BalanceGroundWaves(elements);
		if (operation.m_TacticalCommander.m_iFirstWaveCount > m_iQRFMaxLiveGround)
		{
			FailOperation(operation, "QRF lead wave exceeds live-ground limit; enable waves, reduce recipe or raise configured limit");
			return;
		}
		for (int index = 0; index < elements.Count(); index++)
		{
			AROR_QRFDeferredForce pending = new AROR_QRFDeferredForce();
			pending.m_sOperationId = operation.GetId();
			pending.m_iPackageIndex = index;
			pending.m_bReservation = true;
			pending.m_Profile = elements[index];
			ConfigureForceFunding(pending, operation, pending.m_Profile);
			pending.m_iReservedGroups = pending.m_Profile.GetGroupBudget();
			pending.m_iReservedMembers = pending.m_Profile.m_iPersonnelBudget;
			if (pending.m_Profile.m_eType != AROR_EForceType.INFANTRY)
				pending.m_iReservedVehicles = 1;
			m_aForces.Insert(pending);
		}
	}

	protected int LiveGroundQRFCount()
	{
		int count = 0;
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (force.m_bReservation || AROR_ReaperAirForce.Cast(force))
				continue;
			// A package retained without ever receiving orders (it never boarded) is idle beside its launch site
			// until cleanup; counting it let a few failed launches block every later response
			if (force.m_bTacticalRetained && !force.m_bOrdersIssued)
				continue;
			if (force.m_bTacticalRetained)
			{
				int living = force.CountLivingMembers();
				if (force.m_Group && force.m_Group.IsDormant())
					living = Math.Max(living, force.m_Group.GetDormantAliveCount());
				AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
				if (ground)
				{
					living += ground.CountCrew();
					if (ground.m_CrewGroup && ground.m_CrewGroup.IsDormant())
						living += ground.m_CrewGroup.GetDormantAliveCount();
				}
				if (living == 0)
					continue;
			}
			count++;
		}
		return count;
	}
	protected bool HasPendingQRF(string operationId)
	{
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (force.m_sOperationId == operationId && AROR_QRFDeferredForce.Cast(force))
				return true;
			AROR_ReaperAirForce air = AROR_ReaperAirForce.Cast(force);
			if (force.m_sOperationId == operationId && air && air.m_bConstructionPending)
				return true;
		}
		return false;
	}
	protected bool HasOperatingQRF(string operationId)
	{
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (force.m_sOperationId != operationId || force.m_bTacticalRetained)
				continue;
			if (!force.m_bReservation || AROR_QRFDeferredForce.Cast(force))
				return true;
		}
		return false;
	}
	protected int LiveTacticalAircraftCount()
	{
		int count = AROR_ReaperRetirement.LiveAircraftCount();
		foreach (AROR_OperationForce tracked : m_aForces)
		{
			AROR_ReaperAirForce air = AROR_ReaperAirForce.Cast(tracked);
			if (!air || air.m_bConstructionPending)
				continue;
			if (AROR_ReaperAirForce.AircraftAlive(air.m_Vehicle))
				count++;
			if (air.m_Escort && AROR_ReaperAirForce.AircraftAlive(air.m_Escort.m_Vehicle))
				count++;
		}
		return count;
	}

	//! Recheck the committed launch site immediately before converting a queued package into actors.
	protected bool TacticalOriginAvailable(AROR_Operation operation)
	{
		AROR_QRFTacticalCommander commander = operation.m_TacticalCommander;
		if (!commander)
			return false;
		if (commander.m_sOriginLocationId.IsEmpty())
			return !m_bQRFRequireFriendlyOrigin && !operation.m_bDebugRequest && operation.GetType() == AROR_EOperationType.QRF;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent origin;
		if (locations)
			origin = locations.GetLocationById(commander.m_sOriginLocationId);
		return origin && AROR_QRFOriginPolicy.IsOriginType(origin.GetLocationType()) && origin.GetControllingRole() == operation.GetRole() && !AROR_RecaptureComponent.IsContested(origin.GetId());
	}

	protected void MonitorDeferredQRF(AROR_QRFDeferredForce pending)
	{
		AROR_Operation operation = m_Operations.GetOperation(pending.m_sOperationId);
		if (!operation || operation.IsTerminal())
		{
			ReleaseForce(pending);
			return;
		}
		AROR_QRFTacticalCommander commander = operation.m_TacticalCommander;
		if (!commander || commander.ReserveExpired(m_iExecutorClock))
		{
			ReleaseForce(pending);
			Print(string.Format("[AROR TACTICAL] %1 reserve package=%2 cancelled without spawning; deadline/withdrawal", operation.GetId(), pending.m_iPackageIndex + 1), LogLevel.WARNING);
			FinishGroundQRFIfIdle(operation);
			return;
		}
		if (!TacticalOriginAvailable(operation))
		{
			commander.m_bWithdraw = true;
			FailPackage(pending, operation, "Committed ground launch site lost or contested before construction");
			return;
		}
		if (m_iLastQRFConstructionTick == m_iExecutorClock)
			return;
		int live = LiveGroundQRFCount();
		if (!commander.CanConstruct(pending.m_iPackageIndex, m_iExecutorClock, live, m_iQRFMaxLiveGround, 1))
		{
			if (live + 1 > m_iQRFMaxLiveGround && !commander.m_bWithdraw)
				ReportSpawnWait(operation, string.Format("live ground package cap reached (%1 of %2 in the field)", live, m_iQRFMaxLiveGround));
			return;
		}
		AROR_CommanderManagerComponent strategic = AROR_CommanderManagerComponent.GetInstance();
		if (!strategic || !strategic.GetCommander(operation.GetRole()))
		{
			ReportSpawnWait(operation, string.Format("no AI commander for role %1", operation.GetRole()));
			return;
		}
		m_iLastQRFConstructionTick = m_iExecutorClock;
		m_iQRFConstructionIndex = pending.m_iPackageIndex;
		m_sQRFConstructionOperation = operation.GetId();
		if (pending.m_Profile.m_eType == AROR_EForceType.INFANTRY)
			StartInfantryForce(operation, pending.m_Profile, pending.m_iPackageIndex, pending);
		else
			StartGroundForce(operation, strategic.GetCommander(operation.GetRole()), pending.m_Profile, pending.m_iPackageIndex, pending);
		m_iQRFConstructionIndex = -1;
		m_sQRFConstructionOperation = string.Empty;
		ReleaseForce(pending);
		if (!operation.IsTerminal() && !HasOperatingQRF(operation.GetId()))
			FailOperation(operation, "No usable elements remain after reserve construction");
		else
			TryActivateOperation(operation);
	}

	protected void TickQRFTacticalCommanders()
	{
		array<AROR_Operation> operations = {};
		m_Operations.GetOperations(operations);
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		foreach (AROR_Operation operation : operations)
		{
			AROR_QRFTacticalCommander commander = operation.m_TacticalCommander;
			if (!commander || operation.IsTerminal() || m_iExecutorClock < commander.m_iNextContactCheck)
				continue;
			commander.m_iNextContactCheck = m_iExecutorClock + 5;
			bool wasWithdrawing = commander.m_bWithdraw;
			AROR_LocationComponent target;
			if (locations)
				target = locations.GetLocationById(operation.GetTargetLocationId());
			if (!target)
				commander.m_bWithdraw = true;
			else if (commander.RecordTargetControl(target.GetControllingRole()))
				Print(string.Format("[AROR TACTICAL] %1 town control changed to role=%2; continuing investigation and security task", operation.GetId(), target.GetControllingRole()), LogLevel.NORMAL);
			if (locations && !commander.m_sOriginLocationId.IsEmpty())
			{
				AROR_LocationComponent origin = locations.GetLocationById(commander.m_sOriginLocationId);
				if (!origin || origin.GetControllingRole() != operation.GetRole() || AROR_RecaptureComponent.IsContested(origin.GetId()))
				{
					commander.m_bWithdraw = true;
					RetargetWithdrawalOrigin(operation, commander, locations);
				}
			}
			AROR_CombatContact report = m_Operations.GetRecentCombatContact(operation.GetRole(), commander.m_vTown, commander.m_fTownRadius + commander.m_fMargin, m_iQRFContactMaxAge);
			commander.UpdateContact_S(report, m_iExecutorClock);
			commander.RefreshRoster(m_iExecutorClock);
			if (commander.ShouldWithdraw(m_iExecutorClock, HasPendingQRF(operation.GetId())) && !wasWithdrawing)
				Print(string.Format("[AROR TACTICAL] %1 withdrawing: ownership, casualties or tactical duration", operation.GetId()), LogLevel.WARNING);
			if (target && commander.m_iTownHoldStarted >= 0 && !commander.m_bWithdraw)
				PacifyTown(operation, target);
		}
	}
	protected void RetargetWithdrawalOrigin(AROR_Operation operation, AROR_QRFTacticalCommander commander, AROR_LocationManagerComponent locations)
	{
		array<AROR_LocationComponent> outposts = {};
		locations.GetLocations(outposts);
		AROR_LocationComponent best;
		float bestDistance = float.MAX;
		foreach (AROR_LocationComponent outpost : outposts)
		{
			if (!outpost || !AROR_QRFOriginPolicy.IsOriginType(outpost.GetLocationType()) || outpost.GetControllingRole() != operation.GetRole())
				continue;
			if (AROR_RecaptureComponent.IsContested(outpost.GetId()))
				continue;
			float distance = vector.DistanceXZ(outpost.GetCenter(), commander.m_vTown);
			if (distance < bestDistance && IsLaunchPositionSafe(outpost.GetCenter(), commander.m_vTown))
			{
				best = outpost;
				bestDistance = distance;
			}
		}
		commander.m_bSafeWithdrawalOrigin = best != null;
		if (best)
		{
			commander.m_sOriginLocationId = best.GetId();
			commander.m_vOrigin = best.GetCenter();
		}
	}

	//! distance: below 0 uses the configured cleanup distance
	protected bool TacticalEntitiesFar(AROR_OperationForce force, float distance = -1)
	{
		if (distance < 0)
			distance = m_fQRFGroundCleanupDistance;
		array<IEntity> members = {};
		force.GetOwnedMembers(members);
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
		if (ground && ground.m_Vehicle)
			members.Insert(ground.m_Vehicle);
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!player)
				continue;
			foreach (IEntity member : members)
			{
				if (member && vector.DistanceXZ(player.GetOrigin(), member.GetOrigin()) < distance)
					return false;
			}
		}
		return true;
	}
	protected void RetainTacticalForce(AROR_OperationForce force)
	{
		if (force.m_bTacticalRetained)
			return;
		force.m_bTacticalRetained = true;
		force.m_iTacticalRetainedAt = m_iExecutorClock;
		force.m_iTacticalNextOrder = m_iExecutorClock + 15;
		// Cancel the old charge/search order without deleting casualties or captured actors.
		if (force.m_Group && force.m_Group.GetLeaderEntity())
			ReplaceTacticalWaypoint(force, false, m_sQRFWaypointPrefab, force.m_Group.GetLeaderEntity().GetOrigin());
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
		if (ground && ground.m_CrewGroup && ground.m_Vehicle)
		{
			if (ground.m_TownSearch)
			{
				ground.m_TownSearch.Cleanup_S();
				ground.m_TownSearch = null;
			}
			ReplaceTacticalWaypoint(force, true, m_sQRFWaypointPrefab, ground.m_Vehicle.GetOrigin());
		}
		Print(string.Format("[AROR TACTICAL] %1 package=%2 retained with its budget; minimum age AND all-player distance required", force.m_sOperationId, force.m_iPackageIndex + 1), LogLevel.NORMAL);
	}
	protected void MonitorTacticalRetention(AROR_OperationForce force)
	{
		if (m_iExecutorClock < force.m_iTacticalNextOrder)
			return;
		force.m_iTacticalNextOrder = m_iExecutorClock + 15;
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
		if (ground && !ground.m_bDismounted && ground.m_Vehicle && (ground.m_Profile.m_eType != AROR_EForceType.ARMOUR || ground.m_Profile.m_bFillPassengerSeats))
		{
			Physics physics = ground.m_Vehicle.GetPhysics();
			vector position = ground.m_Vehicle.GetOrigin();
			float height = position[1] - GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			if (physics && physics.GetVelocity().Length() < 2 && height < 5)
				ground.RequestDismount_S();
			if (ground.ArePassengersOut())
			{
				ground.m_bDismounted = true;
				AROR_Operation operation = m_Operations.GetOperation(force.m_sOperationId);
				if (operation && operation.m_TacticalCommander && operation.m_TacticalCommander.m_bSafeWithdrawalOrigin)
					ReplaceTacticalWaypoint(force, false, m_sMoveWaypointPrefab, operation.m_TacticalCommander.m_vOrigin);
			}
		}
		// Never-dispatched packages have no fight to finish: clear them as soon as no player is close by, rather
		// than leave them beside their launch base until everyone is over a kilometre away
		if (!force.m_bOrdersIssued)
		{
			if (TacticalEntitiesFar(force, UNDISPATCHED_CLEANUP_DISTANCE))
				ReleaseForce(force);
			return;
		}
		if (m_iExecutorClock - force.m_iTacticalRetainedAt < m_iQRFGroundRetention)
			return;
		if (TacticalEntitiesFar(force))
			ReleaseForce(force);
	}

	protected bool ReplaceTacticalWaypoint(AROR_OperationForce force, bool crew, ResourceName prefab, vector point)
	{
		SCR_AIGroup group = force.m_Group;
		AIWaypoint previous = force.m_Waypoint;
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
		if (crew && ground && ground.m_CrewGroup != force.m_Group)
		{
			group = ground.m_CrewGroup;
			previous = ground.m_CrewWaypoint;
		}
		if (!group || group.GetPlayerCount() > 0)
			return false;
		AIWaypoint next = SpawnWaypoint(prefab, point);
		if (!next)
			return false;
		next.SetCompletionRadius(20);
		group.AddWaypoint(next);
		if (previous)
		{
			group.RemoveWaypoint(previous);
			SCR_EntityHelper.DeleteEntityAndChildren(previous);
		}
		if (crew && ground && ground.m_CrewGroup != force.m_Group)
			ground.m_CrewWaypoint = next;
		else
			force.m_Waypoint = next;
		return true;
	}

	protected vector TacticalLaunchPoint(AROR_QRFTacticalCommander commander, int index)
	{
		vector heading = commander.m_vTown - commander.m_vOrigin;
		heading[1] = 0;
		if (heading.Length() < 1)
			heading = "0 0 1";
		else
			heading = heading.Normalized();
		vector right = Vector(heading[2], 0, -heading[0]);
		return commander.m_vOrigin + right * ((index - (commander.m_iPackages - 1) * 0.5) * m_fPackageSpacing);
	}

	//! Outpost centres are strategic anchors, not pre-cleared vehicle spawn pads.
	protected bool FindGroundRoadLaunch(AROR_QRFTacticalCommander commander, int index, out vector point)
	{
		BaseWorld world = GetGame().GetWorld();
		vector seed = TacticalLaunchPoint(commander, index);
		int noRoad;
		int outside;
		int crowded;
		int unsafe;
		int wet;
		int steep;
		int obstructed;
		// Preserve the original nearby probes, then cover the outer origin area.
		// The former 30/60m seed-only search could repeatedly snap onto pads occupied
		// by the first two packages and could cancel an otherwise funded response.
		for (int attempt = 0; attempt < 36; attempt++)
		{
			float angle = attempt * 0.78539816;
			float radius = 30;
			if (attempt >= 8)
				radius = 60;
			vector desired = seed;
			if (attempt > 0)
				desired = seed + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius;
			if (attempt >= 12)
			{
				int ring = (attempt - 12) / 8 + 3;
				angle = Math.Mod(attempt - 12, 8) * 0.78539816;
				radius = m_fQRFRoadLaunchRadius * ring / 5;
				desired = commander.m_vOrigin + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius;
			}
			vector road;
			// The base centre can sit inside a walled compound or under a structure, cut off on the navmesh; a road
			// next to the probe point is still a valid place to put the vehicle
			if (!TacticalRoadPoint(commander.m_vOrigin, desired, road) && !TacticalRoadPoint(desired, desired, road))
			{
				noRoad++;
				continue;
			}
			if (vector.DistanceXZ(road, commander.m_vOrigin) > m_fQRFRoadLaunchRadius)
			{
				outside++;
				continue;
			}
			if (!IsLaunchPositionSafe(road, commander.m_vTown))
			{
				unsafe++;
				continue;
			}
			bool separated = true;
			foreach (AROR_OperationForce other : m_aForces)
			{
				AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(other);
				if (ground && ground.m_Vehicle && vector.DistanceXZ(road, ground.m_Vehicle.GetOrigin()) < 25)
					separated = false;
			}
			if (!separated)
			{
				crowded++;
				continue;
			}
			float height = world.GetSurfaceY(road[0], road[2]);
			if (height <= world.GetOceanHeight(road[0], road[2]) + 0.5)
			{
				wet++;
				continue;
			}
			bool level = true;
			for (int side = 0; side < 4; side++)
			{
				float bearing = side * 1.57079633;
				float sample = world.GetSurfaceY(road[0] + Math.Sin(bearing) * 6, road[2] + Math.Cos(bearing) * 6);
				if (Math.AbsFloat(sample - height) > 1.5)
					level = false;
			}
			if (!level)
			{
				steep++;
				continue;
			}
			TraceBox trace = new TraceBox();
			trace.Start = Vector(road[0], height + 0.35, road[2]);
			trace.End = trace.Start + Vector(0, 0.05, 0);
			trace.Mins = Vector(-6, 0, -6);
			trace.Maxs = Vector(6, 4.5, 6);
			trace.Flags = TraceFlags.ENTS;
			if (world.TraceMove(trace, null) < 1)
			{
				obstructed++;
				continue;
			}
			road[1] = height;
			point = road;
			Print(string.Format("[AROR TACTICAL] road launch package=%1 origin=%2 position=%3 queries=%4", index + 1, commander.m_sOriginLocationId, point, attempt + 1), LogLevel.NORMAL);
			return true;
		}
		Print(string.Format("[AROR TACTICAL] road launch rejected package=%1 origin=%2 radius=%3 queries=36", index + 1, commander.m_sOriginLocationId, m_fQRFRoadLaunchRadius), LogLevel.WARNING);
		Print(string.Format("[AROR TACTICAL] launch rejections noRoad=%1 outside=%2 unsafe=%3 spacing=%4 water=%5 slope=%6 obstruction=%7", noRoad, outside, unsafe, crowded, wet, steep, obstructed), LogLevel.WARNING);
		// No road anywhere around the base is fixed geometry, unlike players or parked vehicles in the way:
		// stop choosing it as a launch site so later responses go straight to a base that can launch
		if (noRoad == 36 && !commander.m_sOriginLocationId.IsEmpty() && !m_aRoadlessOrigins.Contains(commander.m_sOriginLocationId))
		{
			m_aRoadlessOrigins.Insert(commander.m_sOriginLocationId);
			Print(string.Format("[AROR TACTICAL] origin %1 has no reachable road within %2m; no longer used as a launch site this session", commander.m_sOriginLocationId, m_fQRFRoadLaunchRadius), LogLevel.WARNING);
		}
		return false;
	}

	protected IEntity SpawnGroundVehicle(Resource resource, vector point, vector destination)
	{
		vector forward = destination - point;
		forward[1] = 0;
		if (forward.Length() < 1)
			forward = "0 0 1";
		else
			forward = forward.Normalized();
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[0] = Vector(forward[2], 0, -forward[0]);
		spawnParams.Transform[1] = "0 1 0";
		spawnParams.Transform[2] = forward;
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 0.35;
		spawnParams.Transform[3] = point;
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
	}
#ifdef WORKBENCH
	//! Geometry/connectivity probe only: bypasses ownership to cover prospective campaigns.
	//! Live player clearance, actual scene obstacles and the normal bounded searches still apply.
	bool ProbeGroundRoute_S(AROR_LocationComponent origin, AROR_LocationComponent target, out vector launch, out vector approach)
	{
		if (!Replication.IsServer() || !origin || !target || origin == target || !TacticalSettingsValid())
			return false;
		AROR_QRFTacticalCommander commander = new AROR_QRFTacticalCommander();
		commander.m_vOrigin = origin.GetCenter();
		commander.m_sOriginLocationId = origin.GetId();
		commander.m_vTown = QRFObjectivePoint(target);
		commander.m_fTownRadius = target.GetRadius();
		commander.m_fMargin = m_fQRFSearchMargin;
		if (!FindGroundRoadLaunch(commander, 0, launch))
			return false;
		AROR_GroundOperationForce probe = new AROR_GroundOperationForce();
		probe.m_sOperationId = "route_audit";
		return FindTacticalApproach(probe, commander, launch, m_fQRFObjectiveDeliveryRadius * 0.6, approach);
	}
#endif

	protected bool TacticalRoadPoint(vector origin, vector desired, out vector reachable, float searchRadius = 75)
	{
		ChimeraAIWorld world = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!world || !world.GetRoadNetworkManager())
			return false;
		// Configured origins commonly store Y=0; native road queries need terrain positions.
		origin[1] = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
		desired[1] = GetGame().GetWorld().GetSurfaceY(desired[0], desired[2]);
		return world.GetRoadNetworkManager().GetReachableWaypointInRoad(origin, desired, searchRadius, reachable);
	}
	//! Two bounded searches: objective delivery first, then a nearby road dismount.
	//! The second search changes the vehicle stop, never the physical security objective.
	protected bool FindTacticalApproach(AROR_GroundOperationForce force, AROR_QRFTacticalCommander commander, vector origin, float standoff, out vector approach, int firstAttempt = 0, bool preferFoot = false)
	{
		int phases = 1;
		if (m_fQRFMaximumRoadDismount > m_fQRFObjectiveDeliveryRadius)
			phases = 2;
		int noRoad;
		int outside;
		int badSnap;
		int tooNear;
		int crowded;
		int noFoot;
		int queries;
		float minimumSpacing = Math.Min(18, m_fQRFSearchStopSpacing);
		for (int phase = 0; phase < phases; phase++)
		{
			bool footApproach = phase == 1;
			if (preferFoot && phases == 2)
				footApproach = phase == 0;
			float maximum = Math.Min(m_fQRFObjectiveDeliveryRadius, commander.m_fTownRadius + commander.m_fMargin);
			if (footApproach)
				maximum = m_fQRFMaximumRoadDismount;
			vector fallback;
			float bestClearance = -1;
			for (int attempt = 0; attempt < m_iQRFApproachAttempts; attempt++)
			{
				int candidate = (attempt + firstAttempt) % m_iQRFApproachAttempts;
				vector desired = commander.ApproachCandidate(force.m_iPackageIndex, origin, standoff, candidate);
				if (footApproach)
				{
					if (preferFoot)
						candidate = attempt;
					// Try the current approach side first, rather than an arbitrary north-facing ring.
					vector ingress = origin - commander.m_vTown;
					float angle = Math.Atan2(ingress[0], ingress[2]) + candidate * 0.78539816 + force.m_iPackageIndex * 0.2;
					float radius = Math.Max(140, m_fQRFObjectiveDeliveryRadius + 60);
					if (attempt >= 8)
						radius += 60;
					radius = Math.Min(maximum, radius);
					desired = commander.m_vTown + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius;
				}
				vector reachable;
				queries++;
				if (!TacticalRoadPoint(origin, desired, reachable))
				{
					noRoad++;
					continue;
				}
				float townDistance = vector.DistanceXZ(reachable, commander.m_vTown);
				if (townDistance > maximum || townDistance < 15)
				{
					outside++;
					continue;
				}
				if (vector.DistanceXZ(reachable, desired) > 75)
				{
					badSnap++;
					continue;
				}
				if (vector.DistanceXZ(reachable, origin) < 40)
				{
					tooNear++;
					continue;
				}
				if (footApproach && !CanDismountOnFoot(reachable))
				{
					noFoot++;
					continue;
				}
				float clearance = float.MAX;
				foreach (AROR_OperationForce tracked : m_aForces)
				{
					AROR_GroundOperationForce other = AROR_GroundOperationForce.Cast(tracked);
					if (!other || other == force || other.m_sOperationId != force.m_sOperationId || other.m_bReservation)
						continue;
					clearance = Math.Min(clearance, vector.DistanceXZ(other.m_vApproachPosition, reachable));
				}
				if (clearance < m_fQRFSearchStopSpacing)
				{
					crowded++;
					if (clearance >= minimumSpacing && clearance > bestClearance)
					{
						bestClearance = clearance;
						fallback = reachable;
					}
					continue;
				}
				approach = reachable;
				force.m_fTacticalDeliveryRadius = 0;
				if (footApproach)
					force.m_fTacticalDeliveryRadius = Math.Min(maximum, townDistance + m_fQRFVehicleArrivalTolerance);
				Print(string.Format("[AROR TACTICAL] %1 package=%2 road stop=%3 townDistance=%4 queries=%5 footApproach=%6", force.m_sOperationId, force.m_iPackageIndex + 1, approach, townDistance, queries, footApproach), LogLevel.NORMAL);
				return true;
			}
			if (bestClearance >= 0)
			{
				approach = fallback;
				force.m_fTacticalDeliveryRadius = 0;
				if (footApproach)
					force.m_fTacticalDeliveryRadius = Math.Min(maximum, vector.DistanceXZ(fallback, commander.m_vTown) + m_fQRFVehicleArrivalTolerance);
				Print(string.Format("[AROR TACTICAL] %1 package=%2 compact road spacing=%3 footApproach=%4", force.m_sOperationId, force.m_iPackageIndex + 1, bestClearance, footApproach), LogLevel.NORMAL);
				return true;
			}
		}
		// Last resort: the nearest launch-reachable road anywhere within the dismount limit of the objective, then on foot.
		// Walled compounds and off-road sites (Levie, Provins) left every ring probe without a reachable road.
		vector nearest;
		if (m_fQRFMaximumRoadDismount > m_fQRFObjectiveDeliveryRadius && TacticalRoadPoint(origin, commander.m_vTown, nearest, m_fQRFMaximumRoadDismount))
		{
			float nearestDistance = vector.DistanceXZ(nearest, commander.m_vTown);
			float nearestClearance = float.MAX;
			foreach (AROR_OperationForce sibling : m_aForces)
			{
				AROR_GroundOperationForce neighbour = AROR_GroundOperationForce.Cast(sibling);
				if (neighbour && neighbour != force && neighbour.m_sOperationId == force.m_sOperationId && !neighbour.m_bReservation)
					nearestClearance = Math.Min(nearestClearance, vector.DistanceXZ(neighbour.m_vApproachPosition, nearest));
			}
			if (nearestDistance >= 15 && nearestDistance <= m_fQRFMaximumRoadDismount && nearestClearance >= minimumSpacing && vector.DistanceXZ(nearest, origin) >= 40 && CanDismountOnFoot(nearest))
			{
				approach = nearest;
				force.m_fTacticalDeliveryRadius = Math.Min(m_fQRFMaximumRoadDismount, nearestDistance + m_fQRFVehicleArrivalTolerance);
				Print(string.Format("[AROR TACTICAL] %1 package=%2 last-resort road stop=%3 townDistance=%4 after %5 queries; infantry walk in", force.m_sOperationId, force.m_iPackageIndex + 1, approach, nearestDistance, queries), LogLevel.WARNING);
				return true;
			}
		}
		Print(string.Format("[AROR TACTICAL] %1 package=%2 approach exhausted queries=%3 noRoad=%4 outside=%5 badSnap=%6", force.m_sOperationId, force.m_iPackageIndex + 1, queries, noRoad, outside, badSnap), LogLevel.WARNING);
		Print(string.Format("[AROR TACTICAL] approach rejections nearOrigin=%1 spacing=%2 noFootPad=%3 origin=%4 objective=%5", tooNear, crowded, noFoot, origin, commander.m_vTown), LogLevel.WARNING);
		return false;
	}
	//! Validate a road dismount against terrain and the soldier navigation world.
	//! Native infantry still has to walk the route; this is not proof of objective arrival.
	protected bool CanDismountOnFoot(vector point)
	{
		BaseWorld world = GetGame().GetWorld();
		if (world.GetSurfaceY(point[0], point[2]) <= world.GetOceanHeight(point[0], point[2]) + 0.3)
			return false;
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		NavmeshWorldComponent soldiers;
		if (aiWorld)
			soldiers = aiWorld.GetNavmeshWorldComponent("Soldiers");
		if (!soldiers)
			return false;
		if (!soldiers.IsTileLoaded(point))
		{
			soldiers.LoadTileIn(point);
			return false;
		}
		vector reachable;
		if (!soldiers.GetReachablePoint(point, 3, reachable))
			return false;
		return vector.DistanceXZ(point, reachable) <= 3 && Math.AbsFloat(point[1] - reachable[1]) <= 2;
	}
	protected void RecordTacticalStrength(AROR_OperationForce force, AROR_Operation operation, int living)
	{
		if (!force.m_bTacticalControlled || !operation.m_TacticalCommander)
			return;
		if (force.m_iTacticalInitialStrength == 0)
			force.m_iTacticalInitialStrength = living;
		operation.m_TacticalCommander.RecordStrength(force.m_iPackageIndex, living);
	}

	protected bool TacticalWithdrawal(AROR_OperationForce force, AROR_Operation operation)
	{
		AROR_QRFTacticalCommander commander = operation.m_TacticalCommander;
		if (!commander)
			return false;
		if (m_iExecutorClock < force.m_iTacticalNextOrder && force.m_bTacticalWithdrawing)
			return true;
		bool first = !force.m_bTacticalWithdrawing;
		force.m_bTacticalWithdrawing = true;
		force.m_iTacticalNextOrder = m_iExecutorClock + m_iQRFOrderInterval;
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
		if (ground && ground.m_CrewGroup && ground.m_Vehicle && commander.m_bSafeWithdrawalOrigin)
		{
			vector home;
			if (TacticalRoadPoint(ground.m_Vehicle.GetOrigin(), commander.m_vOrigin, home))
				ReplaceTacticalWaypoint(force, true, m_sMoveWaypointPrefab, home);
			else
				ReplaceTacticalWaypoint(force, true, m_sQRFWaypointPrefab, ground.m_Vehicle.GetOrigin());
		}
		bool infantryOnFoot = !ground;
		if (ground)
			infantryOnFoot = ground.m_bDismounted || ground.HasDismountedInfantry();
		if (infantryOnFoot && commander.m_bSafeWithdrawalOrigin)
			ReplaceTacticalWaypoint(force, false, m_sMoveWaypointPrefab, commander.m_vOrigin);
		if (ground && ground.m_CrewGroup && ground.m_Vehicle && !commander.m_bSafeWithdrawalOrigin)
			ReplaceTacticalWaypoint(force, true, m_sQRFWaypointPrefab, ground.m_Vehicle.GetOrigin());
		if (!commander.m_bSafeWithdrawalOrigin && force.m_Group && force.m_Group.GetLeaderEntity())
			ReplaceTacticalWaypoint(force, false, m_sQRFWaypointPrefab, force.m_Group.GetLeaderEntity().GetOrigin());
		if (first)
			Print(string.Format("[AROR TACTICAL] %1 package=%2 withdrawing to origin; no remount teleport or nearby deletion", operation.GetId(), force.m_iPackageIndex + 1), LogLevel.WARNING);
		// Recovery requires an actual return, not merely moving away from the observing player.
		if (TacticalForceReturned(force, commander))
		{
			commander.MarkRecovered(force.m_iPackageIndex);
			operation.m_iRecoveredPackages++;
			RetainTacticalForce(force);
			Print(string.Format("[AROR TACTICAL] %1 package=%2 physically returned to friendly origin; retaining until cleanup is safe", operation.GetId(), force.m_iPackageIndex + 1), LogLevel.NORMAL);
			FinishGroundQRFIfIdle(operation);
		}
		return true;
	}
	protected void FinishGroundQRFIfIdle(AROR_Operation operation)
	{
		if (operation.IsTerminal() || HasOperatingQRF(operation.GetId()) || HasPendingQRF(operation.GetId()))
			return;
		if (operation.GetType() == AROR_EOperationType.ATTACK)
		{
			AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
			AROR_LocationComponent target;
			if (locations)
				target = locations.GetLocationById(operation.GetTargetLocationId());
			if (target && target.GetControllingRole() == operation.GetRole() && operation.GetState() == AROR_EOperationState.ACTIVE)
				m_Operations.ResolveOperation_S(operation.GetId(), AROR_EOperationState.SUCCEEDED, "Target captured through canonical territory rules");
			else
				FailOperation(operation, "Attack withdrew without capturing its target");
			return;
		}
		if (operation.m_iRecoveredPackages > 0 && operation.GetState() == AROR_EOperationState.ACTIVE)
			m_Operations.ResolveOperation_S(operation.GetId(), AROR_EOperationState.SUCCEEDED, "Response task ended; surviving packages returned, no ownership change implied");
		else
			FailOperation(operation, "No surviving QRF elements or usable reserve");
	}
	protected bool TacticalForceReturned(AROR_OperationForce force, AROR_QRFTacticalCommander commander)
	{
		if (!commander.m_bSafeWithdrawalOrigin || !force.m_Group || force.m_Group.GetPlayerCount() > 0)
			return false;
		AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
		if (ground && ground.m_CrewGroup && ground.m_CrewGroup.GetPlayerCount() > 0)
			return false;
		if (ground && ground.m_Vehicle)
		{
			DamageManagerComponent damage = DamageManagerComponent.Cast(ground.m_Vehicle.FindComponent(DamageManagerComponent));
			if ((!damage || !damage.IsDestroyed()) && vector.DistanceXZ(ground.m_Vehicle.GetOrigin(), commander.m_vOrigin) > m_fQRFVehicleRecoveryRadius)
				return false;
		}
		array<IEntity> members = {};
		force.GetOwnedMembers(members);
		int living;
		foreach (IEntity member : members)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(member);
			if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
				continue;
			living++;
			if (vector.DistanceXZ(member.GetOrigin(), commander.m_vOrigin) > m_fQRFRecoveryRadius)
				return false;
		}
		return living > 0;
	}

	//! A native empty transport may clear its affiliation after unloading. Never re-faction it.
	//! A genuine active vehicle mismatch degrades its package, not every healthy package.
	protected bool ValidateGroundVehicleFaction(AROR_GroundOperationForce force, AROR_Operation operation, FactionAffiliationComponent affiliation)
	{
		string actual = "<missing component>";
		if (affiliation)
			actual = affiliation.GetAffiliatedFactionKey();
		if (affiliation && actual == force.m_sFactionKey)
		{
			force.m_iVehicleFactionMismatchSince = -1;
			force.m_bEmptyVehicleFactionLogged = false;
			return true;
		}
		bool occupied = force.HasVehicleOccupants();
		bool nativeVehicle = SCR_VehicleFactionAffiliationComponent.Cast(affiliation) != null;
		bool nativeEmptyKey = actual.IsEmpty();
		if (!force.m_sAssetFactionKey.IsEmpty() && actual == force.m_sAssetFactionKey)
			nativeEmptyKey = true;
		if (nativeVehicle && nativeEmptyKey)
		{
			bool unloadingComplete = force.m_bDismounted;
			if (force.m_bMounted && force.CountInfantry() > 0 && force.ArePassengersOut())
				unloadingComplete = true;
			if (unloadingComplete && !occupied && (force.m_Profile.m_eType != AROR_EForceType.ARMOUR || force.m_Profile.m_bFillPassengerSeats))
			{
				// Native occupancy/faction updates can precede this tick's delivery state update.
				force.m_bDismounted = true;
				if (force.m_bTacticalControlled && operation.m_TacticalCommander)
					TacticalInfantryOrders(force, operation);
				force.m_iVehicleFactionMismatchSince = -1;
				if (!force.m_bEmptyVehicleFactionLogged)
				{
					force.m_bEmptyVehicleFactionLogged = true;
					Print(string.Format("[AROR] %1 package=%2 profile=%3 empty delivered vehicle faction='%4'; native affiliation retained, infantry continue", operation.GetId(), force.m_iPackageIndex + 1, force.m_Profile.m_sId, actual), LogLevel.NORMAL);
				}
				return true;
			}
			// Combat deaths/exits can clear native affiliation before delivery, including
			// armour. A verified empty or own-AI-only hull is not a faction capture.
			if (force.HasOnlyOwnedVehicleOccupants())
			{
				force.m_iVehicleFactionMismatchSince = -1;
				if (!force.m_bEmptyVehicleFactionLogged)
				{
					force.m_bEmptyVehicleFactionLogged = true;
					Print(string.Format("[AROR] %1 package=%2 native vehicle faction='%3'; seat audit confirms empty/own-AI hull, keeping combat lifecycle", operation.GetId(), force.m_iPackageIndex + 1, actual), LogLevel.NORMAL);
				}
				return true;
			}
			bool exiting = force.m_bTacticalDropStarted || force.m_bDismountRequested;
			if (exiting && occupied)
			{
				if (force.m_iVehicleFactionMismatchSince < 0)
				{
					force.m_iVehicleFactionMismatchSince = m_iExecutorClock;
					Print(string.Format("[AROR] %1 package=%2 vehicle faction='%3' during passenger exit; allowing at most five seconds for native occupancy updates", operation.GetId(), force.m_iPackageIndex + 1, actual), LogLevel.WARNING);
				}
				if (m_iExecutorClock - force.m_iVehicleFactionMismatchSince < 5)
					return true;
			}
		}
		Print(string.Format("[AROR] Vehicle faction audit failed: operation=%1 package=%2 profile=%3 expected='%4' actual='%5' occupied=%6 delivered=%7 position=%8", operation.GetId(), force.m_iPackageIndex + 1, force.m_Profile.m_sId, force.m_sFactionKey, actual, occupied, force.m_bDismounted, force.m_Vehicle.GetOrigin()), LogLevel.WARNING);
		FailPackage(force, operation, "Vehicle affiliation mismatch; other active packages continue");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Attack staging, recon and local intel
	//------------------------------------------------------------------------------------------------
	//! Attacks: send a package to a road rally point on its own side of the origin line, short of the target.
	//! Skipped (the package drives straight in) when the origin is too close or no road point fits.
	protected void BeginStaging(AROR_GroundOperationForce force, AROR_QRFTacticalCommander commander, AROR_Operation operation)
	{
		if (!force.m_Vehicle || !force.m_CrewGroup)
			return;
		vector heading = commander.m_vOrigin - commander.m_vTown;
		heading[1] = 0;
		float length = heading.Length();
		if (length < m_fAttackStagingDistance + 100)
			return;
		heading = heading * (1.0 / length);
		float angle = Math.Atan2(heading[0], heading[2]);
		if (force.m_iPackageIndex % 2 == 0)
			angle -= 0.35;
		else
			angle += 0.35;
		vector desired = commander.m_vTown + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * m_fAttackStagingDistance;
		vector staging;
		if (!TacticalRoadPoint(force.m_Vehicle.GetOrigin(), desired, staging))
			return;
		float stagingDistance = vector.DistanceXZ(staging, commander.m_vTown);
		if (stagingDistance < m_fAttackStagingDistance * 0.5 || stagingDistance > length)
			return;
		if (!ReplaceTacticalWaypoint(force, true, m_sMoveWaypointPrefab, staging))
			return;
		force.m_bTacticalStaging = true;
		force.m_vStagingPosition = staging;
		if (commander.m_iStagingDeadline < 0)
			commander.m_iStagingDeadline = m_iExecutorClock + m_iAttackStagingTimeout;
		Print(string.Format("[AROR TACTICAL] %1 package=%2 moving to rally point %3 (%4 m from target)", operation.GetId(), force.m_iPackageIndex + 1, staging, stagingDistance), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Leave the rally point for the assault: the road stop for transports, the objective for armour
	protected void ReleaseFromStaging(AROR_GroundOperationForce force, AROR_QRFTacticalCommander commander)
	{
		force.m_bTacticalStaging = false;
		force.m_fTacticalBestDistance = -1;
		force.m_iTacticalLegDeadline = m_iExecutorClock + m_iQRFIngressTimeout;
		force.m_iTacticalNextRouteCheck = m_iExecutorClock + 30;
		if (force.m_Vehicle)
			force.m_vTacticalProgressPosition = force.m_Vehicle.GetOrigin();
		if ((force.m_Profile.m_eType == AROR_EForceType.ARMOUR && !force.m_Profile.m_bFillPassengerSeats))
			ReplaceTacticalWaypoint(force, true, m_sQRFWaypointPrefab, commander.m_vTown);
		else
			ReplaceTacticalWaypoint(force, true, m_sMoveWaypointPrefab, force.m_vApproachPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! The assault goes when every staging package has reached its rally point, or at the staging deadline.
	//! Recon reports where the defenders are, so the first infantry task is the defended position.
	protected void UpdateAssaultRelease(AROR_Operation operation, AROR_QRFTacticalCommander commander, AROR_LocationComponent target)
	{
		if (!commander.m_bAssault || commander.m_bAssaultReleased || commander.m_iStagingDeadline < 0)
			return;
		int staging;
		int staged;
		foreach (AROR_OperationForce tracked : m_aForces)
		{
			AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(tracked);
			if (!ground || ground.m_sOperationId != operation.GetId() || ground.m_bReservation || !ground.m_bTacticalStaging)
				continue;
			staging++;
			if (ground.m_bTacticalStaged)
				staged++;
		}
		if (staging > 0 && staged < staging && m_iExecutorClock < commander.m_iStagingDeadline)
			return;

		commander.m_bAssaultReleased = true;
		Print(string.Format("[AROR TACTICAL] %1 assault released: %2 of %3 packages at rally points", operation.GetId(), staged, staging), LogLevel.NORMAL);
		if (!target)
			return;
		ReportDefenders(operation, target);
		AROR_Notification alert = new AROR_Notification(AROR_ENotificationType.HEADLINE, "ENEMY ASSAULT / " + target.GetName(), string.Empty, AROR_UIColors.Get(AROR_UIColors.DANGER), "HQ");
		alert.SetDetail("Enemy forces are moving on the position from more than one direction.");
		AROR_NotificationManagerComponent.Send_S(alert);
	}

	//------------------------------------------------------------------------------------------------
	//! A snapshot of where the target's holders are, posted as contact for this response (recon before an assault,
	//! or locals reporting when a response arrives). No campaign escalation: it is intel, not a new attack.
	protected bool ReportDefenders(AROR_Operation operation, AROR_LocationComponent target)
	{
		// An assault does not reveal hidden defenders. Reuse only attributed, fresh reports.
		AROR_CombatContact contact = m_Operations.GetRecentCombatContact(operation.GetRole(), target.GetCenter(), target.GetRadius() + m_fQRFSearchMargin, m_iQRFContactMaxAge);
		if (!contact || !operation.m_TacticalCommander)
			return false;
		operation.m_TacticalCommander.UpdateContact_S(contact, m_iExecutorClock);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A response holding a lost town pacifies it: while its troops outnumber the Resistance there, support shifts
	//! back to its army, and the town flips back through the normal support rules
	protected void PacifyTown(AROR_Operation operation, AROR_LocationComponent target)
	{
		if (m_fQRFTownSupportPerMinute <= 0 || target.GetControllingRole() == operation.GetRole())
			return;
		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(target.GetOwner().FindComponent(AROR_SettlementComponent));
		if (!settlement)
			return;
		AROR_LocationForces forces = AROR_LocationForces.Count(target);
		AROR_EFactionRole otherArmy = AROR_EFactionRole.INVADERS;
		if (operation.GetRole() == AROR_EFactionRole.INVADERS)
			otherArmy = AROR_EFactionRole.OCCUPIERS;
		int hostile = forces.GetTotal(AROR_EFactionRole.RESISTANCE) + forces.GetTotal(otherArmy);
		if (forces.GetAI(operation.GetRole()) <= hostile)
			return;
		settlement.AddSupport_S(operation.GetRole(), m_fQRFTownSupportPerMinute * TACTICAL_CHECK_SECONDS / 60.0);
	}

	protected int GetTacticalInfantryLegSeconds(float distance)
	{
		float estimate = 30 + distance / m_fQRFInfantryPlanningSpeed;
		int seconds = 60;
		while (seconds < estimate && seconds < m_iQRFInfantryLegTimeout)
			seconds += 15;
		return Math.Min(seconds, m_iQRFInfantryLegTimeout);
	}

	protected void TacticalInfantryOrders(AROR_OperationForce force, AROR_Operation operation)
	{
		AROR_GroundOperationForce transport = AROR_GroundOperationForce.Cast(force);
		// A group waypoint affects its seated members too. A single early/native exit
		// must not order the rest of a mixed mounted/on-foot group out at the origin.
		if (transport && !transport.m_bDismounted && !transport.m_bTacticalDropStarted && !transport.ArePassengersOut())
			return;
		AROR_QRFTacticalCommander commander = operation.m_TacticalCommander;
		if (!commander || !force.m_Group || force.m_Group.GetPlayerCount() > 0)
			return;
		if (force.m_iTacticalContactVersion != commander.m_iContactVersion)
		{
			force.m_iTacticalContactVersion = commander.m_iContactVersion;
			force.m_iTacticalSearchStep = 0;
		}
		if (m_iExecutorClock < force.m_iTacticalNextOrder)
			return;
		IEntity leader = force.m_Group.GetLeaderEntity();
		if (leader && vector.DistanceXZ(leader.GetOrigin(), commander.m_vTown) <= m_fQRFObjectiveDeliveryRadius)
		{
			CompartmentAccessComponent leaderAccess = CompartmentAccessComponent.Cast(leader.FindComponent(CompartmentAccessComponent));
			if (leaderAccess && !leaderAccess.IsInCompartment() && commander.BeginTownHold(m_iExecutorClock))
				Print(string.Format("[AROR TACTICAL] %1 infantry entered town; fixed security hold starts now for %2 seconds", operation.GetId(), commander.m_iTownHoldSeconds), LogLevel.NORMAL);
		}
		if (force.m_iTacticalSearchStep > 0 && force.m_Waypoint && leader)
		{
			float remaining = vector.DistanceXZ(leader.GetOrigin(), force.m_Waypoint.GetOrigin());
			if (m_iExecutorClock >= force.m_iTacticalNextInfantryLog)
			{
				force.m_iTacticalNextInfantryLog = m_iExecutorClock + 15;
				Print(string.Format("[AROR TACTICAL] %1 infantry package=%2 position=%3 goal=%4 remaining=%5 legSecondsLeft=%6", operation.GetId(), force.m_iPackageIndex + 1, leader.GetOrigin(), force.m_Waypoint.GetOrigin(), remaining, force.m_iTacticalInfantryDeadline - m_iExecutorClock), LogLevel.NORMAL);
			}
			if (remaining > 25 && m_iExecutorClock < force.m_iTacticalInfantryDeadline)
				return;
			if (remaining > 25)
				Print(string.Format("[AROR TACTICAL] %1 infantry package=%2 leg timed out with %3m remaining; selecting next bounded sector", operation.GetId(), force.m_iPackageIndex + 1, remaining), LogLevel.WARNING);
			else
			{
				Print(string.Format("[AROR TACTICAL] %1 infantry package=%2 physically reached its investigation/search objective", operation.GetId(), force.m_iPackageIndex + 1), LogLevel.NORMAL);
				force.m_bArrivalLogged = true;
			}
		}
		int step = force.m_iTacticalSearchStep;
		ResourceName prefab = m_sMoveWaypointPrefab;
		if (step >= 5)
			prefab = m_sQRFWaypointPrefab;
		// Every delivered fireteam investigates contact/town first, then fans out into sectors.
		vector point = commander.InfantryPoint(force.m_iPackageIndex, step);
		force.m_iTacticalNextOrder = m_iExecutorClock + m_iQRFOrderInterval;
		if (!ReplaceTacticalWaypoint(force, false, prefab, point))
			return;
		float walkingDistance = 0;
		if (leader)
			walkingDistance = vector.DistanceXZ(leader.GetOrigin(), point);
		force.m_iTacticalInfantryDeadline = m_iExecutorClock + GetTacticalInfantryLegSeconds(walkingDistance);
		if (step < 5)
			force.m_iTacticalSearchStep++;
		else
			force.m_iTacticalNextOrder = m_iExecutorClock + 60;
		Print(string.Format("[AROR TACTICAL] %1 infantry package=%2 task=%3 sector point=%4 contactVersion=%5", operation.GetId(), force.m_iPackageIndex + 1, step, point, commander.m_iContactVersion), LogLevel.NORMAL);
	}

	//! Watch progress toward the objective, not just movement: a circulating vehicle is not arriving.
	protected void RecoverStalledQRFRoute(AROR_GroundOperationForce force, AROR_QRFTacticalCommander commander)
	{
		if (m_iExecutorClock < force.m_iTacticalNextRouteCheck || force.m_iTacticalRouteRetries >= 4)
			return;
		force.m_iTacticalNextRouteCheck = m_iExecutorClock + 30;
		vector position = force.m_Vehicle.GetOrigin();
		float moved = vector.DistanceXZ(position, force.m_vTacticalProgressPosition);
		force.m_vTacticalProgressPosition = position;
		float remaining = vector.DistanceXZ(position, commander.m_vTown);
		if (force.m_fTacticalBestDistance < 0 || remaining < force.m_fTacticalBestDistance - 20)
		{
			force.m_fTacticalBestDistance = remaining;
			force.m_iTacticalLastAdvance = m_iExecutorClock;
		}
		// Give native road detours time to work. Four retries and a 30 s check period
		// still bound all searches, including vehicles moving without closing for 90 s.
		bool detour = m_iExecutorClock - force.m_iTacticalLastAdvance >= m_iQRFDetourTimeout;
		if (moved >= 15 && !detour)
			return;
		force.m_iTacticalRouteRetries++;
		if (force.RestoreNearbyDriver_S())
			Print(string.Format("[AROR TACTICAL] %1 package=%2 nearby AI crew resumed driver seat for town ingress", force.m_sOperationId, force.m_iPackageIndex + 1), LogLevel.NORMAL);
		vector next;
		float preferred = m_fQRFObjectiveDeliveryRadius * 0.6;
		if (!FindTacticalApproach(force, commander, position, preferred, next, force.m_iTacticalRouteRetries * 3, detour))
			return;
		if (!ReplaceTacticalWaypoint(force, true, m_sMoveWaypointPrefab, next))
			return;
		force.m_vApproachPosition = next;
		force.m_fTacticalBestDistance = remaining;
		force.m_iTacticalLastAdvance = m_iExecutorClock;
		Print(string.Format("[AROR TACTICAL] %1 package=%2 route retry=%3 detour=%4 remaining=%5 moved=%6; town road point=%7", force.m_sOperationId, force.m_iPackageIndex + 1, force.m_iTacticalRouteRetries, detour, remaining, moved, next), LogLevel.WARNING);
	}

	//! Native waypoint completion alone never authorises a remote hillside drop.
	protected bool IsQRFDeliveryReached(float roadDistance, float townDistance, float vicinityLimit, float plannedRoadRadius = 0)
	{
		float limit = Math.Min(m_fQRFObjectiveDeliveryRadius, vicinityLimit);
		if (plannedRoadRadius > limit)
			limit = Math.Min(plannedRoadRadius, m_fQRFMaximumRoadDismount);
		return roadDistance <= m_fQRFVehicleArrivalTolerance && townDistance <= limit;
	}

	//! Returns true when the commander owns this ground package's active orders.
	protected bool UpdateTacticalGround(AROR_GroundOperationForce force, AROR_Operation operation, int infantry, int crew)
	{
		AROR_QRFTacticalCommander commander = operation.m_TacticalCommander;
		if (!commander || !force.m_bTacticalControlled)
			return false;
		if (force.m_Group.GetPlayerCount() > 0)
			return true;
		if (force.m_CrewGroup && force.m_CrewGroup.GetPlayerCount() > 0)
			return true;
		int strength = infantry + crew;
		if ((force.m_Profile.m_eType == AROR_EForceType.ARMOUR && !force.m_Profile.m_bFillPassengerSeats))
			strength = crew;
		RecordTacticalStrength(force, operation, strength);
		bool retreat = commander.m_bWithdraw || force.m_bTacticalWithdrawing;
		if (commander.m_iTownHoldSeconds <= 0 && strength <= force.m_iTacticalInitialStrength * m_fQRFLocalWithdrawFraction)
			retreat = true;
		if (retreat)
			return TacticalWithdrawal(force, operation);
		if (force.m_bTacticalStaging)
		{
			if (force.m_Vehicle)
			{
				Physics stagingPhysics = force.m_Vehicle.GetPhysics();
				force.m_bTacticalStaged = stagingPhysics && stagingPhysics.GetVelocity().Length() < 3 && vector.DistanceXZ(force.m_Vehicle.GetOrigin(), force.m_vStagingPosition) <= 35;
			}
			AROR_LocationManagerComponent stagingLocations = AROR_LocationManagerComponent.GetInstance();
			AROR_LocationComponent stagingTarget;
			if (stagingLocations)
				stagingTarget = stagingLocations.GetLocationById(operation.GetTargetLocationId());
			UpdateAssaultRelease(operation, commander, stagingTarget);
			if (!commander.m_bAssaultReleased)
				return true;
			ReleaseFromStaging(force, commander);
		}
		if ((force.m_Profile.m_eType == AROR_EForceType.ARMOUR && !force.m_Profile.m_bFillPassengerSeats) && force.m_Vehicle && crew > 0)
		{
			if (vector.DistanceXZ(force.m_Vehicle.GetOrigin(), commander.m_vTown) <= m_fQRFObjectiveDeliveryRadius && commander.BeginTownHold(m_iExecutorClock))
				Print(string.Format("[AROR TACTICAL] %1 armour entered town; fixed security hold starts now for %2 seconds", operation.GetId(), commander.m_iTownHoldSeconds), LogLevel.NORMAL);
		}
		if (!force.m_bDismounted && force.HasDismountedInfantry())
		{
			TacticalInfantryOrders(force, operation);
		}
		if (force.m_bDismounted && force.m_Vehicle && crew > 0)
		{
			DamageManagerComponent hull = DamageManagerComponent.Cast(force.m_Vehicle.FindComponent(DamageManagerComponent));
			if ((!hull || !hull.IsDestroyed()) && vector.DistanceXZ(force.m_Vehicle.GetOrigin(), commander.m_vTown) > m_fQRFObjectiveDeliveryRadius)
				RecoverStalledQRFRoute(force, commander);
		}
		if ((force.m_Profile.m_eType != AROR_EForceType.ARMOUR || force.m_Profile.m_bFillPassengerSeats) && !force.m_bDismounted)
		{
			vector vehiclePosition = force.m_Vehicle.GetOrigin();
			float distance = vector.DistanceXZ(vehiclePosition, force.m_vApproachPosition);
			Physics physics = force.m_Vehicle.GetPhysics();
			float speed = -1;
			if (physics)
				speed = physics.GetVelocity().Length();
			bool settled = speed >= 0 && speed < 3;
			if (m_iExecutorClock >= force.m_iTacticalNextTravelLog)
			{
				force.m_iTacticalNextTravelLog = m_iExecutorClock + 15;
				Print(string.Format("[AROR TACTICAL] %1 package=%2 travel position=%3 goal=%4 distance=%5 speed=%6 dropStarted=%7", operation.GetId(), force.m_iPackageIndex + 1, vehiclePosition, force.m_vApproachPosition, distance, speed, force.m_bTacticalDropStarted), LogLevel.NORMAL);
			}
			float townDistance = vector.DistanceXZ(vehiclePosition, commander.m_vTown);
			bool arrived = IsQRFDeliveryReached(distance, townDistance, commander.m_fTownRadius + commander.m_fMargin, force.m_fTacticalDeliveryRadius);
			if (!arrived && !force.m_bTacticalDropStarted)
				RecoverStalledQRFRoute(force, commander);
			if (arrived && settled && !force.m_bTacticalDropStarted)
			{
				// Hold the driver at the actual road stop before requesting animated exits.
				if (ReplaceTacticalWaypoint(force, true, m_sQRFWaypointPrefab, vehiclePosition))
				{
					force.m_bTacticalDropStarted = true;
					force.m_iTacticalExitStarted = m_iExecutorClock;
					force.m_iTacticalExitRetries = 0;
					force.m_iTacticalExitDeadline = m_iExecutorClock + m_iQRFExitTimeout;
					Print(string.Format("[AROR TACTICAL] %1 package=%2 road stop accepted at distance=%3; holding vehicle and unloading infantry", operation.GetId(), force.m_iPackageIndex + 1, distance), LogLevel.NORMAL);
				}
			}
			// An accepted native exit request is not proof of completion. Retry remaining
			// seated passengers; RequestDismount_S skips passengers already getting out.
			if (force.m_bTacticalDropStarted && !force.ArePassengersOut())
			{
				bool recover = m_iQRFExitRecoveryDelay > 0 && m_iExecutorClock - force.m_iTacticalExitStarted >= m_iQRFExitRecoveryDelay;
				force.RequestDismount_S(recover);
			}
			if (force.m_bDismountRequested && force.ArePassengersOut())
			{
				force.m_bDismounted = true;
				force.m_iTacticalNextOrder = 0;
				Print(string.Format("[AROR TACTICAL] %1 package=%2 delivered infantry; separate infantry search and vehicle support", operation.GetId(), force.m_iPackageIndex + 1), LogLevel.NORMAL);
			}
			else
			{
				bool timedOut = !force.m_bTacticalDropStarted && m_iExecutorClock >= force.m_iTacticalLegDeadline;
				if (force.m_bTacticalDropStarted && m_iExecutorClock >= force.m_iTacticalExitDeadline)
					timedOut = true;
				if (timedOut)
				{
					if (force.m_bTacticalDropStarted && commander.m_iTownHoldSeconds > 0 && force.m_iTacticalExitRetries < m_iQRFExitRetryLimit)
					{
						force.m_iTacticalExitRetries++;
						force.m_iTacticalExitDeadline = m_iExecutorClock + m_iQRFExitTimeout;
						Print(string.Format("[AROR TACTICAL] %1 package=%2 holding town delivery point and retrying remaining exits; deployed infantry continue investigation", operation.GetId(), force.m_iPackageIndex + 1), LogLevel.WARNING);
						return true;
					}
					Print(string.Format("[AROR TACTICAL] %1 package=%2 ingress/exit deadline: position=%3 distance=%4 speed=%5 dropStarted=%6; withdrawing", operation.GetId(), force.m_iPackageIndex + 1, vehiclePosition, distance, speed, force.m_bTacticalDropStarted), LogLevel.WARNING);
					return TacticalWithdrawal(force, operation);
				}
				return true;
			}
		}
		if ((force.m_Profile.m_eType != AROR_EForceType.ARMOUR || force.m_Profile.m_bFillPassengerSeats))
			TacticalInfantryOrders(force, operation);
		if (m_iExecutorClock < force.m_iTacticalNextSupport)
			return true;
		// Motorized transport waits at the drop point. Protected vehicles cover a road sector.
		if (force.m_Profile.m_eType == AROR_EForceType.MOTORIZED)
			return true;
		if (crew < 1 || !force.m_Vehicle)
			return true;
		DamageManagerComponent damage = DamageManagerComponent.Cast(force.m_Vehicle.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return true;
		vector desired = commander.SupportPoint(force.m_iPackageIndex);
		vector support;
		if (TacticalRoadPoint(force.m_Vehicle.GetOrigin(), desired, support))
		{
			bool inside = vector.DistanceXZ(support, commander.m_vTown) <= Math.Min(m_fQRFObjectiveDeliveryRadius, commander.m_fTownRadius + commander.m_fMargin);
			if (inside && ReplaceTacticalWaypoint(force, true, m_sQRFWaypointPrefab, support))
				Print(string.Format("[AROR TACTICAL] %1 protected vehicle package=%2 covering separate road sector=%3", operation.GetId(), force.m_iPackageIndex + 1, support), LogLevel.NORMAL);
		}
		force.m_iTacticalNextSupport = m_iExecutorClock + 45;
		return true;
	}
}
