//! A faction preset (see ARCHITECTURE.md -> Factions): everything an army makes available to the game, in fixed
//! categories (AROR_EFactionPresetCategory), each entry a prefab of a type. A preset only lists what exists; garrisons,
//! QRFs, missions and other systems decide what to use. One `.conf` per preset, listed in AROR_FactionPresetRegistry.
[BaseContainerProps(configRoot: true)]
class AROR_FactionPreset
{
	[Attribute("", desc: "Unique ID, e.g. US_ARMY. Saved with campaigns: never change it once released.")]
	protected string m_sId;

	[Attribute("", desc: "Name shown to the admin when choosing presets")]
	protected string m_sName;

	[Attribute("", UIWidgets.ComboBox, desc: "Roles this preset can be chosen for", enums: ParamEnumArray.FromEnum(AROR_EFactionRole))]
	protected ref array<AROR_EFactionRole> m_aUsableRoles;

	// One typed list per category, so the config editor only offers the right entry class
	[Attribute(desc: "AI units: at most one per unit type", category: "Units")]
	protected ref array<ref AROR_FactionPresetUnit> m_aUnits;

	[Attribute(desc: "Starting kit for players spawning in the role this preset serves (in practice the Resistance). Empty keeps the player character's own kit.", category: "Units")]
	protected ref AROR_PlayerLoadout m_PlayerLoadout;

	[Attribute(desc: "Empty vehicles (no crew)", category: "Vehicles")]
	protected ref array<ref AROR_FactionPresetVehicle> m_aVehicles;

	[Attribute(desc: "Clothing and carried kit", category: "Gear")]
	protected ref array<ref AROR_FactionPresetGear> m_aGear;

	[Attribute(desc: "Medical supplies, binoculars, radios and other equipment", category: "Equipment")]
	protected ref array<ref AROR_FactionPresetEquipment> m_aEquipment;

	[Attribute(desc: "Hand-held weapons", category: "Weapons")]
	protected ref array<ref AROR_FactionPresetWeapon> m_aWeapons;

	[Attribute(desc: "Weapon accessories", category: "Weapon accessories")]
	protected ref array<ref AROR_FactionPresetWeaponAccessory> m_aWeaponAccessories;

	[Attribute(desc: "Ammunition, including rocket pods and other munitions loaded onto vehicles and aircraft", category: "Ammunition")]
	protected ref array<ref AROR_FactionPresetAmmunition> m_aAmmunition;

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	bool IsUsableAs(AROR_EFactionRole role)
	{
		return m_aUsableRoles && m_aUsableRoles.Contains(role);
	}

	//------------------------------------------------------------------------------------------------
	// Queries (entries with an unknown type, or whose prefab no enabled content pack offers, are never returned)
	//------------------------------------------------------------------------------------------------
	//! The unit of this type, or null if the preset has none
	AROR_FactionPresetUnit GetUnit(AROR_EUnitType type)
	{
		array<AROR_FactionPresetEntry> units = {};
		GetEntries(AROR_EFactionPresetCategory.UNITS, type, units);
		if (units.IsEmpty())
			return null;

		return AROR_FactionPresetUnit.Cast(units[0]);
	}

