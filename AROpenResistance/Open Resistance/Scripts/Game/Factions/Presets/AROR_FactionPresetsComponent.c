[ComponentEditorProps(category: "OpenResistance/Factions", description: "Faction presets: loads the available presets and holds the campaign's preset per role. Attach to the game mode entity.")]
class AROR_FactionPresetsComponentClass : SCR_BaseGameModeComponentClass
{
}

void AROR_OnFactionPresetsChanged();
typedef func AROR_OnFactionPresetsChanged;

//! Faction presets (see ARCHITECTURE.md -> Factions). Every machine loads the presets listed in the registry, so clients
//! can show and query them without network traffic. The server owns which preset each role uses: chosen by the admin
//! when a campaign is created (defaults otherwise), replicated, and saved with the campaign
//! (AROR_FactionPresetsComponentSerializer). Other systems ask this component for a role's preset.
class AROR_FactionPresetsComponent : SCR_BaseGameModeComponent
{
	[Attribute("{2D8E5A71C4B93F06}Configs/FactionPresets/AROR_FactionPresetRegistry.conf", UIWidgets.ResourceNamePicker, desc: "Registry listing the available faction presets", params: "conf class=AROR_FactionPresetRegistry")]
	protected ResourceName m_sRegistry;

	[Attribute("{B47E2C19D5A6F308}Configs/FactionPresets/AROR_DefaultUnitPrefabs.conf", UIWidgets.ResourceNamePicker, desc: "Default character per role and unit type, for preset units that name none", params: "conf class=AROR_DefaultUnitPrefabs")]
	protected ResourceName m_sDefaultUnits;

	[Attribute("FIA", desc: "Preset ID the Resistance uses unless the admin picks another", category: "Defaults")]
	protected string m_sDefaultResistance;

	[Attribute("US_ARMY", desc: "Preset ID the Occupiers use unless the admin picks another", category: "Defaults")]
	protected string m_sDefaultOccupiers;

	[Attribute("USSR_ARMY", desc: "Preset ID the Invaders use unless the admin picks another", category: "Defaults")]
	protected string m_sDefaultInvaders;

	[Attribute("EVERON_CIV", desc: "Preset ID the Civilians use unless the admin picks another", category: "Defaults")]
	protected string m_sDefaultCivilians;

	[Attribute("4", desc: "Infantry specialists: one AT or AA soldier per this many members of an enemy group (at least one in a group of the minimum size below)", params: "2 20 1", category: "Infantry specialists")]
	protected int m_iMembersPerSpecialist;

	[Attribute("3", desc: "Infantry specialists: smallest enemy group that gets an AT or AA soldier", params: "2 20 1", category: "Infantry specialists")]
	protected int m_iSpecialistMinimumGroup;

	[Attribute("0", desc: "Workbench only: on Play, read the base-game infantry kits and write them into the FIA, US and Soviet presets as readiness 1 loadouts (units with loadouts are kept). Untick after use.", category: "Workbench")]
	protected bool m_bImportBaseUnitLoadouts;

	[Attribute("0", desc: "Workbench only: on Play, capture the kits named by each loaded preset's Loadout Capture (e.g. the WCS armies) into that preset's file as unit loadouts (a unit's existing loadout at the same readiness is kept). Untick after use.", category: "Workbench")]
	protected bool m_bCaptureUnitLoadouts;

	protected static AROR_FactionPresetsComponent s_Instance;

	//! All loaded presets, in registry order
	protected ref array<ref AROR_FactionPreset> m_aPresets = {};

	protected ref AROR_DefaultUnitPrefabs m_DefaultUnits;

	// The campaign's preset ID per role (server-owned, replicated)
	[RplProp(onRplName: "OnPresetsReplicated")]
	protected string m_sResistancePreset;

	[RplProp(onRplName: "OnPresetsReplicated")]
	protected string m_sOccupiersPreset;

	[RplProp(onRplName: "OnPresetsReplicated")]
	protected string m_sInvadersPreset;

	[RplProp(onRplName: "OnPresetsReplicated")]
	protected string m_sCiviliansPreset;

