//! A content pack (see ARCHITECTURE.md -> Content Packs): the items and vehicles one content source (the base game, a
//! mod such as RHS) offers, each with a price, a minimum readiness and import requirements, in the same fixed
//! categories as faction presets (no units). A pack can need addons: it is only available while all of them are
//! loaded. The admin enables or disables available packs per campaign. One `.conf` per pack, listed in
//! AROR_ContentPackRegistry.
[BaseContainerProps(configRoot: true)]
class AROR_ContentPack
{
	[Attribute("", desc: "Unique ID, e.g. BASE_GAME. Saved with campaigns: never change it once released.")]
	protected string m_sId;

	[Attribute("", desc: "Name shown to the admin when enabling packs")]
	protected string m_sName;

	[Attribute("", desc: "GUIDs of the addons this pack needs (as in their addon.gproj). Empty: always available.")]
	protected ref array<string> m_aRequiredAddons;

	[Attribute("1", desc: "Enabled when available, unless the admin turned it off in this campaign")]
	protected bool m_bEnabledByDefault;

	// One typed list per category, so the config editor only offers the right entry class
	[Attribute(desc: "Empty vehicles (no crew)", category: "Vehicles")]
	protected ref array<ref AROR_ContentPackVehicle> m_aVehicles;

	[Attribute(desc: "Clothing and carried kit", category: "Gear")]
	protected ref array<ref AROR_ContentPackGear> m_aGear;

	[Attribute(desc: "Medical supplies, binoculars, radios and other equipment", category: "Equipment")]
	protected ref array<ref AROR_ContentPackEquipment> m_aEquipment;

	[Attribute(desc: "Hand-held weapons", category: "Weapons")]
	protected ref array<ref AROR_ContentPackWeapon> m_aWeapons;

	[Attribute(desc: "Weapon accessories", category: "Weapon accessories")]
	protected ref array<ref AROR_ContentPackWeaponAccessory> m_aWeaponAccessories;

	[Attribute(desc: "Ammunition, including rocket pods and other munitions loaded onto vehicles and aircraft", category: "Ammunition")]
	protected ref array<ref AROR_ContentPackAmmunition> m_aAmmunition;

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
	bool IsEnabledByDefault()
	{
		return m_bEnabledByDefault;
	}

	//------------------------------------------------------------------------------------------------
	//! True when every required addon is in loadedAddons (GUIDs, compared case-insensitively)
	bool HasRequiredAddons(notnull array<string> loadedAddons)
	{
		if (!m_aRequiredAddons)
			return true;

		foreach (string required : m_aRequiredAddons)
		{
			if (required.IsEmpty())
				continue;

			string wanted = required;
			wanted.ToUpper();
			bool found;
			foreach (string loaded : loadedAddons)
			{
				string guid = loaded;
				guid.ToUpper();
				if (guid == wanted)
				{
					found = true;
					break;
				}
			}

			if (!found)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every entry of the category with a known type; returns how many were added
	int GetAllEntries(AROR_EFactionPresetCategory category, notnull array<AROR_ContentPackEntry> outEntries)
	{
		array<AROR_ContentPackEntry> entries = {};
		CollectEntries(category, entries);

		int added;
		foreach (AROR_ContentPackEntry entry : entries)
		{
			if (entry.IsKnownType())
			{
				outEntries.Insert(entry);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs configuration mistakes: missing ID, unknown types, empty prefabs. Returns false if the pack is unusable.
	bool Validate(string source)
	{
		if (m_sId.IsEmpty())
		{
			Print(string.Format("[AROR] Content pack '%1' has no ID and is ignored", source), LogLevel.ERROR);
			return false;
		}

		int empty;
		for (int category = AROR_EFactionPresetCategory.VEHICLES; category <= AROR_EFactionPresetCategory.AMMUNITION; category++)
		{
			array<AROR_ContentPackEntry> entries = {};
			CollectEntries(category, entries);
			foreach (AROR_ContentPackEntry entry : entries)
			{
				if (entry.GetPrefab().IsEmpty())
					empty++;

				if (!entry.IsKnownType())
					Print(string.Format("[AROR] Content pack '%1' has a %2 entry with an unknown type (%3); it is ignored", m_sId, typename.EnumToString(AROR_EFactionPresetCategory, category), entry.GetTypeValue()), LogLevel.WARNING);
			}
		}

		if (empty > 0)
			Print(string.Format("[AROR] Content pack '%1' has %2 entries without a prefab", m_sId, empty), LogLevel.WARNING);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The one place that maps a category to its list; everything else works on the shared entry type
	protected void CollectEntries(AROR_EFactionPresetCategory category, notnull array<AROR_ContentPackEntry> outEntries)
	{
		switch (category)
		{
			case AROR_EFactionPresetCategory.VEHICLES:
				if (m_aVehicles)
				{
					foreach (AROR_ContentPackVehicle vehicle : m_aVehicles)
					{
						if (vehicle)
							outEntries.Insert(vehicle);
					}
				}
				break;

			case AROR_EFactionPresetCategory.GEAR:
				if (m_aGear)
				{
					foreach (AROR_ContentPackGear gear : m_aGear)
					{
						if (gear)
							outEntries.Insert(gear);
					}
				}
				break;

			case AROR_EFactionPresetCategory.EQUIPMENT:
				if (m_aEquipment)
				{
					foreach (AROR_ContentPackEquipment equipment : m_aEquipment)
					{
						if (equipment)
							outEntries.Insert(equipment);
					}
				}
				break;

			case AROR_EFactionPresetCategory.WEAPONS:
				if (m_aWeapons)
				{
					foreach (AROR_ContentPackWeapon weapon : m_aWeapons)
					{
						if (weapon)
							outEntries.Insert(weapon);
					}
				}
				break;

			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES:
				if (m_aWeaponAccessories)
				{
					foreach (AROR_ContentPackWeaponAccessory accessory : m_aWeaponAccessories)
					{
						if (accessory)
							outEntries.Insert(accessory);
					}
				}
				break;

			case AROR_EFactionPresetCategory.AMMUNITION:
				if (m_aAmmunition)
				{
					foreach (AROR_ContentPackAmmunition ammunition : m_aAmmunition)
					{
						if (ammunition)
							outEntries.Insert(ammunition);
					}
				}
				break;
		}
	}
}
