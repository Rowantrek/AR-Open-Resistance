[ComponentEditorProps(category: "OpenResistance/Operations", description: "Server force-pool executor for infantry, motorized, mechanized and armoured operations. Airborne profiles await a flight adapter.")]
class AROR_OperationExecutorComponentClass : SCR_BaseGameModeComponentClass
{
}

class AROR_OperationExecutorComponent : SCR_BaseGameModeComponent
{
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
	[Attribute("20", desc: "Stopped vehicle arrival tolerance around the selected QRF delivery road point, metres", params: "10 40 1")]
	protected float m_fQRFVehicleArrivalTolerance;
	[Attribute("100", desc: "Maximum distance from town objective for ground QRF delivery planning and physical unloading, metres", params: "50 150 5")]
	protected float m_fQRFObjectiveDeliveryRadius;
	[Attribute("600", desc: "Maximum seconds for ground QRF ingress, including bounded stalled-route recovery", params: "120 600 1")]
	protected int m_iQRFIngressTimeout;
	[Attribute("30", desc: "Maximum seconds for animated passenger exits after stopping for a tactical drop", params: "15 90 1")]
	protected int m_iQRFExitTimeout;
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
	protected bool m_bEnabled;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Occupier infantry group prefab; its faction and every member must match the commander", params: "et")]
	protected ResourceName m_sOccupiersGroupPrefab;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Invader infantry group prefab; its faction and every member must match the commander", params: "et")]
	protected ResourceName m_sInvadersGroupPrefab;
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
	[Attribute("50", desc: "Waypoint completion radius and arrival reporting distance in metres")]
	protected float m_fArrivalRadius;
	[Attribute("5", desc: "Seconds between wake requests if the engine evicts an active operation group")]
	protected int m_iWakeRetryInterval;
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
	protected ref AROR_QRFReadinessProvider m_QRFReadinessProvider;

	//! Registration does not enable Invader dispatch or manufacture stock.
	void RegisterQRFReadinessProvider_S(AROR_QRFReadinessProvider provider)
	{
		if (Replication.IsServer())
			m_QRFReadinessProvider = provider;
	}

