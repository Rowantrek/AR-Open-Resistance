#ifdef WORKBENCH
//! Policy tests only: formation contents and garrison team sequence.
class AROR_FormationSmokeTests
{
	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		array<AROR_EUnitType> units = {};
		AROR_Formations.GetUnits(AROR_EFormation.RIFLE_SQUAD, units);
		bool passed = Check(units.Count() == 8 && units[0] == AROR_EUnitType.SQUADLEADER && units[5] == AROR_EUnitType.AT, "Rifle squad is eight, led by the squad leader");
		AROR_Formations.GetUnits(AROR_EFormation.AT_FIRE_TEAM, units);
		passed = Check(units.Count() == 4 && units[0] == AROR_EUnitType.TEAMLEADER && units[2] == AROR_EUnitType.LAT, "AT fire team has a team leader and an LAT") && passed;
		AROR_Formations.GetUnits(AROR_EFormation.AA_FIRE_TEAM, units);
		passed = Check(units.Count() == 4 && units[1] == AROR_EUnitType.AA, "AA fire team has an AA specialist") && passed;

		AROR_Formations.Expand(null, units);
		passed = Check(units.Count() == 8, "No formations means a rifle squad") && passed;
		array<AROR_EFormation> formations = {AROR_EFormation.RIFLE_SQUAD, AROR_EFormation.AT_FIRE_TEAM};
		AROR_Formations.Expand(formations, units);
		passed = Check(units.Count() == 12 && units[8] == AROR_EUnitType.TEAMLEADER, "Formations expand in order") && passed;

		// A garrison of every size fields whole four-man teams with exactly one leader each
		for (int level = 1; level <= 4; level++)
		{
			int atCount;
			int aaCount;
			for (int team = 0; team < AROR_GarrisonPolicy.InfantryTeams(level); team++)
			{
				AROR_Formations.GetUnits(AROR_GarrisonPolicy.TeamFormation(team), units);
				passed = Check(units.Count() == 4 && units[0] == AROR_EUnitType.TEAMLEADER, "Garrison team is four led by a team leader") && passed;
				foreach (AROR_EUnitType type : units)
				{
					if (type == AROR_EUnitType.AT)
						atCount++;
					else if (type == AROR_EUnitType.AA)
						aaCount++;
				}
			}
			passed = Check(atCount >= 1, "Every garrison level fields an AT soldier") && passed;
			passed = Check((level < 3) == (aaCount == 0), "AA appears from level three") && passed;
		}
		return passed;
	}

	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print("[AROR TEST] Formation FAILED: " + label, LogLevel.ERROR);
		return condition;
	}
}
#endif
