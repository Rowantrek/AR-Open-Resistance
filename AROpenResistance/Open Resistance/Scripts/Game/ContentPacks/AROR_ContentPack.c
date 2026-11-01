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

	// Catalog generation: mods such as WCS and RHS add their assets to the base-game faction catalogs, so a pack can
	// offer them without listing each one. Generated entries copy the price and import requirements of the first
	// entry of the same category and type in the listed packs (e.g. a WCS IFV costs what a base-game IFV costs), else
	// AROR_CatalogClassifier's defaults; their readiness is the classifier's default for the type.
	[Attribute(desc: "Factions whose entity catalogs (vehicles, static weapons, items) this pack offers at runtime, besides its listed entries. Prefabs another pack lists are left to that pack. Empty: no generation.", category: "Catalog generation")]
	protected ref array<string> m_aCatalogFactionKeys;

	[Attribute(desc: "Addon filesystems (e.g. ArmaReforger) whose prefabs are not generated: a prefab that exists in one of them is left out", category: "Catalog generation")]
	protected ref array<string> m_aCatalogExcludedSources;

	[Attribute(desc: "Lower-case file name fragments whose prefabs are not generated", category: "Catalog generation")]
	protected ref array<string> m_aCatalogExcludedNames;

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
	//! True when every required addon is in loadedAddons (GUIDs, compared case-insensitively). Entries can have their
	//! own requirements on top (AROR_ContentPackEntry), so one pack can cover many optional mods.
	bool HasRequiredAddons(notnull array<string> loadedAddons)
	{
		return AreAddonsLoaded(m_aRequiredAddons, loadedAddons);
	}

	//------------------------------------------------------------------------------------------------
	//! True when every GUID in required (null or empty: none) is in loadedAddons, compared case-insensitively
	static bool AreAddonsLoaded(array<string> requiredAddons, notnull array<string> loadedAddons)
	{
		if (!requiredAddons)
			return true;

		foreach (string required : requiredAddons)
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
	// Catalog generation
	//------------------------------------------------------------------------------------------------
	//! True when the pack offers faction catalog assets besides its listed entries
	bool HasCatalogGeneration()
	{
		return m_aCatalogFactionKeys && !m_aCatalogFactionKeys.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Key for the first listed entry of a category and type (generated entries copy its terms)
	static int TemplateKey(int category, int type)
	{
		return category * 1000 + type;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine, once the factions exist (deterministic, so every machine generates the same entries): adds the
	//! catalog factions' vehicles, static weapons and items that are not in offered. offered: prefabs other packs list,
	//! updated with what this pack adds. templates: first listed entry per TemplateKey. Returns how many were added.
	int GenerateFromCatalogs(notnull FactionManager factionManager, notnull set<string> offered, notnull map<int, AROR_ContentPackEntry> templates)
	{
		if (!HasCatalogGeneration())
			return 0;

		array<EEntityCatalogType> catalogTypes = {EEntityCatalogType.VEHICLE, EEntityCatalogType.WEAPONS_TRIPOD, EEntityCatalogType.ITEM};
		int added;
		foreach (string factionKey : m_aCatalogFactionKeys)
		{
			SCR_Faction faction = SCR_Faction.Cast(factionManager.GetFactionByKey(factionKey));
			if (!faction)
			{
				Print(string.Format("[AROR] Content pack '%1': catalog faction '%2' not found", m_sId, factionKey), LogLevel.WARNING);
				continue;
			}

			foreach (EEntityCatalogType catalogType : catalogTypes)
			{
				SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(catalogType, false);
				if (!catalog)
					continue;

				array<SCR_EntityCatalogEntry> entries = {};
				catalog.GetEntityList(entries);
				foreach (SCR_EntityCatalogEntry entry : entries)
				{
					ResourceName prefab = entry.GetPrefab();
					if (prefab.IsEmpty() || offered.Contains(prefab))
						continue;

					AROR_EFactionPresetCategory category;
					int type;
					if (!AROR_CatalogClassifier.Classify(catalogType, entry, category, type) || category == AROR_EFactionPresetCategory.UNITS)
						continue;

					if (AROR_CatalogClassifier.HasNameFragment(prefab, m_aCatalogExcludedNames) || AROR_CatalogClassifier.IsFromSource(prefab, m_aCatalogExcludedSources))
						continue;

					// Price and import requirements: as a listed entry of the same type, else the classifier's default.
					// Vehicle munitions share the rocket and missile types with hand-held ones, so they keep their own.
					bool vehicleMunition = category == AROR_EFactionPresetCategory.AMMUNITION && AROR_CatalogClassifier.IsVehicleMunition(entry);
					int cost;
					int importPoints;
					AROR_EImportPointRequirement importRequirement;
					AROR_ContentPackEntry sibling;
					if (!vehicleMunition)
						sibling = templates.Get(TemplateKey(category, type));

					if (sibling)
					{
						cost = sibling.GetCost();
						importPoints = sibling.GetRequiredImportPoints();
						importRequirement = sibling.GetImportPointRequirement();
					}
					else
					{
						AROR_CatalogClassifier.DefaultImportTerms(category, type, cost, importPoints, importRequirement, vehicleMunition);
					}

					int readiness = AROR_CatalogClassifier.DefaultReadiness(category, type, vehicleMunition);
					if (AddGenerated(category, type, prefab, cost, readiness, importPoints, importRequirement))
					{
						offered.Insert(prefab);
						added++;
					}
				}
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a runtime-created entry to the category's list; false for a category packs do not hold
	protected bool AddGenerated(AROR_EFactionPresetCategory category, int type, ResourceName prefab, int cost, int readiness, int importPoints, AROR_EImportPointRequirement importRequirement)
	{
		switch (category)
		{
			case AROR_EFactionPresetCategory.VEHICLES:
			{
				AROR_ContentPackVehicle vehicle = new AROR_ContentPackVehicle();
				vehicle.InitGenerated(prefab, type, cost, readiness, importPoints, importRequirement);
				if (!m_aVehicles)
					m_aVehicles = {};
				m_aVehicles.Insert(vehicle);
				return true;
			}
			case AROR_EFactionPresetCategory.GEAR:
			{
				AROR_ContentPackGear gear = new AROR_ContentPackGear();
				gear.InitGenerated(prefab, type, cost, readiness, importPoints, importRequirement);
				if (!m_aGear)
					m_aGear = {};
				m_aGear.Insert(gear);
				return true;
			}
			case AROR_EFactionPresetCategory.EQUIPMENT:
			{
				AROR_ContentPackEquipment equipment = new AROR_ContentPackEquipment();
				equipment.InitGenerated(prefab, type, cost, readiness, importPoints, importRequirement);
				if (!m_aEquipment)
					m_aEquipment = {};
				m_aEquipment.Insert(equipment);
				return true;
			}
			case AROR_EFactionPresetCategory.WEAPONS:
			{
				AROR_ContentPackWeapon weapon = new AROR_ContentPackWeapon();
				weapon.InitGenerated(prefab, type, cost, readiness, importPoints, importRequirement);
				if (!m_aWeapons)
					m_aWeapons = {};
				m_aWeapons.Insert(weapon);
				return true;
			}
			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES:
			{
				AROR_ContentPackWeaponAccessory accessory = new AROR_ContentPackWeaponAccessory();
				accessory.InitGenerated(prefab, type, cost, readiness, importPoints, importRequirement);
				if (!m_aWeaponAccessories)
					m_aWeaponAccessories = {};
				m_aWeaponAccessories.Insert(accessory);
				return true;
			}
			case AROR_EFactionPresetCategory.AMMUNITION:
			{
				AROR_ContentPackAmmunition ammunition = new AROR_ContentPackAmmunition();
				ammunition.InitGenerated(prefab, type, cost, readiness, importPoints, importRequirement);
				if (!m_aAmmunition)
					m_aAmmunition = {};
				m_aAmmunition.Insert(ammunition);
				return true;
			}
		}

		return false;
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
