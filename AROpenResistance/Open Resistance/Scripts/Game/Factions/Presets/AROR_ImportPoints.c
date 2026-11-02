//! Import facilities: the seaports, heliports and airports a role holds. Black Market and Imports entries need a
//! number of them, plus optionally one seaport or one heliport/airport (AROR_EImportPointRequirement).
//! Reads replicated location control, so it gives the same answer on the server and on clients.
class AROR_ImportPoints
{
	//------------------------------------------------------------------------------------------------
	//! Every seaport, heliport and airport on the map (held or not)
	static void GetFacilities(notnull array<AROR_LocationComponent> outFacilities)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return;

		locations.GetLocationsOfType(AROR_ELocationType.PORT, outFacilities);
		locations.GetLocationsOfType(AROR_ELocationType.HELIPORT, outFacilities);
		locations.GetLocationsOfType(AROR_ELocationType.AIRPORT, outFacilities);
	}

	//------------------------------------------------------------------------------------------------
	//! Seaports held by the role
	static int CountPorts(AROR_EFactionRole role)
	{
		return CountHeld(role, AROR_ELocationType.PORT);
	}

	//------------------------------------------------------------------------------------------------
	//! Heliports and airports held by the role
	static int CountAir(AROR_EFactionRole role)
	{
		return CountHeld(role, AROR_ELocationType.HELIPORT) + CountHeld(role, AROR_ELocationType.AIRPORT);
	}

	//------------------------------------------------------------------------------------------------
	//! All import facilities held by the role (seaports + heliports + airports)
	static int CountFacilities(AROR_EFactionRole role)
	{
		return CountPorts(role) + CountAir(role);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the role holds the one location a special requirement asks for (always true for NONE)
	static bool HoldsSpecial(AROR_EFactionRole role, AROR_EImportPointRequirement requirement)
	{
		switch (requirement)
		{
			case AROR_EImportPointRequirement.PORT: return CountPorts(role) >= 1;
			case AROR_EImportPointRequirement.AIR: return CountAir(role) >= 1;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the role holds at least `required` import facilities and the special location, if any
	static bool IsMet(AROR_EFactionRole role, AROR_EImportPointRequirement requirement, int required)
	{
		return CountFacilities(role) >= required && HoldsSpecial(role, requirement);
	}

	//------------------------------------------------------------------------------------------------
	//! "Seaport" / "Heliport or airport" for a requirement; empty for NONE
	static string SpecialName(AROR_EImportPointRequirement requirement)
	{
		switch (requirement)
		{
			case AROR_EImportPointRequirement.PORT: return "Seaport";
			case AROR_EImportPointRequirement.AIR: return "Heliport or airport";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Locations of one type held by the role
	static int CountHeld(AROR_EFactionRole role, AROR_ELocationType type)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return 0;

		array<AROR_LocationComponent> candidates = {};
		locations.GetLocationsOfType(type, candidates);

		int held;
		foreach (AROR_LocationComponent location : candidates)
		{
			if (location && location.GetControllingRole() == role)
				held++;
		}

		return held;
	}
}
