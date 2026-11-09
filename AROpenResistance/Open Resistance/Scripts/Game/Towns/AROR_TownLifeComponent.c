[ComponentEditorProps(category: "OpenResistance/Locations", description: "Town life: walking civilians and parked civilian cars, plus road checkpoints and a crewed patrol vehicle while an AI army holds the town. Spawned only while players are near. Put on a town or city location.")]
class AROR_TownLifeComponentClass : ScriptComponentClass {}

//! Server-only ambience for one settlement, built while a player is within the activation distance and removed when
//! they leave. Civilians and cars use the Civilians role's faction preset; checkpoints and the patrol use the holding
//! army's. Crackdown sets how many checkpoints and patrols stand; Readiness sets the patrol's vehicle. Session-only:
//! nothing here is saved; wiped-out checkpoints and patrols stay gone for the respawn time.
class AROR_TownLifeComponent : ScriptComponent
{
	[Attribute("900", desc: "Town life is built while a player is this close to the town centre, metres", params: "300 2000 50", category: "Activation")]
	protected float m_fActivationDistance;
	[Attribute("1150", desc: "Town life is removed once no player has been this close for half a minute, metres", params: "400 2500 50", category: "Activation")]
	protected float m_fRetireDistance;
	[Attribute("60", desc: "Nothing spawns this close to a player, metres", params: "20 200 5", category: "Activation")]
	protected float m_fSpawnPlayerClearance;

	[Attribute("1", desc: "Walking civilians", category: "Civilians")]
	protected bool m_bCivilians;
	[Attribute("0.15", desc: "Civilians per resident (settlement population)", params: "0 1 0.01", category: "Civilians")]
	protected float m_fCiviliansPerResident;
	[Attribute("3", params: "0 30 1", category: "Civilians")]
	protected int m_iMinCivilians;
	[Attribute("10", params: "0 30 1", category: "Civilians")]
	protected int m_iMaxCivilians;
	[Attribute("3", desc: "Resistance support lost here (percentage points) when a player kills a civilian", params: "0 25 0.5", category: "Civilians")]
	protected float m_fCivilianKillSupport;

	[Attribute("1", desc: "Unmanned civilian cars parked along the streets; players can take them", category: "Parked cars")]
	protected bool m_bParkedCars;
	[Attribute("0.08", desc: "Parked cars per resident", params: "0 1 0.01", category: "Parked cars")]
	protected float m_fCarsPerResident;
	[Attribute("2", params: "0 12 1", category: "Parked cars")]
	protected int m_iMinCars;
	[Attribute("6", params: "0 12 1", category: "Parked cars")]
	protected int m_iMaxCars;

