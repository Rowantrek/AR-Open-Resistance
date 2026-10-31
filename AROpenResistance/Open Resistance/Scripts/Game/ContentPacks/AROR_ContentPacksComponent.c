[ComponentEditorProps(category: "OpenResistance/ContentPacks", description: "Content packs: loads the packs, tracks which are available and enabled, and answers price, readiness and replacement queries. Attach to the game mode entity.")]
class AROR_ContentPacksComponentClass : SCR_BaseGameModeComponentClass
{
}

void AROR_OnContentPacksChanged();
typedef func AROR_OnContentPacksChanged;

//! Content packs (see ARCHITECTURE.md -> Content Packs). Every machine loads the packs listed in the registry and
//! works out which are available (all their required addons loaded). The server owns which available packs are
//! turned off in this campaign: replicated and saved (AROR_ContentPacksComponentSerializer). Storing the disabled
//! packs, not the enabled ones, means a pack added after a campaign started is on by default.
//! From the enabled packs, in registry order, it builds one catalogue (prefab -> entry, the earlier pack winning) and
//! one replacement map (replaced prefab -> replacer). Shops sell the catalogue; faction preset entries resolve their
//! prefabs through the replacements; enemy systems check a prefab's minimum readiness.
class AROR_ContentPacksComponent : SCR_BaseGameModeComponent
{
	[Attribute("{7C2E4B9A1D3F5E60}Configs/ContentPacks/AROR_ContentPackRegistry.conf", UIWidgets.ResourceNamePicker, desc: "Registry listing the content packs", params: "conf class=AROR_ContentPackRegistry")]
	protected ResourceName m_sRegistry;

	protected static const string ID_SEPARATOR = ",";
	protected static const int MAX_REPLACEMENT_DEPTH = 8;

	protected static AROR_ContentPacksComponent s_Instance;

	//! All loaded packs, in registry order
	protected ref array<ref AROR_ContentPack> m_aPacks = {};

	//! IDs of the loaded packs whose required addons are all loaded
	protected ref set<string> m_AvailablePackIds = new set<string>();

	// Pack IDs the admin turned off in this campaign, joined by ID_SEPARATOR (server-owned, replicated)
	[RplProp(onRplName: "OnOverridesReplicated")]
	protected string m_sDisabledPacks;

	// Pack IDs that are not on by default and the admin turned on in this campaign (server-owned, replicated)
	[RplProp(onRplName: "OnOverridesReplicated")]
	protected string m_sEnabledPacks;

	//! Client: a rebuild is queued for the next frame (see OnOverridesReplicated)
	protected bool m_bRebuildQueued;

	// Built from the enabled packs whenever they change
	protected ref map<ResourceName, AROR_ContentPackEntry> m_mCatalogue = new map<ResourceName, AROR_ContentPackEntry>();
	protected ref array<AROR_ContentPackEntry> m_aCatalogueOrder = {};
	protected ref map<ResourceName, ResourceName> m_mReplacements = new map<ResourceName, ResourceName>();
	//! Prefabs already logged by IsOffered, so each is reported once
	protected ref set<string> m_NotOfferedWarned = new set<string>();

	//! Static so it never repeats within one process (static shop caches outlive a Workbench play session)
	protected static int s_iRevision;

	protected ref ScriptInvokerBase<AROR_OnContentPacksChanged> m_OnPacksChanged = new ScriptInvokerBase<AROR_OnContentPacksChanged>();

