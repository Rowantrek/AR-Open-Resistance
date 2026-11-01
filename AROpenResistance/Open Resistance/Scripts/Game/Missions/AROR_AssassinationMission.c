//! Officer Assassination: a senior enemy officer visits one of its locations with two bodyguards and a security
//! detail. Kill him before the time runs out (he then leaves).
class AROR_AssassinationMission : AROR_Mission
{
	protected static const ref array<AROR_ELocationType> SITE_TYPES = {AROR_ELocationType.TOWN, AROR_ELocationType.CITY, AROR_ELocationType.OUTPOST, AROR_ELocationType.MILITARY_BASE, AROR_ELocationType.FACTORY, AROR_ELocationType.PORT, AROR_ELocationType.HELIPORT, AROR_ELocationType.AIRPORT};

	protected SCR_AIGroup m_Officer;
	protected bool m_bOfficerSeen;

	//------------------------------------------------------------------------------------------------
	override bool Plan_S(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent site = board.PickEnemyLocation_S(SITE_TYPES);
		if (!site)
			return false;

		m_eEnemyRole = site.GetControllingRole();
		if (!board.HasUnit_S(m_eEnemyRole, AROR_EUnitType.SQUADLEADER))
			return false;

		m_sLocationId = site.GetId();
		m_vPosition = site.GetCenter();
		m_fAreaRadius = 70;
		m_iRisk = Math.Max(RiskOf(site), RISK_MODERATE);
		ApplySettings(board.GetSettings(m_eType));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool Deploy_S(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent site = board.GetLocation(m_sLocationId);
		if (!site)
			return false;

		vector spot;
		if (!board.FindClearPosition_S(site.GetCenter(), 40, 2, spot))
			return false;

		// The officer is alone in his group, so his death is unambiguous
		array<AROR_EUnitType> officer = {AROR_EUnitType.SQUADLEADER};
		m_Officer = AddGroup_S(board, m_eEnemyRole, officer, spot);
		if (!m_Officer)
			return false;

		board.AddWaypoint_S(m_Officer, true, spot, 8, m_aEntities);

		array<AROR_EUnitType> bodyguards = {AROR_EUnitType.RIFLEMAN, AROR_EUnitType.RIFLEMAN};
		SCR_AIGroup close = AddGroup_S(board, m_eEnemyRole, bodyguards, spot);
		if (close)
			board.AddWaypoint_S(close, true, spot, 12, m_aEntities);

		SpawnGuards_S(board, spot, m_iGuards, 45);

		m_vPosition = Vector(spot[0], 0, spot[2]);
		m_fAreaRadius = 50;
		m_sProgress = "Locate and eliminate the officer at " + GetLocationName(board);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override AROR_EMissionResult Update_S(AROR_MissionBoardComponent board)
	{
		UpdateGroups_S(board, m_eEnemyRole);

		array<IEntity> officer = {};
		int alive = AROR_MissionBoardComponent.CountLiving(m_Officer, officer);
		if (alive > 0)
			m_bOfficerSeen = true;

		if (m_bOfficerSeen && alive == 0)
		{
			m_sOutcome = "officer eliminated";
			return AROR_EMissionResult.SUCCEEDED;
		}

		return AROR_EMissionResult.ONGOING;
	}

	//------------------------------------------------------------------------------------------------
	override string GetStartProgress(AROR_MissionBoardComponent board)
	{
		return "Head to " + GetLocationName(board) + " and find the officer";
	}

	//------------------------------------------------------------------------------------------------
	override string GetTimeoutOutcome()
	{
		return "the officer left the area";
	}
}
