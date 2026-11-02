//! Supply Sabotage: two loaded enemy supply trucks parked under guard at a military site. Destroy both.
class AROR_SabotageMission : AROR_Mission
{
	protected static const ref array<AROR_ELocationType> SITE_TYPES = {AROR_ELocationType.OUTPOST, AROR_ELocationType.MILITARY_BASE, AROR_ELocationType.FACTORY, AROR_ELocationType.RESOURCES, AROR_ELocationType.PORT, AROR_ELocationType.HELIPORT, AROR_ELocationType.AIRPORT};

	protected static const int TARGETS = 2;

	protected ref array<IEntity> m_aTargets = {};

	//------------------------------------------------------------------------------------------------
	override bool Plan_S(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent site = board.PickEnemyLocation_S(SITE_TYPES);
		if (!site)
			return false;

		m_eEnemyRole = site.GetControllingRole();
		ResourceName truck = board.GetVehiclePrefab_S(m_eEnemyRole, AROR_EVehicleType.SUPPLY_TRUCK);
		if (truck.IsEmpty())
			return false;

		m_sLocationId = site.GetId();
		m_vPosition = site.GetCenter();
		m_fAreaRadius = 60;
		m_iRisk = RiskOf(site);
		ApplySettings(board.GetSettings(m_eType));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool Deploy_S(AROR_MissionBoardComponent board)
	{
		m_aTargets.Clear();
		AROR_LocationComponent site = board.GetLocation(m_sLocationId);
		if (!site)
			return false;

		vector first;
		if (!board.FindClearPosition_S(site.GetCenter(), 45, 6, first))
			return false;

		float yaw = Math.RandomFloat(0, 360);
		for (int i = 0; i < TARGETS; i++)
		{
			vector spot = first;
			if (i > 0)
			{
				// Parked side by side, a few metres apart
				vector found;
				bool placed;
				for (int attempt = 0; attempt < 5; attempt++)
				{
					if (board.FindClearPosition_S(first, 25, 6, found) && vector.DistanceXZ(found, first) >= 9)
					{
						spot = found;
						placed = true;
						break;
					}
				}

				if (!placed)
					return false;
			}

			ResourceName prefab = board.GetVehiclePrefab_S(m_eEnemyRole, AROR_EVehicleType.SUPPLY_TRUCK);
			IEntity truck = board.SpawnVehicle_S(m_eEnemyRole, prefab, spot, yaw);
			if (!truck)
				return false;

			m_aEntities.Insert(truck);
			m_aTargets.Insert(truck);
		}

		SpawnGuards_S(board, first, m_iGuards, 35);
		m_vPosition = Vector(first[0], 0, first[2]);
		m_fAreaRadius = 30;
		m_sProgress = string.Format("Destroy the supply trucks at %1 (0 of %2)", GetLocationName(board), TARGETS);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override AROR_EMissionResult Update_S(AROR_MissionBoardComponent board)
	{
		UpdateGroups_S(board, m_eEnemyRole);

		int destroyed;
		foreach (IEntity target : m_aTargets)
		{
			if (AROR_MissionBoardComponent.IsDestroyed(target))
				destroyed++;
		}

		if (destroyed != m_iPhase)
		{
			m_iPhase = destroyed;
			m_sProgress = string.Format("Destroy the supply trucks at %1 (%2 of %3)", GetLocationName(board), destroyed, TARGETS);
			if (destroyed < TARGETS)
				board.AddLog(string.Format("%1: truck %2 of %3 destroyed", m_sCodename, destroyed, TARGETS));
		}

		if (destroyed >= TARGETS)
		{
			m_sOutcome = "supply trucks destroyed";
			return AROR_EMissionResult.SUCCEEDED;
		}

		return AROR_EMissionResult.ONGOING;
	}

	//------------------------------------------------------------------------------------------------
	override string GetStartProgress(AROR_MissionBoardComponent board)
	{
		return "Head to " + GetLocationName(board) + " and find the supply trucks";
	}

	//------------------------------------------------------------------------------------------------
	override string GetTimeoutOutcome()
	{
		return "the trucks moved out";
	}
}
