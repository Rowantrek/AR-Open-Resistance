//! Exports a content pack (see ARCHITECTURE.md -> Content Packs) from the entity catalogs of chosen addons. Run it from a
//! scratch addon that depends on Open Resistance and on the mods to export, so the mods are loaded in Workbench without
//! becoming dependencies of the core. Each chosen addon's catalogs (standalone catalog configs and the catalogs inside
//! faction configs) are sorted into content pack types with the same rules as the base faction import, which this
//! reuses. The pack has one group per chosen addon whose files hold the entries' prefabs, with that addon as the group's
//! Required Addons, so the pack is always available and only the entries of loaded mods are offered. Prefabs held by any other addon (the base game
//! re-listed by a hub mod, a mod that was not chosen) are left out. Characters are skipped (packs have no units).
//! Prices, readiness, import requirements and replacements are not set: costs stay at 0 (free) until tuned by hand.
//! The pack is written to the profile folder, never into an addon, so tuned packs are not overwritten. Set the existing
//! pack file to skip its prefabs and export only what is new.
//! One loaded addon in the export dialog: a tick box titled with the addon's name
[BaseContainerProps(), BaseContainerCustomTitleField("m_sLabel")]
class AROR_ExportAddonOption
{
	[Attribute("", desc: "Addon title and ID (informational)")]
	string m_sLabel;

	[Attribute("0", desc: "Export this addon")]
	bool m_bExport;

	// An attribute so it survives the dialog, which rebuilds these objects from their attributes
	[Attribute("", desc: "Addon GUID (informational)")]
	string m_sGuid;
}

[WorkbenchPluginAttribute(name: "Export content pack from addons", description: "Write a content pack config from the entity catalogs of the chosen loaded addons.", wbModules: {"WorldEditor", "ResourceManager"}, category: "Open Resistance")]
class AROR_ExportContentPackPlugin : AROR_ImportBaseFactionPresetsPlugin
{
	protected static const string EXPORT_FOLDER = "$profile:AROR_ContentPacks/";

	[Attribute(desc: "Tick the addons to export")]
	protected ref array<ref AROR_ExportAddonOption> m_aOptions;

	[Attribute("", desc: "Also export loaded addons matching these comma-separated words (GUID, ID or part of the title, e.g. WCS, AKI). Optional.")]
	protected string m_sAddons;

	[Attribute("WCS", desc: "Pack ID, unique and permanent once released")]
	protected string m_sPackId;

	[Attribute("WCS", desc: "Pack name shown to the admin")]
	protected string m_sPackName;

	[Attribute("AROR_ContentPack_WCS.conf", desc: "File name written to the profile folder")]
	protected string m_sFileName;

	[Attribute("$AROR_WCSCompat:Configs/ContentPacks/AROR_ContentPack_WCS.conf", desc: "Existing pack whose prefabs are skipped, so only new entries are exported. Ignored when the file does not exist.")]
	protected string m_sExistingPack;

	protected ref array<ResourceName> m_aConfs = {};
	protected ref set<string> m_Seen = new set<string>();
	protected ref set<string> m_Foreign = new set<string>();
	protected ref map<string, string> m_mOwner = new map<string, string>();			// prefab GUID -> GUID of the chosen addon holding it
	protected ref set<string> m_BaseGuids = new set<string>();							// prefab GUIDs of the base game
	protected ref map<string, string> m_mOtherOwner = new map<string, string>();		// prefab GUID -> title of another loaded addon, for the log
	protected ref map<string, int> m_mForeignByAddon = new map<string, int>();
	protected ref map<string, ref array<string>> m_mGroupLines = new map<string, ref array<string>>();	// addon GUID -> entries to write in its group
	protected int m_iCollect;
	protected string m_sCollectOwner;
	protected string m_sCollectTitle;
	protected int m_iFound;
	protected int m_iCatalogs;

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		if (!GetGame())
		{
			Print("[AROR] Export content pack: run this from the World Editor (Plugins > Open Resistance)", LogLevel.ERROR);
			return;
		}

		RefreshOptions();
		ShowDialog();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowDialog()
	{
		Workbench.ScriptDialog("Export content pack from addons", "Tick the addons to export, then press Export. The pack is written to your profile folder (see the log for the path).", this);
	}

