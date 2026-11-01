//! Convoy Ambush: an enemy convoy (armed escort and a cargo truck) drives by road from one of its sites to another.
//! Destroy the vehicles or kill their crews before it arrives. Deploys as soon as it is accepted; the map marker
//! follows the convoy's last known position.
class AROR_ConvoyAmbushMission : AROR_Mission
{
	protected static const ref array<AROR_ELocationType> ORIGIN_TYPES = {AROR_ELocationType.OUTPOST, AROR_ELocationType.MILITARY_BASE, AROR_ELocationType.FACTORY, AROR_ELocationType.RESOURCES, AROR_ELocationType.PORT, AROR_ELocationType.HELIPORT, AROR_ELocationType.AIRPORT};

	protected static const float MIN_ROUTE = 1500;
	protected static const float MAX_ROUTE = 5000;
	protected static const float ARRIVAL_RADIUS = 120;
	protected static const int MARKER_INTERVAL = 30;	//!< Seconds between last-known-position updates
	protected static const int MAX_PASSENGERS = 4;

	protected ref array<IEntity> m_aVehicles = {};
	protected ref array<SCR_AIGroup> m_aCrews = {};
	protected ref array<int> m_aCrewSizes = {};
	protected ref array<IEntity> m_aObservedCrew = {};
	protected ref array<int> m_aSpawnedCrewCounts = {};
	protected int m_iMarkerSeconds;

