//! Supply Theft: a guarded enemy supply truck parked at a military site. Seize it and drive it into a
//! Resistance-held location; the Resistance keeps the truck.
class AROR_SupplyTheftMission : AROR_Mission
{
	protected static const ref array<AROR_ELocationType> SITE_TYPES = {AROR_ELocationType.OUTPOST, AROR_ELocationType.MILITARY_BASE, AROR_ELocationType.FACTORY, AROR_ELocationType.RESOURCES, AROR_ELocationType.PORT, AROR_ELocationType.HELIPORT, AROR_ELocationType.AIRPORT};

	//! The truck counts as seized once it is this far (m) from where it was parked
	protected static const float SEIZED_DISTANCE = 40;

	protected IEntity m_Truck;
	protected vector m_vTruckStart;

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
		AROR_LocationComponent site = board.GetLocation(m_sLocationId);
		if (!site)
			return false;

		ResourceName prefab = board.GetVehiclePrefab_S(m_eEnemyRole, AROR_EVehicleType.SUPPLY_TRUCK);
		vector spot;
		if (!board.FindClearPosition_S(site.GetCenter(), 45, 6, spot))
			return false;

		m_Truck = board.SpawnVehicle_S(m_eEnemyRole, prefab, spot, Math.RandomFloat(0, 360));
		if (!m_Truck)
			return false;

		m_aEntities.Insert(m_Truck);
		m_vTruckStart = m_Truck.GetOrigin();
		m_vPosition = Vector(m_vTruckStart[0], 0, m_vTruckStart[2]);
		m_fAreaRadius = 30;
		SpawnGuards_S(board, spot, m_iGuards, 30);
		m_sProgress = "Seize the supply truck at " + GetLocationName(board);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override AROR_EMissionResult Update_S(AROR_MissionBoardComponent board)
	{
		UpdateGroups_S(board, m_eEnemyRole);
		if (AROR_MissionBoardComponent.IsDestroyed(m_Truck))
		{
			m_sOutcome = "the truck was destroyed";
			return AROR_EMissionResult.FAILED;
		}

		vector truck = m_Truck.GetOrigin();
		if (m_iPhase == 0 && vector.DistanceXZ(truck, m_vTruckStart) > SEIZED_DISTANCE)
		{
			m_iPhase = 1;
			m_fAreaRadius = 0;
			m_sProgress = "Truck seized: drive it into a Resistance-held location";
			board.AddLog(m_sCodename + ": truck seized");
		}

		if (m_iPhase == 1)
		{
			TrackMarker(truck, 150);
			if (board.IsInFriendlyLocation_S(truck))
				return AROR_EMissionResult.SUCCEEDED;
		}

		return AROR_EMissionResult.ONGOING;
	}

	//------------------------------------------------------------------------------------------------
	override string GetStartProgress(AROR_MissionBoardComponent board)
	{
		return "Head to " + GetLocationName(board) + " and find the truck";
	}

	//------------------------------------------------------------------------------------------------
	override string GetTimeoutOutcome()
	{
		return "the truck was not delivered in time";
	}

	//------------------------------------------------------------------------------------------------
	//! A delivered truck stays with the Resistance
	override void Release_S(AROR_MissionBoardComponent board, bool succeeded)
	{
		if (succeeded)
			m_aEntities.RemoveItem(m_Truck);

		super.Release_S(board, succeeded);
	}
}