	//------------------------------------------------------------------------------------------------
	//! The starting kit for players, or null when the preset has none
	AROR_PlayerLoadout GetPlayerLoadout()
	{
		return m_PlayerLoadout;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the category's entries of one type (the int value of that category's type enum, e.g. AROR_EVehicleType.APC)
	//! to outEntries; returns how many were added
	int GetEntries(AROR_EFactionPresetCategory category, int type, notnull array<AROR_FactionPresetEntry> outEntries)
	{
		array<AROR_FactionPresetEntry> entries = {};
		CollectEntries(category, entries);

		int added;
		foreach (AROR_FactionPresetEntry entry : entries)
		{
			if (entry.IsKnownType() && entry.GetTypeValue() == type && IsOffered(entry))
			{
				outEntries.Insert(entry);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every entry of the category (e.g. for a shop listing everything); returns how many were added
	int GetAllEntries(AROR_EFactionPresetCategory category, notnull array<AROR_FactionPresetEntry> outEntries)
	{
		array<AROR_FactionPresetEntry> entries = {};
		CollectEntries(category, entries);

		int added;
		foreach (AROR_FactionPresetEntry entry : entries)
		{
			if (entry.IsKnownType() && IsOffered(entry))
			{
				outEntries.Insert(entry);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Units are always usable (content packs do not list units). Any other entry needs its prefab, after replacements,
	//! in an enabled content pack; otherwise it is skipped and logged once.
	protected bool IsOffered(notnull AROR_FactionPresetEntry entry)
	{
		if (entry.GetCategory() == AROR_EFactionPresetCategory.UNITS)
			return true;

		return AROR_ContentPacksComponent.IsOffered(entry.GetPrefab(), m_sId);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs configuration mistakes: missing ID or roles, unknown types, duplicate unit types, empty prefabs, loadouts.
	//! Returns false if the preset is unusable.
	bool Validate(string source)
	{
		if (m_sId.IsEmpty())
		{
			Print(string.Format("[AROR] Faction preset '%1' has no ID and is ignored", source), LogLevel.ERROR);
			return false;
		}

		if (!m_aUsableRoles || m_aUsableRoles.IsEmpty())
			Print(string.Format("[AROR] Faction preset '%1' has no usable roles", m_sId), LogLevel.WARNING);

		set<int> unitTypes = new set<int>();
		int empty;

		for (int category = 0; category <= AROR_EFactionPresetCategory.AMMUNITION; category++)
		{
			string categoryName = typename.EnumToString(AROR_EFactionPresetCategory, category);

			array<AROR_FactionPresetEntry> entries = {};
			CollectEntries(category, entries);
			foreach (AROR_FactionPresetEntry entry : entries)
			{
				// Units may leave the prefab empty to use the role default
				if (entry.GetListedPrefab().IsEmpty() && category != AROR_EFactionPresetCategory.UNITS)
					empty++;

				if (!entry.IsKnownType())
				{
					Print(string.Format("[AROR] Faction preset '%1' has a %2 entry with an unknown type (%3); it is ignored", m_sId, categoryName, entry.GetTypeValue()), LogLevel.WARNING);
					continue;
				}

				if (category == AROR_EFactionPresetCategory.UNITS)
				{
					if (!unitTypes.Insert(entry.GetTypeValue()))
						Print(string.Format("[AROR] Faction preset '%1' lists unit type %2 more than once; only the first is used", m_sId, entry.GetTypeName()), LogLevel.WARNING);

					AROR_FactionPresetUnit.Cast(entry).ValidateLoadouts(m_sId);
				}
			}
		}

		if (empty > 0)
			Print(string.Format("[AROR] Faction preset '%1' has %2 entries without a prefab", m_sId, empty), LogLevel.WARNING);

		if (m_PlayerLoadout)
			m_PlayerLoadout.Validate(string.Format("Faction preset '%1', player loadout", m_sId));
		else if (IsUsableAs(AROR_EFactionRole.RESISTANCE))
			// Players always join the Resistance and its character is gear-less, so they would spawn with nothing
			Print(string.Format("[AROR] Faction preset '%1' can be used for the Resistance but has no player loadout; players would spawn with no gear", m_sId), LogLevel.ERROR);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The one place that maps a category to its list; everything else works on the shared entry type
	protected void CollectEntries(AROR_EFactionPresetCategory category, notnull array<AROR_FactionPresetEntry> outEntries)
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

			case AROR_EFactionPresetCategory.GEAR:
				if (m_aGear)
				{
					foreach (AROR_FactionPresetGear gear : m_aGear)
					{
						if (gear)
							outEntries.Insert(gear);
					}
				}
				break;

			case AROR_EFactionPresetCategory.EQUIPMENT:
				if (m_aEquipment)
				{
					foreach (AROR_FactionPresetEquipment equipment : m_aEquipment)
					{
						if (equipment)
							outEntries.Insert(equipment);
					}
				}
				break;

			case AROR_EFactionPresetCategory.WEAPONS:
				if (m_aWeapons)
				{
					foreach (AROR_FactionPresetWeapon weapon : m_aWeapons)
					{
						if (weapon)
							outEntries.Insert(weapon);
					}
				}
				break;

			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES:
				if (m_aWeaponAccessories)
				{
					foreach (AROR_FactionPresetWeaponAccessory accessory : m_aWeaponAccessories)
					{
						if (accessory)
							outEntries.Insert(accessory);
					}
				}
				break;

			case AROR_EFactionPresetCategory.AMMUNITION:
				if (m_aAmmunition)
				{
					foreach (AROR_FactionPresetAmmunition ammunition : m_aAmmunition)
					{
						if (ammunition)
							outEntries.Insert(ammunition);
					}
				}
				break;
		}
	}
}
