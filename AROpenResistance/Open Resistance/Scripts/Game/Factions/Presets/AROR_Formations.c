//! The unit types of each formation, in order; the first leads. Every unit spawns as the role's base character dressed
//! from its faction preset loadout (see ARCHITECTURE.md -> Faction presets).
class AROR_Formations
{
	//------------------------------------------------------------------------------------------------
	//! Replaces outUnits with the formation's unit types
	static void GetUnits(AROR_EFormation formation, notnull array<AROR_EUnitType> outUnits)
	{
		outUnits.Clear();
		switch (formation)
		{
			case AROR_EFormation.RIFLE_FIRE_TEAM:
			{
				outUnits.Insert(AROR_EUnitType.TEAMLEADER);
				outUnits.Insert(AROR_EUnitType.AUTORIFLEMAN);
				outUnits.Insert(AROR_EUnitType.GRENADIER);
				outUnits.Insert(AROR_EUnitType.MARKSMAN);
				break;
			}
			case AROR_EFormation.WEAPONS_FIRE_TEAM:
			{
				outUnits.Insert(AROR_EUnitType.TEAMLEADER);
				outUnits.Insert(AROR_EUnitType.AT);
				outUnits.Insert(AROR_EUnitType.RIFLEMAN);
				outUnits.Insert(AROR_EUnitType.MEDIC);
				break;
			}
			case AROR_EFormation.AT_FIRE_TEAM:
			{
				outUnits.Insert(AROR_EUnitType.TEAMLEADER);
				outUnits.Insert(AROR_EUnitType.AT);
				outUnits.Insert(AROR_EUnitType.LAT);
				outUnits.Insert(AROR_EUnitType.RIFLEMAN);
				break;
			}
			case AROR_EFormation.AA_FIRE_TEAM:
			{
				outUnits.Insert(AROR_EUnitType.TEAMLEADER);
				outUnits.Insert(AROR_EUnitType.AA);
				outUnits.Insert(AROR_EUnitType.RIFLEMAN);
				outUnits.Insert(AROR_EUnitType.RIFLEMAN);
				break;
			}
			case AROR_EFormation.RIFLE_SQUAD:
			{
				outUnits.Insert(AROR_EUnitType.SQUADLEADER);
				outUnits.Insert(AROR_EUnitType.AUTORIFLEMAN);
				outUnits.Insert(AROR_EUnitType.GRENADIER);
				outUnits.Insert(AROR_EUnitType.MARKSMAN);
				outUnits.Insert(AROR_EUnitType.TEAMLEADER);
				outUnits.Insert(AROR_EUnitType.AT);
				outUnits.Insert(AROR_EUnitType.RIFLEMAN);
				outUnits.Insert(AROR_EUnitType.MEDIC);
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the formations' unit types in order; an empty list yields the default rifle squad
	static void Expand(array<AROR_EFormation> formations, notnull array<AROR_EUnitType> outUnits)
	{
		outUnits.Clear();
		array<AROR_EUnitType> units = {};
		if (!formations || formations.IsEmpty())
		{
			GetUnits(AROR_EFormation.RIFLE_SQUAD, outUnits);
			return;
		}
		foreach (AROR_EFormation formation : formations)
		{
			GetUnits(formation, units);
			outUnits.InsertAll(units);
		}
	}
}
