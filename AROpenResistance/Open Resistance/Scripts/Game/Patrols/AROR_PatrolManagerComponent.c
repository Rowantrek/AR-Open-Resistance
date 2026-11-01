[ComponentEditorProps(category: "OpenResistance/Patrols", description: "Ambient enemy foot and road patrols that move through the areas around Resistance players. Attach to the game mode entity.")]
class AROR_PatrolManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Patrols (see ARCHITECTURE.md -> Patrols): ambient Occupier and Invader foot and road patrols around the Resistance
//! players. Server only and session-only: nothing is saved and nothing is replicated beyond the AI themselves.
//! Player positions only decide WHERE activity happens. A patrol spawns on a road out of every player's sight, takes a
//! route fixed at spawn that crosses the area, and never re-targets players. It belongs to the army holding the nearest
//! military site, and its route keeps clear of Resistance sites (no free captures) and of the other army (the two
//! armies never meet). Losses report a combat contact to Operations, which may escalate through its own rules.
//! Every step is bounded: spawn and seating timeouts abort and clean up, stalled legs are skipped, and patrols despawn
//! once no player has been near for a while.
class AROR_PatrolManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", desc: "Spawn ambient patrols around Resistance players")]
	protected bool m_bEnabled;

	[Attribute("5", desc: "Seconds between patrol updates", params: "2 30 1")]
	protected int m_iUpdateInterval;

	[Attribute("45", desc: "Seconds between attempts to add a patrol", params: "10 600 5")]
	protected int m_iSpawnInterval;

	[Attribute("2000", desc: "Patrols within this distance of a player count towards that player's area (m)", params: "500 6000 50")]
	protected float m_fAreaRadius;

	[Attribute("1", desc: "Patrols wanted in each player's area; the army's Crackdown adds one per two levels", params: "0 10 1")]
	protected int m_iPatrolsPerArea;

	[Attribute("6", desc: "Most patrols at once", params: "0 30 1")]
	protected int m_iMaxPatrols;

	[Attribute("40", desc: "Most patrol soldiers at once, separate from the garrison and QRF budgets", params: "0 200 1")]
	protected int m_iMaxSoldiers;

	[Attribute("3", desc: "Most patrol vehicles at once", params: "0 20 1")]
	protected int m_iMaxVehicles;

	[Attribute("900", desc: "Patrols spawn at least this far from every player (m)", params: "300 3000 50")]
	protected float m_fSpawnMinDistance;

	[Attribute("1700", desc: "Patrols spawn at most this far from the player whose area they serve (m)", params: "500 4000 50")]
	protected float m_fSpawnMaxDistance;

	[Attribute("600", desc: "A patrol's route passes within this distance of its area's centre (m)", params: "100 2000 50")]
	protected float m_fPassRadius;

	[Attribute("3000", desc: "A patrol belongs to the army holding the nearest military site within this distance; with none, no patrol spawns there. Half this distance is kept from the other army's sites (m)", params: "500 8000 100")]
	protected float m_fTerritoryRadius;

	[Attribute("250", desc: "Spawn points stay this far outside Resistance sites and every military site (m beyond the site radius)", params: "0 1000 10")]
	protected float m_fSiteClearance;

	[Attribute("75", desc: "Patrol routes stay this far outside Resistance military sites and the HQ, so they never enter one to capture it (m beyond the site radius). Kept small so patrols can still pass players who are at their own sites", params: "0 500 5")]
	protected float m_fRouteClearance;

	[Attribute("2500", desc: "A patrol despawns once no player has been within this distance for the despawn delay (m)", params: "1000 6000 50")]
	protected float m_fDespawnDistance;

	[Attribute("90", desc: "Seconds with no player near before a patrol despawns", params: "15 900 5")]
	protected int m_iDespawnDelay;

	[Attribute("120", desc: "Seconds after its last casualty before a patrol may despawn or be judged stalled", params: "0 900 5")]
	protected int m_iCombatHold;

	[Attribute("1800", desc: "Seconds a patrol follows its route before it only waits to despawn", params: "300 7200 60")]
	protected int m_iMaxLifetime;

	[Attribute("40", desc: "Chance (%) that a new patrol is motorized, when the army has a usable vehicle", params: "0 100 5")]
	protected int m_iMotorizedChance;

	[Attribute("4", desc: "Foot patrol size; Crackdown 3 and 5 add one each", params: "2 12 1")]
	protected int m_iFootTeamSize;

	[Attribute("2", desc: "Passengers riding in a motorized patrol's vehicle, if it has the seats; Crackdown 4 adds one", params: "0 8 1")]
	protected int m_iMotorPassengers;

	[Attribute("3", desc: "Readiness from which motorized patrols may use armed cars", params: "1 5 1")]
	protected int m_iArmedCarReadiness;

	[Attribute("4", desc: "Readiness from which motorized patrols may use APCs", params: "1 5 1")]
	protected int m_iAPCReadiness;

	[Attribute("45", desc: "Seconds for a patrol's members to spawn before it is abandoned", params: "10 300 5")]
	protected int m_iSpawnTimeout;

	[Attribute("30", desc: "Seconds to seat a motorized patrol before it is abandoned", params: "10 300 5")]
	protected int m_iSeatTimeout;

	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty AI group prefab patrols spawn in", params: "et", category: "Prefabs")]
	protected ResourceName m_sGroupPrefab;

	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, desc: "Waypoint for patrol legs", params: "et", category: "Prefabs")]
	protected ResourceName m_sMoveWaypointPrefab;

	protected static const int PROGRESS_CHECK = 60;		//!< Seconds between progress checks on a leg
	protected static const float PROGRESS_MIN = 15;		//!< Metres a patrol must cover between checks, unless fighting
	protected static const int STALL_LIMIT = 3;			//!< Failed progress checks before a leg is skipped
	protected static const int PLAN_ATTEMPTS = 8;		//!< Spawn points tried per spawn attempt
	protected static const int MIDDLE_ATTEMPTS = 4;		//!< Route points near the area's centre tried per spawn point
	protected static const int PLAN_REPORT_EVERY = 4;	//!< Failed spawn attempts between route-planning reports

	//! Route-planning failure reasons, counted for the periodic report
	protected static const int PLAN_NO_START_ROAD = 0;
	protected static const int PLAN_START_BLOCKED = 1;
	protected static const int PLAN_NO_ARMY = 2;
	protected static const int PLAN_NO_MIDDLE = 3;
	protected static const int PLAN_NO_EXIT = 4;
	protected static const int PLAN_COUNT = 5;
	protected static const float FOOT_ARRIVAL = 40;
	protected static const float VEHICLE_ARRIVAL = 60;
	protected static const int APC_CHANCE = 40;			//!< Chance (%) of an APC once the readiness allows one

	protected ref array<ref AROR_Patrol> m_aPatrols = {};
	protected ref array<vector> m_aPlayerPositions = {};
	protected int m_iSpawnTimer;
	protected int m_iNextId = 1;
	protected ref array<int> m_aPlanFailures = {0, 0, 0, 0, 0};
	protected int m_iFailedSpawnAttempts;

	//------------------------------------------------------------------------------------------------
	// Update
	//------------------------------------------------------------------------------------------------
	protected void Update_S()
	{
		if (!m_bEnabled)
			return;

		AROR_GameFlowComponent gameFlow = AROR_GameFlowComponent.GetInstance();
		if (gameFlow && !gameFlow.IsPlaythroughReady())
			return;

		RefreshPlayerPositions_S();
		for (int i = m_aPatrols.Count() - 1; i >= 0; i--)
		{
			if (!UpdatePatrol_S(m_aPatrols[i]))
				m_aPatrols.Remove(i);
		}

		m_iSpawnTimer -= m_iUpdateInterval;
		if (m_iSpawnTimer > 0)
			return;

		m_iSpawnTimer = m_iSpawnInterval;
		TrySpawn_S();
	}

	//------------------------------------------------------------------------------------------------
	//! False once the patrol is finished and should be forgotten
	protected bool UpdatePatrol_S(AROR_Patrol patrol)
	{
		int seconds = m_iUpdateInterval;
		patrol.m_iAge += seconds;
		patrol.m_iStateAge += seconds;
		patrol.m_iSinceCasualty += seconds;

		int living = AROR_ForceSpawner.CountAlive(patrol.m_Group, patrol.m_eRole, patrol.m_aAffiliated);
		array<IEntity> members = {};
		AROR_ForceSpawner.CountLiving(patrol.m_Group, members);
		if (patrol.m_Vehicle && AROR_ForceSpawner.IsDestroyed(patrol.m_Vehicle))
			patrol.m_Vehicle = null;

		// Track the soldiers, so a crew that has dismounted is not judged by its parked vehicle
		if (!members.IsEmpty())
			patrol.m_vPosition = members[0].GetOrigin();
		else if (patrol.m_Vehicle)
			patrol.m_vPosition = patrol.m_Vehicle.GetOrigin();

		if (patrol.m_eState == AROR_EPatrolState.SPAWNING)
			return UpdateSpawning_S(patrol, living);

		if (patrol.m_eState == AROR_EPatrolState.SEATING)
			return UpdateSeating_S(patrol, living);

		if (living == 0)
		{
			Print(string.Format("[AROR PATROL] patrol_%1 wiped out", patrol.m_iId), LogLevel.NORMAL);
			Remove_S(patrol, false);
			return false;
		}

		if (living < patrol.m_iLiving)
		{
			patrol.m_iSinceCasualty = 0;
			ReportContact_S(patrol, patrol.m_iLiving - living);
		}

		patrol.m_iLiving = living;

		if (IsPlayerWithin(patrol.m_vPosition, m_fDespawnDistance))
			patrol.m_iUnwatched = 0;
		else
			patrol.m_iUnwatched += seconds;

		if (patrol.m_iUnwatched >= m_iDespawnDelay && patrol.m_iSinceCasualty >= m_iCombatHold)
		{
			Print(string.Format("[AROR PATROL] patrol_%1 despawned: no player within %2 m for %3 s", patrol.m_iId, m_fDespawnDistance, patrol.m_iUnwatched), LogLevel.NORMAL);
			Remove_S(patrol, true);
			return false;
		}

		if (patrol.m_eState == AROR_EPatrolState.LEAVING)
			return true;

		if (patrol.m_iAge >= m_iMaxLifetime)
		{
			Leave_S(patrol, "lifetime over");
			return true;
		}

		AdvanceRoute_S(patrol, seconds);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool UpdateSpawning_S(AROR_Patrol patrol, int living)
	{
		patrol.m_iLiving = living;
		if (living >= patrol.m_iExpected)
		{
			if (patrol.m_Vehicle)
				SetState(patrol, AROR_EPatrolState.SEATING);
			else
				StartRoute_S(patrol);

			return true;
		}

		if (patrol.m_iStateAge < m_iSpawnTimeout)
			return true;

		// A short-handed foot patrol still goes; a vehicle needs its full crew
		if (living > 0 && !patrol.m_bMotorized)
		{
			StartRoute_S(patrol);
			return true;
		}

		Print(string.Format("[AROR PATROL] patrol_%1 abandoned: %2 of %3 members spawned in %4 s", patrol.m_iId, living, patrol.m_iExpected, patrol.m_iStateAge), LogLevel.WARNING);
		Remove_S(patrol, true);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool UpdateSeating_S(AROR_Patrol patrol, int living)
	{
		patrol.m_iLiving = living;
		if (!patrol.m_Vehicle || living == 0)
		{
			Print(string.Format("[AROR PATROL] patrol_%1 abandoned: vehicle or crew lost while seating", patrol.m_iId), LogLevel.WARNING);
			Remove_S(patrol, true);
			return false;
		}

		if (AROR_ForceSpawner.SeatGroup(patrol.m_Group, patrol.m_Vehicle, living, true))
		{
			StartRoute_S(patrol);
			return true;
		}

		if (patrol.m_iStateAge < m_iSeatTimeout)
			return true;

		// Never retry forever: give the spawn up and let a later attempt try again
		Print(string.Format("[AROR PATROL] patrol_%1 abandoned: crew not seated after %2 s", patrol.m_iId, patrol.m_iStateAge), LogLevel.WARNING);
		Remove_S(patrol, true);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void AdvanceRoute_S(AROR_Patrol patrol, int seconds)
	{
		float arrival = FOOT_ARRIVAL;
		if (patrol.m_Vehicle)
			arrival = VEHICLE_ARRIVAL;

		if (vector.DistanceXZ(patrol.m_vPosition, patrol.m_aRoute[patrol.m_iLeg]) <= arrival)
		{
			NextLeg_S(patrol);
			return;
		}

		patrol.m_iProgressTimer += seconds;
		if (patrol.m_iProgressTimer < PROGRESS_CHECK)
			return;

		patrol.m_iProgressTimer = 0;
		float moved = vector.DistanceXZ(patrol.m_vPosition, patrol.m_vProgressPosition);
		patrol.m_vProgressPosition = patrol.m_vPosition;
		if (moved >= PROGRESS_MIN || patrol.m_iSinceCasualty < m_iCombatHold)
		{
			patrol.m_iStalls = 0;
			return;
		}

		patrol.m_iStalls++;
		if (patrol.m_iStalls < STALL_LIMIT)
			return;

		Print(string.Format("[AROR PATROL] patrol_%1 stalled on leg %2; skipping it", patrol.m_iId, patrol.m_iLeg + 1), LogLevel.WARNING);
		NextLeg_S(patrol);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartRoute_S(AROR_Patrol patrol)
	{
		SetState(patrol, AROR_EPatrolState.MOVING);
		patrol.m_iLeg = 0;
		patrol.m_iStalls = 0;
		patrol.m_iProgressTimer = 0;
		patrol.m_vProgressPosition = patrol.m_vPosition;
		IssueLeg_S(patrol);
		Print(string.Format("[AROR PATROL] patrol_%1 under way with %2 members", patrol.m_iId, patrol.m_iLiving), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void NextLeg_S(AROR_Patrol patrol)
	{
		patrol.m_iLeg++;
		patrol.m_iStalls = 0;
		patrol.m_iProgressTimer = 0;
		patrol.m_vProgressPosition = patrol.m_vPosition;
		if (patrol.m_iLeg >= patrol.m_aRoute.Count())
		{
			Leave_S(patrol, "route complete");
			return;
		}

		IssueLeg_S(patrol);
	}

	//------------------------------------------------------------------------------------------------
	protected void IssueLeg_S(AROR_Patrol patrol)
	{
		ClearWaypoint(patrol);
		float radius = FOOT_ARRIVAL;
		if (patrol.m_Vehicle)
			radius = VEHICLE_ARRIVAL;

		patrol.m_Waypoint = AROR_ForceSpawner.AddWaypoint(patrol.m_Group, m_sMoveWaypointPrefab, patrol.m_aRoute[patrol.m_iLeg], radius);
	}

	//------------------------------------------------------------------------------------------------
	//! No more orders: the patrol holds where it is until no player is near, then despawns
	protected void Leave_S(AROR_Patrol patrol, string reason)
	{
		ClearWaypoint(patrol);
		SetState(patrol, AROR_EPatrolState.LEAVING);
		Print(string.Format("[AROR PATROL] patrol_%1 %2; waiting to despawn", patrol.m_iId, reason), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetState(AROR_Patrol patrol, AROR_EPatrolState state)
	{
		patrol.m_eState = state;
		patrol.m_iStateAge = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearWaypoint(AROR_Patrol patrol)
	{
		if (!patrol.m_Waypoint)
			return;

		if (patrol.m_Group)
			patrol.m_Group.RemoveWaypoint(patrol.m_Waypoint);

		SCR_EntityHelper.DeleteEntityAndChildren(patrol.m_Waypoint);
		patrol.m_Waypoint = null;
	}

	//------------------------------------------------------------------------------------------------
	//! despawn: delete the members and the vehicle (unless a player is aboard). Otherwise (wiped out) the bodies and
	//! the vehicle stay for the players to loot or take, and only the empty group goes.
	protected void Remove_S(AROR_Patrol patrol, bool despawn)
	{
		ClearWaypoint(patrol);
		if (!despawn)
		{
			if (patrol.m_Group)
				SCR_EntityHelper.DeleteEntityAndChildren(patrol.m_Group);

			return;
		}

		AROR_ForceSpawner.DeleteGroupAndMembers(patrol.m_Group);
		if (patrol.m_Vehicle && !AROR_ForceSpawner.HasPlayerAboard(patrol.m_Vehicle))
			SCR_EntityHelper.DeleteEntityAndChildren(patrol.m_Vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! Losses tell Operations that its army is in contact here, so its own rules decide any response or escalation
	protected void ReportContact_S(AROR_Patrol patrol, int lost)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return;

		AROR_LocationComponent location = NearestLocation(patrol.m_vPosition);
		if (!location)
			return;

		operations.ReportCombatContact_S(patrol.m_eRole, location.GetId(), patrol.m_vPosition);
		Print(string.Format("[AROR PATROL] patrol_%1 lost %2 member(s) near %3; contact reported", patrol.m_iId, lost, location.GetId()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Spawning
	//------------------------------------------------------------------------------------------------
	protected void TrySpawn_S()
	{
		if (m_aPlayerPositions.IsEmpty() || m_aPatrols.Count() >= m_iMaxPatrols)
			return;

		int soldiers;
		int vehicles;
		foreach (AROR_Patrol existing : m_aPatrols)
		{
			if (existing.m_iLiving > existing.m_iExpected)
				soldiers += existing.m_iLiving;
			else
				soldiers += existing.m_iExpected;
			if (existing.m_Vehicle)
				vehicles++;
		}

		if (soldiers >= m_iMaxSoldiers)
			return;

		// Serve one player's area at random; nearby players share an area through the patrol count below
		vector anchor = m_aPlayerPositions.GetRandomElement();
		AROR_EFactionRole role;
		vector start;
		array<vector> route = {};
		bool planned;
		for (int attempt = 0; attempt < PLAN_ATTEMPTS && !planned; attempt++)
		{
			int failure = PlanRoute_S(anchor, role, start, route);
			planned = failure < 0;
			if (!planned)
				m_aPlanFailures[failure] = m_aPlanFailures[failure] + 1;
		}

		if (!planned)
		{
			ReportPlanFailures_S(anchor);
			return;
		}

		m_iFailedSpawnAttempts = 0;

		int readiness = 2;
		int crackdown = 2;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
		{
			readiness = operations.GetReadinessLevel(role);
			crackdown = operations.GetCrackdownLevel(role);
		}

		int wanted = m_iPatrolsPerArea + crackdown / 2;
		int nearby;
		foreach (AROR_Patrol patrol : m_aPatrols)
		{
			if (vector.DistanceXZ(patrol.m_vPosition, anchor) <= m_fAreaRadius)
				nearby++;
		}

		if (nearby >= wanted)
			return;

		SpawnPatrol_S(role, start, route, readiness, crackdown, m_iMaxSoldiers - soldiers, vehicles < m_iMaxVehicles);
	}

	//------------------------------------------------------------------------------------------------
	//! A road start out of every player's sight in one army's ground, then a road point near the area's centre and a
	//! road exit on the far side outside the other army's ground, with every leg clear of Resistance sites and of the
	//! other army. Returns -1 when planned, else a PLAN_ failure reason.
	protected int PlanRoute_S(vector anchor, out AROR_EFactionRole role, out vector start, notnull array<vector> route)
	{
		float bearing = Math.RandomFloat(0, Math.PI * 2);
		float distance = Math.RandomFloat(m_fSpawnMinDistance, m_fSpawnMaxDistance);
		vector probe = anchor + Vector(Math.Sin(bearing) * distance, 0, Math.Cos(bearing) * distance);
		if (!AROR_ForceSpawner.FindRoadPosition(probe, anchor, start))
			return PLAN_NO_START_ROAD;

		if (!IsSpawnPointClear_S(start))
			return PLAN_START_BLOCKED;

		if (!FindArmy_S(start, role))
			return PLAN_NO_ARMY;

		// Several tries for the pass point: players often stand at their own sites, which the legs must skirt
		vector middle;
		bool middleFound;
		for (int attempt = 0; attempt < MIDDLE_ATTEMPTS && !middleFound; attempt++)
		{
			vector across = anchor + Vector(Math.RandomFloat(-m_fPassRadius, m_fPassRadius), 0, Math.RandomFloat(-m_fPassRadius, m_fPassRadius));
			middleFound = AROR_ForceSpawner.FindRoadPosition(across, across, middle) && IsLegClear_S(start, middle, role);
		}

		if (!middleFound)
			return PLAN_NO_MIDDLE;

		// The exit only has to stay out of the other army's ground; it need not lie in this army's own
		bool exitFound;
		vector exit;
		for (int exitAttempt = 0; exitAttempt < MIDDLE_ATTEMPTS && !exitFound; exitAttempt++)
		{
			float exitBearing = bearing + Math.PI + Math.RandomFloat(-1.2, 1.2);
			vector far = anchor + Vector(Math.Sin(exitBearing) * distance, 0, Math.Cos(exitBearing) * distance);
			exitFound = AROR_ForceSpawner.FindRoadPosition(far, far, exit) && !IsOtherArmyGround_S(exit, role) && IsLegClear_S(middle, exit, role);
		}

		if (!exitFound)
			return PLAN_NO_EXIT;

		route.Clear();
		route.Insert(middle);
		route.Insert(exit);
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! True if a military site of the army other than role lies within half the territory radius
	protected bool IsOtherArmyGround_S(vector position, AROR_EFactionRole role)
	{
		array<AROR_LocationComponent> locations = {};
		GetLocations(locations);
		float separation = m_fTerritoryRadius * 0.5;
		foreach (AROR_LocationComponent location : locations)
		{
			AROR_EFactionRole holder = location.GetControllingRole();
			if (IsMilitarySite(location) && IsArmy(holder) && holder != role && vector.DistanceXZ(position, location.GetCenter()) <= separation)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every few failed spawn attempts, logs why no route could be planned, so an empty map can be explained
	protected void ReportPlanFailures_S(vector anchor)
	{
		m_iFailedSpawnAttempts++;
		if (m_iFailedSpawnAttempts % PLAN_REPORT_EVERY != 0)
			return;

		Print(string.Format("[AROR PATROL] no patrol route around %1 after %2 attempts: no start road %3, start too close to a player/site %4, no army ground %5, no clear pass point %6, no clear exit %7",
			anchor, m_iFailedSpawnAttempts * PLAN_ATTEMPTS, m_aPlanFailures[PLAN_NO_START_ROAD], m_aPlanFailures[PLAN_START_BLOCKED],
			m_aPlanFailures[PLAN_NO_ARMY], m_aPlanFailures[PLAN_NO_MIDDLE], m_aPlanFailures[PLAN_NO_EXIT]), LogLevel.WARNING);

		for (int i = 0; i < PLAN_COUNT; i++)
		{
			m_aPlanFailures[i] = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnPatrol_S(AROR_EFactionRole role, vector start, notnull array<vector> route, int readiness, int crackdown, int soldierRoom, bool vehicleRoom)
	{
		ResourceName vehiclePrefab;
		if (vehicleRoom && Math.RandomInt(0, 100) < m_iMotorizedChance)
			vehiclePrefab = PickVehicle(role, readiness);

		vector heading = route[0] - start;
		heading[1] = 0;
		heading.Normalize();
		float yaw = Math.Atan2(heading[0], heading[2]) * Math.RAD2DEG;

		IEntity vehicle;
		array<ResourceName> members = {};
		if (!vehiclePrefab.IsEmpty())
		{
			vehicle = AROR_ForceSpawner.SpawnVehicle(role, vehiclePrefab, start, yaw);
			if (!vehicle)
				return false;

			ResourceName crew = AROR_ForceSpawner.UnitPrefab(role, AROR_EUnitType.CREW);
			int crewSeats = Math.Max(AROR_ForceSpawner.CountCrewSeats(vehicle), 1);
			for (int c = 0; c < crewSeats; c++)
			{
				if (!crew.IsEmpty())
					members.Insert(crew);
			}

			int passengers = m_iMotorPassengers;
			if (crackdown >= 4)
				passengers++;

			passengers = Math.Min(passengers, AROR_ForceSpawner.CountCargoSeats(vehicle));
			array<ResourceName> team = {};
			AROR_ForceSpawner.BuildTeam(role, passengers, team);
			members.InsertAll(team);
		}
		else
		{
			int size = m_iFootTeamSize;
			if (crackdown >= 3)
				size++;

			if (crackdown >= 5)
				size++;

			AROR_ForceSpawner.BuildTeam(role, size, members);
		}

		if (members.IsEmpty() || members.Count() > soldierRoom)
		{
			if (vehicle)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);

			return false;
		}

		vector spot = start;
		vector found;
		if (AROR_ForceSpawner.FindClearPosition(start, 12, 1, found))
			spot = found;

		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, role, members, spot);
		if (!group)
		{
			if (vehicle)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);

			return false;
		}

		AROR_Patrol patrol = new AROR_Patrol();
		patrol.m_iId = m_iNextId;
		m_iNextId++;
		patrol.m_eRole = role;
		patrol.m_eState = AROR_EPatrolState.SPAWNING;
		patrol.m_bMotorized = vehicle != null;
		patrol.m_Group = group;
		patrol.m_Vehicle = vehicle;
		patrol.m_aRoute.InsertAll(route);
		patrol.m_iExpected = members.Count();
		patrol.m_vPosition = start;
		m_aPatrols.Insert(patrol);

		string kind = "foot";
		if (vehicle)
			kind = "motorized " + vehiclePrefab;

		Print(string.Format("[AROR PATROL] patrol_%1 spawned: %2 %3, %4 soldiers at %5, route %6 legs", patrol.m_iId, typename.EnumToString(AROR_EFactionRole, role), kind, members.Count(), start, route.Count()), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The heaviest vehicle the readiness allows that the army's preset has; empty if it has none
	protected ResourceName PickVehicle(AROR_EFactionRole role, int readiness)
	{
		array<AROR_EVehicleType> types = {};
		if (readiness >= m_iAPCReadiness && Math.RandomInt(0, 100) < APC_CHANCE)
			types.Insert(AROR_EVehicleType.APC);

		if (readiness >= m_iArmedCarReadiness)
			types.Insert(AROR_EVehicleType.ARMED_CAR);

		types.Insert(AROR_EVehicleType.MOTOR_CAR);
		foreach (AROR_EVehicleType type : types)
		{
			ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(role, type, readiness);
			if (!prefab.IsEmpty())
				return prefab;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	// Placement rules
	//------------------------------------------------------------------------------------------------
	//! Dry land, beyond every player's spawn distance, outside every military site and every Resistance location
	protected bool IsSpawnPointClear_S(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (world.GetSurfaceY(position[0], position[2]) <= world.GetOceanHeight(position[0], position[2]) + 0.5)
			return false;

		if (IsPlayerWithin(position, m_fSpawnMinDistance))
			return false;

		array<AROR_LocationComponent> locations = {};
		GetLocations(locations);
		foreach (AROR_LocationComponent location : locations)
		{
			bool military = IsMilitarySite(location);
			if (!military && location.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
				continue;

			if (vector.DistanceXZ(position, location.GetCenter()) <= location.GetRadius() + m_fSiteClearance)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The army holding the nearest Occupier/Invader military site within the territory radius, provided the other
	//! army has no site within half of it
	protected bool FindArmy_S(vector position, out AROR_EFactionRole role)
	{
		array<AROR_LocationComponent> locations = {};
		GetLocations(locations);
		float nearest = m_fTerritoryRadius;
		AROR_LocationComponent holder;
		foreach (AROR_LocationComponent location : locations)
		{
			if (!IsMilitarySite(location) || !IsArmy(location.GetControllingRole()))
				continue;

			float distance = vector.DistanceXZ(position, location.GetCenter());
			if (distance <= nearest)
			{
				nearest = distance;
				holder = location;
			}
		}

		if (!holder)
			return false;

		role = holder.GetControllingRole();
		float separation = m_fTerritoryRadius * 0.5;
		foreach (AROR_LocationComponent other : locations)
		{
			if (IsMilitarySite(other) && IsArmy(other.GetControllingRole()) && other.GetControllingRole() != role && vector.DistanceXZ(position, other.GetCenter()) <= separation)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A straight leg (roads stay close to it over these distances) that keeps clear of Resistance military sites and
	//! the HQ, which an enemy patrol must never wander into and capture, and of the other army's sites
	protected bool IsLegClear_S(vector from, vector to, AROR_EFactionRole role)
	{
		array<AROR_LocationComponent> locations = {};
		GetLocations(locations);
		float separation = m_fTerritoryRadius * 0.5;
		foreach (AROR_LocationComponent location : locations)
		{
			if (!IsMilitarySite(location))
				continue;

			AROR_EFactionRole holder = location.GetControllingRole();
			float clearance;
			if (holder == AROR_EFactionRole.RESISTANCE)
				clearance = location.GetRadius() + m_fRouteClearance;
			else if (IsArmy(holder) && holder != role)
				clearance = Math.Max(location.GetRadius() + m_fSiteClearance, separation);
			else
				continue;

			if (DistanceToLegXZ(location.GetCenter(), from, to) <= clearance)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Flag-captured military sites and the HQ
	protected static bool IsMilitarySite(AROR_LocationComponent location)
	{
		AROR_ELocationType type = location.GetLocationType();
		return type == AROR_ELocationType.HQ || AROR_CaptureLocationAction.IsCapturableType(type);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsArmy(AROR_EFactionRole role)
	{
		return role == AROR_EFactionRole.OCCUPIERS || role == AROR_EFactionRole.INVADERS;
	}

	//------------------------------------------------------------------------------------------------
	protected static float DistanceToLegXZ(vector point, vector from, vector to)
	{
		float dx = to[0] - from[0];
		float dz = to[2] - from[2];
		float lengthSq = dx * dx + dz * dz;
		float t;
		if (lengthSq > 0.01)
			t = Math.Clamp(((point[0] - from[0]) * dx + (point[2] - from[2]) * dz) / lengthSq, 0, 1);

		float ox = from[0] + dx * t - point[0];
		float oz = from[2] + dz * t - point[2];
		return Math.Sqrt(ox * ox + oz * oz);
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest location other than the HQ, for contact reports
	protected AROR_LocationComponent NearestLocation(vector position)
	{
		array<AROR_LocationComponent> locations = {};
		GetLocations(locations);
		float nearest = float.MAX;
		AROR_LocationComponent best;
		foreach (AROR_LocationComponent location : locations)
		{
			if (location.GetLocationType() == AROR_ELocationType.HQ)
				continue;

			float distance = vector.DistanceXZ(position, location.GetCenter());
			if (distance < nearest)
			{
				nearest = distance;
				best = location;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected void GetLocations(notnull array<AROR_LocationComponent> outLocations)
	{
		outLocations.Clear();
		AROR_LocationManagerComponent manager = AROR_LocationManagerComponent.GetInstance();
		if (!manager)
			return;

		array<AROR_LocationComponent> all = {};
		manager.GetLocations(all);
		foreach (AROR_LocationComponent location : all)
		{
			if (location)
				outLocations.Insert(location);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Players
	//------------------------------------------------------------------------------------------------
	protected void RefreshPlayerPositions_S()
	{
		m_aPlayerPositions.Clear();
		array<int> playerIds = {};
		PlayerManager playerManager = GetGame().GetPlayerManager();
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
			if (AROR_ForceSpawner.IsAlive(controlled))
				m_aPlayerPositions.Insert(controlled.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsPlayerWithin(vector position, float range)
	{
		foreach (vector player : m_aPlayerPositions)
		{
			if (vector.DistanceXZ(player, position) <= range)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Lifecycle
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		m_iSpawnTimer = m_iSpawnInterval;
		GetGame().GetCallqueue().CallLater(Update_S, m_iUpdateInterval * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Update_S);
		super.OnDelete(owner);
	}
}
