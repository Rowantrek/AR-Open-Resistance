//! One base-game faction to import and the preset file it writes
class AROR_BaseFactionImport
{
	string m_sFactionKey;	//!< Base-game faction key, e.g. "US"
	string m_sFile;			//!< Preset file name in Configs/FactionPresets/BaseGame/
	string m_sPresetId;
	string m_sPresetName;
	ref array<int> m_aUsableRoles = {};

	//------------------------------------------------------------------------------------------------
	void AROR_BaseFactionImport(string factionKey, string file, string presetId, string presetName, notnull array<int> usableRoles)
	{
		m_sFactionKey = factionKey;
		m_sFile = file;
		m_sPresetId = presetId;
		m_sPresetName = presetName;
		m_aUsableRoles.Copy(usableRoles);
	}
}

//! Entries collected for one preset, already in .conf text form
class AROR_PresetImportResult
{
	ref array<string> m_aUnits = {};
	ref array<string> m_aVehicles = {};
	ref array<string> m_aGear = {};
	ref array<string> m_aEquipment = {};
	ref array<string> m_aWeapons = {};
	ref array<string> m_aAccessories = {};
	ref array<string> m_aAmmunition = {};
	ref set<int> m_UnitTypesUsed = new set<int>();
	ref array<string> m_aSkipped = {};
}

//! Imports the base game's US, USSR, FIA and CIV factions into the core faction presets
//! (Configs/FactionPresets/BaseGame/*.conf). Reads each faction's entity catalogs through our faction manager prefab
//! (which inherits the base-game factions), sorts every entry into the preset types using the base game's own tags
//! (Game Master labels for characters and vehicles, arsenal item type/mode for items) plus a few file-name hints, and
//! rewrites the preset files. Presets keep only units, the player loadout and vehicles; the sorted items are collected
//! too, but only for the content pack export (AROR_ExportContentPackPlugin). Prices, readiness and import requirements
//! live in the Base Game content pack, which this never touches. Anything that cannot be placed is listed in the log.
//! Re-run after game updates; it overwrites the four base preset files, except their units (hand-authored prefabs and
//! loadouts) and player loadout, which are kept as they are. Units are generated only for a preset that has none.
[WorkbenchPluginAttribute(name: "Import base faction presets", description: "Fill the base-game faction presets (US, USSR, FIA, CIV) from the game's faction catalogs.", wbModules: {"WorldEditor", "ResourceManager"}, category: "Open Resistance")]
class AROR_ImportBaseFactionPresetsPlugin : WorkbenchPlugin
{
	protected static const ResourceName FACTION_MANAGER = "{906843134C8EF19C}Prefabs/Managers/AROR_FactionManager.et";
	protected static const string OUTPUT_FOLDER = "$OpenResistance:Configs/FactionPresets/BaseGame/";
	protected static const string FALLBACK_FOLDER = "$profile:AROR_FactionPresets/";

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		if (!GetGame())
		{
			Print("[AROR] Import base faction presets: run this from the World Editor (Plugins > Open Resistance)", LogLevel.ERROR);
			return;
		}

		array<ref AROR_BaseFactionImport> imports = {};
		imports.Insert(new AROR_BaseFactionImport("FIA", "AROR_Preset_FIA.conf", "FIA", "FIA", {AROR_EFactionRole.RESISTANCE}));
		imports.Insert(new AROR_BaseFactionImport("US", "AROR_Preset_US_Army.conf", "US_ARMY", "US Army", {AROR_EFactionRole.OCCUPIERS, AROR_EFactionRole.INVADERS}));
		imports.Insert(new AROR_BaseFactionImport("USSR", "AROR_Preset_USSR_Army.conf", "USSR_ARMY", "Soviet Army", {AROR_EFactionRole.OCCUPIERS, AROR_EFactionRole.INVADERS}));
		imports.Insert(new AROR_BaseFactionImport("CIV", "AROR_Preset_Everon_Civilians.conf", "EVERON_CIV", "Everon Civilians", {AROR_EFactionRole.CIVILIANS}));

