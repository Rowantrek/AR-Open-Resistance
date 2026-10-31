class AROR_MarsTargets
{
	//! HQ is not capturable land. Settlements retain their support-only ownership rules.
	static AROR_LocationComponent NearestRebelTerritory(AROR_LocationManagerComponent locations, vector position)
	{
		if (!locations)
			return null;
		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		AROR_LocationComponent nearest;
		float distance = float.MAX;
		foreach (AROR_LocationComponent location : all)
		{
			if (!location || location.GetControllingRole() != AROR_EFactionRole.RESISTANCE || location.GetLocationType() == AROR_ELocationType.HQ)
				continue;
			float candidate = vector.DistanceXZ(position, location.GetCenter());
			if (candidate < distance)
			{
				nearest = location;
				distance = candidate;
			}
		}
		return nearest;
	}
}