	//------------------------------------------------------------------------------------------------
	override bool Plan_S(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent origin = board.PickEnemyLocation_S(ORIGIN_TYPES);
		if (!origin)
			return false;

		m_eEnemyRole = origin.GetControllingRole();
		ResourceName truck = board.GetVehiclePrefab_S(m_eEnemyRole, AROR_EVehicleType.SUPPLY_TRUCK);
		if (truck.IsEmpty())
			return false;

		array<AROR_LocationComponent> destinations = {};
		board.FindLocationsHeldBy_S(m_eEnemyRole, origin.GetCenter(), MIN_ROUTE, MAX_ROUTE, origin.GetId(), destinations);
		for (int i = destinations.Count() - 1; i >= 0; i--)
		{
			if (board.IsLocationInUse(destinations[i].GetId()))
				destinations.Remove(i);
		}

		if (destinations.IsEmpty())
			return false;

		AROR_LocationComponent destination = destinations.GetRandomElement();
		m_sLocationId = origin.GetId();
		m_vPosition = origin.GetCenter();
		m_sDestinationId = destination.GetId();
		m_vDestination = destination.GetCenter();
		m_fAreaRadius = 0;

		m_iRisk = RISK_MODERATE;
		ResourceName escort = board.GetVehiclePrefab_S(m_eEnemyRole, AROR_EVehicleType.ARMED_CAR);
		if (!escort.IsEmpty())
			m_iRisk = RISK_HIGH;

		ApplySettings(board.GetSettings(m_eType));
		m_iGuards = Math.Min(m_iGuards, MAX_PASSENGERS);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The convoy only needs its origin still held to set off
	override bool IsStillValid_S(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent origin = board.GetLocation(m_sLocationId);
		AROR_LocationComponent destination = board.GetLocation(m_sDestinationId);
		return origin && destination && origin.GetControllingRole() == m_eEnemyRole;
	}

	//------------------------------------------------------------------------------------------------
	override bool DeploysAtOnce()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool Deploy_S(AROR_MissionBoardComponent board)
	{
		m_aVehicles.Clear();
		m_aCrews.Clear();
		m_aCrewSizes.Clear();
		m_aObservedCrew.Clear();
		m_aSpawnedCrewCounts.Clear();
		AROR_LocationComponent origin = board.GetLocation(m_sLocationId);
		if (!origin)
			return false;

		vector start;
		if (!board.FindRoadPosition_S(origin.GetCenter(), origin.GetCenter(), start))
			return false;

		vector heading = m_vDestination - start;
		heading[1] = 0;
		heading.Normalize();
		float yaw = Math.Atan2(heading[0], heading[2]) * Math.RAD2DEG;

		// Escort in front (towards the destination), cargo truck behind
		vector escortSpot = start;
		vector found;
		if (board.FindRoadPosition_S(start + heading * 25, start + heading * 25, found))
			escortSpot = found;

		ResourceName escortPrefab = board.GetVehiclePrefab_S(m_eEnemyRole, AROR_EVehicleType.ARMED_CAR);
		if (escortPrefab.IsEmpty())
			escortPrefab = board.GetVehiclePrefab_S(m_eEnemyRole, AROR_EVehicleType.MOTOR_CAR);

		if (!escortPrefab.IsEmpty() && vector.DistanceXZ(escortSpot, start) > 12)
		{
			if (!AddVehicle_S(board, escortPrefab, escortSpot, yaw, 0))
				return false;
		}

		ResourceName truckPrefab = board.GetVehiclePrefab_S(m_eEnemyRole, AROR_EVehicleType.SUPPLY_TRUCK);
		if (!AddVehicle_S(board, truckPrefab, start, yaw, m_iGuards))
			return false;

		m_iMarkerSeconds = 0;
		m_sProgress = "Convoy forming up at " + GetLocationName(board);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One vehicle with a crew for its driver and turret seats, plus passengers
	protected bool AddVehicle_S(AROR_MissionBoardComponent board, ResourceName prefab, vector position, float yaw, int passengers)
	{
		IEntity vehicle = board.SpawnVehicle_S(m_eEnemyRole, prefab, position, yaw);
		if (!vehicle)
			return false;

		m_aEntities.Insert(vehicle);
		int crewSeats = Math.Max(CountCrewSeats(vehicle), 1);
		int cargoSeats = CountCargoSeats(vehicle);
		passengers = Math.Min(passengers, cargoSeats);

		array<AROR_EUnitType> members = {};
		for (int i = 0; i < crewSeats; i++)
		{
			members.Insert(AROR_EUnitType.CREW);
		}

		for (int p = 0; p < passengers; p++)
		{
			members.Insert(AROR_EUnitType.RIFLEMAN);
		}

		if (members.IsEmpty())
			return false;

		vector side = position;
		vector found;
		if (board.FindClearPosition_S(position, 12, 1, found))
			side = found;

		SCR_AIGroup group = AddGroup_S(board, m_eEnemyRole, members, side);
		if (!group)
			return false;

		m_aVehicles.Insert(vehicle);
		m_aCrews.Insert(group);
		m_aCrewSizes.Insert(group.m_aUnitPrefabSlots.Count());
		m_aSpawnedCrewCounts.Insert(0);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override AROR_EMissionResult Update_S(AROR_MissionBoardComponent board)
	{
		UpdateGroups_S(board, m_eEnemyRole);
		int seconds = board.GetUpdateInterval();

		// Observe materialized crew, including casualties. No living members does not
		// prove destruction while a remote group's members are still spawning.
		bool pendingCrew;
		bool survivingConvoy;
		bool seated = true;
		foreach (int i, IEntity vehicle : m_aVehicles)
		{
			if (AROR_MissionBoardComponent.IsDestroyed(vehicle))
				continue;

			array<AIAgent> agents = {};
			if (m_aCrews[i])
				m_aCrews[i].GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity member = agent.GetControlledEntity();
				if (member && !m_aObservedCrew.Contains(member))
				{
					m_aObservedCrew.Insert(member);
					m_aSpawnedCrewCounts[i] = m_aSpawnedCrewCounts[i] + 1;
				}
			}

			if (m_aSpawnedCrewCounts[i] < m_aCrewSizes[i])
				pendingCrew = true;

			array<IEntity> living = {};
			int alive = AROR_MissionBoardComponent.CountLiving(m_aCrews[i], living);
			if (alive == 0)
				continue;

			survivingConvoy = true;
			if (m_iPhase == 0 && !board.SeatGroup_S(m_aCrews[i], vehicle, alive))
				seated = false;
		}

		if (pendingCrew)
			return AROR_EMissionResult.ONGOING;

		if (!survivingConvoy)
		{
			m_sOutcome = "convoy destroyed";
			return AROR_EMissionResult.SUCCEEDED;
		}

		// Retry dropped requests until every surviving crew is actually seated.
		if (m_iPhase == 0)
		{
			if (!seated)
				return AROR_EMissionResult.ONGOING;

			vector goal = m_vDestination;
			vector found;
			if (board.FindRoadPosition_S(m_vDestination, m_vDestination, found))
				goal = found;

			foreach (SCR_AIGroup crew : m_aCrews)
			{
				board.AddWaypoint_S(crew, false, goal, 40, m_aEntities);
			}

			m_iPhase = 1;
			board.AddLog(m_sCodename + ": convoy has left " + GetLocationName(board));
		}

		// En route: stopped when every vehicle is wrecked or has lost its crew
		bool moving;
		IEntity lead;
		foreach (int v, IEntity vehicle : m_aVehicles)
		{
			if (AROR_MissionBoardComponent.IsDestroyed(vehicle))
				continue;

			array<IEntity> crew = {};
			if (AROR_MissionBoardComponent.CountLiving(m_aCrews[v], crew) == 0)
				continue;

			moving = true;
			if (!lead)
				lead = vehicle;

			if (vector.DistanceXZ(vehicle.GetOrigin(), m_vDestination) <= ARRIVAL_RADIUS)
			{
				m_sOutcome = "the convoy reached " + GetDestinationName(board);
				return AROR_EMissionResult.FAILED;
			}
		}

		if (!moving)
		{
			m_sOutcome = "convoy destroyed";
			return AROR_EMissionResult.SUCCEEDED;
		}

		m_iMarkerSeconds -= seconds;
		if (m_iMarkerSeconds <= 0)
		{
			m_iMarkerSeconds = MARKER_INTERVAL;
			vector position = lead.GetOrigin();
			TrackMarker(position, 50);
			int tenths = Math.Round(vector.DistanceXZ(position, m_vDestination) / 100);
			int whole = tenths / 10;
			int fraction = tenths - whole * 10;
			m_sProgress = string.Format("Convoy en route to %1, %2.%3 km out", GetDestinationName(board), whole, fraction);
		}

		return AROR_EMissionResult.ONGOING;
	}

	//------------------------------------------------------------------------------------------------
	override string GetStartProgress(AROR_MissionBoardComponent board)
	{
		return "Convoy preparing to leave " + GetLocationName(board);
	}

	//------------------------------------------------------------------------------------------------
	override string GetTimeoutOutcome()
	{
		if (m_iPhase == 0)
			return "the convoy could not set off before the deadline";

		return "the convoy slipped through";
	}

	//------------------------------------------------------------------------------------------------
	//! Driver and turret seats
	protected static int CountCrewSeats(IEntity vehicle)
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

	//------------------------------------------------------------------------------------------------
	protected static int CountCargoSeats(IEntity vehicle)
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
}
