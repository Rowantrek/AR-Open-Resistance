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
//! rewrites the preset files. Prices, readiness and import requirements live in the Base Game content pack, which this
//! never touches; add new prefabs there by hand. Anything that cannot be placed is listed in the log.
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
	//! Every catalog of the faction: characters, vehicles, static weapons and inventory items, sorted by
	//! AROR_CatalogClassifier (the same rules the runtime catalog augmentation uses)
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
				AROR_EFactionPresetCategory category;
				int type;
				if (!AROR_CatalogClassifier.Classify(catalogType, entry, category, type))
				{
					result.m_aSkipped.Insert(string.Format("%1 (no matching type): %2", typename.EnumToString(EEntityCatalogType, catalogType), entry.GetPrefab()));
					continue;
				}

				AddClassified(category, type, entry.GetPrefab(), result);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddClassified(AROR_EFactionPresetCategory category, int type, ResourceName prefab, AROR_PresetImportResult result)
	{
		switch (category)
		{
			case AROR_EFactionPresetCategory.UNITS:
				// One unit per type: the first match wins
				if (!result.m_UnitTypesUsed.Insert(type))
				{
					result.m_aSkipped.Insert(string.Format("unit (%1 already set): %2", typename.EnumToString(AROR_EUnitType, type), prefab));
					return;
				}

				AddEntry(result.m_aUnits, "AROR_FactionPresetUnit", AROR_EUnitType, type, prefab);
				break;
			case AROR_EFactionPresetCategory.VEHICLES:
				AddEntry(result.m_aVehicles, "AROR_FactionPresetVehicle", AROR_EVehicleType, type, prefab);
				break;
			case AROR_EFactionPresetCategory.GEAR:
				AddEntry(result.m_aGear, "AROR_FactionPresetGear", AROR_EGearType, type, prefab);
				break;
			case AROR_EFactionPresetCategory.EQUIPMENT:
				AddEntry(result.m_aEquipment, "AROR_FactionPresetEquipment", AROR_EEquipmentType, type, prefab);
				break;
			case AROR_EFactionPresetCategory.WEAPONS:
				AddEntry(result.m_aWeapons, "AROR_FactionPresetWeapon", AROR_EWeaponType, type, prefab);
				break;
			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES:
				AddEntry(result.m_aAccessories, "AROR_FactionPresetWeaponAccessory", AROR_EWeaponAccessoryType, type, prefab);
				break;
			case AROR_EFactionPresetCategory.AMMUNITION:
				AddEntry(result.m_aAmmunition, "AROR_FactionPresetAmmunition", AROR_EAmmunitionType, type, prefab);
				break;
		}
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

		WriteList(file, "m_aVehicles", result.m_aVehicles);
		WriteList(file, "m_aGear", result.m_aGear);
		WriteList(file, "m_aEquipment", result.m_aEquipment);
		WriteList(file, "m_aWeapons", result.m_aWeapons);
		WriteList(file, "m_aWeaponAccessories", result.m_aAccessories);
		WriteList(file, "m_aAmmunition", result.m_aAmmunition);
		file.WriteLine("}");
		file.Close();

		Print(string.Format("[AROR] Import base faction presets: %1 (%2) -> %3: %4 units, %5 vehicles, %6 gear, %7 equipment, %8 weapons, %9 accessories",
			factionImport.m_sPresetId, factionImport.m_sFactionKey, path, result.m_aUnits.Count(), result.m_aVehicles.Count(), result.m_aGear.Count(),
			result.m_aEquipment.Count(), result.m_aWeapons.Count(), result.m_aAccessories.Count()), LogLevel.NORMAL);
		Print(string.Format("[AROR] Import base faction presets: %1: %2 ammunition, %3 skipped", factionImport.m_sPresetId, result.m_aAmmunition.Count(), result.m_aSkipped.Count()), LogLevel.NORMAL);

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
}
