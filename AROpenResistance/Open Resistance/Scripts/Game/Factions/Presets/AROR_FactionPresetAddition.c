//! Extra units and vehicles for a faction preset another addon owns (see AROR_FactionPreset), so a mod can add to
//! another mod's army without copying it or overriding its config. One `.conf` per addition, listed through
//! AROR_FactionPresetRegistry (GetExtraAdditionPaths); any number of addons can each add to the same preset. The
//! preset's own entries come first, then the additions in registration order, and a preset has at most one unit per
//! type, so an addition cannot replace a unit the preset (or an earlier addition) already has.
[BaseContainerProps(configRoot: true)]
class AROR_FactionPresetAddition
{
	[Attribute("", desc: "ID of the faction preset to add to, e.g. RHS_USAF_FORECON. Skipped, with a log line, when no loaded preset has it (its addon may not be loaded).")]
	protected string m_sPresetId;

	[Attribute(desc: "Extra AI units: only for unit types the preset does not list; they need loadouts like any preset unit", category: "Units")]
	protected ref array<ref AROR_FactionPresetUnit> m_aUnits;

	[Attribute(desc: "Extra empty vehicles (no crew)", category: "Vehicles")]
	protected ref array<ref AROR_FactionPresetVehicle> m_aVehicles;

	//------------------------------------------------------------------------------------------------
	string GetPresetId()
	{
		return m_sPresetId;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the units (AROR_EFactionPresetCategory.UNITS) or vehicles of the category to outEntries
	void CollectEntries(AROR_EFactionPresetCategory category, notnull array<AROR_FactionPresetEntry> outEntries)
	{
		switch (category)
		{
			case AROR_EFactionPresetCategory.UNITS:
				if (m_aUnits)
				{
					foreach (AROR_FactionPresetUnit unit : m_aUnits)
					{
						if (unit)
							outEntries.Insert(unit);
					}
				}
				break;

			case AROR_EFactionPresetCategory.VEHICLES:
				if (m_aVehicles)
				{
					foreach (AROR_FactionPresetVehicle vehicle : m_aVehicles)
					{
						if (vehicle)
							outEntries.Insert(vehicle);
					}
				}
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Logs configuration mistakes: unknown types, a unit type listed twice, vehicles without a prefab, loadouts
	void Validate(string source)
	{
		if (m_sPresetId.IsEmpty())
			Print(string.Format("[AROR] Faction preset addition '%1' has no preset ID", source), LogLevel.ERROR);

		set<int> unitTypes = new set<int>();
		array<AROR_FactionPresetEntry> units = {};
		CollectEntries(AROR_EFactionPresetCategory.UNITS, units);
		foreach (AROR_FactionPresetEntry entry : units)
		{
			if (!entry.IsKnownType())
			{
				Print(string.Format("[AROR] Faction preset addition '%1' has a unit with an unknown type (%2); it is ignored", source, entry.GetTypeValue()), LogLevel.WARNING);
				continue;
			}

			if (!unitTypes.Insert(entry.GetTypeValue()))
				Print(string.Format("[AROR] Faction preset addition '%1' lists unit type %2 more than once; only the first is used", source, entry.GetTypeName()), LogLevel.WARNING);

			AROR_FactionPresetUnit.Cast(entry).ValidateLoadouts(m_sPresetId);
		}

		array<AROR_FactionPresetEntry> vehicles = {};
		CollectEntries(AROR_EFactionPresetCategory.VEHICLES, vehicles);
		foreach (AROR_FactionPresetEntry entry : vehicles)
		{
			if (!entry.IsKnownType())
				Print(string.Format("[AROR] Faction preset addition '%1' has a vehicle with an unknown type (%2); it is ignored", source, entry.GetTypeValue()), LogLevel.WARNING);
			else if (entry.GetListedPrefab().IsEmpty())
				Print(string.Format("[AROR] Faction preset addition '%1' has a %2 vehicle without a prefab", source, entry.GetTypeName()), LogLevel.WARNING);
		}
	}
}
