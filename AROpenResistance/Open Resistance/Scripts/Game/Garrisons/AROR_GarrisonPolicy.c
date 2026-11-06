//! Site recipes are rolled once and saved. Outposts never receive vehicles; airbases are always heavy.
class AROR_GarrisonPolicy
{
	static int DefenceLevel(AROR_ELocationType type, int roll)
	{
		switch (type)
		{
			case AROR_ELocationType.OUTPOST: return 1;
			case AROR_ELocationType.AIRPORT: return 4;
			case AROR_ELocationType.MILITARY_BASE: return 3;
			case AROR_ELocationType.RESOURCES:
			case AROR_ELocationType.FACTORY:
			case AROR_ELocationType.HELIPORT:
			case AROR_ELocationType.PORT: return 1 + Math.Clamp(roll, 0, 2);
		}
		return 0;
	}
	static int InfantryTeams(int level)
	{
		return Math.Clamp(level, 1, 4) * 2;
	}
	//! The formation of the site's team at this index (0-based): mostly rifle teams, with the launcher teams spread
	//! through the sequence so small sites get one AT and only larger ones gain AA
	static AROR_EFormation TeamFormation(int team)
	{
		switch (Math.Mod(team, 8))
		{
			case 1:
			case 7: return AROR_EFormation.WEAPONS_FIRE_TEAM;
			case 3: return AROR_EFormation.AT_FIRE_TEAM;
			case 5: return AROR_EFormation.AA_FIRE_TEAM;
		}
		return AROR_EFormation.RIFLE_FIRE_TEAM;
	}
	static int StaticCount(int level)
	{
		return Math.Clamp(level, 1, 4);
	}
	static int MannedCount(int level)
	{
		if (level < 2)
			return 0;
		if (level == 4)
			return 3;
		return 1;
	}
	static bool IsEnemyRole(AROR_EFactionRole role)
	{
		return role == AROR_EFactionRole.OCCUPIERS || role == AROR_EFactionRole.INVADERS;
	}
}
