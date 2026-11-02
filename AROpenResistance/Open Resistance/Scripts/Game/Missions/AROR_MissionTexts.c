//! Player-facing text for missions (OPS dossier, notifications, map markers), built from the mission's type and
//! target so the replicated snapshot stays small
class AROR_MissionTexts
{
	static const string CATEGORY_LOGISTICS = "Logistics";
	static const string CATEGORY_RESCUE = "Rescue";
	static const string CATEGORY_ASSAULT = "Assault";
	static const string CATEGORY_SABOTAGE = "Sabotage";

	//------------------------------------------------------------------------------------------------
	static string TypeName(AROR_EMissionType type)
	{
		switch (type)
		{
			case AROR_EMissionType.SUPPLY_THEFT:
				return "Supply Theft";
			case AROR_EMissionType.CONVOY_AMBUSH:
				return "Convoy Ambush";
			case AROR_EMissionType.PRISONER_RESCUE:
				return "Prisoner Rescue";
			case AROR_EMissionType.SABOTAGE:
				return "Supply Sabotage";
			case AROR_EMissionType.ASSASSINATION:
				return "Officer Assassination";
			case AROR_EMissionType.CACHE_RECOVERY:
				return "Arms Cache Recovery";
		}

		return "Operation";
	}

	//------------------------------------------------------------------------------------------------
	static string Category(AROR_EMissionType type)
	{
		switch (type)
		{
			case AROR_EMissionType.SUPPLY_THEFT:
				return CATEGORY_LOGISTICS;
			case AROR_EMissionType.CACHE_RECOVERY:
				return CATEGORY_LOGISTICS;
			case AROR_EMissionType.PRISONER_RESCUE:
				return CATEGORY_RESCUE;
			case AROR_EMissionType.SABOTAGE:
				return CATEGORY_SABOTAGE;
		}

		return CATEGORY_ASSAULT;
	}

	//------------------------------------------------------------------------------------------------
	static string RiskName(int risk)
	{
		if (risk <= AROR_Mission.RISK_LOW)
			return "LOW";

		if (risk == AROR_Mission.RISK_MODERATE)
			return "MODERATE";

		if (risk == AROR_Mission.RISK_HIGH)
			return "HIGH";

		return "EXTREME";
	}

	//------------------------------------------------------------------------------------------------
	static string SquadSize(AROR_EMissionType type, int risk)
	{
		switch (type)
		{
			case AROR_EMissionType.ASSASSINATION:
				return "2-4";
			case AROR_EMissionType.CACHE_RECOVERY:
				return "2-4";
			case AROR_EMissionType.CONVOY_AMBUSH:
				return "6-8";
		}

		if (risk >= AROR_Mission.RISK_HIGH)
			return "8+";

		return "4-6";
	}

	//------------------------------------------------------------------------------------------------
	//! "the " and the faction name of an enemy role (e.g. "the Occupiers")
	static string EnemyName(AROR_EFactionRole role)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (roles)
		{
			Faction faction = roles.GetFaction(role);
			if (faction)
			{
				string name = faction.GetFactionName();
				if (!name.IsEmpty())
					return "the " + name;
			}
		}

		if (role == AROR_EFactionRole.INVADERS)
			return "the Invaders";