		Resource resource = BaseContainerTools.LoadContainer(FACTION_MANAGER);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[AROR] Import base faction presets: cannot load %1", FACTION_MANAGER), LogLevel.ERROR);
			return;
		}

		BaseContainerList factions = resource.GetResource().ToBaseContainer().GetObjectArray("Factions");
		if (!factions)
		{
			Print("[AROR] Import base faction presets: the faction manager has no Factions list", LogLevel.ERROR);
			return;
		}

		foreach (AROR_BaseFactionImport factionImport : imports)
		{
			BaseContainer faction = FindFaction(factions, factionImport.m_sFactionKey);
			if (!faction)
			{
				Print(string.Format("[AROR] Import base faction presets: faction '%1' not found; '%2' left unchanged", factionImport.m_sFactionKey, factionImport.m_sFile), LogLevel.WARNING);
				continue;
			}

			AROR_PresetImportResult result = new AROR_PresetImportResult();
			ReadFaction(faction, result);
			WritePreset(factionImport, result);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected BaseContainer FindFaction(BaseContainerList factions, string factionKey)
	{
		for (int i = 0; i < factions.Count(); i++)
		{
			BaseContainer faction = factions.Get(i);
			string key;
			if (faction && faction.Get("FactionKey", key) && key == factionKey)
				return faction;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Every catalog of the faction: characters, vehicles, static weapons and inventory items
	protected void ReadFaction(BaseContainer faction, AROR_PresetImportResult result)
	{
		BaseContainerList catalogs = faction.GetObjectArray("m_aEntityCatalogs");
		if (!catalogs)
			return;

		for (int i = 0; i < catalogs.Count(); i++)
		{
			SCR_EntityCatalog catalog = SCR_EntityCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(catalogs.Get(i)));
			if (!catalog)
				continue;

			// Merges multi-lists, drops disabled entries and loads each entry's labels and data
			catalog.InitCatalog();

			array<SCR_EntityCatalogEntry> entries = {};
			catalog.GetEntityList(entries);

			EEntityCatalogType catalogType = catalog.GetCatalogType();
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				switch (catalogType)
				{
					case EEntityCatalogType.CHARACTER:
						AddUnit(entry, result);
						break;
					case EEntityCatalogType.VEHICLE:
						AddVehicle(entry, result);
						break;
					case EEntityCatalogType.WEAPONS_TRIPOD:
						AddEntry(result.m_aVehicles, "AROR_FactionPresetVehicle", AROR_EVehicleType, StaticType(LowerName(entry.GetPrefab())), entry.GetPrefab());
						break;
					case EEntityCatalogType.ITEM:
						AddItem(entry, result);
						break;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Characters
	//------------------------------------------------------------------------------------------------
	protected void AddUnit(SCR_EntityCatalogEntry entry, AROR_PresetImportResult result)
	{
		int type = UnitType(entry);
		if (type < 0)
		{
			result.m_aSkipped.Insert("unit (no matching type): " + entry.GetPrefab());
			return;
		}

		// One unit per type: the first match wins
		if (!result.m_UnitTypesUsed.Insert(type))
		{
			result.m_aSkipped.Insert(string.Format("unit (%1 already set): %2", typename.EnumToString(AROR_EUnitType, type), entry.GetPrefab()));
			return;
		}

		AddEntry(result.m_aUnits, "AROR_FactionPresetUnit", AROR_EUnitType, type, entry.GetPrefab());
	}

	//------------------------------------------------------------------------------------------------
	protected int UnitType(SCR_EntityCatalogEntry entry)
	{
		string name = LowerName(entry.GetPrefab());

		// Roles the labels cannot tell apart are recognised by file name
		if (name.Contains("pilot") || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_HELI_CREW))
			return AROR_EUnitType.PILOT;
		if (name.Contains("crew") || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_VEHICLE_CREW))
			return AROR_EUnitType.CREW;
		if (name.Contains("officer"))
			return AROR_EUnitType.SQUADLEADER;
		if (name.Contains("sniper"))
			return AROR_EUnitType.SNIPER;
		if (name.Contains("_aa.") || name.Contains("_aa_") || name.Contains("antiair"))
			return AROR_EUnitType.AA;
		if (name.Contains("_ar.") || name.Contains("_ar_") || name.Contains("autorifle"))
			return AROR_EUnitType.AUTORIFLEMAN;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.FACTION_CIV) || name.Contains("civilian"))
			return AROR_EUnitType.CIVILIAN;

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_LEADER))
			return AROR_EUnitType.TEAMLEADER;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_MEDIC))
			return AROR_EUnitType.MEDIC;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_ANTITANK))
			return AROR_EUnitType.AT;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_MACHINEGUNNER))
			return AROR_EUnitType.MACHINEGUNNER;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_GRENADIER))
			return AROR_EUnitType.GRENADIER;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_SHARPSHOOTER))
			return AROR_EUnitType.MARKSMAN;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_SAPPER))
			return AROR_EUnitType.ENGINEER;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_RADIOOPERATOR))
			return AROR_EUnitType.RADIO_OPERATOR;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_RIFLEMAN))
			return AROR_EUnitType.RIFLEMAN;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	// Vehicles
	//------------------------------------------------------------------------------------------------
	protected void AddVehicle(SCR_EntityCatalogEntry entry, AROR_PresetImportResult result)
	{
		int type = VehicleType(entry);
		if (type < 0)
		{
			result.m_aSkipped.Insert("vehicle (no matching type): " + entry.GetPrefab());
			return;
		}

		AddEntry(result.m_aVehicles, "AROR_FactionPresetVehicle", AROR_EVehicleType, type, entry.GetPrefab());
	}

	//------------------------------------------------------------------------------------------------
	//! Static weapons are told apart by name: mortars, the tall 6T7 tripod (high) and the rest (low)
	protected int StaticType(string name)
	{
		if (name.Contains("mortar"))
			return AROR_EVehicleType.STATIC_MORTAR;
		if (name.Contains("6t7"))
			return AROR_EVehicleType.STATIC_MG_HIGH;

		return AROR_EVehicleType.STATIC_MG_LOW;
	}

	//------------------------------------------------------------------------------------------------
	protected int VehicleType(SCR_EntityCatalogEntry entry)
	{
		string name = LowerName(entry.GetPrefab());

		if (name.Contains("boat"))
			return AROR_EVehicleType.BOAT;

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TURRET))
			return StaticType(name);

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER))
		{
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED))
				return AROR_EVehicleType.HELI_ATTACK;

			return AROR_EVehicleType.HELI_TRANSPORT;
		}

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC))
		{
			// Tracked/armoured fighting vehicles carrying an autocannon are treated as IFVs by name
			if (name.Contains("bmp") || name.Contains("ifv"))
				return AROR_EVehicleType.IFV;

			return AROR_EVehicleType.APC;
		}

		if (name.Contains("tank") || name.Contains("t72") || name.Contains("m1a"))
			return AROR_EVehicleType.TANK;

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK))
		{
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED))
				return AROR_EVehicleType.ARMED_TRUCK;
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_REFUELING))
				return AROR_EVehicleType.FUEL_TRUCK;
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_MEDICAL))
				return AROR_EVehicleType.AMBULANCE;
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_SUPPLYSTORAGE_SMALL) || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_SUPPLYSTORAGE_LARGE)
				|| entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_LOGISTICS))
				return AROR_EVehicleType.SUPPLY_TRUCK;

			return AROR_EVehicleType.MOTOR_TRUCK;
		}

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_CAR))
		{
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_MEDICAL))
				return AROR_EVehicleType.AMBULANCE;
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED))
				return AROR_EVehicleType.ARMED_CAR;

			return AROR_EVehicleType.MOTOR_CAR;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	// Items: weapons, ammunition, accessories, gear and equipment, from the arsenal data
	//------------------------------------------------------------------------------------------------
	protected void AddItem(SCR_EntityCatalogEntry entry, AROR_PresetImportResult result)
	{
		SCR_ArsenalItem arsenal = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
		if (!arsenal)
		{
			result.m_aSkipped.Insert("item (no arsenal data): " + entry.GetPrefab());
			return;
		}

		ResourceName prefab = entry.GetPrefab();
		string name = LowerName(prefab);
		SCR_EArsenalItemType itemType = arsenal.GetItemType();
		SCR_EArsenalItemMode itemMode = arsenal.GetItemMode();

		// Munitions loaded onto aircraft and vehicles as items (e.g. helicopter rocket pods)
		if (itemMode == SCR_EArsenalItemMode.PYLON || itemType == SCR_EArsenalItemType.HELICOPTER || itemType == SCR_EArsenalItemType.VEHICLE)
		{
			int vehicleAmmunition = AROR_EAmmunitionType.ROCKET;
			if (name.Contains("missile") || name.Contains("atgm"))
				vehicleAmmunition = AROR_EAmmunitionType.MISSILE;
			else if (name.Contains("bomb"))
				vehicleAmmunition = AROR_EAmmunitionType.BOMB;

			AddEntry(result.m_aAmmunition, "AROR_FactionPresetAmmunition", AROR_EAmmunitionType, vehicleAmmunition, prefab);
			return;
		}

		if (itemMode == SCR_EArsenalItemMode.AMMUNITION)
		{
			int ammo = AmmunitionType(itemType, name);
			AddEntry(result.m_aAmmunition, "AROR_FactionPresetAmmunition", AROR_EAmmunitionType, ammo, prefab);
			return;
		}

		if (itemMode == SCR_EArsenalItemMode.ATTACHMENT || itemType == SCR_EArsenalItemType.WEAPON_ATTACHMENT)
		{
			AddEntry(result.m_aAccessories, "AROR_FactionPresetWeaponAccessory", AROR_EWeaponAccessoryType, AccessoryType(name), prefab);
			return;
		}

		int weapon = WeaponType(itemType, name);
		if (weapon >= 0)
		{
			AddEntry(result.m_aWeapons, "AROR_FactionPresetWeapon", AROR_EWeaponType, weapon, prefab);
			return;
		}

		int gear = GearType(itemType, name);
		if (gear >= 0)
		{
			AddEntry(result.m_aGear, "AROR_FactionPresetGear", AROR_EGearType, gear, prefab);
			return;
		}

		int equipment = EquipmentType(itemType, name);
		if (equipment >= 0)
		{
			AddEntry(result.m_aEquipment, "AROR_FactionPresetEquipment", AROR_EEquipmentType, equipment, prefab);
			return;
		}

		result.m_aSkipped.Insert(string.Format("item (%1 has no matching type): %2", typename.EnumToString(SCR_EArsenalItemType, itemType), prefab));
	}

	//------------------------------------------------------------------------------------------------
	protected int WeaponType(SCR_EArsenalItemType itemType, string name)
	{
		switch (itemType)
		{
			case SCR_EArsenalItemType.PISTOL:
				return AROR_EWeaponType.PISTOL;
			case SCR_EArsenalItemType.RIFLE:
				return AROR_EWeaponType.ASSAULT_RIFLE;
			case SCR_EArsenalItemType.MACHINE_GUN:
				return AROR_EWeaponType.MACHINEGUN;
			case SCR_EArsenalItemType.SNIPER_RIFLE:
				return AROR_EWeaponType.SNIPER_RIFLE;
			case SCR_EArsenalItemType.ROCKET_LAUNCHER:
				return AROR_EWeaponType.LAUNCHER_AT;
			case SCR_EArsenalItemType.LETHAL_THROWABLE:
			case SCR_EArsenalItemType.NON_LETHAL_THROWABLE:
				if (name.Contains("smoke"))
					return AROR_EWeaponType.SMOKE_GRENADE;

				return AROR_EWeaponType.EXPLOSIVE_GRENADE;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected int AmmunitionType(SCR_EArsenalItemType itemType, string name)
	{
		if (itemType == SCR_EArsenalItemType.ROCKET_LAUNCHER)
			return AROR_EAmmunitionType.ROCKET;
		if (itemType == SCR_EArsenalItemType.MORTARS)
			return AROR_EAmmunitionType.SHELL;
		if (name.Contains("40mm") || name.Contains("vog") || name.Contains("gp25") || name.Contains("m203") || name.Contains("grenade"))
			return AROR_EAmmunitionType.GRENADE_ROUND;
		if (itemType == SCR_EArsenalItemType.MACHINE_GUN || name.Contains("belt") || name.Contains("box"))
			return AROR_EAmmunitionType.BELT;

		return AROR_EAmmunitionType.MAGAZINE;
	}

	//------------------------------------------------------------------------------------------------
	protected int AccessoryType(string name)
	{
		if (name.Contains("suppressor") || name.Contains("silencer") || name.Contains("pbs"))
			return AROR_EWeaponAccessoryType.SUPPRESSOR;
		if (name.Contains("bayonet"))
			return AROR_EWeaponAccessoryType.BAYONET;
		if (name.Contains("ugl") || name.Contains("m203") || name.Contains("gp25") || name.Contains("bipod"))
			return AROR_EWeaponAccessoryType.UNDERBARREL;
		if (name.Contains("flashlight") || name.Contains("light"))
			return AROR_EWeaponAccessoryType.LIGHT;
		if (name.Contains("optic") || name.Contains("scope") || name.Contains("sight") || name.Contains("artii") || name.Contains("pso") || name.Contains("kobra") || name.Contains("_pu"))
			return AROR_EWeaponAccessoryType.OPTIC;

		return AROR_EWeaponAccessoryType.OTHER;
	}

	//------------------------------------------------------------------------------------------------
	protected int GearType(SCR_EArsenalItemType itemType, string name)
	{
		if (itemType == SCR_EArsenalItemType.HEADWEAR)
		{
			if (name.Contains("helmet") || name.Contains("pasgt") || name.Contains("ssh") || name.Contains("6b"))
				return AROR_EGearType.HELMET;
			if (name.Contains("glasses") || name.Contains("goggles") || name.Contains("balaclava") || name.Contains("mask"))
				return AROR_EGearType.FACEWEAR;

			return AROR_EGearType.HEADGEAR;
		}

		// Reforger splits clothing into jacket, trousers, boots and gloves; all count as uniform
		if (itemType == SCR_EArsenalItemType.TORSO || itemType == SCR_EArsenalItemType.LEGS
			|| itemType == SCR_EArsenalItemType.FOOTWEAR || itemType == SCR_EArsenalItemType.HANDWEAR)
			return AROR_EGearType.UNIFORM;

		if (itemType == SCR_EArsenalItemType.VEST_AND_WAIST)
			return AROR_EGearType.VEST;

		if (itemType == SCR_EArsenalItemType.BACKPACK || itemType == SCR_EArsenalItemType.RADIO_BACKPACK)
			return AROR_EGearType.BACKPACK;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected int EquipmentType(SCR_EArsenalItemType itemType, string name)
	{
		if (itemType == SCR_EArsenalItemType.HEAL)
			return AROR_EEquipmentType.MEDICAL;
		if (itemType == SCR_EArsenalItemType.EXPLOSIVES)
			return AROR_EEquipmentType.EXPLOSIVE;
		if (itemType != SCR_EArsenalItemType.EQUIPMENT)
			return -1;

		if (name.Contains("binocular"))
			return AROR_EEquipmentType.BINOCULARS;
		if (name.Contains("radio"))
			return AROR_EEquipmentType.RADIO;
		if (name.Contains("map"))
			return AROR_EEquipmentType.MAP;
		if (name.Contains("compass"))
			return AROR_EEquipmentType.COMPASS;
		if (name.Contains("watch"))
			return AROR_EEquipmentType.WATCH;
		if (name.Contains("flashlight") || name.Contains("torch"))
			return AROR_EEquipmentType.FLASHLIGHT;

		return AROR_EEquipmentType.TOOL;
	}

	//------------------------------------------------------------------------------------------------
	// Output
	//------------------------------------------------------------------------------------------------
	//! Types are written by enum name (e.g. "MOTOR_TRUCK"), like Workbench does, so the files are readable and do
	//! not silently change meaning if an enum is ever reordered
	protected void AddEntry(array<string> lines, string className, typename enumType, int type, ResourceName prefab)
	{
		lines.Insert(string.Format("  %1 {\n   m_sPrefab \"%2\"\n   m_eType %3\n  }", className, prefab, typename.EnumToString(enumType, type)));
	}

	//------------------------------------------------------------------------------------------------
	protected void WritePreset(AROR_BaseFactionImport factionImport, AROR_PresetImportResult result)
	{
		string path = OUTPUT_FOLDER + factionImport.m_sFile;

		// Units carry hand-authored loadouts and prefab choices: keep the existing block, generate only when there is none
		array<string> existingUnits = {};
		if (ReadTopLevelList(path, "m_aUnits", existingUnits) > 0)
		{
			result.m_aUnits.Clear();
			result.m_aUnits.InsertAll(existingUnits);
			Print(string.Format("[AROR] Import base faction presets: %1 kept its existing units (and their loadouts)", factionImport.m_sPresetId), LogLevel.NORMAL);
		}
		else
		{
			Print(string.Format("[AROR] Import base faction presets: %1 had no units, so they were generated from the base game", factionImport.m_sPresetId), LogLevel.NORMAL);
		}

		// The player loadout is hand-authored too and has no base-game source: always keep it
		array<string> playerLoadout = {};
		ReadTopLevelBlock(path, "m_PlayerLoadout", playerLoadout);

		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			FileIO.MakeDirectory(FALLBACK_FOLDER);
			path = FALLBACK_FOLDER + factionImport.m_sFile;
			file = FileIO.OpenFile(path, FileMode.WRITE);
			if (!file)
			{
				Print(string.Format("[AROR] Import base faction presets: cannot write %1", factionImport.m_sFile), LogLevel.ERROR);
				return;
			}

			Print(string.Format("[AROR] Import base faction presets: the addon folder is read-only here, so %1 was written to your profile folder; copy it into Configs/FactionPresets/BaseGame/", factionImport.m_sFile), LogLevel.WARNING);
		}

		file.WriteLine("AROR_FactionPreset {");
		file.WriteLine(string.Format(" m_sId \"%1\"", factionImport.m_sPresetId));
		file.WriteLine(string.Format(" m_sName \"%1\"", factionImport.m_sPresetName));
		file.WriteLine(" m_aUsableRoles {");
		foreach (int role : factionImport.m_aUsableRoles)
		{
			file.WriteLine(string.Format("  %1", typename.EnumToString(AROR_EFactionRole, role)));
		}
		file.WriteLine(" }");

		WriteList(file, "m_aUnits", result.m_aUnits);
		foreach (string playerLine : playerLoadout)
		{
			file.WriteLine(playerLine);
		}

		// Items (gear, weapons, ammunition...) are not part of presets: they belong to content packs
		WriteList(file, "m_aVehicles", result.m_aVehicles);
		file.WriteLine("}");
		file.Close();

		Print(string.Format("[AROR] Import base faction presets: %1 (%2) -> %3: %4 units, %5 vehicles, %6 skipped",
			factionImport.m_sPresetId, factionImport.m_sFactionKey, path, result.m_aUnits.Count(), result.m_aVehicles.Count(), result.m_aSkipped.Count()), LogLevel.NORMAL);

		foreach (string skipped : result.m_aSkipped)
		{
			Print(string.Format("[AROR]   %1 skipped %2", factionImport.m_sPresetId, skipped), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void WriteList(FileHandle file, string listName, array<string> lines)
	{
		if (lines.IsEmpty())
			return;

		file.WriteLine(string.Format(" %1 {", listName));
		foreach (string line : lines)
		{
			file.WriteLine(line);
		}
		file.WriteLine(" }");
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the lines inside a top-level list of an existing preset file (between " name {" and its closing " }", both
	//! at one-space indent as Workbench and this plugin write them), verbatim. Returns how many lines were added; 0 when
	//! the file or the list does not exist.
	protected int ReadTopLevelList(string path, string listName, notnull array<string> outLines)
	{
		if (!FileIO.FileExists(path))
			return 0;

		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return 0;

		string header = string.Format(" %1 {", listName);
		bool inside;
		string line;
		while (file.ReadLine(line) >= 0)
		{
			// Lines may keep the carriage return of CRLF files
			line.Replace("\r", string.Empty);

			if (!inside)
			{
				inside = line == header;
				continue;
			}

			if (line == " }")
				break;

			outLines.Insert(line);
		}

		file.Close();
		return outLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a whole top-level object member of an existing preset file, verbatim: its " name Class {" line, its body and
	//! its closing " }". Returns how many lines were added; 0 when the file or the member does not exist.
	protected int ReadTopLevelBlock(string path, string memberName, notnull array<string> outLines)
	{
		if (!FileIO.FileExists(path))
			return 0;

		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return 0;

		string prefix = string.Format(" %1 ", memberName);
		bool inside;
		string line;
		while (file.ReadLine(line) >= 0)
		{
			line.Replace("\r", string.Empty);
			if (!inside)
			{
				inside = line.IndexOf(prefix) == 0;
				if (!inside)
					continue;
			}

			outLines.Insert(line);
			if (line == " }")
				break;
		}

		file.Close();
		return outLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Lower-case file name of a prefab, for name hints
	protected static string LowerName(ResourceName prefab)
	{
		string path = prefab;
		int slash = path.LastIndexOf("/");
		if (slash >= 0)
			path = path.Substring(slash + 1, path.Length() - slash - 1);

		path.ToLower();
		return path;
	}
}
