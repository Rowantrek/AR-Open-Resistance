//! Tuning for one mission type on the OPS board (see ARCHITECTURE.md -> Missions). Listed on
//! AROR_MissionBoardComponent; types left out of the list use the built-in defaults (AROR_MissionType.CreateDefault).
[BaseContainerProps()]
class AROR_MissionType
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Mission type these settings apply to", enums: ParamEnumArray.FromEnum(AROR_EMissionType))]
	AROR_EMissionType m_eType;

	[Attribute("1", desc: "Offer this mission type on the board")]
	bool m_bEnabled;

	[Attribute("1200", desc: "Resistance funds paid on success, before the threat bonus", params: "0 inf")]
	int m_iFunds;

	[Attribute("2", desc: "Resistance manpower gained on success, before the threat bonus", params: "0 inf")]
	int m_iManpower;

	[Attribute("45", desc: "Minutes to complete the mission once accepted", params: "5 240 1")]
	int m_iTimeLimit;

	[Attribute("4", desc: "Enemy soldiers guarding the objective (escort crews for convoys)", params: "0 12 1")]
	int m_iGuards;

	//------------------------------------------------------------------------------------------------
	//! Built-in settings for a type (used when the board lists none for it)
	static AROR_MissionType CreateDefault(AROR_EMissionType type)
	{
		AROR_MissionType settings = new AROR_MissionType();
		settings.m_eType = type;
		settings.m_bEnabled = true;
		switch (type)
		{
			case AROR_EMissionType.SUPPLY_THEFT:
				settings.Set(1200, 3, 45, 4);
				break;
			case AROR_EMissionType.CONVOY_AMBUSH:
				settings.Set(1800, 2, 40, 2);
				break;
			case AROR_EMissionType.PRISONER_RESCUE:
				settings.Set(600, 6, 50, 5);
				break;
			case AROR_EMissionType.SABOTAGE:
				settings.Set(1500, 1, 45, 5);
				break;
			case AROR_EMissionType.ASSASSINATION:
				settings.Set(2000, 1, 35, 4);
				break;
			default:
				settings.Set(900, 2, 60, 3);
				break;
		}

		return settings;
	}

	//------------------------------------------------------------------------------------------------
	protected void Set(int funds, int manpower, int timeLimit, int guards)
	{
		m_iFunds = funds;
		m_iManpower = manpower;
		m_iTimeLimit = timeLimit;
		m_iGuards = guards;
	}
}