	[Attribute("1", desc: "Road checkpoints on the way into town while an AI army holds it: 1 at first, 2 from Crackdown 3, 3 at Crackdown 5, one more in a city", category: "Military")]
	protected bool m_bCheckpoints;
	[Attribute("4", desc: "Guards at each checkpoint", params: "1 8 1", category: "Military")]
	protected int m_iCheckpointGuards;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Props placed at each checkpoint, alternating either side of the road. Empty: a sandbag wall each side.", params: "et", category: "Military")]
	protected ref array<ResourceName> m_aCheckpointProps;
	[Attribute("1", desc: "A crewed vehicle patrolling the town's roads while an AI army holds it (two from Crackdown 4). Readiness picks it: armed car, then APC, then IFV.", category: "Military")]
	protected bool m_bPatrol;
	[Attribute("2", desc: "Riflemen riding in the patrol vehicle besides its crew", params: "0 6 1", category: "Military")]
	protected int m_iPatrolRiders;
	[Attribute("2700", desc: "Seconds before a wiped-out checkpoint or patrol stands again", params: "300 14400 60", category: "Military")]
	protected int m_iMilitaryRespawnSeconds;

	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty AI group prefab", params: "et", category: "Prefabs")]
	protected ResourceName m_sGroupPrefab;
	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, params: "et", category: "Prefabs")]
	protected ResourceName m_sMoveWaypointPrefab;
	[Attribute("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", UIWidgets.ResourceNamePicker, params: "et", category: "Prefabs")]
	protected ResourceName m_sDefendWaypointPrefab;

	protected static const int TICK_SECONDS = 2;
	protected static const int RETIRE_DELAY = 30;
	protected static const int KILL_CREDIT_SECONDS = 30;
	protected static const int STREET_SAMPLES = 12;
	protected static const int ENTRY_SAMPLES = 16;
	protected static const int PATROL_SEAT_DEADLINE = 90;
	protected static const int CIVILIAN_SPAWN_DEADLINE = 60;
	protected static const ResourceName DEFAULT_CHECKPOINT_PROP = "{21D83950DF36F250}Prefabs/Props/Military/Sandbags/Sandbag_01_long_burlap.et";

	protected AROR_LocationComponent m_Location;
	protected AROR_SettlementComponent m_Settlement;
	protected bool m_bActive;
	protected bool m_bPointsReady;
	protected int m_iClock;
	protected int m_iFarSince = -1;
	protected AROR_EFactionRole m_eMilitaryRole;
	protected ref array<vector> m_aStreets = {};
	protected ref array<vector> m_aEntries = {};
	protected ref array<ref AROR_TownCivilian> m_aCivilians = {};
	protected ref array<IEntity> m_aCars = {};
	protected ref array<vector> m_aCarSpots = {};
	protected ref array<ref AROR_TownCheckpoint> m_aCheckpoints = {};
	protected ref array<ref AROR_TownPatrol> m_aPatrols = {};
	protected ref array<int> m_aCheckpointLostAt = {};	//!< Per entry point: clock when its checkpoint was wiped out
	protected ref array<int> m_aPatrolLostAt = {};
	protected ref array<IEntity> m_aLeftovers = {};	//!< Props of wiped-out checkpoints, removed on retirement

	//------------------------------------------------------------------------------------------------
	//! Living checkpoint guards and patrol crews (for the town's wave response)
	void GetMilitaryMembers_S(notnull array<IEntity> members)
	{
		array<IEntity> living = {};
		foreach (AROR_TownCheckpoint checkpoint : m_aCheckpoints)
		{
			AROR_ForceSpawner.CountLiving(checkpoint.m_Group, living);
			members.InsertAll(living);
		}
		foreach (AROR_TownPatrol patrol : m_aPatrols)
		{
			AROR_ForceSpawner.CountLiving(patrol.m_Group, living);
			members.InsertAll(living);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!Replication.IsServer() || !m_Location)
			return;
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!commanders || !commanders.IsCampaignActive())
			return;
		m_iClock += TICK_SECONDS;

		if (!m_bActive)
		{
			if (PlayerWithin(m_Location.GetCenter(), m_fActivationDistance))
				Activate();
			return;
		}
		if (PlayerWithin(m_Location.GetCenter(), m_fRetireDistance))
			m_iFarSince = -1;
		else if (m_iFarSince < 0)
			m_iFarSince = m_iClock;
		else if (m_iClock - m_iFarSince >= RETIRE_DELAY)
		{
			Deactivate();
			return;
		}

		UpdateCivilians();
		UpdateCars();
		UpdateMilitary();
	}

	//------------------------------------------------------------------------------------------------
	protected void Activate()
	{
		m_bActive = true;
		m_iFarSince = -1;
		EnsurePoints();
		Print(string.Format("[AROR TOWN] %1 town life active: %2 street and %3 entry road points", m_Location.GetId(), m_aStreets.Count(), m_aEntries.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes everything built here. Cars a player has moved are left in the world and forgotten.
	protected void Deactivate()
	{
		m_bActive = false;
		foreach (AROR_TownCivilian civilian : m_aCivilians)
			civilian.Remove_S();
		m_aCivilians.Clear();
		// ClearMilitary also removes the leftovers (wrecked checkpoints, bodies)
		foreach (int index, IEntity car : m_aCars)
		{
			if (car && !AROR_ForceSpawner.IsDestroyed(car) && vector.DistanceXZ(car.GetOrigin(), m_aCarSpots[index]) < 25)
				SCR_EntityHelper.DeleteEntityAndChildren(car);
		}
		m_aCars.Clear();
		m_aCarSpots.Clear();
		ClearMilitary();
		Print(string.Format("[AROR TOWN] %1 town life retired", m_Location.GetId()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Road points through the streets and on the roads into town, found once (bounded road queries)
	protected void EnsurePoints()
	{
		if (m_bPointsReady)
			return;
		m_bPointsReady = true;
		vector center = m_Location.GetCenter();
		float radius = m_Location.GetRadius();
		for (int sample = 0; sample < STREET_SAMPLES; sample++)
		{
			float angle = sample * Math.PI2 / STREET_SAMPLES;
			float distance = radius * (0.2 + 0.25 * Math.Mod(sample, 3));
			vector desired = center + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * distance;
			vector road;
			if (AROR_ForceSpawner.FindRoadPosition(desired, desired, road) && vector.DistanceXZ(road, center) <= radius && IsSeparated(road, m_aStreets, 30))
				m_aStreets.Insert(road);
		}
		for (int entry = 0; entry < ENTRY_SAMPLES; entry++)
		{
			float bearing = entry * Math.PI2 / ENTRY_SAMPLES;
			vector edge = center + Vector(Math.Sin(bearing), 0, Math.Cos(bearing)) * radius * 0.95;
			vector found;
			if (!AROR_ForceSpawner.FindRoadPosition(edge, edge, found))
				continue;
			float reach = vector.DistanceXZ(found, center);
			if (reach >= radius * 0.7 && reach <= radius * 1.25 && IsSeparated(found, m_aEntries, 120))
			{
				m_aEntries.Insert(found);
				m_aCheckpointLostAt.Insert(-1);
			}
		}
		if (m_aStreets.IsEmpty())
			m_aStreets.Insert(center);
	}

	protected static bool IsSeparated(vector point, notnull array<vector> points, float spacing)
	{
		foreach (vector other : points)
		{
			if (vector.DistanceXZ(point, other) < spacing)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlayerWithin(vector position, float distance)
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (player && vector.DistanceXZ(player.GetOrigin(), position) <= distance)
				return true;
		}
		return false;
	}

	//! A clear spot near a random street point that no player is standing next to
	protected bool FindSpawnSpot(float clearance, out vector spot)
	{
		for (int attempt = 0; attempt < 3; attempt++)
		{
			vector street = m_aStreets.GetRandomElement();
			vector found;
			if (!AROR_ForceSpawner.FindClearPosition(street, 15, clearance, found))
				continue;
			if (PlayerWithin(found, m_fSpawnPlayerClearance))
				continue;
			spot = found;
			return true;
		}
		return false;
	}

	protected int ScaledCount(float perResident, int minimum, int maximum)
	{
		int population;
		if (m_Settlement)
			population = m_Settlement.GetPopulation();
		return Math.Clamp(Math.Round(population * perResident), minimum, Math.Max(minimum, maximum));
	}

	//------------------------------------------------------------------------------------------------
	//! One civilian spawns per tick until the town has its share; each wanders between street points
	protected void UpdateCivilians()
	{
		for (int index = m_aCivilians.Count() - 1; index >= 0; index--)
		{
			AROR_TownCivilian civilian = m_aCivilians[index];
			civilian.Observe_S();
			bool dead = civilian.m_Member && !AROR_ForceSpawner.IsAlive(civilian.m_Member);
			if (civilian.m_Watch && civilian.m_Watch.IsDead())
				dead = true;
			if (dead && civilian.m_Watch && civilian.m_Watch.WasPlayerKill(KILL_CREDIT_SECONDS))
				OnCivilianKilled(civilian.m_Watch.m_iPlayerId);
			// Gone: killed, deleted, or never spawned in time
			bool unspawned = !civilian.m_Member && m_iClock - civilian.m_iSpawnedAt > CIVILIAN_SPAWN_DEADLINE;
			if (dead || !civilian.m_Group || unspawned)
			{
				// A body lies where it fell until the town retires
				if (dead && civilian.m_Member)
					m_aLeftovers.Insert(civilian.m_Member);
				civilian.Remove_S(false);
				m_aCivilians.Remove(index);
				continue;
			}
			if (m_iClock >= civilian.m_iNextOrderAt)
			{
				vector street = m_aStreets.GetRandomElement();
				vector goal = street + Vector(Math.RandomFloat(-12, 12), 0, Math.RandomFloat(-12, 12));
				civilian.Order_S(m_sMoveWaypointPrefab, goal);
				civilian.m_iNextOrderAt = m_iClock + Math.RandomInt(45, 120);
			}
		}
		if (!m_bCivilians || m_aCivilians.Count() >= ScaledCount(m_fCiviliansPerResident, m_iMinCivilians, m_iMaxCivilians))
			return;
		vector spot;
		if (!FindSpawnSpot(1, spot))
			return;
		array<AROR_EUnitType> units = {AROR_EUnitType.CIVILIAN};
		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, AROR_EFactionRole.CIVILIANS, units, spot);
		if (!group)
			return;
		AROR_TownCivilian added = new AROR_TownCivilian();
		added.m_Group = group;
		added.m_iSpawnedAt = m_iClock;
		added.m_iNextOrderAt = m_iClock + Math.RandomInt(4, 20);
		m_aCivilians.Insert(added);
	}

	protected void OnCivilianKilled(int playerId)
	{
		if (m_Settlement && m_fCivilianKillSupport > 0)
			m_Settlement.AddSupport_S(AROR_EFactionRole.RESISTANCE, -m_fCivilianKillSupport);
		Print(string.Format("[AROR TOWN] %1 civilian killed by player %2", m_Location.GetId(), playerId), LogLevel.NORMAL);
		AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("A civilian was killed. Support for the Resistance in %1 falls.", m_Location.GetName()));
	}

	//------------------------------------------------------------------------------------------------
	//! One parked car per tick until the town has its share
	protected void UpdateCars()
	{
		if (!m_bParkedCars || m_aCars.Count() >= ScaledCount(m_fCarsPerResident, m_iMinCars, m_iMaxCars))
			return;
		vector spot;
		if (!FindSpawnSpot(3, spot))
			return;
		ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(AROR_EFactionRole.CIVILIANS, AROR_EVehicleType.MOTOR_CAR);
		IEntity car = AROR_ForceSpawner.SpawnVehicle(AROR_EFactionRole.CIVILIANS, prefab, spot, Math.RandomFloat(0, 360));
		if (!car)
		{
			// Keep the count moving so a missing preset never retries every tick
			m_aCars.Insert(null);
			m_aCarSpots.Insert(spot);
			return;
		}
		m_aCars.Insert(car);
		m_aCarSpots.Insert(spot);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateMilitary()
	{
		AROR_EFactionRole role = m_Location.GetControllingRole();
		if (role != m_eMilitaryRole)
		{
			// A new holder: the old army's checkpoints and patrols are no longer this town's (their men stay in the
			// world and fight on); the new holder builds its own
			ForgetMilitary();
			m_eMilitaryRole = role;
		}
		if (!AROR_GarrisonPolicy.IsEnemyRole(role))
			return;
		UpdateCheckpoints(role);
		UpdatePatrols(role);
	}

	protected int CrackdownLevel(AROR_EFactionRole role)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return 1;
		return Math.Clamp(operations.GetCrackdownLevel(role), 1, 5);
	}

	protected int ReadinessLevel(AROR_EFactionRole role)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return 1;
		return Math.Clamp(operations.GetReadinessLevel(role), 1, 5);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCheckpoints(AROR_EFactionRole role)
	{
		for (int index = m_aCheckpoints.Count() - 1; index >= 0; index--)
		{
			AROR_TownCheckpoint checkpoint = m_aCheckpoints[index];
			array<IEntity> living = {};
			int alive = AROR_ForceSpawner.CountLiving(checkpoint.m_Group, living);
			if (checkpoint.m_bManned && alive == 0)
			{
				m_aCheckpointLostAt[checkpoint.m_iEntry] = m_iClock;
				Print(string.Format("[AROR TOWN] %1 checkpoint %2 wiped out", m_Location.GetId(), checkpoint.m_iEntry), LogLevel.NORMAL);
				// The sandbags stay where the fight was until the town retires
				m_aLeftovers.InsertAll(checkpoint.m_aProps);
				checkpoint.m_aProps.Clear();
				checkpoint.Remove_S();
				m_aCheckpoints.Remove(index);
				continue;
			}
			// Manned once every guard is in, or once some are and the rest are overdue
			if (!checkpoint.m_bManned && (alive >= checkpoint.m_iSize || (alive > 0 && m_iClock - checkpoint.m_iRaisedAt > 60)))
				checkpoint.m_bManned = true;
			if (!checkpoint.m_bManned && alive == 0 && m_iClock - checkpoint.m_iRaisedAt > 90)
			{
				// Nobody ever spawned: drop it without a respawn timer and try again later
				checkpoint.Remove_S();
				m_aCheckpoints.Remove(index);
			}
		}
		if (!m_bCheckpoints)
			return;
		int wanted = 1;
		int crackdown = CrackdownLevel(role);
		if (crackdown >= 3)
			wanted++;
		if (crackdown >= 5)
			wanted++;
		if (m_Location.GetLocationType() == AROR_ELocationType.CITY)
			wanted++;
		if (m_aCheckpoints.Count() >= Math.Min(wanted, m_aEntries.Count()))
			return;
		// One new checkpoint per tick, at the first entry road without one that is not on its respawn timer
		foreach (int entry, vector point : m_aEntries)
		{
			if (HasCheckpoint(entry) || PlayerWithin(point, m_fSpawnPlayerClearance))
				continue;
			int lost = m_aCheckpointLostAt[entry];
			if (lost >= 0 && m_iClock - lost < m_iMilitaryRespawnSeconds)
				continue;
			SpawnCheckpoint(role, entry, point);
			return;
		}
	}

	protected bool HasCheckpoint(int entry)
	{
		foreach (AROR_TownCheckpoint checkpoint : m_aCheckpoints)
		{
			if (checkpoint.m_iEntry == entry)
				return true;
		}
		return false;
	}

	protected void SpawnCheckpoint(AROR_EFactionRole role, int entry, vector point)
	{
		vector outward = point - m_Location.GetCenter();
		outward[1] = 0;
		if (outward.Length() < 1)
			outward = "0 0 1";
		outward.Normalize();
		vector right = Vector(outward[2], 0, -outward[0]);
		float yaw = Math.Atan2(outward[0], outward[2]) * Math.RAD2DEG;
		AROR_TownCheckpoint checkpoint = new AROR_TownCheckpoint();
		checkpoint.m_iEntry = entry;
		checkpoint.m_iRaisedAt = m_iClock;
		array<ResourceName> props = {};
		if (m_aCheckpointProps && !m_aCheckpointProps.IsEmpty())
			props.InsertAll(m_aCheckpointProps);
		else
		{
			props.Insert(DEFAULT_CHECKPOINT_PROP);
			props.Insert(DEFAULT_CHECKPOINT_PROP);
		}
		foreach (int index, ResourceName prop : props)
		{
			float side = 5;
			if (Math.Mod(index, 2) == 1)
				side = -5;
			IEntity placed = AROR_ForceSpawner.SpawnPrefab(prop, point + right * side + outward * (index / 2) * 3, yaw);
			if (placed)
				checkpoint.m_aProps.Insert(placed);
		}
		array<AROR_EUnitType> units = {};
		AROR_Formations.GetUnits(AROR_EFormation.RIFLE_FIRE_TEAM, units);
		array<AROR_EUnitType> guards = {};
		for (int guard = 0; guard < m_iCheckpointGuards && !units.IsEmpty(); guard++)
			guards.Insert(units[guard % units.Count()]);
		vector stand = point - outward * 8;
		vector clear;
		if (AROR_ForceSpawner.FindClearPosition(stand, 10, 1, clear))
			stand = clear;
		checkpoint.m_Group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, role, guards, stand);
		checkpoint.m_iSize = guards.Count();
		if (checkpoint.m_Group)
			checkpoint.m_Waypoint = AROR_ForceSpawner.AddWaypoint(checkpoint.m_Group, m_sDefendWaypointPrefab, point, 15);
		m_aCheckpoints.Insert(checkpoint);
		Print(string.Format("[AROR TOWN] %1 checkpoint %2 raised for role %3 at %4", m_Location.GetId(), entry, role, point), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdatePatrols(AROR_EFactionRole role)
	{
		for (int index = m_aPatrols.Count() - 1; index >= 0; index--)
		{
			AROR_TownPatrol patrol = m_aPatrols[index];
			array<IEntity> crew = {};
			int alive = AROR_ForceSpawner.CountLiving(patrol.m_Group, crew);
			if (AROR_ForceSpawner.IsDestroyed(patrol.m_Vehicle) || (patrol.m_bSeated && alive == 0))
			{
				m_aPatrolLostAt.Insert(m_iClock);
				Print(string.Format("[AROR TOWN] %1 patrol lost", m_Location.GetId()), LogLevel.NORMAL);
				m_aPatrols.Remove(index);
				continue;
			}
			if (!patrol.m_bSeated)
			{
				// Seat whoever has spawned once the rest are overdue
				int expected = patrol.m_iSize;
				if (m_iClock - patrol.m_iRaisedAt > 30)
					expected = Math.Max(alive, 1);
				if (AROR_ForceSpawner.SeatGroup(patrol.m_Group, patrol.m_Vehicle, expected))
				{
					patrol.m_bSeated = true;
					patrol.m_iNextOrderAt = m_iClock;
				}
				else if (m_iClock - patrol.m_iRaisedAt > PATROL_SEAT_DEADLINE)
				{
					Print(string.Format("[AROR TOWN] %1 patrol could not board; removed", m_Location.GetId()), LogLevel.WARNING);
					patrol.Remove_S();
					m_aPatrols.Remove(index);
					m_aPatrolLostAt.Insert(m_iClock);
				}
				continue;
			}
			// Drive a loop: town streets and the roads in, next point on arrival or after two minutes
			bool arrived = patrol.m_bHasGoal && vector.DistanceXZ(patrol.m_Vehicle.GetOrigin(), patrol.m_vGoal) < 35;
			if (m_iClock >= patrol.m_iNextOrderAt || arrived)
			{
				vector goal = NextPatrolPoint(patrol);
				if (patrol.m_Waypoint)
				{
					patrol.m_Group.RemoveWaypoint(patrol.m_Waypoint);
					SCR_EntityHelper.DeleteEntityAndChildren(patrol.m_Waypoint);
				}
				patrol.m_Waypoint = AROR_ForceSpawner.AddWaypoint(patrol.m_Group, m_sMoveWaypointPrefab, goal, 25);
				patrol.m_vGoal = goal;
				patrol.m_bHasGoal = true;
				patrol.m_iNextOrderAt = m_iClock + 120;
			}
		}
		// Forget losses older than the respawn time
		for (int lost = m_aPatrolLostAt.Count() - 1; lost >= 0; lost--)
		{
			if (m_iClock - m_aPatrolLostAt[lost] >= m_iMilitaryRespawnSeconds)
				m_aPatrolLostAt.Remove(lost);
		}
		if (!m_bPatrol)
			return;
		int wanted = 1;
		if (CrackdownLevel(role) >= 4)
			wanted = 2;
		if (m_aPatrols.Count() + m_aPatrolLostAt.Count() >= wanted)
			return;
		SpawnPatrol(role);
	}

	protected vector NextPatrolPoint(AROR_TownPatrol patrol)
	{
		int total = m_aStreets.Count() + m_aEntries.Count();
		if (total < 1)
			total = 1;
		patrol.m_iNextPoint = (patrol.m_iNextPoint + 1 + Math.RandomInt(0, 2)) % total;
		if (patrol.m_iNextPoint < m_aStreets.Count())
			return m_aStreets[patrol.m_iNextPoint];
		return m_aEntries[patrol.m_iNextPoint - m_aStreets.Count()];
	}

	//! Readiness picks the kit: armed car at 1-2, APC at 3-4, IFV at 5, each falling back to lighter types
	protected ResourceName PatrolVehicle(AROR_EFactionRole role)
	{
		array<AROR_EVehicleType> types = {};
		int readiness = ReadinessLevel(role);
		if (readiness >= 5)
			types.Insert(AROR_EVehicleType.IFV);
		if (readiness >= 3)
			types.Insert(AROR_EVehicleType.APC);
		types.Insert(AROR_EVehicleType.ARMED_CAR);
		types.Insert(AROR_EVehicleType.MOTOR_CAR);
		foreach (AROR_EVehicleType type : types)
		{
			ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(role, type);
			if (!prefab.IsEmpty())
				return prefab;
		}
		return string.Empty;
	}

	protected void SpawnPatrol(AROR_EFactionRole role)
	{
		vector start;
		bool found;
		foreach (vector street : m_aStreets)
		{
			if (!PlayerWithin(street, m_fSpawnPlayerClearance * 2))
			{
				start = street;
				found = true;
				break;
			}
		}
		ResourceName prefab = PatrolVehicle(role);
		if (!found || prefab.IsEmpty())
			return;
		IEntity vehicle = AROR_ForceSpawner.SpawnVehicle(role, prefab, start, Math.RandomFloat(0, 360));
		if (!vehicle)
			return;
		array<AROR_EUnitType> members = {};
		for (int seat = 0; seat < CrewSeats(vehicle); seat++)
			members.Insert(AROR_EUnitType.CREW);
		array<AROR_EUnitType> riflemen = {};
		AROR_Formations.GetUnits(AROR_EFormation.RIFLE_FIRE_TEAM, riflemen);
		for (int rider = 0; rider < Math.Min(m_iPatrolRiders, CargoSeats(vehicle)) && !riflemen.IsEmpty(); rider++)
			members.Insert(riflemen[rider % riflemen.Count()]);
		vector side = start;
		vector clear;
		if (AROR_ForceSpawner.FindClearPosition(start, 12, 1, clear))
			side = clear;
		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, role, members, side);
		if (!group)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return;
		}
		AROR_TownPatrol patrol = new AROR_TownPatrol();
		patrol.m_Vehicle = vehicle;
		patrol.m_Group = group;
		patrol.m_iSize = members.Count();
		patrol.m_iRaisedAt = m_iClock;
		patrol.m_iNextPoint = Math.RandomInt(0, Math.Max(m_aStreets.Count(), 1));
		m_aPatrols.Insert(patrol);
		Print(string.Format("[AROR TOWN] %1 patrol raised for role %2: %3 with %4 aboard", m_Location.GetId(), role, prefab, members.Count()), LogLevel.NORMAL);
	}

	//! Driver and turret seats
	protected static int CrewSeats(IEntity vehicle)
	{
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return 1;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		int seats = 1;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && TurretCompartmentSlot.Cast(slot) && slot.IsCompartmentAccessible())
				seats++;
		}
		return seats;
	}

	protected static int CargoSeats(IEntity vehicle)
	{
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return 0;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		int seats;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && CargoCompartmentSlot.Cast(slot) && slot.IsCompartmentAccessible())
				seats++;
		}
		return seats;
	}

	//------------------------------------------------------------------------------------------------
	//! Retirement: delete the checkpoints and patrols built here (no player is near)
	protected void ClearMilitary()
	{
		foreach (AROR_TownCheckpoint checkpoint : m_aCheckpoints)
			checkpoint.Remove_S();
		m_aCheckpoints.Clear();
		foreach (AROR_TownPatrol patrol : m_aPatrols)
			patrol.Remove_S();
		m_aPatrols.Clear();
		foreach (IEntity leftover : m_aLeftovers)
		{
			if (leftover)
				SCR_EntityHelper.DeleteEntityAndChildren(leftover);
		}
		m_aLeftovers.Clear();
	}

	//! Control changed while players are here: stop managing the old holder's forces without deleting them
	protected void ForgetMilitary()
	{
		m_aCheckpoints.Clear();
		m_aPatrols.Clear();
		m_aPatrolLostAt.Clear();
		for (int entry = 0; entry < m_aCheckpointLostAt.Count(); entry++)
			m_aCheckpointLostAt[entry] = -1;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Location = AROR_LocationComponent.Cast(owner.FindComponent(AROR_LocationComponent));
		m_Settlement = AROR_SettlementComponent.Cast(owner.FindComponent(AROR_SettlementComponent));
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;
		if (!m_Location)
		{
			Print("[AROR TOWN] Town life needs a location component; disabled", LogLevel.ERROR);
			return;
		}
		m_eMilitaryRole = m_Location.GetControllingRole();
		GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Tick);
		if (Replication.IsServer() && m_bActive)
			Deactivate();
		super.OnDelete(owner);
	}
}