	//------------------------------------------------------------------------------------------------
	static AROR_ContentPacksComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab to use for a listed one: its replacement from an enabled pack, or the prefab itself. Safe to call
	//! before the component exists (returns the prefab unchanged).
	static ResourceName Resolve(ResourceName prefab)
	{
		if (!s_Instance || prefab.IsEmpty())
			return prefab;

		return s_Instance.ResolvePrefab(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! True when an enabled pack offers the prefab, so a faction preset may use it. Otherwise logs once per prefab
	//! (source names who asked, e.g. the preset ID) and returns false. True before the component exists.
	static bool IsOffered(ResourceName prefab, string source)
	{
		if (!s_Instance || prefab.IsEmpty() || s_Instance.FindEntry(prefab))
			return true;

		if (s_Instance.m_NotOfferedWarned.Insert(prefab))
			Print(string.Format("[AROR] '%1' lists %2, which no enabled content pack offers; it is not used", source, prefab), LogLevel.WARNING);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine; fires when the enabled packs change (campaign start, a save restored, admin toggle, replicated)
	ScriptInvokerBase<AROR_OnContentPacksChanged> GetOnPacksChanged()
	{
		return m_OnPacksChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Changes every time the catalogue is rebuilt; shops use it as their cache key
	int GetRevision()
	{
		return s_iRevision;
	}

	//------------------------------------------------------------------------------------------------
	// Packs
	//------------------------------------------------------------------------------------------------
	//! Adds every loaded pack (available or not), in registry order; returns how many were added
	int GetPacks(notnull array<AROR_ContentPack> outPacks)
	{
		foreach (AROR_ContentPack pack : m_aPacks)
		{
			outPacks.Insert(pack);
		}

		return m_aPacks.Count();
	}

	//------------------------------------------------------------------------------------------------
	AROR_ContentPack FindPack(string packId)
	{
		foreach (AROR_ContentPack pack : m_aPacks)
		{
			if (pack.GetId() == packId)
				return pack;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the pack is loaded and all the addons it needs are loaded
	bool IsAvailable(string packId)
	{
		return m_AvailablePackIds.Contains(packId);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the pack is available and on in this campaign
	bool IsEnabled(string packId)
	{
		if (!IsAvailable(packId))
			return false;

		AROR_ContentPack pack = FindPack(packId);
		if (pack.IsEnabledByDefault())
			return !ContainsId(m_sDisabledPacks, packId);

		return ContainsId(m_sEnabledPacks, packId);
	}

	//------------------------------------------------------------------------------------------------
	// Catalogue
	//------------------------------------------------------------------------------------------------
	//! The prefab's replacement from an enabled pack (following chains), or the prefab itself
	ResourceName ResolvePrefab(ResourceName prefab)
	{
		ResourceName resolved = prefab;
		ResourceName replacement;
		for (int depth = 0; depth < MAX_REPLACEMENT_DEPTH; depth++)
		{
			if (!m_mReplacements.Find(resolved, replacement))
				break;

			resolved = replacement;
		}

		return resolved;
	}

	//------------------------------------------------------------------------------------------------
	//! The enabled entry for the prefab, or null if no enabled pack offers it (or it is replaced)
	AROR_ContentPackEntry FindEntry(ResourceName prefab)
	{
		return m_mCatalogue.Get(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the prefabs the weapon or vehicle is listed as working with, limited to those an enabled pack offers
	//! (replacements applied, no duplicates); returns how many were added. Nothing uses this yet.
	int GetCompatible(ResourceName prefab, notnull array<ResourceName> outPrefabs)
	{
		AROR_ContentPackCompatibleEntry entry = AROR_ContentPackCompatibleEntry.Cast(FindEntry(prefab));
		if (!entry)
			return 0;

		array<ResourceName> listed = {};
		entry.GetCompatible(listed);

		int added;
		foreach (ResourceName item : listed)
		{
			ResourceName resolved = ResolvePrefab(item);
			if (!FindEntry(resolved) || outPrefabs.Contains(resolved))
				continue;

			outPrefabs.Insert(resolved);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every enabled entry of the category, in pack order then list order; returns how many were added
	int GetEnabledEntries(AROR_EFactionPresetCategory category, notnull array<AROR_ContentPackEntry> outEntries)
	{
		int added;
		foreach (AROR_ContentPackEntry entry : m_aCatalogueOrder)
		{
			if (entry.GetCategory() == category)
			{
				outEntries.Insert(entry);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Readiness from which the prefab may be used: its enabled entry's minimum, or 1 if no enabled pack lists it
	int GetMinimumReadiness(ResourceName prefab)
	{
		AROR_ContentPackEntry entry = FindEntry(prefab);
		if (!entry)
			return 1;

		return entry.GetMinimumReadinessLevel();
	}

	//------------------------------------------------------------------------------------------------
	//! True when the role may field the prefab at its current readiness. Roles without readiness (Resistance,
	//! Civilians) may always use it.
	static bool IsAvailableAtReadiness(ResourceName prefab, AROR_EFactionRole role)
	{
		if (!s_Instance || (role != AROR_EFactionRole.OCCUPIERS && role != AROR_EFactionRole.INVADERS))
			return true;

		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return true;

		return s_Instance.GetMinimumReadiness(prefab) <= operations.GetReadinessLevel(role);
	}

	//------------------------------------------------------------------------------------------------
	// Server
	//------------------------------------------------------------------------------------------------
	//! Server only. Turns an available pack on or off for this campaign.
	void SetPackEnabled_S(string packId, bool enabled)
	{
		if (!Replication.IsServer())
			return;

		AROR_ContentPack pack = FindPack(packId);
		if (!pack)
		{
			Print(string.Format("[AROR] Content pack '%1' does not exist", packId), LogLevel.WARNING);
			return;
		}

		m_sDisabledPacks = WithId(m_sDisabledPacks, packId, pack.IsEnabledByDefault() && !enabled);
		m_sEnabledPacks = WithId(m_sEnabledPacks, packId, !pack.IsEnabledByDefault() && enabled);
		Changed_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Sets this campaign's overrides of the packs' defaults (new campaign, or a save restored). IDs of
	//! packs that are not loaded are kept, so a save keeps its choice if their mod comes back.
	void SetPackOverrides_S(array<string> disabledIds, array<string> enabledIds)
	{
		if (!Replication.IsServer())
			return;

		m_sDisabledPacks = JoinIds(disabledIds);
		m_sEnabledPacks = JoinIds(enabledIds);
		Changed_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: this campaign's overrides of the packs' defaults (for the serializer)
	void GetPackOverrides(notnull array<string> outDisabledIds, notnull array<string> outEnabledIds)
	{
		SplitIds(m_sDisabledPacks, outDisabledIds);
		SplitIds(m_sEnabledPacks, outEnabledIds);
	}

	//------------------------------------------------------------------------------------------------
	protected void Changed_S()
	{
		Replication.BumpMe();
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	//! Both override properties use this callback; when both change in one update, rebuild once on the next frame
	protected void OnOverridesReplicated()
	{
		if (m_bRebuildQueued)
			return;

		m_bRebuildQueued = true;
		GetGame().GetCallqueue().CallLater(RebuildQueued, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildQueued()
	{
		m_bRebuildQueued = false;
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	// Building
	//------------------------------------------------------------------------------------------------
	//! Every machine: rebuild the replacement map and the catalogue from the enabled packs
	protected void Rebuild()
	{
		m_mReplacements.Clear();
		m_mCatalogue.Clear();
		m_aCatalogueOrder.Clear();

		array<AROR_ContentPack> enabled = {};
		foreach (AROR_ContentPack pack : m_aPacks)
		{
			if (IsEnabled(pack.GetId()))
				enabled.Insert(pack);
		}

		// Replacements first, so a replaced prefab is left out of the catalogue whichever pack lists it
		foreach (AROR_ContentPack pack : enabled)
		{
			for (int category = AROR_EFactionPresetCategory.VEHICLES; category <= AROR_EFactionPresetCategory.AMMUNITION; category++)
			{
				array<AROR_ContentPackEntry> entries = {};
				pack.GetAllEntries(category, entries);
				foreach (AROR_ContentPackEntry entry : entries)
				{
					AddReplacements(pack, entry);
				}
			}
		}

		foreach (AROR_ContentPack pack : enabled)
		{
			for (int category = AROR_EFactionPresetCategory.VEHICLES; category <= AROR_EFactionPresetCategory.AMMUNITION; category++)
			{
				array<AROR_ContentPackEntry> entries = {};
				pack.GetAllEntries(category, entries);
				foreach (AROR_ContentPackEntry entry : entries)
				{
					ResourceName prefab = entry.GetPrefab();
					if (prefab.IsEmpty() || m_mReplacements.Contains(prefab) || m_mCatalogue.Contains(prefab))
						continue;

					m_mCatalogue.Insert(prefab, entry);
					m_aCatalogueOrder.Insert(entry);
				}
			}
		}

		s_iRevision++;
		Print(string.Format("[AROR] Content packs enabled: %1 (%2 entries, %3 replacements)", DescribeEnabled(enabled), m_aCatalogueOrder.Count(), m_mReplacements.Count()), LogLevel.NORMAL);
		m_OnPacksChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void AddReplacements(notnull AROR_ContentPack pack, notnull AROR_ContentPackEntry entry)
	{
		ResourceName replacer = entry.GetPrefab();
		if (replacer.IsEmpty())
			return;

		array<ResourceName> replaced = {};
		entry.GetReplaces(replaced);
		foreach (ResourceName prefab : replaced)
		{
			if (prefab == replacer)
				continue;

			ResourceName existing;
			if (m_mReplacements.Find(prefab, existing))
			{
				if (existing != replacer)
					Print(string.Format("[AROR] Content pack '%1' replaces %2, which an earlier pack already replaces; the earlier replacement is used", pack.GetId(), prefab), LogLevel.WARNING);

				continue;
			}

			m_mReplacements.Insert(prefab, replacer);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string DescribeEnabled(notnull array<AROR_ContentPack> packs)
	{
		if (packs.IsEmpty())
			return "none";

		string text;
		foreach (int i, AROR_ContentPack pack : packs)
		{
			if (i > 0)
				text += ", ";
			text += pack.GetId();
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	// ID lists
	//------------------------------------------------------------------------------------------------
	protected static void SplitIds(string joined, notnull array<string> outIds)
	{
		if (joined.IsEmpty())
			return;

		array<string> ids = {};
		joined.Split(ID_SEPARATOR, ids, true);
		outIds.InsertAll(ids);
	}

	//------------------------------------------------------------------------------------------------
	protected static string JoinIds(array<string> ids)
	{
		string joined;
		if (!ids)
			return joined;

		foreach (string id : ids)
		{
			if (id.IsEmpty() || ContainsId(joined, id))
				continue;

			if (!joined.IsEmpty())
				joined += ID_SEPARATOR;
			joined += id;
		}

		return joined;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ContainsId(string joined, string id)
	{
		array<string> ids = {};
		SplitIds(joined, ids);
		return ids.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! The joined list with the ID added (include) or removed (!include)
	protected static string WithId(string joined, string id, bool include)
	{
		array<string> ids = {};
		SplitIds(joined, ids);
		ids.RemoveItem(id);
		if (include)
			ids.Insert(id);

		return JoinIds(ids);
	}

	//------------------------------------------------------------------------------------------------
	// Loading
	//------------------------------------------------------------------------------------------------
	//! Every machine: load the registry and each pack it lists, and work out which are available
	protected void LoadPacks()
	{
		m_aPacks.Clear();
		m_AvailablePackIds.Clear();

		AROR_ContentPackRegistry registry = AROR_ContentPackRegistry.Cast(LoadConfig(m_sRegistry));
		if (!registry)
		{
			Print(string.Format("[AROR] Content pack registry '%1' could not be loaded", m_sRegistry), LogLevel.ERROR);
			return;
		}

		array<string> loadedAddons = {};
		GameProject.GetLoadedAddons(loadedAddons);

		array<ResourceName> paths = {};
		registry.GetPackPaths(paths);
		foreach (ResourceName path : paths)
		{
			AROR_ContentPack pack = AROR_ContentPack.Cast(LoadConfig(path));
			if (!pack)
			{
				Print(string.Format("[AROR] Content pack '%1' could not be loaded", path), LogLevel.ERROR);
				continue;
			}

			if (!pack.Validate(path))
				continue;

			if (FindPack(pack.GetId()))
			{
				Print(string.Format("[AROR] Content pack ID '%1' is used more than once; '%2' is ignored", pack.GetId(), path), LogLevel.ERROR);
				continue;
			}

			m_aPacks.Insert(pack);
			if (pack.HasRequiredAddons(loadedAddons))
				m_AvailablePackIds.Insert(pack.GetId());
			else
				Print(string.Format("[AROR] Content pack '%1' needs an addon that is not loaded; it is unavailable", pack.GetId()), LogLevel.NORMAL);
		}

		Print(string.Format("[AROR] Loaded %1 content packs (%2 available)", m_aPacks.Count(), m_AvailablePackIds.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static Managed LoadConfig(ResourceName path)
	{
		if (path.IsEmpty())
			return null;

		Resource resource = BaseContainerTools.LoadContainer(path);
		if (!resource || !resource.IsValid())
			return null;

		return BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer());
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;
		LoadPacks();

		// Pack defaults until a save restores the campaign's choice; the initial replication snapshot carries any
		// overrides to clients
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;

		GetGame().GetCallqueue().Remove(RebuildQueued);
		super.OnDelete(owner);
	}
}
