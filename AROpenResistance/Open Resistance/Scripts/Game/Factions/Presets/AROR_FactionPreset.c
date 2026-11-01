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

	[Attribute("", desc: "Preset ID this one takes over as a role default while it is loaded, e.g. US_ARMY for a content mod's variant of the US Army. Empty: none.")]
	protected string m_sReplacesPreset;

	[Attribute(desc: "Workbench only: characters whose kits are captured into this preset's unit loadouts (see the presets component's Capture Unit Loadouts)", category: "Units")]
	protected ref AROR_LoadoutCapture m_LoadoutCapture;

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

	// Catalog augmentation: mods such as WCS add their weapons, vehicles and aircraft to the base-game faction
	// catalogs, so a preset picks them up at runtime without an extender listing them. Only the prefab and its type
	// are added: price, readiness and import requirements come from the content pack that offers it (see
	// AROR_ContentPack's catalog generation), and an entry no enabled pack offers is not used.
	[Attribute("", desc: "Faction whose entity catalogs augment this preset at runtime (e.g. US): everything they hold that the preset does not list is added, units only for unit types the preset lacks. Empty: no augmentation.", category: "Catalog augmentation")]
	protected string m_sCatalogFactionKey;

	[Attribute(desc: "Further factions whose hand-held weapons, weapon accessories and ammunition are added too", category: "Catalog augmentation")]
	protected ref array<string> m_aHandHeldFactionKeys;

	[Attribute(desc: "Addon filesystems (e.g. ArmaReforger) whose items are not taken from the further factions; an item whose prefab exists in one of them is left out", category: "Catalog augmentation")]
	protected ref array<string> m_aHandHeldExcludedSources;

	[Attribute(desc: "Lower-case file name fragments (e.g. javelin) whose items are not taken from the further factions, e.g. guided launchers kept for the Black Market", category: "Catalog augmentation")]
	protected ref array<string> m_aHandHeldExcludedNames;

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
	//! The base-game faction whose catalogs augment this preset (also the native faction of its assets), or empty
	string GetCatalogFactionKey()
	{
		return m_sCatalogFactionKey;
	}

	//------------------------------------------------------------------------------------------------
	bool IsUsableAs(AROR_EFactionRole role)
	{
		return m_aUsableRoles && m_aUsableRoles.Contains(role);
	}

	//------------------------------------------------------------------------------------------------
	//! The preset ID this one takes over as a role default, or empty
	string GetReplacedPresetId()
	{
		return m_sReplacesPreset;
	}

	//------------------------------------------------------------------------------------------------
	//! The Workbench capture settings, or null
	AROR_LoadoutCapture GetLoadoutCapture()
	{
		return m_LoadoutCapture;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the preset dresses its own infantry: its rifleman names no character and has loadouts, so its soldiers
	//! are the role's gear-less defaults dressed by Readiness (see AROR_AIOutfitter). Spawning systems then build their
	//! infantry from the preset's units instead of their authored characters.
	bool DressesInfantry()
	{
		AROR_FactionPresetUnit rifleman = GetUnit(AROR_EUnitType.RIFLEMAN);
		return rifleman && !rifleman.HasPrefab() && rifleman.HasLoadouts();
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
	//! in an enabled content pack; otherwise it is skipped and logged once. Entries added from the catalogs are skipped
	//! quietly: their pack (e.g. mod assets) may simply be turned off in this campaign.
	protected bool IsOffered(notnull AROR_FactionPresetEntry entry)
	{
		if (entry.GetCategory() == AROR_EFactionPresetCategory.UNITS)
			return true;

		return AROR_ContentPacksComponent.IsOffered(entry.GetPrefab(), m_sId, !entry.IsGenerated());
	}

	//------------------------------------------------------------------------------------------------
	// Catalog augmentation
	//------------------------------------------------------------------------------------------------
	//! Every machine, once the factions exist: adds what the catalog faction's entity catalogs hold that the preset
	//! does not list (sorted by AROR_CatalogClassifier), and the hand-held items of the further factions that do not
	//! come from an excluded source. Listed entries always win. Returns how many were added.
	int AugmentFromCatalogs(notnull FactionManager factionManager)
	{
		if (m_sCatalogFactionKey.IsEmpty())
			return 0;

		set<string> known = new set<string>();
		set<int> unitTypes = new set<int>();
		for (int category = 0; category <= AROR_EFactionPresetCategory.AMMUNITION; category++)
		{
			array<AROR_FactionPresetEntry> entries = {};
			CollectEntries(category, entries);
			foreach (AROR_FactionPresetEntry entry : entries)
			{
				known.Insert(entry.GetListedPrefab());
				if (category == AROR_EFactionPresetCategory.UNITS)
					unitTypes.Insert(entry.GetTypeValue());
			}
		}

		int added = AugmentFromFaction(factionManager, m_sCatalogFactionKey, false, known, unitTypes);
		if (m_aHandHeldFactionKeys)
		{
			foreach (string factionKey : m_aHandHeldFactionKeys)
			{
				added += AugmentFromFaction(factionManager, factionKey, true, known, unitTypes);
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	protected int AugmentFromFaction(FactionManager factionManager, string factionKey, bool handHeldOnly, set<string> known, set<int> unitTypes)
	{
		SCR_Faction faction = SCR_Faction.Cast(factionManager.GetFactionByKey(factionKey));
		if (!faction)
		{
			Print(string.Format("[AROR] Faction preset '%1': catalog faction '%2' not found", m_sId, factionKey), LogLevel.WARNING);
			return 0;
		}

		array<EEntityCatalogType> catalogTypes = {};
		if (!handHeldOnly)
		{
			catalogTypes.Insert(EEntityCatalogType.CHARACTER);
			catalogTypes.Insert(EEntityCatalogType.VEHICLE);
			catalogTypes.Insert(EEntityCatalogType.WEAPONS_TRIPOD);
		}
		catalogTypes.Insert(EEntityCatalogType.ITEM);

		int added;
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
				if (prefab.IsEmpty() || known.Contains(prefab))
					continue;

				AROR_EFactionPresetCategory category;
				int type;
				if (!AROR_CatalogClassifier.Classify(catalogType, entry, category, type))
					continue;

				if (handHeldOnly && !AROR_CatalogClassifier.IsHandHeldFromAllowedSource(category, prefab, m_aHandHeldExcludedSources, m_aHandHeldExcludedNames))
					continue;

				// One unit per type: listed units and earlier catalog units win
				if (category == AROR_EFactionPresetCategory.UNITS && !unitTypes.Insert(type))
					continue;

				AddGenerated(category, type, prefab);
				known.Insert(prefab);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a runtime-created entry to the category's list
	protected void AddGenerated(AROR_EFactionPresetCategory category, int type, ResourceName prefab)
	{
		switch (category)
		{
			case AROR_EFactionPresetCategory.UNITS:
			{
				AROR_FactionPresetUnit unit = new AROR_FactionPresetUnit();
				unit.InitGenerated(prefab, type);
				if (!m_aUnits)
					m_aUnits = {};
				m_aUnits.Insert(unit);
				break;
			}
			case AROR_EFactionPresetCategory.VEHICLES:
			{
				AROR_FactionPresetVehicle vehicle = new AROR_FactionPresetVehicle();
				vehicle.InitGenerated(prefab, type);
				if (!m_aVehicles)
					m_aVehicles = {};
				m_aVehicles.Insert(vehicle);
				break;
			}
			case AROR_EFactionPresetCategory.GEAR:
			{
				AROR_FactionPresetGear gear = new AROR_FactionPresetGear();
				gear.InitGenerated(prefab, type);
				if (!m_aGear)
					m_aGear = {};
				m_aGear.Insert(gear);
				break;
			}
			case AROR_EFactionPresetCategory.EQUIPMENT:
			{
				AROR_FactionPresetEquipment equipment = new AROR_FactionPresetEquipment();
				equipment.InitGenerated(prefab, type);
				if (!m_aEquipment)
					m_aEquipment = {};
				m_aEquipment.Insert(equipment);
				break;
			}
			case AROR_EFactionPresetCategory.WEAPONS:
			{
				AROR_FactionPresetWeapon weapon = new AROR_FactionPresetWeapon();
				weapon.InitGenerated(prefab, type);
				if (!m_aWeapons)
					m_aWeapons = {};
				m_aWeapons.Insert(weapon);
				break;
			}
			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES:
			{
				AROR_FactionPresetWeaponAccessory accessory = new AROR_FactionPresetWeaponAccessory();
				accessory.InitGenerated(prefab, type);
				if (!m_aWeaponAccessories)
					m_aWeaponAccessories = {};
				m_aWeaponAccessories.Insert(accessory);
				break;
			}
			case AROR_EFactionPresetCategory.AMMUNITION:
			{
				AROR_FactionPresetAmmunition ammunition = new AROR_FactionPresetAmmunition();
				ammunition.InitGenerated(prefab, type);
				if (!m_aAmmunition)
					m_aAmmunition = {};
				m_aAmmunition.Insert(ammunition);
				break;
			}
		}
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
