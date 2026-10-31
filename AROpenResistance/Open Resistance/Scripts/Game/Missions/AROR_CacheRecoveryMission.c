//! Arms Cache Recovery: an old weapons crate hidden somewhere in a search area in the countryside near a town,
//! sometimes watched by a patrol. Find it and stay beside it to secure it. The crate stays to be looted.
class AROR_CacheRecoveryMission : AROR_Mission
{
	protected static const ref array<AROR_ELocationType> NEAR_TYPES = {AROR_ELocationType.TOWN, AROR_ELocationType.CITY, AROR_ELocationType.RESOURCES, AROR_ELocationType.FACTORY};

	protected static const float SEARCH_RADIUS = 150;
	protected static const float SECURE_DISTANCE = 4;
	protected static const int SECURE_SECONDS = 10;
	protected static const float PATROL_CHANCE = 0.6;

	protected IEntity m_Crate;
	protected int m_iSecured;		//!< Seconds a player has stayed beside the crate

	//------------------------------------------------------------------------------------------------
	override bool Plan_S(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent town = board.PickAnyLocation_S(NEAR_TYPES);
		if (!town)
			return false;

		// The search area lies out in the countryside, away from every location
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		BaseWorld world = GetGame().GetWorld();
		bool found;
		vector area;
		for (int attempt = 0; attempt < 12; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(300, 700) + town.GetRadius();
			area = town.GetCenter() + Vector(Math.Sin(angle) * distance, 0, Math.Cos(angle) * distance);
			if (world.GetSurfaceY(area[0], area[2]) <= world.GetOceanHeight(area[0], area[2]) + 2)
				continue;

			if (locations && locations.GetLocationAt(area))
				continue;

			found = true;
			break;
		}

		if (!found)
			return false;

		m_sLocationId = town.GetId();
		m_vPosition = Vector(area[0], 0, area[2]);
		m_fAreaRadius = SEARCH_RADIUS;
		m_eEnemyRole = NearestEnemyRole(m_vPosition);
		m_iRisk = RISK_LOW;
		bool patrol = Math.RandomFloat01() < PATROL_CHANCE;
		if (patrol)
			m_iRisk = RISK_MODERATE;

		ApplySettings(board.GetSettings(m_eType));
		if (!patrol)
			m_iGuards = 0;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The cache does not depend on who holds the town
	override bool IsStillValid_S(AROR_MissionBoardComponent board)
	{
		return board.GetLocation(m_sLocationId) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool Deploy_S(AROR_MissionBoardComponent board)
	{
		vector spot;
		if (!board.FindClearPosition_S(m_vPosition, SEARCH_RADIUS * 0.8, 1.5, spot))
			return false;

		m_Crate = board.SpawnPrefab_S(board.GetCachePrefab(), spot, Math.RandomFloat(0, 360));
		if (!m_Crate)
			return false;

		m_aEntities.Insert(m_Crate);
		if (m_iGuards > 0)
		{
			vector patrolSpot = spot;
			vector found;
			if (board.FindClearPosition_S(spot, 40, 1, found))
				patrolSpot = found;

			SpawnGuards_S(board, patrolSpot, m_iGuards, 60);
		}

		m_sProgress = string.Format("Search the marked area near %1 (%2 m radius)", GetLocationName(board), SEARCH_RADIUS);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override AROR_EMissionResult Update_S(AROR_MissionBoardComponent board)
	{
		UpdateGroups_S(board, m_eEnemyRole);
		if (!m_Crate)
		{
			m_sOutcome = "the cache was lost";
			return AROR_EMissionResult.FAILED;
		}

		if (!board.IsResistanceNear_S(m_Crate.GetOrigin(), SECURE_DISTANCE))
		{
			if (m_iSecured > 0)
			{
				m_iSecured = 0;
				m_iPhase = 0;
				m_sProgress = string.Format("Search the marked area near %1 (%2 m radius)", GetLocationName(board), SEARCH_RADIUS);
			}

			return AROR_EMissionResult.ONGOING;
		}

		m_iSecured += board.GetUpdateInterval();
		if (m_iPhase == 0)
		{
			m_iPhase = 1;
			m_sProgress = "Cache found: stay with it to secure it";
			board.AddLog(m_sCodename + ": cache found");
		}

		if (m_iSecured >= SECURE_SECONDS)
		{
			m_sOutcome = "cache secured";
			return AROR_EMissionResult.SUCCEEDED;
		}

		return AROR_EMissionResult.ONGOING;
	}

	//------------------------------------------------------------------------------------------------
	override void Release_S(AROR_MissionBoardComponent board, bool succeeded)
	{
		if (succeeded && m_Crate)
			m_aEntities.RemoveItem(m_Crate);

		super.Release_S(board, succeeded);
	}

	//------------------------------------------------------------------------------------------------
	override string GetStartProgress(AROR_MissionBoardComponent board)
	{
		return string.Format("Search the marked area near %1 (%2 m radius)", GetLocationName(board), SEARCH_RADIUS);
	}

	//------------------------------------------------------------------------------------------------
	override string GetTimeoutOutcome()
	{
		return "the trail went cold";
	}

	//------------------------------------------------------------------------------------------------
	//! Role of the nearest enemy-held location, whose patrols roam the countryside here
	protected static AROR_EFactionRole NearestEnemyRole(vector position)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return AROR_EFactionRole.OCCUPIERS;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		float nearest = float.MAX;
		AROR_EFactionRole role = AROR_EFactionRole.OCCUPIERS;
		foreach (AROR_LocationComponent location : all)
		{
			if (!location || !AROR_MissionBoardComponent.IsEnemyRole(location.GetControllingRole()))
				continue;

			float distance = vector.DistanceXZ(position, location.GetCenter());
			if (distance < nearest)
			{
				nearest = distance;
				role = location.GetControllingRole();
			}
		}

		return role;
	}
}