	//------------------------------------------------------------------------------------------------
	//! One tick box per loaded addon (excluding the base game and ours). Ticks are kept between runs of the dialog.
	protected void RefreshOptions()
	{
		if (!m_aOptions)
			m_aOptions = {};

		array<string> guids = {};
		GameProject.GetLoadedAddons(guids);

		array<ref AROR_ExportAddonOption> options = {};
		foreach (string guid : guids)
		{
			if (IsExcluded(guid))
				continue;

			AROR_ExportAddonOption option = new AROR_ExportAddonOption();
			option.m_sGuid = guid;
			option.m_sLabel = string.Format("%1  (%2)", GameProject.GetAddonTitle(guid), GameProject.GetAddonID(guid));
			foreach (AROR_ExportAddonOption previous : m_aOptions)
			{
				if (previous.m_sGuid == guid)
					option.m_bExport = previous.m_bExport;
			}

			options.Insert(option);
		}

		m_aOptions = options;
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Select all")]
	protected void ButtonSelectAll()
	{
		// Every button closes the dialog, so show it again with the new ticks
		SetAll(true);
		ShowDialog();
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Select none")]
	protected void ButtonSelectNone()
	{
		SetAll(false);
		ShowDialog();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetAll(bool export)
	{
		if (!m_aOptions)
			return;

		foreach (AROR_ExportAddonOption option : m_aOptions)
		{
			option.m_bExport = export;
		}
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Export", true)]
	protected void ButtonExport()
	{
		array<string> wanted = {};
		m_sAddons.Split(",", wanted, true);

		array<string> chosen = {};
		foreach (AROR_ExportAddonOption option : m_aOptions)
		{
			if (option.m_sGuid.IsEmpty())
				continue;

			if (option.m_bExport || (!wanted.IsEmpty() && MatchesAny(option.m_sGuid, wanted)))
				chosen.Insert(option.m_sGuid);
		}

		if (chosen.IsEmpty())
		{
			Print("[AROR] Export content pack: no addon is ticked", LogLevel.ERROR);
			return;
		}

		if (!BuildOwnerIndex(chosen))
			return;

		m_Seen.Clear();
		m_Foreign.Clear();
		m_mForeignByAddon.Clear();
		m_mGroupLines.Clear();
		LoadExistingPrefabs();

		AROR_PresetImportResult result = new AROR_PresetImportResult();
		foreach (string guid : chosen)
		{
			ExportAddon(guid, result);
		}

		WritePack(result);
	}

	//------------------------------------------------------------------------------------------------
	//! The base game, Open Resistance itself and the WCS compat addon are never exported, whatever the filter says
	protected bool IsExcluded(string guid)
	{
		if (GameProject.IsVanillaAddon(guid))
			return true;

		string id = GameProject.GetAddonID(guid);
		return id == "ArmaReforger" || id == "OpenResistance" || id == "AROR_WCSCompat";
	}

	//------------------------------------------------------------------------------------------------
	protected bool MatchesAny(string guid, notnull array<string> wanted)
	{
		string id = GameProject.GetAddonID(guid);
		string title = GameProject.GetAddonTitle(guid);
		id.ToLower();
		title.ToLower();
		string lowerGuid = guid;
		lowerGuid.ToLower();

		foreach (string token : wanted)
		{
			string word = token.Trim();
			word.ToLower();
			if (word.IsEmpty())
				continue;

			if (word == lowerGuid || word == id || title.Contains(word))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefabs already in the existing pack count as seen, so they are not exported again
	protected void LoadExistingPrefabs()
	{
		if (m_sExistingPack.IsEmpty() || !FileIO.FileExists(m_sExistingPack))
			return;

		FileHandle file = FileIO.OpenFile(m_sExistingPack, FileMode.READ);
		if (!file)
			return;

		string line;
		while (file.ReadLine(line) >= 0)
		{
			if (!line.Contains("m_sPrefab"))
				continue;

			array<string> parts = {};
			line.Split("\"", parts, false);
			if (parts.Count() > 1)
				m_Seen.Insert(parts[1]);
		}

		file.Close();
		Print(string.Format("[AROR] Export content pack: %1 prefabs already in %2 will be skipped", m_Seen.Count(), m_sExistingPack), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Indexes which addon holds each prefab, by prefab GUID, so an entry is attributed to the addon whose files contain
	//! it. Catalogs often list items of other addons (a hub mod re-listing the base game, or another mod), and those must
	//! not be tagged with the catalog's addon. Paths are not used: Workbench will not turn an addon file into an absolute
	//! path here, but a search of an addon does return the GUID of every prefab inside it. The base game is indexed too,
	//! so a prefab that exists there (including one a mod only overrides) is left out: the base game pack offers it.
	//! Returns false when the chosen addons hold no prefab at all.
	protected bool BuildOwnerIndex(notnull array<string> chosen)
	{
		m_mOwner.Clear();
		m_BaseGuids.Clear();
		m_mOtherOwner.Clear();

		int total;
		foreach (string guid : chosen)
		{
			IndexAddon(guid, 0);
			total += m_iFound;
			Print(string.Format("[AROR] Export content pack: addon %1 (%2) holds %3 prefabs", GameProject.GetAddonID(guid), guid, m_iFound), LogLevel.NORMAL);
		}

		if (total == 0)
		{
			Print("[AROR] Export content pack: the chosen addons hold no prefabs; nothing was exported", LogLevel.ERROR);
			return false;
		}

		array<string> loaded = {};
		GameProject.GetLoadedAddons(loaded);
		foreach (string guid : loaded)
		{
			if (chosen.Find(guid) != -1)
				continue;

			if (GameProject.IsVanillaAddon(guid) || GameProject.GetAddonID(guid) == "ArmaReforger")
				IndexAddon(guid, 1);
			else if (!IsExcluded(guid))
				IndexAddon(guid, 2);
		}

		Print(string.Format("[AROR] Export content pack: the base game holds %1 prefabs", m_BaseGuids.Count()), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! mode 0: prefabs of a chosen addon, 1: of the base game, 2: of another loaded addon (to name who owns a left-out prefab)
	protected void IndexAddon(string guid, int mode)
	{
		m_iCollect = mode;
		m_sCollectOwner = guid;
		m_sCollectTitle = GameProject.GetAddonTitle(guid);
		m_iFound = 0;
		Workbench.SearchResources(OnPrefabFound, {"et"}, null, "$" + GameProject.GetAddonID(guid) + ":", true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPrefabFound(ResourceName resName, string filePath = "")
	{
		string guid = GuidOf(resName);
		if (guid.IsEmpty())
			return;

		m_iFound++;
		switch (m_iCollect)
		{
			case 0:
				m_mOwner.Set(guid, m_sCollectOwner);
				break;
			case 1:
				m_BaseGuids.Insert(guid);
				break;
			case 2:
				m_mOtherOwner.Set(guid, m_sCollectTitle);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The GUID inside the braces of a resource name, e.g. "{A671B84699A59E15}Prefabs/x.et"
	protected string GuidOf(ResourceName prefab)
	{
		string text = prefab;
		int end = text.IndexOf("}");
		if (text.IndexOf("{") != 0 || end < 2)
			return string.Empty;

		return text.Substring(1, end - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! GUID of the chosen addon whose files hold the prefab; empty when it belongs to the base game or any addon that was
	//! not chosen
	protected string FindOwner(ResourceName prefab)
	{
		string guid = GuidOf(prefab);
		string owner;
		if (guid.IsEmpty() || m_BaseGuids.Contains(guid) || !m_mOwner.Find(guid, owner))
			return string.Empty;

		return owner;
	}

	//------------------------------------------------------------------------------------------------
	//! Who holds a prefab that was left out, for the log
	protected string DescribeForeign(ResourceName prefab)
	{
		string guid = GuidOf(prefab);
		if (m_BaseGuids.Contains(guid))
			return "base game (listed or overridden)";

		string title;
		if (m_mOtherOwner.Find(guid, title))
			return title;

		return "(an addon that is not loaded)";
	}

	//------------------------------------------------------------------------------------------------
	protected void ExportAddon(string guid, AROR_PresetImportResult result)
	{
		string id = GameProject.GetAddonID(guid);
		m_aConfs.Clear();
		m_iCatalogs = 0;

		// The callback only collects; the configs are read afterwards
		Workbench.SearchResources(OnConfFound, {"conf"}, null, "$" + id + ":", true);

		foreach (ResourceName conf : m_aConfs)
		{
			ReadConf(conf, result);
		}

		Print(string.Format("[AROR] Export content pack: %1 (%2): %3 configs searched, %4 catalogs read", id, guid, m_aConfs.Count(), m_iCatalogs), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnConfFound(ResourceName resName, string filePath = "")
	{
		m_aConfs.Insert(resName);
	}

	//------------------------------------------------------------------------------------------------
	//! A standalone catalog, or a faction config carrying catalogs; any other config is ignored
	protected void ReadConf(ResourceName conf, AROR_PresetImportResult result)
	{
		Resource resource = BaseContainerTools.LoadContainer(conf);
		if (!resource || !resource.IsValid())
			return;

		BaseContainer container = resource.GetResource().ToBaseContainer();
		if (!container)
			return;

		string className = container.GetClassName();
		if (className.Contains("EntityCatalog"))
		{
			ReadCatalog(container, result);
		}
		else if (className.Contains("Faction"))
		{
			BaseContainerList catalogs = container.GetObjectArray("m_aEntityCatalogs");
			if (!catalogs)
				return;

			for (int i = 0; i < catalogs.Count(); i++)
			{
				BaseContainer catalogContainer = catalogs.Get(i);
				if (catalogContainer)
					ReadCatalog(catalogContainer, result);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadCatalog(BaseContainer container, AROR_PresetImportResult result)
	{
		SCR_EntityCatalog catalog = SCR_EntityCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
		if (!catalog)
			return;

		// Merges multi-lists, drops disabled entries and loads each entry's labels and data
		catalog.InitCatalog();
		m_iCatalogs++;

		array<SCR_EntityCatalogEntry> entries = {};
		catalog.GetEntityList(entries);

		EEntityCatalogType catalogType = catalog.GetCatalogType();
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			switch (catalogType)
			{
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

	//------------------------------------------------------------------------------------------------
	// Type rules for what mods add that the base game's sorting does not place
	//------------------------------------------------------------------------------------------------
	override protected void AddItem(SCR_EntityCatalogEntry entry, AROR_PresetImportResult result)
	{
		ResourceName prefab = entry.GetPrefab();
		string name = LowerName(prefab);
		SCR_ArsenalItem arsenal = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));

		// Under-barrel launchers that carry no arsenal data
		if (!arsenal && name.Contains("ugl"))
		{
			AddEntry(result.m_aAccessories, "AROR_FactionPresetWeaponAccessory", AROR_EWeaponAccessoryType, AROR_EWeaponAccessoryType.UNDERBARREL, prefab);
			return;
		}

		// Mortar rounds and their ammo boxes are ammunition; the ballistic tables fall through to equipment
		if (arsenal && arsenal.GetItemType() == SCR_EArsenalItemType.MORTARS && arsenal.GetItemMode() != SCR_EArsenalItemMode.AMMUNITION && !name.Contains("ballistic"))
		{
			AddEntry(result.m_aAmmunition, "AROR_FactionPresetAmmunition", AROR_EAmmunitionType, AROR_EAmmunitionType.SHELL, prefab);
			return;
		}

		super.AddItem(entry, result);
	}

	//------------------------------------------------------------------------------------------------
	override protected int GearType(SCR_EArsenalItemType itemType, string name)
	{
		// ARMBAND is added to the enum by a mod (WCS), not the base game, so it is matched by name: naming it directly
		// would not compile wherever that mod's scripts are not loaded first
		if (typename.EnumToString(SCR_EArsenalItemType, itemType) == "ARMBAND")
			return AROR_EGearType.OTHER;

		return super.GearType(itemType, name);
	}

	//------------------------------------------------------------------------------------------------
	override protected int EquipmentType(SCR_EArsenalItemType itemType, string name)
	{
		if (itemType == SCR_EArsenalItemType.MORTARS && name.Contains("ballistic"))
			return AROR_EEquipmentType.TOOL;

		return super.EquipmentType(itemType, name);
	}

	//------------------------------------------------------------------------------------------------
	override protected int AccessoryType(string name)
	{
		// The AP2k collimator sight
		if (name.Contains("collim") || name.Contains("ap2k"))
			return AROR_EWeaponAccessoryType.OPTIC;

		return super.AccessoryType(name);
	}

	//------------------------------------------------------------------------------------------------
	override protected int VehicleType(SCR_EntityCatalogEntry entry)
	{
		// The base sorting reads "tank" in "tanker" as a battle tank
		if (LowerName(entry.GetPrefab()).Contains("tanker"))
			return AROR_EVehicleType.FUEL_TRUCK;

		return super.VehicleType(entry);
	}

	//------------------------------------------------------------------------------------------------
	//! The base importer's sorting writes preset entries; this writes the same entry as a content pack entry of the
	//! addon being exported, once per prefab
	override protected void AddEntry(array<string> lines, string className, typename enumType, int type, ResourceName prefab)
	{
		if (m_Seen.Contains(prefab) || m_Foreign.Contains(prefab))
			return;

		// Only what the chosen addons themselves contain; the owner, not the catalog's addon, is the requirement
		string owner = FindOwner(prefab);
		if (owner.IsEmpty())
		{
			m_Foreign.Insert(prefab);
			string other = DescribeForeign(prefab);
			m_mForeignByAddon.Set(other, m_mForeignByAddon.Get(other) + 1);
			return;
		}

		m_Seen.Insert(prefab);
		string packClass = className;
		packClass.Replace("AROR_FactionPreset", "AROR_ContentPack");

		// The category list only counts entries for the summary; the entry itself goes into its addon's group
		string entry = string.Format("    %1 {\n     m_sPrefab \"%2\"\n     m_eType %3\n     m_iCost 0\n    }", packClass, prefab, typename.EnumToString(enumType, type));
		lines.Insert(entry);

		array<string> group;
		if (!m_mGroupLines.Find(owner, group))
		{
			group = {};
			m_mGroupLines.Set(owner, group);
		}

		group.Insert(entry);
	}

	//------------------------------------------------------------------------------------------------
	protected void WritePack(AROR_PresetImportResult result)
	{
		FileIO.MakeDirectory(EXPORT_FOLDER);
		string path = EXPORT_FOLDER + m_sFileName;
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			Print(string.Format("[AROR] Export content pack: cannot write %1", path), LogLevel.ERROR);
			return;
		}

		file.WriteLine("AROR_ContentPack {");
		file.WriteLine(string.Format(" m_sId \"%1\"", m_sPackId));
		file.WriteLine(string.Format(" m_sName \"%1\"", m_sPackName));

		// One group per addon, holding the entries whose prefabs that addon contains
		file.WriteLine(" m_aGroups {");
		for (int group = 0; group < m_mGroupLines.Count(); group++)
		{
			string owner = m_mGroupLines.GetKey(group);
			file.WriteLine("  AROR_ContentPackGroup {");
			file.WriteLine(string.Format("   m_sLabel \"%1\"", GameProject.GetAddonTitle(owner)));
			file.WriteLine("   m_aRequiredAddons {");
			file.WriteLine(string.Format("    \"%1\"", owner));
			file.WriteLine("   }");
			file.WriteLine("   m_aEntries {");
			foreach (string entryLines : m_mGroupLines.GetElement(group))
			{
				file.WriteLine(entryLines);
			}

			file.WriteLine("   }");
			file.WriteLine("  }");
		}

		file.WriteLine(" }");
		file.WriteLine("}");
		file.Close();

		string absolute;
		Workbench.GetAbsolutePath(path, absolute, false);
		Print(string.Format("[AROR] Export content pack: wrote %1", absolute), LogLevel.NORMAL);
		// The same catalog entry is met once per catalog that lists it
		set<string> uniqueSkipped = new set<string>();
		foreach (string skipped : result.m_aSkipped)
		{
			uniqueSkipped.Insert(skipped);
		}

		Print(string.Format("[AROR] Export content pack: %1 vehicles, %2 gear, %3 equipment, %4 weapons, %5 accessories, %6 ammunition; %7 prefabs belong to other addons and were left out; %8 could not be typed",
			result.m_aVehicles.Count(), result.m_aGear.Count(), result.m_aEquipment.Count(), result.m_aWeapons.Count(),
			result.m_aAccessories.Count(), result.m_aAmmunition.Count(), m_Foreign.Count(), uniqueSkipped.Count()), LogLevel.NORMAL);

		for (int addon = 0; addon < m_mForeignByAddon.Count(); addon++)
		{
			Print(string.Format("[AROR]   left out %1 prefabs of: %2", m_mForeignByAddon.GetElement(addon), m_mForeignByAddon.GetKey(addon)), LogLevel.NORMAL);
		}

		for (int i = 0; i < uniqueSkipped.Count(); i++)
		{
			Print("[AROR]   skipped " + uniqueSkipped.Get(i), LogLevel.NORMAL);
		}
	}
}