//! One civilian (in a group of their own) and the damage watch that tells a player kill
class AROR_TownCivilian
{
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	ref AROR_TownCasualtyWatch m_Watch;
	IEntity m_Member;
	int m_iSpawnedAt;
	int m_iNextOrderAt;

	//! Attach the watch once the member has spawned
	void Observe_S()
	{
		if (m_Member || !m_Group)
			return;
		array<IEntity> living = {};
		if (AROR_ForceSpawner.CountLiving(m_Group, living) == 0)
			return;
		m_Member = living[0];
		m_Watch = new AROR_TownCasualtyWatch();
		if (!m_Watch.Attach_S(m_Member))
			m_Watch = null;
	}

	void Order_S(ResourceName prefab, vector goal)
	{
		if (!m_Group)
			return;
		if (m_Waypoint)
		{
			m_Group.RemoveWaypoint(m_Waypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		}
		m_Waypoint = AROR_ForceSpawner.AddWaypoint(m_Group, prefab, goal, 4);
	}

	//! Deletes the group (its characters go with it) unless only the body is left to lie where it fell
	void Remove_S(bool deleteBody = true)
	{
		if (m_Watch)
			m_Watch.Detach_S();
		m_Watch = null;
		if (m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		m_Waypoint = null;
		array<IEntity> living = {};
		AROR_ForceSpawner.CountLiving(m_Group, living);
		foreach (IEntity member : living)
			SCR_EntityHelper.DeleteEntityAndChildren(member);
		if (m_Member && deleteBody)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Member);
		if (m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
		m_Group = null;
		m_Member = null;
	}
}

//! A road checkpoint: props either side of the road and a guard group holding it
class AROR_TownCheckpoint
{
	int m_iEntry;
	int m_iSize;
	int m_iRaisedAt;
	bool m_bManned;	//!< The guards have spawned; from then on no living guard means wiped out
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	ref array<IEntity> m_aProps = {};

	void Remove_S()
	{
		array<IEntity> living = {};
		AROR_ForceSpawner.CountLiving(m_Group, living);
		foreach (IEntity member : living)
			SCR_EntityHelper.DeleteEntityAndChildren(member);
		if (m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
		if (m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		foreach (IEntity prop : m_aProps)
		{
			if (prop)
				SCR_EntityHelper.DeleteEntityAndChildren(prop);
		}
		m_aProps.Clear();
		m_Group = null;
	}
}

//! A crewed vehicle driving a loop of the town's road points
class AROR_TownPatrol
{
	IEntity m_Vehicle;
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	int m_iSize;
	int m_iRaisedAt;
	int m_iNextPoint;
	int m_iNextOrderAt;
	bool m_bSeated;
	bool m_bHasGoal;
	vector m_vGoal;

	void Remove_S()
	{
		array<IEntity> living = {};
		AROR_ForceSpawner.CountLiving(m_Group, living);
		foreach (IEntity member : living)
			SCR_EntityHelper.DeleteEntityAndChildren(member);
		if (m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
		if (m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		if (m_Vehicle && !AROR_ForceSpawner.IsDestroyed(m_Vehicle))
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
		m_Group = null;
		m_Vehicle = null;
	}
}
