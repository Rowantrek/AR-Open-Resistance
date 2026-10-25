//! Experimental REAPER dispatch: existing Chicken defender reports drive the normal scheduler.
//! This development branch loads REAPER through the normal project dependency list.
modded class AROR_OperationExecutorComponent
{
	[Attribute("0", desc: "EXPERIMENTAL: replace new QRF dispatches with REAPER transport/fireteam/gunship packages. Workbench response level sets package count; defaults to one otherwise. Disable to restore native ground QRFs. Requires the REAPER project dependency.")]
	protected bool m_bReaperQRFTest;
	[Attribute("0 0 0", UIWidgets.Coords, desc: "Centre of the bounded LZ search relative to the attacked town marker")]
	protected vector m_vReaperLZOffset;
	[Attribute("180", desc: "Maximum seconds per flight/unload stage", params: "30 600 1")]
	protected int m_iReaperFlightTimeout;
	[Attribute("120", desc: "Initial occupied aircraft height above terrain", params: "60 300 1")]
	protected float m_fReaperInsertionHeight;
	[Attribute("160", desc: "Transport ingress/landing waypoint maximum speed, km/h; REAPER controls final braking", params: "100 200 1")]
	protected float m_fReaperTransportSpeed;
	[Attribute("60", desc: "Transport ingress/landing commanded height above terrain; backend controls touchdown", params: "40 120 1")]
	protected float m_fReaperTransportHeight;
	[Attribute("700", desc: "Waiting transports hold this far behind the LZ instead of hovering over town, metres", params: "400 1500 1")]
	protected float m_fReaperHoldingDistance;
	[Attribute("2", desc: "Consecutive stable ground-contact ticks before starting animated passenger exits", params: "1 5 1")]
	protected int m_iReaperTouchdownTicks;
	[Attribute("180", desc: "Transport departure waypoint maximum speed, km/h", params: "120 220 1")]
	protected float m_fReaperDepartureSpeed;
	[Attribute("400", desc: "Minimum town LZ search radius; expands to town radius plus 150 m (capped at 800 m)", params: "60 800 1")]
	protected float m_fReaperLZSearchRadius;
	[Attribute("90", desc: "Minimum separation between simultaneous landing pads/corridors, metres", params: "70 180 1")]
	protected float m_fReaperLandingSeparation;
	[Attribute("18", desc: "Clear rotor-pad half width and vertical landing corridor, metres", params: "14 30 1")]
	protected float m_fReaperPadClearance;
	[Attribute("75", desc: "Escort offset to the side and behind transport during ingress, metres", params: "60 150 1")]
	protected float m_fReaperFormationSpacing;
	[Attribute("1500", desc: "Departing transport AND retained forces must be this far from EVERY player before removal", params: "800 3000 1")]
	protected float m_fReaperCleanupDistance;
	[Attribute("600", desc: "Minimum terminal-force/wreck retention, seconds; player-distance check still applies afterward", params: "120 1800 1")]
	protected int m_iReaperRetainSeconds;
	[Attribute("300", desc: "Gunship support time before egress, seconds", params: "60 600 1")]
	protected int m_iReaperSupportSeconds;
	[Attribute("100", desc: "Suppress REAPER explosive attacks near non-hostile personnel/vehicles, metres", params: "75 200 1")]
	protected float m_fReaperFriendlyClearance;
	[Attribute("2000", desc: "Maximum distance from the defended town for responding to allied combat reports", params: "500 5000 1")]
	protected float m_fReaperSupportRadius;
	[Attribute("90", desc: "Seconds before unrefreshed gunship contact reports expire", params: "15 180 1")]
	protected int m_iReaperContactAge;
	[Attribute("210", desc: "Gunship commanded altitude above terrain, separate from transport routes", params: "180 300 1")]
	protected float m_fReaperGunshipHeight;
	[Attribute("35", desc: "Gunship altitude-band spacing per package, metres", params: "30 60 1")]
	protected float m_fReaperGunshipBand;
	[Attribute("100", desc: "Predicted aircraft separation that triggers gunship yielding, metres", params: "60 150 1")]
	protected float m_fReaperTrafficClearance;

	protected float ReaperGunshipHeight(AROR_ReaperAirForce force)
	{
		return m_fReaperGunshipHeight + force.m_iPackageIndex * m_fReaperGunshipBand;
	}

	protected ref AROR_ReaperLandingSearch m_ReaperLandingSearch;
	override bool CanFundQRFThreat_S(AROR_EFactionRole role, int desired)
	{
		if (!GetGroundQRFRecipeId(role).IsEmpty() || !m_bReaperQRFTest)
			return super.CanFundQRFThreat_S(role, desired);
		if (!Replication.IsServer() || !m_Operations)
			return false;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles || desired < 1 || desired > 5)
			return false;
		foreach (AROR_ForceProfile profile : m_aForceProfiles)
		{
			if (!profile || profile.m_eType != AROR_EForceType.AIRBORNE || profile.m_eRole != role)
				continue;
			if (!profile.IsValid() || profile.m_sFactionKey != roles.GetFactionKey(role))
				continue;
			return AffordableReaperTier(profile, desired) > 0;
		}
		return false;
	}
	protected int AffordableReaperTier(AROR_ForceProfile profile, int desired)
	{
		if (!AROR_EconomyComponent.GetInstance() || m_Operations.GetQRFFinance().Debt(profile.m_eRole) > 0)
			return 0;
		for (int tier = Math.Clamp(desired, 1, 5); tier >= 1; tier--)
		{
			int quote = profile.ReplacementQuote() * tier + m_Operations.QRFOverhead(tier);
			if (quote > m_Operations.GetQRFFinance().Available(profile.m_eRole))
				continue;
			if (GetReservedGroups() + 3 * tier > m_iMaxGroups || GetReservedVehicles() + 2 * tier > m_iMaxVehicles)
				continue;
			if (GetReservedMembers() + profile.m_iPersonnelBudget * tier > m_iMaxMembers)
				continue;
			return tier;
		}
		return 0;
	}
	protected string m_sReaperPlanningOperation;
	protected int m_iLastReaperAlternateSearchTick = -1;
	protected int m_iLastReaperConstructionTick = -1;
	protected ResourceName ReaperMovePrefab()
	{
		return "{F6FB686B76E77E7D}Prefabs/AI/Waypoints/REAPER_AiHelicopterMoveWaypoint.et";
	}
	protected ResourceName ReaperLandPrefab()
	{
		return "{6D8ADA4DF1A482C6}Prefabs/AI/Waypoints/REAPER_AiHelicopterLandWaypoint.et";
	}
	protected ResourceName ReaperHoverPrefab()
	{
		return "{471EDCB44D26C193}Prefabs/AI/Waypoints/REAPER_AiHelicopterHoverWaypoint.et";
	}

	protected bool ReaperOrder(AROR_GroundOperationForce aircraft, ResourceName prefab, vector point, float speed = 110, float height = 100)
	{
		return AROR_ReaperAirForce.SetFlightOrder_S(aircraft, prefab, point, speed, height);
	}

	protected SCR_AIGroup ReaperGroup(AROR_GroundOperationForce owner, ResourceName prefab, ResourceName pilot, vector point)
	{
		SCR_AIGroup group = SpawnEmptyGroup(prefab, point);
		if (!group)
			return null;
		// Track the group before any fallible initialization so terminal cleanup owns it.
		if (pilot.IsEmpty())
			owner.m_Group = group;
		else
			owner.m_CrewGroup = group;
		if (!owner.InitializeSpawnGroupFaction_S(group))
			return null;
		group.GetOnAgentAdded().Insert(owner.OnAgentAdded);
		group.SetCanDeleteIfNoPlayer(false);
		group.SetPrivate(true);
		group.SetPrivacyChangeable(false);
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		group.SetImportance(SCR_EAISpawnImportance.HIGH);
		if (!pilot.IsEmpty())
		{
			if (!group.m_aUnitPrefabSlots)
				group.m_aUnitPrefabSlots = {};
			group.m_aUnitPrefabSlots.Clear();
			group.m_aUnitPrefabSlots.Insert(pilot);
		}
		return group;
	}

	override protected void StartForce(AROR_Operation operation)
	{
		if (!GetGroundQRFRecipeId(operation.GetRole()).IsEmpty() || !m_bReaperQRFTest || operation.GetType() != AROR_EOperationType.QRF)
		{
			super.StartForce(operation);
			return;
		}
		StartReaperQRF(operation);
	}

	protected void StartReaperQRF(AROR_Operation operation)
	{
		if (!Replication.IsServer() || FindForce(operation.GetId()))
			return;
		foreach (AROR_OperationForce existing : m_aForces)
		{
			if (AROR_ReaperAirForce.Cast(existing))
			{
				ReportSpawnWait(operation, "Experimental airspace/LZ reserved by another QRF; one airborne operation at a time");
				return;
			}
		}
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_Commander commander;
		AROR_LocationComponent target;
		if (commanders)
			commander = commanders.GetCommander(operation.GetRole());
		if (locations)
			target = locations.GetLocationById(operation.GetTargetLocationId());
		if (!commander || !target || !ReaperSettingsValid())
		{
			FailOperation(operation, "REAPER QRF test configuration/commander/target unavailable");
			return;
		}
		AROR_ForceProfile profile;
		foreach (AROR_ForceProfile candidate : m_aForceProfiles)
		{
			if (candidate && candidate.m_eType == AROR_EForceType.AIRBORNE && candidate.m_eRole == operation.GetRole() && candidate.m_sFactionKey == commander.GetFactionKey())
			{
				profile = candidate;
				break;
			}
		}
		if (!profile || !profile.IsValid() || profile.m_iEscortCount != 1)
		{
			FailOperation(operation, "REAPER test requires an airborne profile with exactly one escort");
			return;
		}
		int level = 1;
		if (m_Operations.IsQRFStrategyEnabled())
		{
			level = AffordableReaperTier(profile, Math.Max(1, operation.m_iRequestedTier));
			if (level == 0)
			{
				ReportSpawnWait(operation, "No affordable complete airborne pair within the desired war tier");
				return;
			}
		}
#ifdef WORKBENCH
		if (!m_Operations.IsQRFStrategyEnabled() && m_iWorkbenchResponseLevel > 0)
			level = m_iWorkbenchResponseLevel;
#endif
		if (level < 1 || level > 5)
		{
			FailOperation(operation, "REAPER response level must be 1-5");
			return;
		}
		if (GetReservedGroups() + 3 * level > m_iMaxGroups)
		{
			ReportSpawnWait(operation, "Full airborne response exceeds remaining group capacity");
			return;
		}
		if (GetReservedMembers() + profile.m_iPersonnelBudget * level > m_iMaxMembers)
		{
			ReportSpawnWait(operation, "Full airborne response exceeds remaining personnel capacity");
			return;
		}
		if (GetReservedVehicles() + 2 * level > m_iMaxVehicles)
		{
			ReportSpawnWait(operation, "Full airborne response exceeds remaining aircraft capacity");
			return;
		}
		vector home = m_vOccupiersSpawnPosition;
		ResourceName pilot = "{2B5FFD713CEF5848}Prefabs/Characters/REAPER_US_Pilot.et.et";
		if (operation.GetRole() == AROR_EFactionRole.INVADERS)
		{
			home = m_vInvadersSpawnPosition;
			pilot = "{6F0A8F3C1F9C95A3}Prefabs/Characters/REAPER_USSR_Pilot.et";
		}
		if (!PrepareReaperLanding(operation, target, level))
			return;
		home = ResolveQRFOrigin(operation, target, home);
		int padCount = m_ReaperLandingSearch.GetResultCount();
		vector commonHeading = AROR_ReaperFlightGeometry.Direction(home, target.GetCenter());
		vector right = Vector(commonHeading[2], 0, -commonHeading[0]);
		array<ref AROR_ReaperAirForce> prepared = {};
		for (int index = 0; index < level; index++)
		{
			// Reuse a pad only when fewer safe sites exist than transports; those users queue.
			int padIndex = index - (index / padCount) * padCount;
			vector landing = m_ReaperLandingSearch.GetResult(padIndex);
			vector launch = home + right * ((index - (level - 1) * 0.5) * 200);
			vector heading = AROR_ReaperFlightGeometry.Direction(launch, landing);
			vector escortLaunch = AROR_ReaperFlightGeometry.FormationPoint(launch, heading, m_fReaperFormationSpacing);
			if (!IsLaunchPositionSafe(launch, target.GetCenter()) || !IsLaunchPositionSafe(escortLaunch, target.GetCenter()))
			{
				FailOperation(operation, "An airborne insertion lane fails target/player distance checks; adjust role launch origin");
				return;
			}
			float height = m_fReaperInsertionHeight + index * 15;
			if (!AROR_ReaperFlightGeometry.IsAirspaceClear(launch, height, 14))
			{
				FailOperation(operation, "Transport air insertion volume obstructed; adjust role launch origin");
				return;
			}
			if (!AROR_ReaperFlightGeometry.IsAirspaceClear(escortLaunch, m_fReaperGunshipHeight + index * m_fReaperGunshipBand, 14))
			{
				FailOperation(operation, "Escort air insertion volume obstructed; adjust role launch origin");
				return;
			}
			AROR_ReaperAirForce force = new AROR_ReaperAirForce();
			force.m_sOperationId = operation.GetId();
			force.m_sFactionKey = commander.GetFactionKey();
			force.m_sAssetFactionKey = profile.m_sAssetFactionKey;
			force.m_Profile = profile;
			force.m_iPackageIndex = index;
			force.m_iPackageCount = level;
			force.m_bConstructionPending = true;
			force.m_sPilotPrefab = pilot;
			force.m_iReservedGroups = 3;
			force.m_iReservedMembers = profile.m_iPersonnelBudget;
			force.m_iReservedVehicles = 2;
			force.m_bLogFriendlyDamage = m_bLogFriendlyDamage;
			force.m_vTown = target.GetCenter();
			force.m_fTownRadius = target.GetRadius();
			force.m_vLaunch = launch;
			force.m_vEscortLaunch = escortLaunch;
			force.m_vHeading = heading;
			force.m_vLanding = landing;
			float holdingDistance = Math.Max(m_fReaperHoldingDistance, target.GetRadius() + 300);
			force.m_vAirApproach = landing - commonHeading * holdingDistance + right * ((index - (level - 1) * 0.5) * 100);
			force.m_vCover = AROR_ReaperFlightGeometry.FormationPoint(force.m_vAirApproach, heading, m_fReaperFormationSpacing);
			force.m_vEgress = landing - heading * (m_fReaperCleanupDistance + 1600);
			force.m_iRetainSeconds = m_iReaperRetainSeconds;
			force.m_fCleanupDistance = m_fReaperCleanupDistance;
			prepared.Insert(force);
		}
		array<AROR_ForceProfile> financialElements = {};
		for (int priced = 0; priced < level; priced++)
			financialElements.Insert(profile);
		if (!ReserveQRFFunding(operation, financialElements, level))
			return;
		if (!operation.BindForceProfile_S(profile.m_sId, level))
			return;
		CreateQRFTacticalCommander(operation, level, home);
		if (operation.m_TacticalCommander && operation.m_TacticalCommander.m_iFirstWaveCount * 2 > m_iQRFMaxLiveAircraft)
		{
			FailOperation(operation, "QRF lead air wave exceeds live-aircraft limit; enable waves or raise configured limit");
			return;
		}
		// Reserve the entire fleet before the first entity/group enters the engine queue.
		AROR_OperationForce reservation = new AROR_OperationForce();
		reservation.m_sOperationId = operation.GetId();
		reservation.m_bReservation = true;
		reservation.m_iReservedGroups = 3 * level;
		reservation.m_iReservedMembers = profile.m_iPersonnelBudget * level;
		reservation.m_iReservedVehicles = 2 * level;
		m_aForces.Insert(reservation);
		foreach (AROR_ReaperAirForce packageForce : prepared)
		{
			ConfigureForceFunding(packageForce, operation, profile);
			packageForce.m_bTacticalControlled = operation.m_TacticalCommander != null;
			reservation.m_iReservedGroups -= packageForce.m_iReservedGroups;
			reservation.m_iReservedMembers -= packageForce.m_iReservedMembers;
			reservation.m_iReservedVehicles -= packageForce.m_iReservedVehicles;
			m_aForces.Insert(packageForce);
		}
		ReleaseForce(reservation);
		Print(string.Format("[AROR REAPER] QRF %1 level=%2 queued: %2 transports/fireteams and %2 gunships; all packages reserved, constructing one pair per executor tick", operation.GetId(), level), LogLevel.NORMAL);
	}

	//! Cache progress between retry calls. Advance itself rejects duplicate calls in one tick.
	protected bool PrepareReaperLanding(AROR_Operation operation, AROR_LocationComponent target, int level)
	{
		if (m_sReaperPlanningOperation != operation.GetId())
		{
			AROR_Operation previous = m_Operations.GetOperation(m_sReaperPlanningOperation);
			if (previous && !previous.IsTerminal())
			{
				ReportSpawnWait(operation, "Another airborne QRF is selecting landing sites");
				return false;
			}
			m_ReaperLandingSearch = new AROR_ReaperLandingSearch();
			m_ReaperLandingSearch.Initialize(target.GetCenter() + m_vReaperLZOffset, ReaperLandingRadius(target.GetRadius()), m_fReaperPadClearance, level, m_fReaperLandingSeparation);
			m_sReaperPlanningOperation = operation.GetId();
			ReportSpawnWait(operation, "Selecting separate town LZs incrementally: two candidates per tick, 73 total; fewer clear sites queue locally without reducing fleet size");
		}
		m_ReaperLandingSearch.Advance(m_iExecutorClock);
		if (!m_ReaperLandingSearch.IsComplete())
			return false;
		if (!m_ReaperLandingSearch.HasResult())
		{
			FailOperation(operation, "All 73 town-LZ candidates rejected; bounded planning stopped");
			return false;
		}
		Print(string.Format("[AROR REAPER] QRF %1 landing plan ready after %2 checks: %3 separate pads for %4 transports; different pads land concurrently", operation.GetId(), m_ReaperLandingSearch.GetCheckedCount(), m_ReaperLandingSearch.GetResultCount(), level), LogLevel.NORMAL);
		return true;
	}

	protected IEntity SpawnReaperAircraft(Resource resource, vector point, float height)
	{
		EntitySpawnParams parameters = new EntitySpawnParams();
		parameters.TransformMode = ETransformMode.WORLD;
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + height;
		parameters.Transform[3] = point;
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
	}

	protected bool SpawnReaperPackage(AROR_Operation operation, AROR_ReaperAirForce force, ResourceName pilot)
	{
		force.m_Escort = new AROR_GroundOperationForce();
		force.m_Escort.m_sOperationId = operation.GetId();
		force.m_Escort.m_iPackageIndex = force.m_iPackageIndex;
		force.m_Escort.m_sFactionKey = force.m_sFactionKey;
		force.m_Escort.m_sAssetFactionKey = force.m_sAssetFactionKey;
		force.m_Escort.m_bLogFriendlyDamage = m_bLogFriendlyDamage;
		ConfigureForceFunding(force.m_Escort, operation, force.m_Profile);
		force.m_Escort.m_iVehicleLossValue = force.m_Profile.m_iEscortLossValue;
		Resource transport = Resource.Load(force.m_Profile.m_sVehicle);
		Resource gunship = Resource.Load(force.m_Profile.m_sEscortVehicle);
		if (transport && transport.IsValid())
			force.m_Vehicle = SpawnReaperAircraft(transport, force.m_vLaunch, m_fReaperInsertionHeight + force.m_iPackageIndex * 15);
		if (gunship && gunship.IsValid())
			force.m_Escort.m_Vehicle = SpawnReaperAircraft(gunship, force.m_vEscortLaunch, ReaperGunshipHeight(force));
		if (!force.InitializeSpawnFaction_S(force.m_Vehicle))
			return false;
		if (!force.m_Escort.InitializeSpawnFaction_S(force.m_Escort.m_Vehicle))
			return false;
		if (!force.m_Vehicle.GetPhysics() || !force.m_Escort.m_Vehicle.GetPhysics())
			return false;
		force.m_bAircraftHeld = true;
		force.HoldAircraft_S();
		force.TrackDamage_S(force.m_Vehicle);
		force.m_Escort.TrackDamage_S(force.m_Escort.m_Vehicle);
		vector troopPoint = force.m_vLaunch + Vector(-15, 0, 0);
		vector pilotPoint = force.m_vLaunch + Vector(8, 0, 0);
		vector escortPilotPoint = force.m_vEscortLaunch + Vector(8, 0, 0);
		SCR_AIGroup crew = ReaperGroup(force, m_sCrewGroupPrefab, pilot, pilotPoint);
		SCR_AIGroup infantry = ReaperGroup(force, force.m_Profile.m_sInfantryGroup, "", troopPoint);
		SCR_AIGroup escortCrew = ReaperGroup(force.m_Escort, m_sCrewGroupPrefab, pilot, escortPilotPoint);
		force.m_Escort.m_Group = force.m_Escort.m_CrewGroup;
		if (!crew || !infantry || !escortCrew)
			return false;
		if (!infantry.m_aUnitPrefabSlots || infantry.m_aUnitPrefabSlots.Count() < 1)
			return false;
		if (infantry.m_aUnitPrefabSlots.Count() + 2 > force.m_iReservedMembers)
			return false;
		force.m_Waypoint = SpawnWaypoint(m_sQRFWaypointPrefab, force.m_vTown);
		if (!force.m_Waypoint)
			return false;
		crew.RequestSpawn(1);
		infantry.RequestSpawn(infantry.m_aUnitPrefabSlots.Count());
		escortCrew.RequestSpawn(1);
		Print(string.Format("[AROR REAPER] QRF %1 package=%2/%3 queued; faction=%4 LZ=%5", operation.GetId(), force.m_iPackageIndex + 1, force.m_iPackageCount, force.m_sFactionKey, force.m_vLanding), LogLevel.NORMAL);
		return true;
	}

	//! Aircraft are held in the air while personnel board, then dispatched together.
	//! This avoids both falling aircraft and expensive/unnecessary ground assembly searches.
	protected bool InsertReaperAircraft(AROR_GroundOperationForce aircraft, float extraHeight = 0)
	{
		if (!aircraft || !aircraft.m_Vehicle)
			return false;
		HelicopterControllerComponent controller = HelicopterControllerComponent.Cast(aircraft.m_Vehicle.FindComponent(HelicopterControllerComponent));
		Physics physics = aircraft.m_Vehicle.GetPhysics();
		if (!controller || !physics)
			return false;
		vector position = aircraft.m_Vehicle.GetOrigin();
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + m_fReaperInsertionHeight + extraHeight;
		aircraft.m_Vehicle.SetOrigin(position);
		physics.SetVelocity(vector.Zero);
		ForceStartEngineParams parameters = new ForceStartEngineParams();
		parameters.m_bAirborne = true;
		controller.ForceStartEngine(parameters);
		return controller.IsEngineOn();
	}

	override protected void TryActivateOperation(AROR_Operation operation)
	{
		AROR_ReaperAirForce first = AROR_ReaperAirForce.Cast(FindForce(operation.GetId()));
		if (!first)
		{
			super.TryActivateOperation(operation);
			return;
		}
		AROR_QRFTacticalCommander tactical = operation.m_TacticalCommander;
		if (operation.IsTerminal())
			return;
		if (operation.GetState() != AROR_EOperationState.SPAWNING && !tactical)
			return;
		array<AROR_ReaperAirForce> packages = {};
		foreach (AROR_OperationForce tracked : m_aForces)
		{
			if (tracked.m_sOperationId != operation.GetId())
				continue;
			AROR_ReaperAirForce deferredAir = AROR_ReaperAirForce.Cast(tracked);
			if (tactical && deferredAir && deferredAir.m_bConstructionPending && tactical.IsReserve(tracked.m_iPackageIndex))
				continue;
			if (tracked.m_bOrdersIssued)
				continue;
			if (tracked.m_bReservation || !tracked.m_bReady)
				return;
			AROR_ReaperAirForce packageForce = AROR_ReaperAirForce.Cast(tracked);
			if (!packageForce)
				return;
			packages.Insert(packageForce);
		}
		if (packages.Count() == 0)
			return;
		int required = operation.GetResponseLevel();
		if (tactical)
			required = tactical.m_iFirstWaveCount;
		if (operation.GetState() == AROR_EOperationState.SPAWNING && packages.Count() != required)
			return;
		int dispatched = 0;
		foreach (AROR_ReaperAirForce dispatchForce : packages)
		{
			if (!DispatchReaperPackage(dispatchForce))
			{
				FailReaperPackage(dispatchForce, operation, "REAPER wave flight orders/airborne startup failed");
				if (operation.IsTerminal())
					return;
				continue;
			}
			dispatched++;
		}
		if (operation.GetState() == AROR_EOperationState.SPAWNING)
		{
			m_Operations.MarkActive_S(operation.GetId());
			if (tactical)
				tactical.MarkDispatched(m_iExecutorClock);
		}
		AROR_QRFSearchPlan plan = m_Operations.PrepareQRFSearch_S(operation.GetId(), m_iQRFContactMaxAge);
		foreach (AROR_ReaperAirForce activeForce : packages)
		{
			if (!m_aForces.Contains(activeForce))
				continue;
			activeForce.m_iSupportEndTime = m_iExecutorClock + m_iReaperSupportSeconds;
			activeForce.m_vInvestigation = activeForce.m_vTown;
			activeForce.m_SearchPlan = plan;
			if (plan)
				plan.GetContactCenter(activeForce.m_vTown, activeForce.m_fTownRadius, m_fQRFSearchMargin, activeForce.m_vInvestigation);
			activeForce.m_vGunshipSearch = activeForce.m_vTown;
		}
		Print(string.Format("[AROR REAPER] QRF %1 ACTIVE level=%2: %3 newly ready packages dispatched; reserved later-wave elements may remain", operation.GetId(), operation.GetResponseLevel(), dispatched), LogLevel.NORMAL);
	}

	protected bool DispatchReaperPackage(AROR_ReaperAirForce force)
	{
		force.ReleaseAircraft_S();
		float transportHeight = m_fReaperTransportHeight + force.m_iPackageIndex * 15;
		if (!ReaperOrder(force, ReaperMovePrefab(), force.m_vAirApproach, m_fReaperTransportSpeed, transportHeight))
			return false;
		if (!ReaperOrder(force.m_Escort, ReaperMovePrefab(), force.m_vCover, m_fReaperTransportSpeed, ReaperGunshipHeight(force)))
			return false;
		if (!InsertReaperAircraft(force, force.m_iPackageIndex * 15))
			return false;
		if (!InsertReaperAircraft(force.m_Escort, ReaperGunshipHeight(force) - m_fReaperInsertionHeight))
			return false;
		force.m_bOrdersIssued = true;
		force.m_iFlightStage = 1;
		force.m_iFlightDeadline = m_iExecutorClock + m_iReaperFlightTimeout * force.m_iPackageCount;
		force.m_iEscortDeadline = force.m_iFlightDeadline;
		return true;
	}

	protected void WithdrawTacticalAir(AROR_ReaperAirForce force, AROR_Operation operation)
	{
		AROR_QRFTacticalCommander commander = operation.m_TacticalCommander;
		force.m_iSupportEndTime = m_iExecutorClock;
		MonitorReaperEscort(force);
		if (!force.m_bTacticalWithdrawing)
		{
			force.m_bTacticalWithdrawing = true;
			if (!force.m_bTransportRetired && !force.m_bTransportLost && force.m_CrewGroup && force.m_CrewGroup.GetPlayerCount() == 0)
				ReaperOrder(force, ReaperMovePrefab(), force.m_vEgress, m_fReaperDepartureSpeed, m_fReaperTransportHeight + force.m_iPackageIndex * 15);
			if (force.m_bDismounted && commander.m_bSafeWithdrawalOrigin)
				ReplaceTacticalWaypoint(force, false, m_sMoveWaypointPrefab, commander.m_vOrigin);
			Print(string.Format("[AROR TACTICAL] %1 air package=%2 withdrawing; no nearby removal", operation.GetId(), force.m_iPackageIndex + 1), LogLevel.WARNING);
		}
		if (TacticalEntitiesFar(force))
		{
			bool escortFar = true;
			if (force.m_Escort)
				escortFar = TacticalEntitiesFar(force.m_Escort);
			if (escortFar)
				FailReaperPackage(force, operation, "Air package withdrew beyond all players; normal retention applies");
		}
	}

	protected void FailReaperPackage(AROR_ReaperAirForce force, AROR_Operation operation, string reason)
	{
		if (operation.GetState() == AROR_EOperationState.SPAWNING)
			FailOperation(operation, reason);
		else
			FailPackage(force, operation, reason);
	}

	protected float ReaperLandingRadius(float townRadius)
	{
		return Math.Min(800, Math.Max(m_fReaperLZSearchRadius, townRadius + 150));
	}

	//! Distinct pads can land concurrently. Only overlapping landing corridors queue.
	protected bool ReserveReaperLanding(AROR_ReaperAirForce force)
	{
		foreach (AROR_OperationForce tracked : m_aForces)
		{
			AROR_ReaperAirForce other = AROR_ReaperAirForce.Cast(tracked);
			if (!other || other == force || other.m_sOperationId != force.m_sOperationId)
				continue;
			if (other.m_iFlightStage < 2 || other.m_bTransportLost || other.m_bTransportRetired)
				continue;
			if (vector.DistanceXZ(other.m_vLanding, force.m_vLanding) >= m_fReaperLandingSeparation)
				continue;
			if (!other.m_bDismounted)
				return false;
			if (other.m_Vehicle && vector.DistanceXZ(other.m_Vehicle.GetOrigin(), force.m_vLanding) < m_fReaperLandingSeparation)
				return false;
		}
		return true;
	}

	//! Each replacement belongs to one package, with a fleet-wide two-candidate budget.
	protected bool RefreshReaperLanding(AROR_ReaperAirForce force, AROR_Operation operation)
	{
		if (!force.m_AlternateLandingSearch)
		{
			if (AROR_ReaperFlightGeometry.IsLandingClear(force.m_vLanding, m_fReaperPadClearance))
				return true;
			force.m_AlternateLandingSearch = new AROR_ReaperLandingSearch();
			force.m_AlternateLandingSearch.Initialize(force.m_vTown + m_vReaperLZOffset, ReaperLandingRadius(force.m_fTownRadius), m_fReaperPadClearance, 1, m_fReaperLandingSeparation);
		}
		// Refresh reservations, because another transport may have selected a new pad.
		array<vector> reserved = {};
		foreach (AROR_OperationForce tracked : m_aForces)
		{
			AROR_ReaperAirForce other = AROR_ReaperAirForce.Cast(tracked);
			if (!other || other == force || other.m_sOperationId != force.m_sOperationId)
				continue;
			if (!other.m_bTransportRetired && !other.m_bTransportLost)
				reserved.Insert(other.m_vLanding);
		}
		force.m_AlternateLandingSearch.SetReservedPads(reserved);
		if (m_iLastReaperAlternateSearchTick == m_iExecutorClock)
			return false;
		m_iLastReaperAlternateSearchTick = m_iExecutorClock;
		force.m_AlternateLandingSearch.Advance(m_iExecutorClock);
		if (!force.m_AlternateLandingSearch.IsComplete())
			return false;
		if (!force.m_AlternateLandingSearch.HasResult())
		{
			FailReaperPackage(force, operation, "Package landing area blocked and bounded replacement search exhausted");
			return false;
		}
		force.m_vLanding = force.m_AlternateLandingSearch.GetResult();
		force.m_vEgress = force.m_vLanding - force.m_vHeading * (m_fReaperCleanupDistance + 1600);
		force.m_AlternateLandingSearch = null;
		Print(string.Format("[AROR REAPER] QRF %1 package=%2 selected its own alternate LZ=%3", operation.GetId(), force.m_iPackageIndex + 1, force.m_vLanding), LogLevel.NORMAL);
		return true;
	}
	override protected void MonitorForce(AROR_OperationForce force)
	{
		AROR_ReaperAirForce airForce = AROR_ReaperAirForce.Cast(force);
		if (!airForce)
		{
			super.MonitorForce(force);
			return;
		}
		MonitorReaperQRF(airForce);
	}

	protected bool ReaperVehicleFaction(AROR_GroundOperationForce aircraft)
	{
		if (!aircraft || !aircraft.m_Vehicle)
			return false;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(aircraft.m_Vehicle.FindComponent(FactionAffiliationComponent));
		return affiliation && affiliation.GetAffiliatedFactionKey() == aircraft.m_sFactionKey;
	}

	protected bool ReaperGroupFaction(SCR_AIGroup group, string key)
	{
		return group && group.GetFaction() && group.GetFaction().GetFactionKey() == key;
	}

	protected bool ReaperNearAirborne(IEntity vehicle, vector point)
	{
		if (!vehicle || vector.DistanceXZ(vehicle.GetOrigin(), point) > 75)
			return false;
		vector position = vehicle.GetOrigin();
		return position[1] - GetGame().GetWorld().GetSurfaceY(position[0], position[2]) > 10;
	}

	protected bool ReaperSettingsValid()
	{
		// Enforce has a low formula-complexity limit; keep each range independent.
		if (m_iReaperFlightTimeout < 30 || m_iReaperFlightTimeout > 600)
			return false;
		if (m_fReaperInsertionHeight < 60 || m_fReaperInsertionHeight > 300)
			return false;
		if (m_fReaperTransportSpeed < 100 || m_fReaperTransportSpeed > 200)
			return false;
		if (m_fReaperTransportHeight < 40 || m_fReaperTransportHeight > 120)
			return false;
		if (m_fReaperHoldingDistance < 400 || m_fReaperHoldingDistance > 1500)
			return false;
		if (m_iReaperTouchdownTicks < 1 || m_iReaperTouchdownTicks > 5)
			return false;
		if (m_fReaperDepartureSpeed < 120 || m_fReaperDepartureSpeed > 220)
			return false;
		if (m_vReaperLZOffset.Length() > 600)
			return false;
		if (m_fReaperLZSearchRadius < 60 || m_fReaperLZSearchRadius > 800)
			return false;
		if (m_fReaperLandingSeparation < 70 || m_fReaperLandingSeparation > 180)
			return false;
		if (m_fReaperPadClearance < 14 || m_fReaperPadClearance > 30)
			return false;
		if (m_fReaperFormationSpacing < 60 || m_fReaperFormationSpacing > 150)
			return false;
		if (m_fReaperCleanupDistance < 800 || m_fReaperCleanupDistance > 3000)
			return false;
		if (m_iReaperRetainSeconds < 120 || m_iReaperRetainSeconds > 1800)
			return false;
		if (m_iReaperSupportSeconds < 60 || m_iReaperSupportSeconds > 600)
			return false;
		if (m_fReaperFriendlyClearance < 75 || m_fReaperFriendlyClearance > 200)
			return false;
		if (m_fReaperSupportRadius < 500 || m_fReaperSupportRadius > 5000)
			return false;
		if (m_iReaperContactAge < 15 || m_iReaperContactAge > 180)
			return false;
		if (m_fReaperGunshipHeight < 180 || m_fReaperGunshipHeight > 300)
			return false;
		if (m_fReaperGunshipBand < 30 || m_fReaperGunshipBand > 60)
			return false;
		if (m_fReaperTrafficClearance < 60 || m_fReaperTrafficClearance > 150)
			return false;
		return true;
	}

	protected bool ReaperEscortClearOfPlayers(AROR_ReaperAirForce force)
	{
		AROR_GroundOperationForce escort = force.m_Escort;
		if (!ReaperNearAirborne(escort.m_Vehicle, escort.m_Vehicle.GetOrigin()))
			return false;
		if (vector.DistanceXZ(escort.m_Vehicle.GetOrigin(), force.m_vTown) <= 600)
			return false;
		if (AROR_ReaperRetirement.NearPlayers(escort.m_Vehicle, m_fReaperCleanupDistance))
			return false;
		return !AROR_ReaperRetirement.MembersNearPlayers(escort, m_fReaperCleanupDistance);
	}

	protected bool ReaperTransportClearOfPlayers(AROR_ReaperAirForce force)
	{
		if (!force.m_Vehicle || m_iExecutorClock - force.m_iDepartureTime < 10)
			return false;
		if (!ReaperNearAirborne(force.m_Vehicle, force.m_Vehicle.GetOrigin()))
			return false;
		if (vector.DistanceXZ(force.m_Vehicle.GetOrigin(), force.m_vLanding) < 500)
			return false;
		if (AROR_ReaperRetirement.NearPlayers(force.m_Vehicle, m_fReaperCleanupDistance))
			return false;
		return !AROR_ReaperRetirement.GroupNearPlayers(force.m_CrewGroup, m_fReaperCleanupDistance);
	}

	protected bool ReaperFactionsValid(AROR_ReaperAirForce force)
	{
		if (!force.HasCorrectMemberFactions())
			return false;
		if (AROR_ReaperAirForce.AircraftAlive(force.m_Vehicle) && !ReaperVehicleFaction(force))
			return false;
		if (!force.m_Escort)
			return true;
		if (!force.m_Escort.HasCorrectMemberFactions())
			return false;
		if (AROR_ReaperAirForce.AircraftAlive(force.m_Escort.m_Vehicle) && !ReaperVehicleFaction(force.m_Escort))
			return false;
		return true;
	}

	override protected int GetReservedGroups()
	{
		return super.GetReservedGroups() + AROR_ReaperRetirement.Reserved(0);
	}

	override protected int GetReservedMembers()
	{
		return super.GetReservedMembers() + AROR_ReaperRetirement.Reserved(1);
	}

	override protected int GetReservedVehicles()
	{
		return super.GetReservedVehicles() + AROR_ReaperRetirement.Reserved(2);
	}

	protected void RetireReaperEscort(AROR_ReaperAirForce force, string reason)
	{
		force.m_bEscortLost = true;
		if (force.m_Escort)
		{
			REAPER_AiHelicopterControllerComponent controller = AROR_ReaperAirForce.Controller(force.m_Escort.m_Vehicle);
			if (controller)
				controller.AROR_ConfigureQRF(force.m_sFactionKey, force.m_vTown, force.m_fTownRadius, m_fReaperFriendlyClearance, false);
		}
		// Keep the aircraft, crew and casualties owned. Terminal retention cleans them
		// only after its grace period and when every player is far away.
		Print(string.Format("[AROR REAPER] QRF %1 escort disabled: %2; wreck/crew retained, remaining forces continue", force.m_sOperationId, reason), LogLevel.WARNING);
	}

	protected bool IssueReaperPass(AROR_ReaperAirForce force)
	{
		float angle = Math.Atan2(force.m_vHeading[0], force.m_vHeading[2]) + force.m_iPassOrdinal * 2.4 + force.m_iPackageIndex * 6.2831853 / force.m_iPackageCount;
		int ring = force.m_iPassOrdinal - (force.m_iPassOrdinal / 3) * 3;
		force.m_vPassPoint = force.m_vGunshipSearch + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * (320 + ring * 140);
		force.m_iPassOrdinal++;
		force.m_iEscortStage = 1;
		force.m_iEscortDeadline = m_iExecutorClock + 75;
		REAPER_AiHelicopterControllerComponent controller = AROR_ReaperAirForce.Controller(force.m_Escort.m_Vehicle);
		if (!controller)
			return false;
		controller.AROR_ConfigureQRF(force.m_sFactionKey, force.m_vGunshipSearch, 500, m_fReaperFriendlyClearance, true);
		Print(string.Format("[AROR REAPER] QRF %1 gunship pass %2 around last reported contact/town; perceived hostile targets only", force.m_sOperationId, force.m_iPassOrdinal), LogLevel.NORMAL);
		return ReaperOrder(force.m_Escort, ReaperMovePrefab(), force.m_vPassPoint, 130, ReaperGunshipHeight(force));
	}

	protected void UpdateGunshipContact(AROR_ReaperAirForce force)
	{
		if (m_iExecutorClock < force.m_iNextContactCheck)
			return;
		force.m_iNextContactCheck = m_iExecutorClock + 5;
		AROR_Operation operation = m_Operations.GetOperation(force.m_sOperationId);
		if (!operation)
			return;
		AROR_CombatContact contact = m_Operations.GetRecentCombatContact(operation.GetRole(), force.m_vTown, m_fReaperSupportRadius, m_iReaperContactAge);
		if (!contact)
		{
			if (force.m_iGunshipContactSequence > 0)
			{
				force.m_iGunshipContactSequence = 0;
				force.m_vGunshipSearch = force.m_vTown;
				force.m_iEscortDeadline = 0;
			}
			return;
		}
		if (contact.m_iSequence == force.m_iGunshipContactSequence)
			return;
		bool retask = force.m_iGunshipContactSequence == 0;
		if (vector.DistanceXZ(force.m_vGunshipSearch, contact.m_vPosition) > 100)
			retask = true;
		force.m_iGunshipContactSequence = contact.m_iSequence;
		force.m_vGunshipSearch = contact.m_vPosition;
		if (!retask)
			return;
		// Nearby updates refresh intel without interrupting every pass for every bullet.
		force.m_iEscortDeadline = 0;
		Print(string.Format("[AROR REAPER] QRF %1 gunship=%2 investigating allied report=%3 at %4; snapshot only", force.m_sOperationId, force.m_iPackageIndex + 1, contact.m_iSequence, contact.m_vPosition), LogLevel.NORMAL);
	}

	protected IEntity GunshipTrafficConflict(AROR_ReaperAirForce force)
	{
		IEntity aircraft = force.m_Escort.m_Vehicle;
		Physics physics = aircraft.GetPhysics();
		if (!physics)
			return null;
		foreach (AROR_OperationForce tracked : m_aForces)
		{
			AROR_ReaperAirForce other = AROR_ReaperAirForce.Cast(tracked);
			if (!other || !other.m_bOrdersIssued)
				continue;
			array<IEntity> candidates = {};
			if (!other.m_bTransportLost && !other.m_bTransportRetired)
				candidates.Insert(other.m_Vehicle);
			// Stable priority prevents two gunships from both choosing evasive paths.
			if (other.m_iPackageIndex < force.m_iPackageIndex && !other.m_bEscortLost && other.m_Escort)
				candidates.Insert(other.m_Escort.m_Vehicle);
			foreach (IEntity candidate : candidates)
			{
				if (!candidate || candidate == aircraft || !candidate.GetPhysics())
					continue;
				float distance = AROR_AirTrafficPolicy.PredictedSeparation(aircraft.GetOrigin(), physics.GetVelocity(), candidate.GetOrigin(), candidate.GetPhysics().GetVelocity(), 5);
				if (distance < m_fReaperTrafficClearance)
					return candidate;
			}
		}
		return null;
	}

	//! Gunships yield to transports and lower-index gunships using actual world positions.
	protected bool YieldGunshipTraffic(AROR_ReaperAirForce force)
	{
		if (force.m_bTrafficYielding && m_iExecutorClock < force.m_iTrafficYieldUntil)
			return true;
		if (force.m_bTrafficYielding)
		{
			force.m_bTrafficYielding = false;
			force.m_iEscortDeadline = 0;
		}
		IEntity conflict = GunshipTrafficConflict(force);
		if (!conflict)
			return false;
		IEntity aircraft = force.m_Escort.m_Vehicle;
		vector position = aircraft.GetOrigin();
		vector away = AROR_ReaperFlightGeometry.Direction(conflict.GetOrigin(), position);
		float currentHeight = position[1] - GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		float height = Math.Min(600, Math.Max(ReaperGunshipHeight(force), currentHeight + 40));
		REAPER_AiHelicopterControllerComponent controller = AROR_ReaperAirForce.Controller(aircraft);
		if (controller)
			controller.AROR_ConfigureQRF(force.m_sFactionKey, force.m_vGunshipSearch, 500, m_fReaperFriendlyClearance, false);
		if (!ReaperOrder(force.m_Escort, ReaperMovePrefab(), position + away * 250, 140, height))
		{
			RetireReaperEscort(force, "traffic-yield order unavailable");
			return true;
		}
		force.m_bTrafficYielding = true;
		force.m_iTrafficYieldUntil = m_iExecutorClock + 6;
		Print(string.Format("[AROR REAPER] QRF %1 gunship=%2 yielding to predicted aircraft conflict", force.m_sOperationId, force.m_iPackageIndex + 1), LogLevel.WARNING);
		return true;
	}

	protected void MonitorReaperEscort(AROR_ReaperAirForce force)
	{
		if (force.m_bEscortLost)
			return;
		AROR_GroundOperationForce escort = force.m_Escort;
		if (!escort || !escort.m_CrewGroup || !AROR_ReaperAirForce.AircraftAlive(escort.m_Vehicle) || escort.CountLivingMembers() < 1)
		{
			RetireReaperEscort(force, "aircraft or pilot lost");
			return;
		}
		if (escort.m_CrewGroup.GetPlayerCount() > 0)
			return;
		if (force.m_iEscortStage != 2 && m_iExecutorClock < force.m_iSupportEndTime)
		{
			if (YieldGunshipTraffic(force))
				return;
		}
		if (force.m_iEscortStage == 2)
		{
			if (ReaperEscortClearOfPlayers(force))
			{
				escort.Cleanup_S();
				force.m_bEscortLost = true;
				force.m_iReservedGroups--;
				force.m_iReservedVehicles--;
				Print(string.Format("[AROR REAPER] QRF %1 gunship departed beyond all players; removed", force.m_sOperationId), LogLevel.NORMAL);
			}
			else
			{
				bool resumeEgress = force.m_bTrafficYielding;
				if (YieldGunshipTraffic(force))
					return;
				if (resumeEgress)
				{
					if (!ReaperOrder(escort, ReaperMovePrefab(), force.m_vEgress, 160, ReaperGunshipHeight(force)))
						RetireReaperEscort(force, "departure resumption after traffic yield unavailable");
				}
			}
			return;
		}
		if (m_iExecutorClock >= force.m_iSupportEndTime)
		{
			REAPER_AiHelicopterControllerComponent controller = AROR_ReaperAirForce.Controller(escort.m_Vehicle);
			if (controller)
				controller.AROR_ConfigureQRF(force.m_sFactionKey, force.m_vTown, force.m_fTownRadius, m_fReaperFriendlyClearance, false);
			if (!ReaperOrder(escort, ReaperMovePrefab(), force.m_vEgress, 160, ReaperGunshipHeight(force)))
				RetireReaperEscort(force, "departure order unavailable");
			else
				force.m_iEscortStage = 2;
			return;
		}
		UpdateGunshipContact(force);
		if (force.m_iFlightStage == 1 && !force.m_bTransportLost && force.m_Vehicle && !force.m_bWaitingForLanding)
		{
			// Relative offset on the common ingress heading, updated at a bounded rate.
			// Separate altitude and side offset leave the transport's rotor corridor clear.
			if (m_iExecutorClock >= force.m_iNextFormationUpdate)
			{
				vector point = AROR_ReaperFlightGeometry.FormationPoint(force.m_Vehicle.GetOrigin(), force.m_vHeading, m_fReaperFormationSpacing);
				force.m_iNextFormationUpdate = m_iExecutorClock + 8;
				if (!ReaperOrder(escort, ReaperMovePrefab(), point, m_fReaperTransportSpeed, ReaperGunshipHeight(force)))
					RetireReaperEscort(force, "formation order unavailable");
			}
			return;
		}
		// Move waypoints allow REAPER's autonomous attack runs; Hover inhibits rockets.
		if (force.m_iEscortStage == 0 || m_iExecutorClock >= force.m_iEscortDeadline || (force.m_iEscortStage == 1 && vector.DistanceXZ(escort.m_Vehicle.GetOrigin(), force.m_vPassPoint) <= 220))
		{
			if (!IssueReaperPass(force))
				RetireReaperEscort(force, "attack-pass order unavailable");
		}
	}

	protected void IssueReaperInfantrySearch(AROR_ReaperAirForce force)
	{
		if (!force.m_Group || force.m_Group.GetPlayerCount() > 0)
			return;
		AROR_Operation operation = m_Operations.GetOperation(force.m_sOperationId);
		if (operation && operation.m_TacticalCommander)
		{
			TacticalInfantryOrders(force, operation);
			return;
		}
		if (force.m_iPatrolStep > 5)
			return;
		vector point = force.m_vInvestigation;
		ResourceName prefab = m_sMoveWaypointPrefab;
		if (force.m_iPatrolStep > 0 && force.m_iPatrolStep < 5 && force.m_SearchPlan)
			point = force.m_SearchPlan.GetSectorPoint(force.m_vTown, force.m_fTownRadius * 0.6, force.m_iPackageIndex, force.m_iPackageCount, force.m_iPatrolStep);
		if (force.m_iPatrolStep == 5)
		{
			point = force.m_vTown;
			prefab = m_sQRFWaypointPrefab;
		}
		AIWaypoint next = SpawnWaypoint(prefab, point);
		if (!next)
			return;
		force.m_Group.AddWaypoint(next);
		if (force.m_Waypoint)
		{
			force.m_Group.RemoveWaypoint(force.m_Waypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(force.m_Waypoint);
		}
		force.m_Waypoint = next;
		force.m_iPatrolStep++;
		force.m_iPatrolDeadline = m_iExecutorClock + 60;
	}

	protected void MonitorReaperDeparture(AROR_ReaperAirForce force)
	{
		if (force.m_bTransportLost || force.m_bTransportRetired || !force.m_CrewGroup || force.m_CrewGroup.GetPlayerCount() > 0)
			return;
		if (force.m_iFlightStage != 4)
		{
			force.m_iFlightStage = 4;
			force.m_iDepartureTime = m_iExecutorClock;
			if (!ReaperOrder(force, ReaperMovePrefab(), force.m_vEgress, m_fReaperDepartureSpeed, m_fReaperTransportHeight + force.m_iPackageIndex * 15))
				force.m_bTransportLost = true;
			else
				Print(string.Format("[AROR REAPER] QRF %1 transport taking off; removal requires airborne departure and >%2m from every player", force.m_sOperationId, m_fReaperCleanupDistance), LogLevel.NORMAL);
		}
		if (ReaperTransportClearOfPlayers(force))
		{
			force.RemoveDepartedTransport_S();
			Print(string.Format("[AROR REAPER] QRF %1 transport departed beyond every player; infantry continue", force.m_sOperationId), LogLevel.NORMAL);
		}
		else if (!force.m_bDepartureWarning && m_iExecutorClock - force.m_iDepartureTime > m_iReaperFlightTimeout)
		{
			force.m_bDepartureWarning = true;
			Print(string.Format("[AROR REAPER] QRF %1 departure stalled/players nearby; aircraft retained, infantry continue", force.m_sOperationId), LogLevel.WARNING);
		}
	}

	protected void MonitorReaperQRF(AROR_ReaperAirForce force)
	{
		AROR_Operation operation = m_Operations.GetOperation(force.m_sOperationId);
		if (!operation || operation.IsTerminal())
		{
			ReleaseForce(force);
			return;
		}
		if (force.m_bConstructionPending)
		{
			AROR_QRFTacticalCommander tactical = operation.m_TacticalCommander;
			if (tactical && tactical.ReserveExpired(m_iExecutorClock))
			{
				ReleaseForce(force);
				Print(string.Format("[AROR TACTICAL] %1 unspawned air package=%2 cancelled; reserve deadline/withdrawal", operation.GetId(), force.m_iPackageIndex + 1), LogLevel.WARNING);
				if (!HasOperatingQRF(operation.GetId()))
					FailOperation(operation, "Air reserve expired without surviving elements");
				return;
			}
			if (tactical && !tactical.CanConstruct(force.m_iPackageIndex, m_iExecutorClock, LiveTacticalAircraftCount(), m_iQRFMaxLiveAircraft, 2))
				return;
			if (tactical && m_iLastQRFConstructionTick == m_iExecutorClock)
				return;
			if (m_iLastReaperConstructionTick == m_iExecutorClock)
				return;
			if (tactical)
				m_iLastQRFConstructionTick = m_iExecutorClock;
			m_iLastReaperConstructionTick = m_iExecutorClock;
			force.m_bConstructionPending = false;
			if (!SpawnReaperPackage(operation, force, force.m_sPilotPrefab))
				FailReaperPackage(force, operation, "REAPER staged wave construction failed");
			return;
		}
		if (!force.m_Group)
		{
			FailReaperPackage(force, operation, "REAPER infantry group no longer exists; surviving aircraft/wrecks enter retention");
			return;
		}
		if (!force.m_bOrdersIssued)
			force.HoldAircraft_S();
		array<SCR_AIGroup> groups = {force.m_Group, force.m_CrewGroup};
		if (force.m_Escort && !force.m_bEscortLost)
			groups.Insert(force.m_Escort.m_CrewGroup);
		force.m_iWakeRetryTicks--;
		bool waiting;
		foreach (SCR_AIGroup group : groups)
		{
			if (group && group.IsDormant() && (!force.m_bReady || group.GetDormantAliveCount() > 0))
			{
				waiting = true;
				if (force.m_iWakeRetryTicks <= 0)
					group.RequestSpawn();
			}
		}
		if (waiting)
		{
			if (force.m_iWakeRetryTicks <= 0)
				force.m_iWakeRetryTicks = m_iWakeRetryInterval;
			return;
		}
		int infantry = force.CountInfantry();
		int crew = force.CountCrew();
		int escorts;
		if (force.m_Escort && !force.m_bEscortLost)
			escorts = force.m_Escort.CountLivingMembers();
		if (operation.m_TacticalCommander && force.m_bReady)
			RecordTacticalStrength(force, operation, infantry);
		if (!ReaperFactionsValid(force))
		{
			FailReaperPackage(force, operation, "REAPER aircraft/personnel campaign faction mismatch");
			return;
		}
		if (operation.GetState() == AROR_EOperationState.SPAWNING || !force.m_bReady)
		{
			if (force.m_bReady)
			{
				TryActivateOperation(operation);
				return;
			}
			if (!force.m_CrewGroup || !AROR_ReaperAirForce.AircraftAlive(force.m_Vehicle) || !force.m_Escort || !AROR_ReaperAirForce.AircraftAlive(force.m_Escort.m_Vehicle) || !force.m_Escort.m_CrewGroup)
			{
				FailReaperPackage(force, operation, "REAPER required aircraft lost before dispatch");
				return;
			}
			if (crew < 1 || infantry < 1 || escorts < 1 || !force.m_Group.IsExpandComplete() || !force.m_CrewGroup.IsExpandComplete() || !force.m_Escort.m_CrewGroup.IsExpandComplete())
				return;
			if (!ReaperGroupFaction(force.m_Group, force.m_sFactionKey) || !ReaperGroupFaction(force.m_CrewGroup, force.m_sFactionKey) || !ReaperGroupFaction(force.m_Escort.m_CrewGroup, force.m_sFactionKey))
			{
				FailReaperPackage(force, operation, "REAPER infantry/pilot group faction mismatch");
				return;
			}
			if (!force.m_bMounted && !force.BoardPackage_S())
			{
				FailReaperPackage(force, operation, "REAPER pilot/passenger boarding failed");
				return;
			}
			REAPER_AiHelicopterControllerComponent transportController = AROR_ReaperAirForce.Controller(force.m_Vehicle);
			REAPER_AiHelicopterControllerComponent escortController = AROR_ReaperAirForce.Controller(force.m_Escort.m_Vehicle);
			if (!transportController || !escortController)
				return;
			transportController.AROR_ConfigureQRF(force.m_sFactionKey, force.m_vTown, force.m_fTownRadius, m_fReaperFriendlyClearance, false);
			escortController.AROR_ConfigureQRF(force.m_sFactionKey, force.m_vTown, force.m_fTownRadius, m_fReaperFriendlyClearance, false);
			force.m_bReady = true;
			if (operation.m_TacticalCommander)
				RecordTacticalStrength(force, operation, infantry);
			Print(string.Format("[AROR REAPER] QRF %1 package=%2/%3 faction=%4 verified; boarded infantry=%5", operation.GetId(), force.m_iPackageIndex + 1, force.m_iPackageCount, force.m_sFactionKey, infantry), LogLevel.NORMAL);
			TryActivateOperation(operation);
			return;
		}
		if (operation.GetState() != AROR_EOperationState.ACTIVE)
			return;
		if (!force.m_bOrdersIssued)
		{
			TryActivateOperation(operation);
			return;
		}
		bool airWithdrawal = force.m_bTacticalWithdrawing;
		if (operation.m_TacticalCommander)
		{
			if (operation.m_TacticalCommander.m_bWithdraw)
				airWithdrawal = true;
			// Successful transport/gunship departure must not count as an infantry casualty.
			if (force.m_iTacticalInitialStrength > 0 && infantry <= force.m_iTacticalInitialStrength * m_fQRFLocalWithdrawFraction)
				airWithdrawal = true;
		}
		if (operation.m_TacticalCommander && airWithdrawal)
		{
			WithdrawTacticalAir(force, operation);
			return;
		}
		if (infantry < 1)
		{
			FailReaperPackage(force, operation, "REAPER QRF infantry eliminated");
			return;
		}
		if (!ReaperGroupFaction(force.m_Group, force.m_sFactionKey))
		{
			FailReaperPackage(force, operation, "REAPER infantry group faction changed");
			return;
		}
		if (!force.m_bTransportRetired && !force.m_bTransportLost && (!AROR_ReaperAirForce.AircraftAlive(force.m_Vehicle) || crew < 1))
		{
			force.m_bTransportLost = true;
			force.m_iFlightDeadline = m_iExecutorClock + m_iReaperFlightTimeout;
			Print(string.Format("[AROR REAPER] QRF %1 transport/pilot lost; aircraft retained, recovering surviving passengers", operation.GetId()), LogLevel.WARNING);
		}
		MonitorReaperEscort(force);
		if (force.m_bDismounted)
		{
			MonitorReaperDeparture(force);
			if (operation.m_TacticalCommander)
			{
				TacticalInfantryOrders(force, operation);
				return;
			}
			IEntity leader = force.m_Group.GetLeaderEntity();
			bool needsOrder = force.m_iPatrolStep == 0 || m_iExecutorClock >= force.m_iPatrolDeadline;
			if (!needsOrder && leader && force.m_Waypoint)
				needsOrder = vector.DistanceXZ(leader.GetOrigin(), force.m_Waypoint.GetOrigin()) <= 30;
			if (needsOrder)
				IssueReaperInfantrySearch(force);
			return;
		}
		if (force.m_CrewGroup && force.m_CrewGroup.GetPlayerCount() > 0)
			return;
		if (force.m_bTransportLost)
		{
			if (force.m_Vehicle && AROR_ReaperAirForce.SettledAt(force.m_Vehicle, force.m_Vehicle.GetOrigin()))
				force.RequestDismount_S();
			if (force.PassengersSafeOnGround())
			{
				force.m_bDismounted = true;
				IssueReaperInfantrySearch(force);
				Print(string.Format("[AROR REAPER] QRF %1 surviving passengers recovered on foot; aircraft/casualties retained", operation.GetId()), LogLevel.NORMAL);
				return;
			}
		}
		if (!force.m_bTransportLost && force.m_iFlightStage == 1)
		{
			// A cleared transport commits directly: no mandatory approach-hover stop.
			if (ReserveReaperLanding(force))
			{
				if (!RefreshReaperLanding(force, operation))
				{
					if (!operation.IsTerminal() && m_iExecutorClock > force.m_iFlightDeadline)
						FailReaperPackage(force, operation, "Alternate LZ search timed out");
					return;
				}
				if (!ReaperOrder(force, ReaperLandPrefab(), force.m_vLanding, m_fReaperTransportSpeed, m_fReaperTransportHeight))
				{
					FailReaperPackage(force, operation, "REAPER direct landing order unavailable");
					return;
				}
				force.m_iFlightStage = 2;
				force.m_iFlightDeadline = m_iExecutorClock + m_iReaperFlightTimeout;
				Print(string.Format("[AROR REAPER] QRF %1 package=%2/%3 direct Land ordered at package pad; no approach-hover stop, LZ=%4", operation.GetId(), force.m_iPackageIndex + 1, force.m_iPackageCount, force.m_vLanding), LogLevel.NORMAL);
				return;
			}
			bool atApproach = force.m_bWaitingForLanding;
			if (!atApproach)
				atApproach = ReaperNearAirborne(force.m_Vehicle, force.m_vAirApproach);
			if (atApproach)
			{
				if (!force.m_bWaitingForLanding)
				{
					force.m_bWaitingForLanding = true;
					if (!ReaperOrder(force, ReaperHoverPrefab(), force.m_vAirApproach, m_fReaperTransportSpeed, m_fReaperTransportHeight + force.m_iPackageIndex * 15))
					{
						FailReaperPackage(force, operation, "Transport holding order unavailable");
						return;
					}
				}
			}
		}
		else if (force.m_iFlightStage == 2)
		{
			if (AROR_ReaperAirForce.SettledAt(force.m_Vehicle, force.m_vLanding))
				force.m_iSettleTicks++;
			else
				force.m_iSettleTicks = 0;
			if (force.m_iSettleTicks >= m_iReaperTouchdownTicks)
			{
				force.m_iFlightStage = 3;
				force.m_iFlightDeadline = m_iExecutorClock + m_iReaperFlightTimeout;
				force.RequestDismount_S();
				Print(string.Format("[AROR REAPER] QRF %1 ground contact and low speed stable; requesting normal passenger exits", operation.GetId()), LogLevel.NORMAL);
			}
		}
		else if (force.m_iFlightStage == 3)
		{
			if (!force.ArePassengersOut() && AROR_ReaperAirForce.SettledAt(force.m_Vehicle, force.m_vLanding))
				force.RequestDismount_S();
			if (force.m_bDismountRequested && force.ArePassengersOut())
			{
				force.m_bDismounted = true;
				IssueReaperInfantrySearch(force);
				MonitorReaperDeparture(force);
				Print(string.Format("[AROR REAPER] QRF %1 infantry physically delivered; investigate last contact then patrol town, transport egress ordered", operation.GetId()), LogLevel.NORMAL);
			}
		}
		if (m_iExecutorClock > force.m_iFlightDeadline)
			FailReaperPackage(force, operation, "REAPER flight/landing/queue stage timed out");
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (GetGame().InPlayMode() && Replication.IsServer() && m_bEnabled && m_bReaperQRFTest)
			Print("[AROR REAPER] Experimental airborne QRF override ENABLED: response level controls complete transport/fireteam/gunship packages; disable Reaper QRF Test on the executor to restore ground responses", LogLevel.WARNING);
	}

	override void OnDelete(IEntity owner)
	{
		// Clear session references/timers even if replication has already shut down.
		foreach (AROR_OperationForce trackedForce : m_aForces)
		{
			AROR_ReaperAirForce airForce = AROR_ReaperAirForce.Cast(trackedForce);
			if (airForce)
				airForce.m_bImmediateCleanup = true;
		}
		m_ReaperLandingSearch = null;
		m_iLastReaperAlternateSearchTick = -1;
		m_sReaperPlanningOperation = string.Empty;
		AROR_ReaperRetirement.Flush_S();
		super.OnDelete(owner);
	}
}