	//! Read-only planning seam for future territory/strategy integration. Never spawns.
	AROR_QRFSelectionDecision PreviewInvaderQRF_S(string targetId, AROR_QRFSelectionContext context)
	{
		AROR_QRFSelectionDecision rejected = new AROR_QRFSelectionDecision();
		rejected.m_sReason = "Server campaign/executor not ready for planning";
		if (!Replication.IsServer() || !m_bEnabled || !m_Operations)
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
		rejected.m_sReason = "No military readiness provider registered; no resources assumed";
		if (!m_QRFReadinessProvider)
			return rejected;
		int now = m_Operations.GetClockSeconds();
		AROR_QRFReadinessSnapshot snapshot = m_QRFReadinessProvider.CaptureReadiness_S(AROR_EFactionRole.INVADERS, commander.GetFactionKey(), targetId, now);
		rejected.m_sReason = "Readiness provider returned no usable snapshot";
		if (!snapshot || !snapshot.IsValidAt(now))
			return rejected;
		AROR_LocationComponent origin = locations.GetLocationById(snapshot.m_sOriginLocationId);
		rejected.m_sReason = "Resource origin is not an eligible friendly outpost";
		if (!origin || origin == target || !AROR_QRFOriginPolicy.IsOriginType(origin.GetLocationType()))
			return rejected;
		if (origin.GetControllingRole() != AROR_EFactionRole.INVADERS || !IsLaunchPositionSafe(origin.GetCenter(), target.GetCenter()))
			return rejected;
		AROR_QRFReadinessSnapshot limited = snapshot.WithRuntimeCapacity(m_iMaxGroups - GetReservedGroups(), m_iMaxMembers - GetReservedMembers(), m_iMaxVehicles - GetReservedVehicles());
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
			if (!ground || !ground.m_Profile || force.m_bReservation || !operation || operation.GetType() != AROR_EOperationType.QRF)
				continue;
			Faction campaignFaction = roles.GetFaction(operation.GetRole());
			if (!campaignFaction || !resistance.IsFactionEnemy(campaignFaction))
				continue;
			if (ground.m_Vehicle)
			{
				vehicles.Insert(ground.m_Vehicle);
				kinds.Insert(ground.m_Profile.m_eType);
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
		}
		AROR_ReaperAirForce pendingAir = AROR_ReaperAirForce.Cast(force);
		if (pendingAir && pendingAir.m_bConstructionPending && pendingAir.m_Funding && pendingAir.m_Profile)
		{
			pendingAir.m_Funding.m_iQuote = Math.Max(0, pendingAir.m_Funding.m_iQuote - pendingAir.m_Profile.ReplacementQuote());
			pendingAir.m_bConstructionPending = false;
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
			if (packageForce.m_bTacticalControlled && packageForce.m_bOrdersIssued && !AROR_ReaperAirForce.Cast(packageForce))
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
		int packages;
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (force.m_sOperationId != operation.GetId())
				continue;
			if (force.m_bTacticalRetained)
				continue;
			AROR_QRFDeferredForce deferred = AROR_QRFDeferredForce.Cast(force);
			if (tactical && deferred && tactical.IsReserve(force.m_iPackageIndex))
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
			if (ready.m_sOperationId != operation.GetId() || ready.m_bOrdersIssued || ready.m_bReservation || ready.m_bTacticalRetained)
				continue;
			AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(ready);
			if (ground)
			{
				if (ground.m_Profile.m_eType == AROR_EForceType.ARMOUR)
					ground.m_CrewGroup.AddWaypoint(ground.m_Waypoint);
				else
					ground.m_CrewGroup.AddWaypoint(ground.m_CrewWaypoint);
			}
			else
				ready.m_Group.AddWaypoint(ready.m_Waypoint);
			ready.m_bOrdersIssued = true;
			ready.m_iTacticalLegDeadline = m_iExecutorClock + m_iQRFIngressTimeout;
		}
		if (operation.GetState() == AROR_EOperationState.SPAWNING)
		{
			m_Operations.MarkActive_S(operation.GetId());
			if (tactical)
				tactical.MarkDispatched(m_iExecutorClock);
		}
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
		if (force.m_bTacticalControlled && force.m_bOrdersIssued && !AROR_ReaperAirForce.Cast(force))
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
				ReleaseForce(partial);
			operation.m_TacticalCommander.MarkLost(m_iQRFConstructionIndex);
			Print(string.Format("[AROR TACTICAL] %1 reserve construction failed: %2; existing force continues", operation.GetId(), reason), LogLevel.WARNING);
			return;
		}
		m_Operations.ResolveOperation_S(operation.GetId(), AROR_EOperationState.FAILED, reason);
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
		reservation.m_iReservedGroups -= profile.GetGroupBudget();
		reservation.m_iReservedMembers -= profile.m_iPersonnelBudget;
		if (profile.m_eType != AROR_EForceType.INFANTRY)
			reservation.m_iReservedVehicles--;
	}

	protected AROR_ForceProfile SelectProfile(AROR_Operation operation, AROR_Commander commander)
	{
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
		if (!target || operation.GetType() != AROR_EOperationType.QRF)
			return fallback;
		AROR_LocationManagerComponent manager = AROR_LocationManagerComponent.GetInstance();
		if (!manager)
			return fallback;
		array<AROR_LocationComponent> outposts = {};
		manager.GetLocations(outposts);
		AROR_LocationComponent nearest;
		float distance = float.MAX;
		foreach (AROR_LocationComponent outpost : outposts)
		{
			if (!outpost || !AROR_QRFOriginPolicy.IsOriginType(outpost.GetLocationType()) || outpost == target || outpost.GetControllingRole() != operation.GetRole())
				continue;
			if (!IsLaunchPositionSafe(outpost.GetCenter(), target.GetCenter()))
				continue;
			float candidateDistance = vector.DistanceXZ(outpost.GetCenter(), target.GetCenter());
			if (candidateDistance < distance)
			{
				nearest = outpost;
				distance = candidateDistance;
			}
		}
		if (nearest)
		{
			if (logOrigin)
				Print(string.Format("[AROR] QRF %1 origin outpost=%2 distance=%3m", operation.GetId(), nearest.GetId(), distance), LogLevel.NORMAL);
			return nearest.GetCenter();
		}
		if (logOrigin)
			Print(string.Format("[AROR] QRF %1 has no eligible friendly outpost; using configured role origin", operation.GetId()), LogLevel.WARNING);
		return fallback;
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
			return;
		AROR_GroundQRFRecipe recipe;
		if (m_Operations.IsQRFStrategyEnabled())
			recipe = SelectStrategicGroundRecipe(operation, commander);
		foreach (AROR_GroundQRFRecipe candidate : m_aGroundQRFRecipes)
		{
			if (!m_Operations.IsQRFStrategyEnabled() && candidate && candidate.m_sId == GetGroundQRFRecipeId(operation.GetRole()) && candidate.m_eRole == operation.GetRole())
				recipe = candidate;
		}
		if (!recipe && m_Operations.IsQRFStrategyEnabled())
		{
			ReportSpawnWait(operation, "No affordable, capacity-eligible ground recipe at or below the bound desired tier");
			return;
		}
		array<AROR_ForceProfile> elements = {};
		if (!recipe || !recipe.Resolve(m_aForceProfiles, operation.GetRole(), commander.GetFactionKey(), elements))
		{
			FailOperation(operation, "Ground QRF recipe missing, disabled, unsupported or faction-incompatible");
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
		if (operation.GetType() == AROR_EOperationType.QRF && (m_Operations.IsQRFStrategyEnabled() || !GetGroundQRFRecipeId(operation.GetRole()).IsEmpty()))
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
		if (m_bQRFTacticalCommander && operation.GetType() == AROR_EOperationType.QRF)
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
		if (operation.GetType() == AROR_EOperationType.QRF)
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
		if (!target || !IsLaunchPositionSafe(launchPosition, target.GetCenter()))
		{
			FailOperation(operation, "Ground-force target or launch position invalid");
			return;
		}
		if (operation.m_TacticalCommander)
		{
			vector roadLaunch;
			if (!FindGroundRoadLaunch(operation.m_TacticalCommander, packageIndex, roadLaunch) || !IsLaunchPositionSafe(roadLaunch, target.GetCenter()))
			{
				FailOperation(operation, "No clear separated road launch near the selected QRF origin");
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
		if (!force.m_CrewGroup.m_aUnitPrefabSlots)
			force.m_CrewGroup.m_aUnitPrefabSlots = {};
		force.m_CrewGroup.m_aUnitPrefabSlots.Clear();
		for (int c = 0; c < profile.m_iCrewCount; c++)
			force.m_CrewGroup.m_aUnitPrefabSlots.Insert(profile.m_sCrewCharacter);
		if (profile.m_eType == AROR_EForceType.ARMOUR)
			force.m_Group = force.m_CrewGroup;
		else
		{
			vector infantryPosition = launchPosition;
			infantryPosition[0] = infantryPosition[0] - 8;
			force.m_Group = SpawnEmptyGroup(profile.m_sInfantryGroup, infantryPosition);
			if (force.m_Group)
				force.m_Group.GetOnAgentAdded().Insert(force.OnAgentAdded);
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
		if (operation.GetType() == AROR_EOperationType.QRF)
			objectiveWaypoint = m_sQRFWaypointPrefab;
		force.m_Waypoint = SpawnWaypoint(objectiveWaypoint, target.GetCenter());
		vector approach = target.GetCenter();
		if (profile.m_eType != AROR_EForceType.ARMOUR)
		{
			float distance = vector.DistanceXZ(launchPosition, target.GetCenter());
			if (distance <= profile.m_fDismountDistance + 20)
			{
				FailOperation(operation, "Launch position is too close for profile dismount distance");
				return;
			}
			approach[0] = approach[0] + (launchPosition[0] - approach[0]) / distance * profile.m_fDismountDistance;
			approach[2] = approach[2] + (launchPosition[2] - approach[2]) / distance * profile.m_fDismountDistance;
			if (operation.m_TacticalCommander)
			{
				float standoff = profile.m_fDismountDistance;
				if (operation.m_TacticalCommander.m_bHasContact)
					standoff = Math.Max(standoff, m_fQRFThreatDismount);
				// Profile/contact stand-off must not turn a town response into a hillside drop.
				standoff = Math.Min(standoff, m_fQRFObjectiveDeliveryRadius * 0.6);
				standoff = Math.Min(standoff, distance - 40);
				if (!FindTacticalApproach(force, operation.m_TacticalCommander, launchPosition, standoff, approach))
				{
					FailOperation(operation, "Bounded tactical approach search exhausted; no reachable separated road point");
					return;
				}
			}
			force.m_CrewWaypoint = SpawnWaypoint(m_sMoveWaypointPrefab, approach);
		}
		force.m_vApproachPosition = approach;
		if (!force.m_Waypoint || (profile.m_eType != AROR_EForceType.ARMOUR && !force.m_CrewWaypoint))
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

		ResourceName groupPrefab = m_sOccupiersGroupPrefab;
		vector launchPosition = m_vOccupiersSpawnPosition;
		if (operation.GetRole() == AROR_EFactionRole.INVADERS)
		{
			groupPrefab = m_sInvadersGroupPrefab;
			launchPosition = m_vInvadersSpawnPosition;
		}
		if (profile)
			groupPrefab = profile.m_sInfantryGroup;
		if (operation.m_TacticalCommander)
			launchPosition = TacticalLaunchPoint(operation.m_TacticalCommander, packageIndex);
		else
		{
			launchPosition = ResolveQRFOrigin(operation, target, launchPosition, packageIndex == 0);
			launchPosition[0] = launchPosition[0] + packageIndex * m_fPackageSpacing;
		}
		ResourceName waypointPrefab = m_sAttackWaypointPrefab;
		if (operation.GetType() == AROR_EOperationType.QRF)
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
		if (group.m_aUnitPrefabSlots)
			force.m_iReservedMembers = group.m_aUnitPrefabSlots.Count();
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
		if (operation.GetType() == AROR_EOperationType.QRF)
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
		if (!force.m_Group || (!force.m_CrewGroup && !force.m_bDismounted))
		{
			FailPackage(force, operation, "Ground-force group deleted before delivery");
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
				return;
			}
			force.m_bMounted = true;
			force.m_bReady = true;
			int initialStrength = infantry + crew;
			if (force.m_Profile.m_eType == AROR_EForceType.ARMOUR)
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
		if (force.m_Profile.m_eType != AROR_EForceType.ARMOUR && !force.m_bDismounted)
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
					if (operation.GetType() == AROR_EOperationType.QRF)
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
		if (!m_bQRFVehicleSearch || operation.GetType() != AROR_EOperationType.QRF || force.m_bDismounted || force.m_bTownSearchFinished)
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
			if (force.m_Profile.m_eType == AROR_EForceType.ARMOUR)
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
		if (force.m_Profile.m_eType == AROR_EForceType.ARMOUR)
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
		if (!m_bEnabled)
		{
			Print("[AROR] Operation executor disabled: enable m_bEnabled on the Chicken game-mode instance", LogLevel.WARNING);
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
		if (m_iMaxGroups < 1 || m_iMaxMembers < 1 || m_iMaxVehicles < 0 || m_iWakeRetryInterval < 1 || m_fPackageSpacing < 10 || m_iWorkbenchResponseLevel < 0 || m_iWorkbenchResponseLevel > 5 || m_fMinTargetDistance < 0 || m_fMinPlayerDistance < 0 || m_fArrivalRadius <= 0 || owner.FindComponent(AROR_OperationExecutorComponent) != this || !ValidatePool())
		{
			Print("[AROR] Invalid or duplicate operation executor; disabled", LogLevel.ERROR);
			return;
		}
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);
		Print("[AROR] Operation force executor enabled", LogLevel.NORMAL);
		if (m_bQRFVehicleSearch)
			Print("[AROR] QRF search policy: coordinated contact investigation and sector patrol", LogLevel.NORMAL);
		Print(string.Format("[AROR] Executor configuration: profiles=%1 levels=%2 budgets groups=%3 members=%4 vehicles=%5", m_aForceProfiles.Count(), m_aResponseLevels.Count(), m_iMaxGroups, m_iMaxMembers, m_iMaxVehicles), LogLevel.NORMAL);
	}

	protected bool ValidatePool()
	{
		if (m_aGroundQRFRecipes.Count() > 32)
			return false;
		array<string> recipeKeys = {};
		foreach (AROR_GroundQRFRecipe recipe : m_aGroundQRFRecipes)
		{
			if (!recipe || !recipe.IsValid())
				return false;
			string recipeKey = string.Format("%1:%2", recipe.m_sId, recipe.m_eRole);
			if (recipeKeys.Contains(recipeKey))
				return false;
			recipeKeys.Insert(recipeKey);
			array<AROR_ForceProfile> resolved = {};
			if (!recipe.Resolve(m_aForceProfiles, recipe.m_eRole, recipe.m_sFactionKey, resolved))
				return false;
		}
		if (m_aForceProfiles.Count() > 64)
			return false;
		if (m_aResponseLevels.Count() > 5)
			return false;
		array<int> levels = {};
		foreach (AROR_ResponseLevel response : m_aResponseLevels)
		{
			if (!response || !response.IsValid() || levels.Contains(response.m_iLevel))
				return false;
			levels.Insert(response.m_iLevel);
		}
		array<string> ids = {};
		foreach (AROR_ForceProfile profile : m_aForceProfiles)
		{
			if (!profile || !profile.IsValid() || ids.Contains(profile.m_sId))
			{
				Print("[AROR] Force profiles must be valid and have globally unique IDs", LogLevel.ERROR);
				return false;
			}
			ids.Insert(profile.m_sId);
			if (!profile.HasNativeExecutor())
				Print(string.Format("[AROR] Profile %1 excluded from native pool: airborne requires a separate flight executor", profile.m_sId), LogLevel.WARNING);
		}
#ifdef WORKBENCH
		if ((!m_sWorkbenchForceProfileId.IsEmpty() && !ids.Contains(m_sWorkbenchForceProfileId)) || (m_iWorkbenchResponseLevel > 0 && !levels.Contains(m_iWorkbenchResponseLevel)))
		{
			Print("[AROR] Workbench forced profile/level is absent from configuration", LogLevel.ERROR);
			return false;
		}
#endif
		return true;
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Tick);
		if (m_Operations)
			m_Operations.GetOnOperationChanged().Remove(OnOperationChanged);
		foreach (AROR_OperationForce force : m_aForces)
			force.Cleanup_S();
		m_aForces.Clear();
		super.OnDelete(owner);
	}

	// Shared native ground/air tactical policy. Backend-only actions stay in the REAPER layer.
	//! Choose once before binding: downgrade for funds/capacity, never silently exceed war intent.
	protected AROR_GroundQRFRecipe SelectStrategicGroundRecipe(AROR_Operation operation, AROR_Commander commander)
	{
		int desired = operation.m_iRequestedTier;
		if (desired < 1)
		{
			desired = m_Operations.GetWarLevel(operation.GetRole());
			operation.m_iRequestedTier = desired;
		}
		return FindStrategicGroundRecipe(operation.GetRole(), commander.GetFactionKey(), desired);
	}
	//! Advisory queue preflight; ReserveQRFFunding remains the atomic admission boundary.
	bool CanFundQRFThreat_S(AROR_EFactionRole role, int desired)
	{
		if (!Replication.IsServer() || !m_Operations)
			return false;
		if (!AROR_EconomyComponent.GetInstance() || m_Operations.GetQRFFinance().Debt(role) > 0)
			return false;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return false;
		return FindStrategicGroundRecipe(role, roles.GetFactionKey(role), desired) != null;
	}
	protected AROR_GroundQRFRecipe FindStrategicGroundRecipe(AROR_EFactionRole role, string faction, int desired)
	{
		AROR_GroundQRFRecipe best;
		int available = m_Operations.GetQRFFinance().Available(role);
		foreach (AROR_GroundQRFRecipe recipe : m_aGroundQRFRecipes)
		{
			if (!recipe || recipe.m_iLevel > desired || (best && recipe.m_iLevel <= best.m_iLevel))
				continue;
			array<AROR_ForceProfile> elements = {};
			if (!recipe.Resolve(m_aForceProfiles, role, faction, elements))
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
				if (profile.m_eType != AROR_EForceType.INFANTRY)
					vehicles++;
			}
			if (quote > available || groups + GetReservedGroups() > m_iMaxGroups)
				continue;
			if (personnel + GetReservedMembers() > m_iMaxMembers || vehicles + GetReservedVehicles() > m_iMaxVehicles)
				continue;
			best = recipe;
		}
		return best;
	}
	protected bool ReserveQRFFunding(AROR_Operation operation, array<AROR_ForceProfile> elements, int tier)
	{
		if (!m_Operations.IsQRFStrategyEnabled() || operation.GetType() != AROR_EOperationType.QRF)
			return true;
		if (operation.m_Funding)
			return true;
		int quote;
		foreach (AROR_ForceProfile profile : elements)
			quote += profile.ReplacementQuote();
		operation.m_Funding = m_Operations.GetQRFFinance().Reserve_S(operation, quote, m_Operations.QRFOverhead(tier));
		if (!operation.m_Funding)
			ReportSpawnWait(operation, "Faction funds cannot cover full response collateral and deployment overhead");
		return operation.m_Funding != null;
	}
	protected void ConfigureForceFunding(AROR_OperationForce force, AROR_Operation operation, AROR_ForceProfile profile)
	{
		force.m_Funding = operation.m_Funding;
		if (!profile)
			return;
		force.m_iPersonnelLossValue = profile.m_iPersonnelLossValue;
		force.m_iVehicleLossValue = profile.VehicleLossValue();
	}

	protected bool TacticalSettingsValid()
	{
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
		if (m_fQRFVehicleArrivalTolerance < 10 || m_fQRFVehicleArrivalTolerance > 40)
			return false;
		if (m_fQRFObjectiveDeliveryRadius < 50 || m_fQRFObjectiveDeliveryRadius > 150)
			return false;
		if (m_iQRFIngressTimeout < 120 || m_iQRFIngressTimeout > 600)
			return false;
		if (m_iQRFExitTimeout < 15 || m_iQRFExitTimeout > 90)
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
		if (!m_bQRFTacticalCommander || operation.GetType() != AROR_EOperationType.QRF || operation.m_TacticalCommander)
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
		commander.m_vTown = target.GetCenter();
		commander.m_fTownRadius = target.GetRadius();
		commander.m_fMargin = m_fQRFSearchMargin;
		commander.m_fThreatStandoff = m_fQRFThreatDismount;
		commander.m_vOrigin = origin;
		array<AROR_LocationComponent> outposts = {};
		locations.GetLocations(outposts);
		foreach (AROR_LocationComponent outpost : outposts)
		{
			if (outpost && AROR_QRFOriginPolicy.IsOriginType(outpost.GetLocationType()) && outpost.GetControllingRole() == operation.GetRole() && vector.DistanceXZ(origin, outpost.GetCenter()) < 1)
				commander.m_sOriginLocationId = outpost.GetId();
		}
		operation.m_TacticalCommander = commander;
		Print(string.Format("[AROR TACTICAL] %1 role=%2 tier=%3 first wave=%4 reserve=%5; entire force reserved, one construction per tick", operation.GetId(), operation.GetRole(), operation.GetResponseLevel(), commander.m_iFirstWaveCount, packages - commander.m_iFirstWaveCount), LogLevel.NORMAL);
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
		if (m_iLastQRFConstructionTick == m_iExecutorClock)
			return;
		if (!commander.CanConstruct(pending.m_iPackageIndex, m_iExecutorClock, LiveGroundQRFCount(), m_iQRFMaxLiveGround, 1))
			return;
		AROR_CommanderManagerComponent strategic = AROR_CommanderManagerComponent.GetInstance();
		if (!strategic || !strategic.GetCommander(operation.GetRole()))
			return;
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
				if (!origin || origin.GetControllingRole() != operation.GetRole())
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

	protected bool TacticalEntitiesFar(AROR_OperationForce force)
	{
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
				if (member && vector.DistanceXZ(player.GetOrigin(), member.GetOrigin()) < m_fQRFGroundCleanupDistance)
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
		if (ground && !ground.m_bDismounted && ground.m_Vehicle && ground.m_Profile.m_eType != AROR_EForceType.ARMOUR)
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
		for (int attempt = 0; attempt < 12; attempt++)
		{
			float angle = attempt * 0.78539816;
			float radius = 30;
			if (attempt >= 8)
				radius = 60;
			vector desired = seed;
			if (attempt > 0)
				desired = seed + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius;
			vector road;
			if (!TacticalRoadPoint(commander.m_vOrigin, desired, road))
				continue;
			if (vector.DistanceXZ(road, commander.m_vOrigin) > 150)
				continue;
			bool separated = true;
			foreach (AROR_OperationForce other : m_aForces)
			{
				AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(other);
				if (ground && ground.m_Vehicle && vector.DistanceXZ(road, ground.m_Vehicle.GetOrigin()) < 25)
					separated = false;
			}
			if (!separated)
				continue;
			float height = world.GetSurfaceY(road[0], road[2]);
			if (height <= world.GetOceanHeight(road[0], road[2]) + 0.5)
				continue;
			bool level = true;
			for (int side = 0; side < 4; side++)
			{
				float bearing = side * 1.57079633;
				float sample = world.GetSurfaceY(road[0] + Math.Sin(bearing) * 6, road[2] + Math.Cos(bearing) * 6);
				if (Math.AbsFloat(sample - height) > 1.5)
					level = false;
			}
			if (!level)
				continue;
			TraceBox trace = new TraceBox();
			trace.Start = Vector(road[0], height + 0.35, road[2]);
			trace.End = trace.Start + Vector(0, 0.05, 0);
			trace.Mins = Vector(-6, 0, -6);
			trace.Maxs = Vector(6, 4.5, 6);
			trace.Flags = TraceFlags.ENTS;
			if (world.TraceMove(trace, null) < 1)
				continue;
			road[1] = height;
			point = road;
			Print(string.Format("[AROR TACTICAL] road launch package=%1 origin=%2 position=%3 queries=%4", index + 1, commander.m_sOriginLocationId, point, attempt + 1), LogLevel.NORMAL);
			return true;
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
	protected bool TacticalRoadPoint(vector origin, vector desired, out vector reachable)
	{
		ChimeraAIWorld world = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!world || !world.GetRoadNetworkManager())
			return false;
		// Configured origins commonly store Y=0; native road queries need terrain positions.
		origin[1] = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
		desired[1] = GetGame().GetWorld().GetSurfaceY(desired[0], desired[2]);
		return world.GetRoadNetworkManager().GetReachableWaypointInRoad(origin, desired, 75, reachable);
	}
	protected bool FindTacticalApproach(AROR_GroundOperationForce force, AROR_QRFTacticalCommander commander, vector origin, float standoff, out vector approach, int firstAttempt = 0)
	{
		vector fallback;
		float bestClearance = -1;
		float minimumSpacing = Math.Min(18, m_fQRFSearchStopSpacing);
		for (int attempt = 0; attempt < m_iQRFApproachAttempts; attempt++)
		{
			int candidate = (attempt + firstAttempt) % m_iQRFApproachAttempts;
			vector desired = commander.ApproachCandidate(force.m_iPackageIndex, origin, standoff, candidate);
			vector reachable;
			if (!TacticalRoadPoint(origin, desired, reachable))
				continue;
			float townDistance = vector.DistanceXZ(reachable, commander.m_vTown);
			if (townDistance > Math.Min(m_fQRFObjectiveDeliveryRadius, commander.m_fTownRadius + commander.m_fMargin))
				continue;
			if (townDistance < 15)
				continue;
			if (vector.DistanceXZ(reachable, desired) > 75 || vector.DistanceXZ(reachable, origin) < 40)
				continue;
			float clearance = float.MAX;
			foreach (AROR_OperationForce tracked : m_aForces)
			{
				AROR_GroundOperationForce other = AROR_GroundOperationForce.Cast(tracked);
				if (!other || other == force || other.m_sOperationId != force.m_sOperationId)
					continue;
				clearance = Math.Min(clearance, vector.DistanceXZ(other.m_vApproachPosition, reachable));
			}
			if (clearance < m_fQRFSearchStopSpacing)
			{
				if (clearance >= minimumSpacing && clearance > bestClearance)
				{
					bestClearance = clearance;
					fallback = reachable;
				}
				continue;
			}
			approach = reachable;
			Print(string.Format("[AROR TACTICAL] %1 package=%2 delivery road point selected after %3 queries: %4 townDistance=%5 maximum=%6", force.m_sOperationId, force.m_iPackageIndex + 1, attempt + 1, approach, townDistance, m_fQRFObjectiveDeliveryRadius), LogLevel.NORMAL);
			return true;
		}
		if (bestClearance >= minimumSpacing)
		{
			approach = fallback;
			Print(string.Format("[AROR TACTICAL] %1 package=%2 using compact town delivery point %3 clearance=%4m after bounded search; preferred spacing unavailable", force.m_sOperationId, force.m_iPackageIndex + 1, approach, bestClearance), LogLevel.NORMAL);
			return true;
		}
		Print(string.Format("[AROR TACTICAL] %1 package=%2 approach search failed after %3 queries; origin=%4 town=%5 standoff=%6", force.m_sOperationId, force.m_iPackageIndex + 1, m_iQRFApproachAttempts, origin, commander.m_vTown, standoff), LogLevel.WARNING);
		return false;
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
			if (unloadingComplete && !occupied && force.m_Profile.m_eType != AROR_EForceType.ARMOUR)
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

	//! Native driving can stop before completion; rotate bounded road candidates after no progress.
	protected void RecoverStalledQRFRoute(AROR_GroundOperationForce force, AROR_QRFTacticalCommander commander)
	{
		if (m_iExecutorClock < force.m_iTacticalNextRouteCheck || force.m_iTacticalRouteRetries >= 4)
			return;
		force.m_iTacticalNextRouteCheck = m_iExecutorClock + 30;
		vector position = force.m_Vehicle.GetOrigin();
		float moved = vector.DistanceXZ(position, force.m_vTacticalProgressPosition);
		force.m_vTacticalProgressPosition = position;
		if (moved >= 15)
			return;
		force.m_iTacticalRouteRetries++;
		if (force.RestoreNearbyDriver_S())
			Print(string.Format("[AROR TACTICAL] %1 package=%2 nearby AI crew resumed driver seat for town ingress", force.m_sOperationId, force.m_iPackageIndex + 1), LogLevel.NORMAL);
		vector next;
		float preferred = m_fQRFObjectiveDeliveryRadius * 0.6;
		if (!FindTacticalApproach(force, commander, position, preferred, next, force.m_iTacticalRouteRetries * 3))
			return;
		if (!ReplaceTacticalWaypoint(force, true, m_sMoveWaypointPrefab, next))
			return;
		force.m_vApproachPosition = next;
		Print(string.Format("[AROR TACTICAL] %1 package=%2 stalled route retry=%3; continuing to town road point %4", force.m_sOperationId, force.m_iPackageIndex + 1, force.m_iTacticalRouteRetries, next), LogLevel.WARNING);
	}

	//! Native waypoint completion alone never authorises a remote hillside drop.
	protected bool IsQRFDeliveryReached(float roadDistance, float townDistance, float vicinityLimit)
	{
		return roadDistance <= m_fQRFVehicleArrivalTolerance && townDistance <= Math.Min(m_fQRFObjectiveDeliveryRadius, vicinityLimit);
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
		if (force.m_Profile.m_eType == AROR_EForceType.ARMOUR)
			strength = crew;
		RecordTacticalStrength(force, operation, strength);
		bool retreat = commander.m_bWithdraw || force.m_bTacticalWithdrawing;
		if (commander.m_iTownHoldSeconds <= 0 && strength <= force.m_iTacticalInitialStrength * m_fQRFLocalWithdrawFraction)
			retreat = true;
		if (retreat)
			return TacticalWithdrawal(force, operation);
		if (force.m_Profile.m_eType == AROR_EForceType.ARMOUR && force.m_Vehicle && crew > 0)
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
		if (force.m_Profile.m_eType != AROR_EForceType.ARMOUR && !force.m_bDismounted)
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
			bool arrived = IsQRFDeliveryReached(distance, townDistance, commander.m_fTownRadius + commander.m_fMargin);
			if (!arrived && !force.m_bTacticalDropStarted)
				RecoverStalledQRFRoute(force, commander);
			if (arrived && settled && !force.m_bTacticalDropStarted)
			{
				// Hold the driver at the actual road stop before requesting animated exits.
				if (ReplaceTacticalWaypoint(force, true, m_sQRFWaypointPrefab, vehiclePosition))
				{
					force.m_bTacticalDropStarted = true;
					force.m_iTacticalExitStarted = m_iExecutorClock;
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
					if (force.m_bTacticalDropStarted && commander.m_iTownHoldSeconds > 0)
					{
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
		if (force.m_Profile.m_eType != AROR_EForceType.ARMOUR)
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