	protected ref ScriptInvokerBase<AROR_OnFactionPresetsChanged> m_OnPresetsChanged = new ScriptInvokerBase<AROR_OnFactionPresetsChanged>();

	protected bool m_bCatalogsAugmented;

	//------------------------------------------------------------------------------------------------
	static AROR_FactionPresetsComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! True once catalog augmentation has run (a frame after loading); caches of preset entries built before it are stale
	bool AreCatalogsAugmented()
	{
		return m_bCatalogsAugmented;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine; fires when the campaign's presets change (set at campaign creation, restored from a save, or replicated)
	ScriptInvokerBase<AROR_OnFactionPresetsChanged> GetOnPresetsChanged()
	{
		return m_OnPresetsChanged;
	}

	//------------------------------------------------------------------------------------------------
	// Queries
	//------------------------------------------------------------------------------------------------
	//! The preset the role uses in this campaign, or null if it has none
	AROR_FactionPreset GetPreset(AROR_EFactionRole role)
	{
		return FindPreset(GetPresetId(role));
	}

	//------------------------------------------------------------------------------------------------
	string GetPresetId(AROR_EFactionRole role)
	{
		switch (role)
		{
			case AROR_EFactionRole.RESISTANCE:
				return m_sResistancePreset;
			case AROR_EFactionRole.OCCUPIERS:
				return m_sOccupiersPreset;
			case AROR_EFactionRole.INVADERS:
				return m_sInvadersPreset;
			case AROR_EFactionRole.CIVILIANS:
				return m_sCiviliansPreset;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	AROR_FactionPreset FindPreset(string presetId)
	{
		if (presetId.IsEmpty())
			return null;

		foreach (AROR_FactionPreset preset : m_aPresets)
		{
			if (preset.GetId() == presetId)
				return preset;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every loaded preset, in registry order (e.g. for shops that sell every faction's equipment)
	int GetAllPresets(notnull array<AROR_FactionPreset> outPresets)
	{
		foreach (AROR_FactionPreset preset : m_aPresets)
		{
			outPresets.Insert(preset);
		}

		return m_aPresets.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the presets that can be chosen for the role, in registry order; returns how many were added
	int GetPresetsForRole(AROR_EFactionRole role, notnull array<AROR_FactionPreset> outPresets)
	{
		int added;
		foreach (AROR_FactionPreset preset : m_aPresets)
		{
			if (preset.IsUsableAs(role))
			{
				outPresets.Insert(preset);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! The character for a unit type of the role: the role's preset unit prefab if it names one, else the role's default
	//! for the type (AROR_DefaultUnitPrefabs). Empty when there is neither.
	ResourceName GetUnitPrefab(AROR_EFactionRole role, AROR_EUnitType type)
	{
		AROR_FactionPreset preset = GetPreset(role);
		if (preset)
		{
			AROR_FactionPresetUnit unit = preset.GetUnit(type);
			if (unit && unit.HasPrefab())
				return unit.GetPrefab();
		}

		return GetDefaultUnitPrefab(role, type);
	}

	//------------------------------------------------------------------------------------------------
	//! The role's default character for the unit type, or empty
	ResourceName GetDefaultUnitPrefab(AROR_EFactionRole role, AROR_EUnitType type)
	{
		if (!m_DefaultUnits)
			return string.Empty;

		return m_DefaultUnits.GetPrefab(role, type);
	}

	//------------------------------------------------------------------------------------------------
	//! The loadout a unit type of the role gets now: picked from its preset unit's variants by the role's current
	//! Readiness (1 for roles without one). Null when the unit has no loadouts, so the character keeps its own kit.
	AROR_UnitLoadout GetUnitLoadout(AROR_EFactionRole role, AROR_EUnitType type)
	{
		return GetUnitLoadoutAt(role, type, GetReadiness(role));
	}

	//------------------------------------------------------------------------------------------------
	//! As GetUnitLoadout, at a given readiness (e.g. scripted responses frozen at a level)
	AROR_UnitLoadout GetUnitLoadoutAt(AROR_EFactionRole role, AROR_EUnitType type, int readiness)
	{
		AROR_FactionPreset preset = GetPreset(role);
		if (!preset)
			return null;

		AROR_FactionPresetUnit unit = preset.GetUnit(type);
		if (!unit)
			return null;

		return unit.PickLoadout(readiness);
	}

	//------------------------------------------------------------------------------------------------
	//! The starting kit for players spawning in the role, from its preset. Null when the preset has none, so the player
	//! character keeps its own kit.
	AROR_PlayerLoadout GetPlayerLoadout(AROR_EFactionRole role)
	{
		AROR_FactionPreset preset = GetPreset(role);
		if (!preset)
			return null;

		return preset.GetPlayerLoadout();
	}

	//------------------------------------------------------------------------------------------------
	//! The role's current Readiness; 1 for roles without one (Resistance, Civilians) or before Operations exists
	protected static int GetReadiness(AROR_EFactionRole role)
	{
		if (role != AROR_EFactionRole.OCCUPIERS && role != AROR_EFactionRole.INVADERS)
			return 1;

		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return 1;

		return operations.GetReadinessLevel(role);
	}

	//------------------------------------------------------------------------------------------------
	//! The role's default preset: the configured one, or a loaded preset that takes it over (e.g. a content mod's
	//! variant of the same army, see AROR_FactionPreset.GetReplacedPresetId)
	string GetDefaultPresetId(AROR_EFactionRole role)
	{
		string configured = GetConfiguredDefaultPresetId(role);
		if (configured.IsEmpty())
			return configured;

		foreach (AROR_FactionPreset preset : m_aPresets)
		{
			if (preset.GetReplacedPresetId() == configured && preset.IsUsableAs(role))
				return preset.GetId();
		}

		return configured;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetConfiguredDefaultPresetId(AROR_EFactionRole role)
	{
		switch (role)
		{
			case AROR_EFactionRole.RESISTANCE:
				return m_sDefaultResistance;
			case AROR_EFactionRole.OCCUPIERS:
				return m_sDefaultOccupiers;
			case AROR_EFactionRole.INVADERS:
				return m_sDefaultInvaders;
			case AROR_EFactionRole.CIVILIANS:
				return m_sDefaultCivilians;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The unit type whose character, for the role, is this prefab (the preset unit's own, or the role default it falls
	//! back to). False when no infantry unit type of the role uses it.
	bool FindUnitTypeOf(AROR_EFactionRole role, ResourceName prefab, out AROR_EUnitType type)
	{
		if (prefab.IsEmpty())
			return false;

		for (int candidate = 0; candidate <= AROR_EUnitType.CIVILIAN; candidate++)
		{
			if (GetUnitPrefab(role, candidate) == prefab)
			{
				type = candidate;
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of an enemy group's members are AT or AA specialists (see AROR_InfantryRoster)
	int GetSpecialistCount(int members)
	{
		if (members < m_iSpecialistMinimumGroup)
			return 0;

		int perSpecialist = m_iMembersPerSpecialist;
		if (perSpecialist < 1)
			perSpecialist = 1;

		int count = members / perSpecialist;
		if (count < 1)
			count = 1;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	// Server
	//------------------------------------------------------------------------------------------------
	//! Server only. Sets the campaign's presets from IDs indexed by role (AROR_EFactionRole). A missing, unknown or
	//! unusable ID falls back to the role's default. Called when a campaign is created.
	void SetPresets_S(array<string> presetIds)
	{
		if (!Replication.IsServer())
			return;

		array<AROR_EFactionRole> roles = {};
		AROR_FactionRoles.GetAll(roles);
		foreach (AROR_EFactionRole role : roles)
		{
			string requested;
			if (presetIds && role < presetIds.Count())
				requested = presetIds[role];

			SetRolePreset(role, ResolvePresetId(role, requested));
		}

		Replication.BumpMe();
		LogPresets();
		m_OnPresetsChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: restore saved presets (used by the serializer). Same fallback rules as SetPresets_S, so a save
	//! whose preset no longer exists (e.g. its mod was removed) still loads, with that role on its default.
	void RestorePresets_S(array<string> presetIds)
	{
		SetPresets_S(presetIds);
	}

	//------------------------------------------------------------------------------------------------
	//! The requested ID if it exists and is usable for the role, otherwise the role's default
	protected string ResolvePresetId(AROR_EFactionRole role, string requested)
	{
		string roleName = typename.EnumToString(AROR_EFactionRole, role);
		if (!requested.IsEmpty())
		{
			AROR_FactionPreset preset = FindPreset(requested);
			if (preset && preset.IsUsableAs(role))
				return requested;

			Print(string.Format("[AROR] Faction preset '%1' is not available for %2; using the default", requested, roleName), LogLevel.WARNING);
		}

		string fallback = GetDefaultPresetId(role);
		AROR_FactionPreset defaultPreset = FindPreset(fallback);
		if (!defaultPreset || !defaultPreset.IsUsableAs(role))
			Print(string.Format("[AROR] Default faction preset '%1' for %2 is missing or not usable for that role", fallback, roleName), LogLevel.ERROR);

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetRolePreset(AROR_EFactionRole role, string presetId)
	{
		switch (role)
		{
			case AROR_EFactionRole.RESISTANCE:
				m_sResistancePreset = presetId;
				break;
			case AROR_EFactionRole.OCCUPIERS:
				m_sOccupiersPreset = presetId;
				break;
			case AROR_EFactionRole.INVADERS:
				m_sInvadersPreset = presetId;
				break;
			case AROR_EFactionRole.CIVILIANS:
				m_sCiviliansPreset = presetId;
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: saved preset IDs indexed by role (for the serializer)
	void GetPresetIds(notnull array<string> outPresetIds)
	{
		array<AROR_EFactionRole> roles = {};
		AROR_FactionRoles.GetAll(roles);
		foreach (AROR_EFactionRole role : roles)
		{
			outPresetIds.Insert(GetPresetId(role));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void LogPresets()
	{
		Print(string.Format("[AROR] Faction presets: Resistance %1 | Occupiers %2 | Invaders %3 | Civilians %4",
			m_sResistancePreset, m_sOccupiersPreset, m_sInvadersPreset, m_sCiviliansPreset), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPresetsReplicated()
	{
		m_OnPresetsChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	// Loading
	//------------------------------------------------------------------------------------------------
	//! Every machine: load the registry and each preset it lists
	protected void LoadPresets()
	{
		m_aPresets.Clear();

		AROR_FactionPresetRegistry registry = AROR_FactionPresetRegistry.Cast(LoadConfig(m_sRegistry));
		if (!registry)
		{
			Print(string.Format("[AROR] Faction preset registry '%1' could not be loaded", m_sRegistry), LogLevel.ERROR);
			return;
		}

		array<ResourceName> paths = {};
		registry.GetPresetPaths(paths);
		set<string> loadedPaths = new set<string>();
		foreach (ResourceName path : paths)
		{
			// An addon's registry override may repeat the core's entries; the same file twice is not a mistake
			if (!loadedPaths.Insert(path))
				continue;

			AROR_FactionPreset preset = AROR_FactionPreset.Cast(LoadConfig(path));
			if (!preset)
			{
				Print(string.Format("[AROR] Faction preset '%1' could not be loaded", path), LogLevel.ERROR);
				continue;
			}

			if (!preset.Validate(path))
				continue;

			if (FindPreset(preset.GetId()))
			{
				Print(string.Format("[AROR] Faction preset ID '%1' is used more than once; '%2' is ignored", preset.GetId(), path), LogLevel.ERROR);
				continue;
			}

			m_aPresets.Insert(preset);
		}

		Print(string.Format("[AROR] Loaded %1 faction presets", m_aPresets.Count()), LogLevel.NORMAL);

		m_DefaultUnits = AROR_DefaultUnitPrefabs.Cast(LoadConfig(m_sDefaultUnits));
		if (!m_DefaultUnits)
			Print(string.Format("[AROR] Default unit prefabs '%1' could not be loaded; preset units without a prefab are unusable", m_sDefaultUnits), LogLevel.ERROR);

		CheckUnitPrefabs();
	}

	//------------------------------------------------------------------------------------------------
	//! Warns for preset units that name no character while a role they serve has no default for their type
	protected void CheckUnitPrefabs()
	{
		array<AROR_EFactionRole> roles = {};
		AROR_FactionRoles.GetAll(roles);

		foreach (AROR_FactionPreset preset : m_aPresets)
		{
			array<AROR_FactionPresetEntry> units = {};
			preset.GetAllEntries(AROR_EFactionPresetCategory.UNITS, units);
			foreach (AROR_FactionPresetEntry entry : units)
			{
				AROR_FactionPresetUnit unit = AROR_FactionPresetUnit.Cast(entry);
				if (!unit || unit.HasPrefab())
					continue;

				foreach (AROR_EFactionRole role : roles)
				{
					if (preset.IsUsableAs(role) && GetDefaultUnitPrefab(role, unit.GetTypeValue()).IsEmpty())
						Print(string.Format("[AROR] Faction preset '%1': %2 has no prefab and %3 has no default for it", preset.GetId(), unit.GetTypeName(), typename.EnumToString(AROR_EFactionRole, role)), LogLevel.WARNING);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine, a frame after loading so the factions and their catalogs exist: adds what mods put into the
	//! faction catalogs (e.g. WCS weapons, vehicles and helicopters) to the presets that ask for it
	protected void AugmentPresetsFromCatalogs()
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
		{
			Print("[AROR] Faction presets: no faction manager, catalog augmentation skipped", LogLevel.WARNING);
			return;
		}

		foreach (AROR_FactionPreset preset : m_aPresets)
		{
			int added = preset.AugmentFromCatalogs(factionManager);
			if (added > 0)
				Print(string.Format("[AROR] Faction preset '%1': %2 entries added from the faction catalogs", preset.GetId(), added), LogLevel.NORMAL);
		}

		m_bCatalogsAugmented = true;
		m_OnPresetsChanged.Invoke();
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
		LoadPresets();
		GetGame().GetCallqueue().CallLater(AugmentPresetsFromCatalogs, 0);

		// Defaults until a campaign is created or a save restores the real choice; set directly (no BumpMe during
		// init), the initial replication snapshot carries them to clients
		if (Replication.IsServer())
		{
			array<AROR_EFactionRole> roles = {};
			AROR_FactionRoles.GetAll(roles);
			foreach (AROR_EFactionRole role : roles)
			{
				SetRolePreset(role, ResolvePresetId(role, string.Empty));
			}

			LogPresets();
		}

#ifdef WORKBENCH
		// Let the world settle before spawning the characters to read
		if (m_bImportBaseUnitLoadouts && Replication.IsServer())
			GetGame().GetCallqueue().CallLater(StartLoadoutImport, 5000);
		else if (m_bCaptureUnitLoadouts && Replication.IsServer())
			GetGame().GetCallqueue().CallLater(StartLoadoutCapture, 5000);
#endif
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	protected void StartLoadoutImport()
	{
		AROR_UnitLoadoutImporter.Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartLoadoutCapture()
	{
		AROR_UnitLoadoutImporter.StartCapture();
	}
#endif

	//------------------------------------------------------------------------------------------------
	//! Server: dresses a spawned player from their role's preset player loadout. Only an undressed character is dressed,
	//! so a kit applied by something else (e.g. a saved arsenal loadout) is never doubled.
	override void OnPlayerSpawnFinalize_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		super.OnPlayerSpawnFinalize_S(requestComponent, handlerComponent, data, entity);

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character || !AROR_LoadoutOutfitter.IsUndressed(character))
			return;

		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		AROR_EFactionRole role;
		if (!roles || !roles.GetRoleOfFaction(character.GetFaction(), role))
			return;

		AROR_PlayerLoadout loadout = GetPlayerLoadout(role);
		if (!loadout)
			return;

		int failed = AROR_LoadoutOutfitter.Apply_S(character, loadout);
		if (failed > 0)
			Print(string.Format("[AROR] Player loadout of %1 applied with %2 pieces missing", GetPresetId(role), failed), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(AugmentPresetsFromCatalogs);

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