		return "the Occupiers";
	}

	//------------------------------------------------------------------------------------------------
	static string LocationName(string locationId)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (locations)
		{
			AROR_LocationComponent location = locations.GetLocationById(locationId);
			if (location)
				return location.GetName();
		}

		return "the area";
	}

	//------------------------------------------------------------------------------------------------
	//! Situation paragraph for the dossier
	static string Brief(notnull AROR_MissionInfo info)
	{
		string place = LocationName(info.m_sLocationId);
		string enemy = EnemyName(info.m_eEnemyRole);
		switch (info.m_eType)
		{
			case AROR_EMissionType.SUPPLY_THEFT:
				return string.Format("A supply truck belonging to %1 is parked at %2 under a guard of %3. Get in, take the truck and drive it to any Resistance-held location before the shift changes.", enemy, place, info.m_iGuards);
			case AROR_EMissionType.CONVOY_AMBUSH:
				return string.Format("Signals intercept puts a convoy of %1 on the road from %2 to %3: an armed escort and a cargo truck. Catch it on the road and stop it before it arrives.", enemy, place, LocationName(info.m_sDestinationId));
			case AROR_EMissionType.PRISONER_RESCUE:
				return string.Format("Three of our sympathisers are being held at %1 ahead of transfer, watched by %2 guards from %3. Clear the guards, free the prisoners and escort them to safety. Each one brought home joins the Resistance.", place, info.m_iGuards, enemy);
			case AROR_EMissionType.SABOTAGE:
				return string.Format("Two loaded supply trucks of %1 are parked at %2 for their forward units. Destroying both cuts their resupply in the area. Expect %3 guards.", enemy, place, info.m_iGuards);
			case AROR_EMissionType.ASSASSINATION:
				return string.Format("A senior officer of %1 is visiting %2 with two bodyguards and a security detail. Security is tight but complacent. Remove him before he leaves.", enemy, place);
			case AROR_EMissionType.CACHE_RECOVERY:
				return string.Format("Before the occupation a smuggler buried weapons crates in the countryside near %1. A villager has narrowed it down to the marked area. Find the cache and secure it; patrols may be close.", place);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static void Objectives(notnull AROR_MissionInfo info, notnull array<string> outLines)
	{
		outLines.Clear();
		switch (info.m_eType)
		{
			case AROR_EMissionType.SUPPLY_THEFT:
				outLines.Insert("Neutralise or bypass the guard");
				outLines.Insert("Seize the supply truck intact");
				outLines.Insert("Drive it into a Resistance-held location");
				break;
			case AROR_EMissionType.CONVOY_AMBUSH:
				outLines.Insert("Intercept the convoy on the road");
				outLines.Insert("Destroy the vehicles or kill their crews");
				break;
			case AROR_EMissionType.PRISONER_RESCUE:
				outLines.Insert("Clear the guards around the prisoners");
				outLines.Insert("Reach the prisoners to free them");
				outLines.Insert("Escort them to the nearest friendly location");
				break;
			case AROR_EMissionType.SABOTAGE:
				outLines.Insert("Locate both supply trucks");
				outLines.Insert("Destroy them (explosives or AT recommended)");
				break;
			case AROR_EMissionType.ASSASSINATION:
				outLines.Insert("Locate the officer");
				outLines.Insert("Eliminate the officer");
				break;
			case AROR_EMissionType.CACHE_RECOVERY:
				outLines.Insert("Search the marked area");
				outLines.Insert("Stay at the cache to secure it");
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	static void Failures(notnull AROR_MissionInfo info, notnull array<string> outLines)
	{
		outLines.Clear();
		switch (info.m_eType)
		{
			case AROR_EMissionType.SUPPLY_THEFT:
				outLines.Insert("Truck destroyed");
				break;
			case AROR_EMissionType.CONVOY_AMBUSH:
				outLines.Insert("Convoy reaches " + LocationName(info.m_sDestinationId));
				break;
			case AROR_EMissionType.PRISONER_RESCUE:
				outLines.Insert("All prisoners killed");
				break;
			case AROR_EMissionType.ASSASSINATION:
				outLines.Insert("The officer leaves when time runs out");
				break;
		}

		outLines.Insert("Time limit expires");
	}

	//------------------------------------------------------------------------------------------------
	//! Recommended kit as name|amount pairs
	static void Kit(AROR_EMissionType type, notnull array<string> outNames, notnull array<string> outAmounts)
	{
		outNames.Clear();
		outAmounts.Clear();
		switch (type)
		{
			case AROR_EMissionType.SUPPLY_THEFT:
				AddKit(outNames, outAmounts, "Light vehicle", "1");
				AddKit(outNames, outAmounts, "Suppressed weapon", "rec.");
				AddKit(outNames, outAmounts, "Repair kit", "1");
				break;
			case AROR_EMissionType.CONVOY_AMBUSH:
				AddKit(outNames, outAmounts, "AT launcher", "2");
				AddKit(outNames, outAmounts, "Machine gunner", "1");
				AddKit(outNames, outAmounts, "Fast vehicle", "1");
				break;
			case AROR_EMissionType.PRISONER_RESCUE:
				AddKit(outNames, outAmounts, "Transport vehicle", "1");
				AddKit(outNames, outAmounts, "Medic", "1");
				AddKit(outNames, outAmounts, "Smoke grenades", "rec.");
				break;
			case AROR_EMissionType.SABOTAGE:
				AddKit(outNames, outAmounts, "Demolition charge", "2");
				AddKit(outNames, outAmounts, "AT launcher", "1");
				AddKit(outNames, outAmounts, "Engineer", "1");
				break;
			case AROR_EMissionType.ASSASSINATION:
				AddKit(outNames, outAmounts, "Marksman rifle", "1");
				AddKit(outNames, outAmounts, "Suppressed weapons", "all");
				AddKit(outNames, outAmounts, "Binoculars", "1");
				break;
			case AROR_EMissionType.CACHE_RECOVERY:
				AddKit(outNames, outAmounts, "Light vehicle", "1");
				AddKit(outNames, outAmounts, "Binoculars", "1");
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddKit(array<string> names, array<string> amounts, string name, string amount)
	{
		names.Insert(name);
		amounts.Insert(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! mm:ss, or h:mm:ss from an hour up
	static string Countdown(int seconds)
	{
		seconds = Math.Max(seconds, 0);
		int hours = seconds / 3600;
		int minutes = (seconds - hours * 3600) / 60;
		int rest = seconds - hours * 3600 - minutes * 60;
		if (hours > 0)
			return string.Format("%1:%2:%3", hours, minutes.ToString(2), rest.ToString(2));

		return string.Format("%1:%2", minutes.ToString(2), rest.ToString(2));
	}
}
