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
