void AROR_OnSaveListReceived(array<string> saveLabels);
typedef func AROR_OnSaveListReceived;
typedef ScriptInvokerBase<AROR_OnSaveListReceived> AROR_OnSaveListReceivedInvoker;

void AROR_OnSetupResult(bool success, string message);
typedef func AROR_OnSetupResult;
typedef ScriptInvokerBase<AROR_OnSetupResult> AROR_OnSetupResultInvoker;

[ComponentEditorProps(category: "OpenResistance/GameFlow", description: "Holds the game in pre-game until the server admin creates or loads a playthrough. Attach to the game mode entity.")]
class AROR_GameFlowComponentClass : SCR_BaseGameModeStateComponentClass
{
}

//! Pre-game state of the game flow (see ARCHITECTURE.md -> Game Flow).
//! Server: validates admin requests, spawns the Resistance HQ, arsenal and garage, starts or loads playthroughs.
//! Clients: receive setup messages through the AROR_ methods on SCR_PlayerController and raise the events below for the UI.
class AROR_GameFlowComponent : SCR_BaseGameModeStateComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Resistance HQ prefab spawned at the chosen position", params: "et")]
	protected ResourceName m_sHQPrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Arsenal prefab spawned at world positions (first use: beside the HQ)", params: "et")]
	protected ResourceName m_sArsenalPrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Garage prefab spawned beside the HQ (opposite side from the arsenal)", params: "et")]
	protected ResourceName m_sGaragePrefab;

	[Attribute("0.5 0 0", UIWidgets.Coords, desc: "Gap from the HQ origin (flag) to the nearest edge of each prop: arsenal on the +offset side, garage on the -offset side (opposite sides)")]
	protected vector m_vArsenalOffsetFromHQ;

	[Attribute("100", UIWidgets.Slider, desc: "Minimum distance in metres between the HQ and the edge (radius) of any location", params: "0 5000 10")]
	protected float m_fHQMinDistanceFromLocation;

	[Attribute("5", UIWidgets.Slider, desc: "Half-extent in metres of the clear pad required for the HQ (5 = 10x10 m)", params: "1 50 0.5")]
	protected float m_fHQClearanceHalfExtent;

	[Attribute("3", UIWidgets.Slider, desc: "Height in metres of the clear pad TraceBox for the HQ", params: "1 10 0.5")]
	protected float m_fHQClearanceHeight;

	[Attribute("1", desc: "Players can move at the staging spawn while the admin sets up the campaign")]
	protected bool m_bAllowControlsInSetup;

	[Attribute("8", UIWidgets.Slider, desc: "Players are moved to random points within this radius of the HQ when it is created", params: "0 50 1")]
	protected float m_fHQArrivalRadius;

	[Attribute("300", UIWidgets.Slider, desc: "Seconds between automatic saves while a playthrough is running (0 = off)", params: "0 1800 30")]
	protected int m_iAutosaveIntervalSeconds;

	[Attribute("5", UIWidgets.Slider, desc: "Autosaves kept per playthrough; older ones are deleted after each autosave. Manual saves are never deleted", params: "1 50 1")]
	protected int m_iAutosavesKept;

	[Attribute("1", desc: "Workbench only: treat every player as admin so setup can be tested", category: "Workbench")]
	protected bool m_bWorkbenchEveryoneIsAdmin;

	[Attribute("0", desc: "Workbench only: skip the setup screen and create a new playthrough with the HQ at the position below", category: "Workbench")]
	protected bool m_bWorkbenchAutoNewPlaythrough;

	[Attribute("0 0 0", UIWidgets.Coords, desc: "Workbench only: HQ position used by the auto new playthrough", category: "Workbench")]
	protected vector m_vWorkbenchHQPosition;

	protected static AROR_GameFlowComponent s_Instance;

	//! Server: written when the admin picks a save in setup, so the world reload that follows restores it. A file,
	//! because in-memory state does not reliably survive the reload; it expires so an old one never restores a save.
	protected static const string LOAD_MARKER_FILE = "$profile:AROR_LoadRequest.txt";
	protected static const int LOAD_MARKER_MAX_AGE_S = 300;

	//! Server: written before restarting the scenario to drop a save the game resumed by itself (prevents a loop)
	protected static const string RESTART_MARKER_FILE = "$profile:AROR_FreshRestart.txt";

	//! Server: staging spawns that initialised before the game flow existed
	protected static ref array<SCR_SpawnPoint> s_aPendingStagingSpawns;

	//! Server: staging spawn points (enabled only until a playthrough exists)
	protected ref array<SCR_SpawnPoint> m_aStagingSpawns = {};

	//! Server: true once a playthrough is created or loaded; lets the game mode leave pre-game
	protected bool m_bPlaythroughReady;

	//! Server: the game restored a save before setup; its world state cannot take a new playthrough on top
	protected bool m_bResumedSave;

	//! Server: the game resumed a save by itself and the scenario is being reloaded fresh. The engine ignores a world
	//! change during world init, so the reload is requested once the game has started and retried a few times.
	protected bool m_bFreshRestartPending;
	protected int m_iFreshRestartAttempts;
	protected static const int FRESH_RESTART_RETRY_MS = 2000;
	protected static const int FRESH_RESTART_MAX_ATTEMPTS = 5;
	protected static const int FRESH_RESTART_START_TIMEOUT_MS = 60000;
	protected bool m_bFreshRestartAttempting;	//!< The game has started; attempts are running

	//! Server: players already offered the setup screen (join and role change can both trigger it)
	protected ref array<int> m_aSetupOfferedPlayerIds = {};

	//! Server: saves last sent to the admin, newest first; the client refers to them by index
	protected ref array<SaveGame> m_aListedSaves = {};
	protected int m_iSaveListRequesterId;
	protected ref SaveGameObtainCallback m_SaveListCallback;
	protected ref SaveGameOperationCallback m_InitialSaveCallback;
	protected ref SaveGameOperationCallback m_AutosaveCallback;
	protected ref SaveGameOperationCallback m_ManualSaveCallback;
	protected int m_iManualSaveRequesterId;	//!< Admin whose manual save is pending, or 0
	protected int m_iManualSaveAttempts;
	protected static const int MANUAL_SAVE_MAX_WAITS = 20;	//!< Busy checks PRUNE_RETRY_MS apart before giving up

	//! Server: the mission's saves at boot, to spot a new playthrough reusing an existing playthrough number
	protected ref array<SaveGame> m_aKnownSaves = {};
	protected ref SaveGameObtainCallback m_KnownSavesCallback;

	//! Server: old autosaves of the running playthrough waiting to be deleted, one at a time
	protected ref SaveGameObtainCallback m_PruneCallback;
	protected ref array<SaveGame> m_aPruneQueue = {};
	protected static const int PRUNE_RETRY_MS = 500;

	//! Autosave names start with this, followed by the resistance name; manual saves are the resistance name alone.
	//! The "-" is not allowed in resistance names (AROR_ResistanceName), so no manual save can look like an autosave.
	static const string AUTOSAVE_PREFIX = "Autosave - ";
	protected static const string DIGITS = "0123456789";
	protected static const int ENGINE_NAME_TIME_TOLERANCE_S = 600;

	//! Key-event saves wait this long, so several changes in a row become one save and the world has settled
	protected static const int AUTOSAVE_EVENT_DELAY_MS = 5000;
	protected bool m_bAutosavePending;
	protected bool m_bAutosaveTimerStarted;

	//! After a load, wait this long for the saved entities to restore before checking the garage prop exists
	protected static const int ENSURE_GARAGE_DELAY_MS = 10000;
	protected static const float ENSURE_GARAGE_RADIUS = 30;

	//! Client events for the setup screen
	protected ref ScriptInvoker m_OnSetupRequested = new ScriptInvoker();
	protected ref AROR_OnSaveListReceivedInvoker m_OnSaveListReceived = new AROR_OnSaveListReceivedInvoker();
	protected ref AROR_OnSetupResultInvoker m_OnSetupResult = new AROR_OnSetupResultInvoker();

	//------------------------------------------------------------------------------------------------
	static AROR_GameFlowComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// Pre-game state
	//------------------------------------------------------------------------------------------------
	override SCR_EGameModeState GetAffiliatedState()
	{
		return SCR_EGameModeState.PREGAME;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetAllowControls()
	{
		return m_bAllowControlsInSetup;
	}

	//------------------------------------------------------------------------------------------------
	// Staging spawn (server)
	//------------------------------------------------------------------------------------------------
	static void RegisterStagingSpawn(notnull SCR_SpawnPoint spawnPoint)
	{
		if (s_Instance)
		{
			s_Instance.AddStagingSpawn(spawnPoint);
			return;
		}

		if (!s_aPendingStagingSpawns)
			s_aPendingStagingSpawns = {};

		if (!s_aPendingStagingSpawns.Contains(spawnPoint))
			s_aPendingStagingSpawns.Insert(spawnPoint);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddStagingSpawn(notnull SCR_SpawnPoint spawnPoint)
	{
		if (m_aStagingSpawns.Contains(spawnPoint))
			return;

		m_aStagingSpawns.Insert(spawnPoint);
		spawnPoint.SetFactionKey(GetResistanceFactionKey());
		spawnPoint.SetSpawnPointEnabled_S(!m_bPlaythroughReady);
	}

	//------------------------------------------------------------------------------------------------
	protected void DisableStagingSpawns()
	{
		foreach (SCR_SpawnPoint spawnPoint : m_aStagingSpawns)
		{
			if (spawnPoint)
				spawnPoint.SetSpawnPointEnabled_S(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Moves every spawned player to random points around the HQ (Antistasi-style start)
	protected void MovePlayersToHQ(vector hqPosition)
	{
		BaseWorld world = GetGame().GetWorld();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			vector target = hqPosition;
			target[0] = target[0] + Math.RandomFloatInclusive(-m_fHQArrivalRadius, m_fHQArrivalRadius);
			target[2] = target[2] + Math.RandomFloatInclusive(-m_fHQArrivalRadius, m_fHQArrivalRadius);
			target[1] = world.GetSurfaceY(target[0], target[2]);

			if (!SCR_Global.TeleportPlayer(playerId, target))
				Print(string.Format("[AROR] Could not move player %1 to the HQ (not spawned?)", playerId), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True once a playthrough has been started or loaded (setup is over)
	bool IsPlaythroughReady()
	{
		return m_bPlaythroughReady;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanAdvanceState(SCR_EGameModeState nextState)
	{
		return m_bPlaythroughReady;
	}

	//------------------------------------------------------------------------------------------------
	// Client events (raised on the admin's machine)
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnSetupRequested()
	{
		return m_OnSetupRequested;
	}

	//------------------------------------------------------------------------------------------------
	AROR_OnSaveListReceivedInvoker GetOnSaveListReceived()
	{
		return m_OnSaveListReceived;
	}

	//------------------------------------------------------------------------------------------------
	AROR_OnSetupResultInvoker GetOnSetupResult()
	{
		return m_OnSetupResult;
	}

	//------------------------------------------------------------------------------------------------
	void OnSetupRequestedReceived()
	{
		Print("[AROR] Setup requested: open the setup screen", LogLevel.NORMAL);
		m_OnSetupRequested.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	void OnSaveListReceived(array<string> saveLabels)
	{
		Print(string.Format("[AROR] Received %1 saves", saveLabels.Count()), LogLevel.NORMAL);
		m_OnSaveListReceived.Invoke(saveLabels);
	}

	//------------------------------------------------------------------------------------------------
	void OnSetupResultReceived(bool success, string message)
	{
		Print(string.Format("[AROR] Setup result: %1 %2", success, message), LogLevel.NORMAL);
		m_OnSetupResult.Invoke(success, message);
	}

	//------------------------------------------------------------------------------------------------
	// HQ placement rules (usable on clients for previews; the server re-checks)
	//------------------------------------------------------------------------------------------------
	bool IsValidHQPosition(vector position, out string reason)
	{
		// Sea level is 0; anything at or below it is water (or off the map)
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			reason = "World missing";
			return false;
		}

		float surfaceY = world.GetSurfaceY(position[0], position[2]);
		if (surfaceY <= 0)
		{
			reason = "The HQ must be placed on land";
			return false;
		}

		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (!locationManager)
		{
			reason = "Location manager missing on the game mode";
			return false;
		}

		array<AROR_LocationComponent> locations = {};
		locationManager.GetLocations(locations);
		foreach (AROR_LocationComponent location : locations)
		{
			// The HQ's own (or a previous) HQ location does not block placement
			if (!location || location.GetLocationType() == AROR_ELocationType.HQ)
				continue;

			float distanceFromEdge = vector.DistanceXZ(location.GetCenter(), position) - location.GetRadius();
			if (distanceFromEdge < m_fHQMinDistanceFromLocation)
			{
				reason = string.Format("Too close to %1: the HQ must be at least %2 m outside any location", location.GetName(), m_fHQMinDistanceFromLocation);
				return false;
			}
		}

		if (!IsHQPadClear(world, Vector(position[0], surfaceY, position[2])))
		{
			float padSize = m_fHQClearanceHalfExtent * 2;
			reason = string.Format("Need a clear %1x%2 m area free of buildings and props", padSize, padSize);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a TraceBox at position (surface-grounded) finds no entity obstruction
	protected bool IsHQPadClear(notnull BaseWorld world, vector position)
	{
		TraceBox trace = new TraceBox();
		trace.Start = position + Vector(0, 0.25, 0);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-m_fHQClearanceHalfExtent, 0, -m_fHQClearanceHalfExtent);
		trace.Maxs = Vector(m_fHQClearanceHalfExtent, m_fHQClearanceHeight, m_fHQClearanceHalfExtent);
		trace.Flags = TraceFlags.ENTS | TraceFlags.VISIBILITY;
		return world.TraceMove(trace, null) == 1;
	}

	//------------------------------------------------------------------------------------------------
	// Server: requests from the admin (arrive through SCR_PlayerController RPCs)
	//------------------------------------------------------------------------------------------------
	//! The admin's choices from the setup screen. resistanceName: checked by AROR_ResistanceName, also the playthrough
	//! (save) name. presetIds: faction preset ID per role, indexed by AROR_EFactionRole (null or empty entries use the
	//! defaults). disabledPackIds / enabledPackIds: content pack overrides (null = every pack on its default).
	//! settingIds / settingValues: campaign settings (missing or invalid = default).
	void RequestNewPlaythrough_S(int playerId, vector hqPosition, string resistanceName, array<string> presetIds = null, array<string> disabledPackIds = null, array<string> enabledPackIds = null, array<string> settingIds = null, array<string> settingValues = null)
	{
		if (!CanAcceptSetupRequest(playerId))
			return;

		string nameReason;
		if (!AROR_ResistanceName.Validate(resistanceName, nameReason))
		{
			SendSetupResult(playerId, false, nameReason);
			return;
		}

		if (m_bResumedSave)
		{
			SendSetupResult(playerId, false, "A saved campaign was resumed on this server. Load a save, or restart the mission to start a new one");
			return;
		}

		string reason;
		if (!IsValidHQPosition(hqPosition, reason))
		{
			SendSetupResult(playerId, false, reason);
			return;
		}

		IEntity hq = SpawnHQ(hqPosition);
		if (!hq)
		{
			SendSetupResult(playerId, false, "Failed to spawn the Resistance HQ");
			return;
		}

		IEntity arsenal = SpawnArsenal(hq.GetOrigin() + m_vArsenalOffsetFromHQ);
		PlaceClearOfFlag(arsenal, hq.GetOrigin(), m_vArsenalOffsetFromHQ);
		if (!arsenal)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(hq);
			SendSetupResult(playerId, false, "Failed to spawn the arsenal");
			return;
		}

		// Same distance as the arsenal, opposite side of the HQ flag
		IEntity garage = SpawnGarage(hq.GetOrigin() - m_vArsenalOffsetFromHQ);
		PlaceClearOfFlag(garage, hq.GetOrigin(), -m_vArsenalOffsetFromHQ);
		if (!garage)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(arsenal);
			SCR_EntityHelper.DeleteEntityAndChildren(hq);
			SendSetupResult(playerId, false, "Failed to spawn the garage");
			return;
		}

		Print(string.Format("[AROR] Garage spawned at %1", garage.GetOrigin()), LogLevel.NORMAL);

		// Before the initial save point, so the first save already holds the admin's choices
		AROR_FactionPresetsComponent factionPresets = AROR_FactionPresetsComponent.GetInstance();
		if (factionPresets)
			factionPresets.SetPresets_S(presetIds);

		AROR_ContentPacksComponent contentPacks = AROR_ContentPacksComponent.GetInstance();
		if (contentPacks)
			contentPacks.SetPackOverrides_S(disabledPackIds, enabledPackIds);

		AROR_CampaignSettingsComponent campaignSettings = AROR_CampaignSettingsComponent.GetInstance();
		if (campaignSettings)
			campaignSettings.SetValues_S(settingIds, settingValues);

		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (highCommand)
			highCommand.SetResistanceName_S(resistanceName);

		SaveGameManager saveManager = SaveGameManager.Get();
		if (saveManager)
		{
			// TODO: verify that transition = false starts the playthrough without reloading the world
			saveManager.StartPlaythrough(SaveGameManager.GetCurrentMissionResource(), resistanceName, false);
			saveManager.SetSavingAllowed(true);

			LogSaveState();
			CheckPlaythroughNumberUnused();

			// Initial save point (an autosave) so new and loaded playthroughs share the same restore path
			m_InitialSaveCallback = new SaveGameOperationCallback(OnInitialSaveCompleted);
			saveManager.RequestSavePoint(ESaveGameType.SCRIPTED, AUTOSAVE_PREFIX + resistanceName, 0, m_InitialSaveCallback);
		}

		m_bPlaythroughReady = true;
		StartAutosaveTimer();
		DisableStagingSpawns();
		MovePlayersToHQ(hq.GetOrigin());

		SendHQEstablishedHeadline(hq);

		Print(string.Format("[AROR] New playthrough '%1' started, HQ at %2", resistanceName, hq.GetOrigin()), LogLevel.NORMAL);
		SendSetupResult(playerId, true, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	void RequestSaveList_S(int playerId)
	{
		if (!CanAcceptSetupRequest(playerId))
			return;

		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager)
		{
			SendSetupResult(playerId, false, "Saving is not available");
			return;
		}

		m_iSaveListRequesterId = playerId;
		m_SaveListCallback = new SaveGameObtainCallback(OnSavesObtained);
		saveManager.GetSaves(SaveGameManager.GetCurrentMissionResource(), m_SaveListCallback);
	}

	//------------------------------------------------------------------------------------------------
	//! saveIndex refers to the list last sent by RequestSaveList_S
	void RequestLoad_S(int playerId, int saveIndex)
	{
		if (!CanAcceptSetupRequest(playerId))
			return;

		if (!m_aListedSaves.IsIndexValid(saveIndex) || !m_aListedSaves[saveIndex])
		{
			SendSetupResult(playerId, false, "Save not found, refresh the list");
			return;
		}

		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager)
			return;

		Print(string.Format("[AROR] Loading save '%1'", m_aListedSaves[saveIndex].GetSavePointName()), LogLevel.NORMAL);
		WriteMarker(LOAD_MARKER_FILE);
		saveManager.Load(m_aListedSaves[saveIndex], true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteMarker(string path)
	{
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			Print("[AROR] Could not write " + path, LogLevel.WARNING);
			return;
		}

		file.WriteLine(System.GetUnixTime().ToString());
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! True if the marker was written within the last few minutes (admin chose a save / we just restarted). Always removes it.
	protected static bool ConsumeMarker(string path)
	{
		if (!FileIO.FileExists(path))
			return false;

		int requested;
		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (file)
		{
			string line;
			if (file.ReadLine(line) >= 0)
				requested = line.Trim().ToInt();

			file.Close();
		}

		FileIO.DeleteFile(path);
		return requested > 0 && System.GetUnixTime() - requested <= LOAD_MARKER_MAX_AGE_S;
	}

	//------------------------------------------------------------------------------------------------
	// Server internals
	//------------------------------------------------------------------------------------------------
	protected bool CanAcceptSetupRequest(int playerId)
	{
		if (!Replication.IsServer())
			return false;

		// The world is about to be reloaded without the save the game resumed
		if (m_bFreshRestartPending)
			return false;

		if (m_bPlaythroughReady)
		{
			Print(string.Format("[AROR] Setup request from player %1 ignored: playthrough already running", playerId), LogLevel.WARNING);
			return false;
		}

		if (!IsAdmin(playerId))
		{
			Print(string.Format("[AROR] Setup request from non-admin player %1 rejected", playerId), LogLevel.WARNING);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAdmin(int playerId)
	{
#ifdef WORKBENCH
		if (m_bWorkbenchEveryoneIsAdmin)
			return true;
#endif

		PlayerManager playerManager = GetGame().GetPlayerManager();
		return playerManager.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR) || playerManager.HasPlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_PlayerController GetPlayerController(int playerId)
	{
		return SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected string GetResistanceFactionKey()
	{
		// Look up on our own entity: the roles component may not have initialised its instance yet
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.Cast(GetOwner().FindComponent(AROR_FactionRolesComponent));
		if (!roles)
		{
			Print("[AROR] AROR_FactionRolesComponent missing on the game mode; Resistance faction unknown", LogLevel.ERROR);
			return string.Empty;
		}

		return roles.GetFactionKey(AROR_EFactionRole.RESISTANCE);
	}

	//------------------------------------------------------------------------------------------------
	//! Headline for everyone, with the HQ's map icon in the Resistance colour
	protected void SendHQEstablishedHeadline(notnull IEntity hq)
	{
		ResourceName icon;
		AROR_LocationComponent location = AROR_LocationComponent.Cast(hq.FindComponent(AROR_LocationComponent));
		if (location)
			icon = location.GetMapIcon();

		Color color;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (roles && roles.GetFaction(AROR_EFactionRole.RESISTANCE))
			color = roles.GetFaction(AROR_EFactionRole.RESISTANCE).GetFactionColor();

		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.HEADLINE, "Resistance HQ established", icon, color));
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnHQ(vector position)
	{
		Resource resource = Resource.Load(m_sHQPrefab);
		if (!resource || !resource.IsValid())
		{
			Print("[AROR] HQ prefab is not set or invalid on AROR_GameFlowComponent", LogLevel.ERROR);
			return null;
		}

		BaseWorld world = GetGame().GetWorld();
		position[1] = world.GetSurfaceY(position[0], position[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;

		IEntity hq = GetGame().SpawnEntityPrefab(resource, world, params);

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.Cast(hq);
		if (spawnPoint)
			spawnPoint.SetFactionKey(GetResistanceFactionKey());
		else
			Print("[AROR] HQ prefab is not an SCR_SpawnPoint; players will have nowhere to spawn", LogLevel.ERROR);

		return hq;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns an arsenal at a world position (surface-snapped). Reusable for HQ and future sites.
	IEntity SpawnArsenal(vector position)
	{
		return SpawnSurfacePrefab(m_sArsenalPrefab, position, "Arsenal");
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a garage at a world position (surface-snapped). Reusable for HQ and future sites.
	IEntity SpawnGarage(vector position)
	{
		return SpawnSurfacePrefab(m_sGaragePrefab, position, "Garage");
	}

	//------------------------------------------------------------------------------------------------
	//! Slides a prop along the offset direction so its near edge (bounds) is the offset's length from the flag,
	//! instead of its origin, so wide props neither overlap each other nor the flag
	protected void PlaceClearOfFlag(IEntity prop, vector flagPosition, vector offset)
	{
		float gap = offset.Length();
		if (!prop || gap <= 0)
			return;

		vector direction = offset / gap;
		vector mins, maxs;
		prop.GetWorldBounds(mins, maxs);

		// Half-size of the world-aligned bounds measured along the offset direction
		float halfExtent = Math.AbsFloat(direction[0]) * (maxs[0] - mins[0]) * 0.5 + Math.AbsFloat(direction[2]) * (maxs[2] - mins[2]) * 0.5;

		vector boundsCentre = (mins + maxs) * 0.5;
		vector target = flagPosition + direction * (gap + halfExtent);

		// Keep the prop's height above the terrain (its pivot offset) at the new spot
		BaseWorld world = GetGame().GetWorld();
		vector origin = prop.GetOrigin();
		float heightAboveSurface = origin[1] - world.GetSurfaceY(origin[0], origin[2]);
		origin[0] = origin[0] + (target[0] - boundsCentre[0]);
		origin[2] = origin[2] + (target[2] - boundsCentre[2]);
		origin[1] = world.GetSurfaceY(origin[0], origin[2]) + heightAboveSurface;
		prop.SetOrigin(origin);
		prop.Update();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnSurfacePrefab(ResourceName prefab, vector position, string label)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[AROR] %1 prefab is not set or invalid on AROR_GameFlowComponent", label), LogLevel.ERROR);
			return null;
		}

		BaseWorld world = GetGame().GetWorld();
		position[1] = world.GetSurfaceY(position[0], position[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;

		return GetGame().SpawnEntityPrefab(resource, world, params);
	}

	//------------------------------------------------------------------------------------------------
	protected void SendSetupResult(int playerId, bool success, string message)
	{
		SCR_PlayerController playerController = GetPlayerController(playerId);
		if (playerController)
			playerController.AROR_SendSetupResult_S(success, message);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the setup screen to an admin while the game is still in pre-game
	protected void TryOfferSetup(int playerId)
	{
		if (!Replication.IsServer() || m_bPlaythroughReady || m_bFreshRestartPending || !IsAdmin(playerId))
			return;

		if (m_aSetupOfferedPlayerIds.Contains(playerId))
			return;

		m_aSetupOfferedPlayerIds.Insert(playerId);

#ifdef WORKBENCH
		if (m_bWorkbenchAutoNewPlaythrough)
		{
			RequestNewPlaythrough_S(playerId, m_vWorkbenchHQPosition, "Workbench test");
			return;
		}
#endif

		SCR_PlayerController playerController = GetPlayerController(playerId);
		if (playerController)
			playerController.AROR_OpenSetup_S();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSavesObtained(bool success, array<SaveGame> saves, Managed context = null)
	{
		m_aListedSaves.Clear();

		array<SaveGame> newestFirst = {};
		if (success && saves)
		{
			foreach (SaveGame obtained : saves)
			{
				InsertNewestFirst(newestFirst, obtained);
			}
		}

		// Group by playthrough: groups in order of their newest save, each group's saves newest first. The client
		// refers to saves by their index in this flat order (RequestLoad_S).
		array<string> groupKeys = {};
		foreach (SaveGame sorted : newestFirst)
		{
			string sortedKey = GetPlaythroughKey(sorted);
			if (!groupKeys.Contains(sortedKey))
				groupKeys.Insert(sortedKey);
		}

		array<string> labels = {};
		foreach (string key : groupKeys)
		{
			array<SaveGame> groupSaves = {};
			foreach (SaveGame candidate : newestFirst)
			{
				if (GetPlaythroughKey(candidate) == key)
					groupSaves.Insert(candidate);
			}

			string groupName = GetGroupName(groupSaves);
			foreach (SaveGame listed : groupSaves)
			{
				m_aListedSaves.Insert(listed);
				labels.Insert(FormatSaveLabel(listed, key, groupName));
			}
		}

		SCR_PlayerController playerController = GetPlayerController(m_iSaveListRequesterId);
		if (playerController)
			playerController.AROR_SendSaveList_S(labels);
	}

	//------------------------------------------------------------------------------------------------
	protected static void InsertNewestFirst(notnull array<SaveGame> list, SaveGame save)
	{
		if (!save)
			return;

		int created = save.GetSavePointCreatedUnix();
		for (int i = 0, count = list.Count(); i < count; i++)
		{
			if (created > list[i].GetSavePointCreatedUnix())
			{
				list.InsertAt(save, i);
				return;
			}
		}

		list.Insert(save);
	}

	//------------------------------------------------------------------------------------------------
	//! Saves of one playthrough share its number and start time (the start time tells apart a reused number)
	protected static string GetPlaythroughKey(notnull SaveGame save)
	{
		return string.Format("%1-%2", save.GetPlaythroughNumber(), save.GetPlaythroughStartedUnix());
	}

	//------------------------------------------------------------------------------------------------
	//! Save lists hold their own SaveGame objects, so compare by playthrough and save point number, not by object
	protected static bool IsSameSave(SaveGame a, SaveGame b)
	{
		if (!a || !b)
			return false;

		return GetPlaythroughKey(a) == GetPlaythroughKey(b) && a.GetSavePointNumber() == b.GetSavePointNumber();
	}

	//------------------------------------------------------------------------------------------------
	//! True for our autosaves and the engine's own saves (shutdown, unnamed). Everything else is a manual save, which
	//! is never pruned: no resistance name can start with AUTOSAVE_PREFIX or match an engine name.
	static bool IsAutosave(notnull SaveGame save)
	{
		string name = save.GetSavePointName();
		return name.StartsWith(AUTOSAVE_PREFIX) || IsGeneratedSaveName(save);
	}

	//------------------------------------------------------------------------------------------------
	//! Names we did not give: empty, or the engine's own name for its saves (e.g. shutdown saves), which is the
	//! creation time in Unix seconds. A resistance named only with digits is not mistaken for it.
	protected static bool IsGeneratedSaveName(notnull SaveGame save)
	{
		string name = save.GetSavePointName();
		if (name.IsEmpty())
			return true;

		for (int i = 0, length = name.Length(); i < length; i++)
		{
			if (!DIGITS.Contains(name.Get(i)))
				return false;
		}

		return Math.AbsInt(name.ToInt() - save.GetSavePointCreatedUnix()) <= ENGINE_NAME_TIME_TOLERANCE_S;
	}

	//------------------------------------------------------------------------------------------------
	//! The resistance name a save was made under, or empty when the engine named it
	protected static string GetResistanceNameOf(notnull SaveGame save)
	{
		string name = save.GetSavePointName();
		if (name.StartsWith(AUTOSAVE_PREFIX))
			return name.Substring(AUTOSAVE_PREFIX.Length(), name.Length() - AUTOSAVE_PREFIX.Length());

		if (IsGeneratedSaveName(save))
			return string.Empty;

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! A playthrough is listed under the resistance name of its newest named save
	protected static string GetGroupName(notnull array<SaveGame> newestFirst)
	{
		foreach (SaveGame save : newestFirst)
		{
			string name = GetResistanceNameOf(save);
			if (!name.IsEmpty())
				return name;
		}

		if (!newestFirst.IsEmpty() && !newestFirst[0].GetPlaythroughName().IsEmpty())
			return newestFirst[0].GetPlaythroughName();

		return "Resistance";
	}

	//------------------------------------------------------------------------------------------------
	//! Fields for the load list: group key | group name | save name | autosave (1/0) | date | compatible (1/0)
	protected string FormatSaveLabel(notnull SaveGame save, string groupKey, string groupName)
	{
		int year, month, day, hour, minute, second;
		save.GetSavePointCreatedLocalDateTime(year, month, day, hour, minute, second);

		string compatible = "1";
		if (!save.IsSavePointGameVersionCompatible() || !save.AreSavePointAddonsCompatible())
			compatible = "0";

		string autosave = "0";
		if (IsAutosave(save))
			autosave = "1";

		string saveName = GetResistanceNameOf(save);
		if (saveName.IsEmpty())
			saveName = groupName;

		string separator = AROR_SetupMenu.FIELD_SEPARATOR;
		string date = string.Format("%1-%2-%3 %4:%5", year, month.ToString(2), day.ToString(2), hour.ToString(2), minute.ToString(2));
		return groupKey + separator + CleanSaveField(groupName) + separator + CleanSaveField(saveName) + separator + autosave + separator + date + separator + compatible;
	}

	//------------------------------------------------------------------------------------------------
	//! Names are typed by admins: keep the list and field separators out of them
	protected static string CleanSaveField(string text)
	{
		text.Replace(AROR_SetupMenu.FIELD_SEPARATOR, "/");
		text.Replace("\n", " ");
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP: diagnose save availability (mission header, saving enabled); remove once saving works
	protected void LogSaveState()
	{
		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager)
			return;

		MissionHeader missionHeader = GetGame().GetMissionHeader();
		if (missionHeader)
			Print(string.Format("[AROR] Mission header: %1", missionHeader), LogLevel.NORMAL);
		else
			Print("[AROR] Mission header: none (world started directly)", LogLevel.NORMAL);

		Print(string.Format("[AROR] Save state: mission '%1', enabled %2, allowed %3, types %4, busy %5, playthrough %6", SaveGameManager.GetCurrentMissionResource(), saveManager.IsSavingEnabled(), saveManager.IsSavingAllowed(), saveManager.GetEnabledSaveTypes(), saveManager.IsBusy(), saveManager.GetCurrentPlaythroughNumber()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: save soon because something worth keeping changed (no callers yet). Does nothing before a playthrough runs.
	static void RequestAutosave_S()
	{
		if (s_Instance)
			s_Instance.ScheduleAutosave_S();
	}

	//------------------------------------------------------------------------------------------------
	protected void ScheduleAutosave_S()
	{
		if (!Replication.IsServer() || !m_bPlaythroughReady || m_bAutosavePending)
			return;

		m_bAutosavePending = true;
		GetGame().GetCallqueue().CallLater(Autosave_S, AUTOSAVE_EVENT_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Once per world: a playthrough is either created or loaded in it
	protected void StartAutosaveTimer()
	{
		if (m_bAutosaveTimerStarted || m_iAutosaveIntervalSeconds <= 0)
			return;

		m_bAutosaveTimerStarted = true;
		GetGame().GetCallqueue().CallLater(Autosave_S, m_iAutosaveIntervalSeconds * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a new autosave point named after the resistance, then deletes the playthrough's oldest autosaves
	protected void Autosave_S()
	{
		m_bAutosavePending = false;
		if (!m_bPlaythroughReady)
			return;

		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager)
			return;

		// Busy (another save in progress): try again shortly instead of dropping the request
		if (saveManager.IsBusy())
		{
			ScheduleAutosave_S();
			return;
		}

		if (!saveManager.IsSavingAllowed())
			return;

		m_AutosaveCallback = new SaveGameOperationCallback(OnAutosaveCompleted);
		saveManager.RequestSavePoint(ESaveGameType.SCRIPTED, GetSaveName(true), 0, m_AutosaveCallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an admin saves the running campaign (phone quick dock). Manual saves are never pruned. One at a time:
	//! presses while a manual save is pending are refused.
	void RequestManualSave_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		// Same rule as the phone's save slot (includes the host of a listen server)
		if (!AROR_AdminAccess.IsAdministrator(playerId))
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Only the server admin can save the campaign");
			return;
		}

		SaveGameManager saveManager = SaveGameManager.Get();
		if (!m_bPlaythroughReady || !saveManager || !saveManager.IsSavingEnabled())
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Saving is not available right now");
			return;
		}

		if (m_iManualSaveRequesterId != 0)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "The campaign is already being saved");
			return;
		}

		m_iManualSaveRequesterId = playerId;
		m_iManualSaveAttempts = 0;
		TryManualSave_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Waits a few seconds at most for another save to finish
	protected void TryManualSave_S()
	{
		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager || !m_bPlaythroughReady)
		{
			FinishManualSave_S("Saving is not available right now");
			return;
		}

		if (saveManager.IsBusy())
		{
			m_iManualSaveAttempts++;
			if (m_iManualSaveAttempts > MANUAL_SAVE_MAX_WAITS)
			{
				FinishManualSave_S("Saving is busy, try again in a moment");
				return;
			}

			GetGame().GetCallqueue().CallLater(TryManualSave_S, PRUNE_RETRY_MS);
			return;
		}

		if (!saveManager.IsSavingAllowed())
		{
			FinishManualSave_S("Saving is not available right now");
			return;
		}

		// RequestSavePoint returns nothing; a failure arrives through the callback
		m_ManualSaveCallback = new SaveGameOperationCallback(OnManualSaveCompleted);
		saveManager.RequestSavePoint(ESaveGameType.SCRIPTED, GetSaveName(false), 0, m_ManualSaveCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnManualSaveCompleted(bool success, Managed context = null)
	{
		if (success)
		{
			Print(string.Format("[AROR] Manual save by player %1", m_iManualSaveRequesterId), LogLevel.NORMAL);
			FinishManualSave_S("Campaign saved");
		}
		else
		{
			Print("[AROR] Manual save failed", LogLevel.ERROR);
			FinishManualSave_S("Saving failed");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the requester the outcome and allows the next manual save
	protected void FinishManualSave_S(string message)
	{
		int playerId = m_iManualSaveRequesterId;
		m_iManualSaveRequesterId = 0;
		if (playerId != 0)
			AROR_NotificationManagerComponent.SendHint_S(playerId, message);
	}

	//------------------------------------------------------------------------------------------------
	//! Save points are named after the resistance.
	//! Autosaves carry AUTOSAVE_PREFIX.
	protected static string GetSaveName(bool autosave)
	{
		string name = "Resistance";
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (highCommand)
			name = highCommand.GetResistanceName();

		if (autosave)
			return AUTOSAVE_PREFIX + name;

		return name;
	}

	//------------------------------------------------------------------------------------------------
	// Autosave pruning (server)
	//------------------------------------------------------------------------------------------------
	protected void PruneAutosaves_S()
	{
		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager || !m_aPruneQueue.IsEmpty())
			return;

		m_PruneCallback = new SaveGameObtainCallback(OnPruneSavesObtained);
		saveManager.GetSaves(SaveGameManager.GetCurrentMissionResource(), m_PruneCallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the newest m_iAutosavesKept autosaves of the running playthrough; never the active save or manual saves
	protected void OnPruneSavesObtained(bool success, array<SaveGame> saves, Managed context = null)
	{
		SaveGameManager saveManager = SaveGameManager.Get();
		if (!success || !saves || !saveManager)
			return;

		// The active save is the one just written; its key (number and start time) keeps another campaign that
		// reused the playthrough number out of the pruning
		SaveGame active = saveManager.GetActiveSave();
		if (!active)
			return;

		string playthroughKey = GetPlaythroughKey(active);
		array<SaveGame> autosaves = {};
		foreach (SaveGame save : saves)
		{
			if (save && GetPlaythroughKey(save) == playthroughKey && IsAutosave(save))
				InsertNewestFirst(autosaves, save);
		}

		for (int i = m_iAutosavesKept, count = autosaves.Count(); i < count; i++)
		{
			if (!IsSameSave(autosaves[i], active))
				m_aPruneQueue.Insert(autosaves[i]);
		}

		if (!m_aPruneQueue.IsEmpty())
			DeleteNextPruned_S();
	}

	//------------------------------------------------------------------------------------------------
	//! One delete at a time: the manager is busy while it deletes
	protected void DeleteNextPruned_S()
	{
		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager)
		{
			m_aPruneQueue.Clear();
			return;
		}

		if (saveManager.IsBusy())
		{
			GetGame().GetCallqueue().CallLater(DeleteNextPruned_S, PRUNE_RETRY_MS);
			return;
		}

		SaveGame save = m_aPruneQueue[0];
		m_aPruneQueue.RemoveOrdered(0);
		if (save && !IsSameSave(save, saveManager.GetActiveSave()))
		{
			Print(string.Format("[AROR] Deleting old autosave '%1' (save point %2)", save.GetSavePointName(), save.GetSavePointNumber()), LogLevel.NORMAL);
			saveManager.Delete(save);
		}

		if (!m_aPruneQueue.IsEmpty())
			GetGame().GetCallqueue().CallLater(DeleteNextPruned_S, PRUNE_RETRY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, after a load: the garage prop does not reliably come back with the save, so spawn it beside the HQ
	//! (where a new campaign puts it) if none stands there. Stored vehicles are restored separately by the Garage.
	protected void EnsureGarage_S()
	{
		AROR_LocationComponent hq = AROR_HighCommandComponent.FindHQ();
		if (!hq)
		{
			Print("[AROR] No Resistance HQ after loading; the garage was not checked", LogLevel.WARNING);
			return;
		}

		vector hqPosition = hq.GetOwner().GetOrigin();
		if (AROR_GarageSiteQuery.HasGarageNear(hqPosition, ENSURE_GARAGE_RADIUS))
			return;

		IEntity garage = SpawnGarage(hqPosition - m_vArsenalOffsetFromHQ);
		PlaceClearOfFlag(garage, hqPosition, -m_vArsenalOffsetFromHQ);
		if (garage)
			Print(string.Format("[AROR] Garage missing after load: respawned at %1", garage.GetOrigin()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAutosaveCompleted(bool success, Managed context = null)
	{
		if (success)
			PruneAutosaves_S();
		else
			Print("[AROR] Autosave failed", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnInitialSaveCompleted(bool success, Managed context = null)
	{
		if (success)
			Print("[AROR] Initial save point created", LogLevel.NORMAL);
		else
			Print("[AROR] Failed to create the initial save point", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	// Fresh boot (server)
	//------------------------------------------------------------------------------------------------
	//! The game resumed a save by itself: reload the scenario without it. Requesting the world change during world
	//! init is ignored by the engine ("No active game state"), so attempts only begin once the game has started.
	protected void BeginFreshRestart_S()
	{
		WriteMarker(RESTART_MARKER_FILE);
		m_bFreshRestartPending = true;
		m_bFreshRestartAttempting = false;
		m_iFreshRestartAttempts = 0;
		Print("[AROR] A save was resumed by the game: reloading the scenario fresh once the game has started", LogLevel.NORMAL);

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetOwner());
		if (gameMode)
			gameMode.GetOnGameStart().Insert(StartFreshRestartAttempts_S);

		// In case the game-start event never comes: start the attempts anyway rather than stay locked
		GetGame().GetCallqueue().CallLater(StartFreshRestartAttempts_S, FRESH_RESTART_START_TIMEOUT_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Game started (or the timeout passed): first attempt next frame, then retries
	protected void StartFreshRestartAttempts_S()
	{
		if (!m_bFreshRestartPending || m_bFreshRestartAttempting)
			return;

		m_bFreshRestartAttempting = true;
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetOwner());
		if (gameMode)
			gameMode.GetOnGameStart().Remove(StartFreshRestartAttempts_S);

		GetGame().GetCallqueue().Remove(StartFreshRestartAttempts_S);
		GetGame().GetCallqueue().CallLater(TryFreshRestart_S, 0);
		GetGame().GetCallqueue().CallLater(TryFreshRestart_S, FRESH_RESTART_RETRY_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! An accepted request unloads this world (and its call queue); an ignored one only logs an engine error
	protected void TryFreshRestart_S()
	{
		if (!m_bFreshRestartPending)
			return;

		if (m_iFreshRestartAttempts >= FRESH_RESTART_MAX_ATTEMPTS)
		{
			FailFreshRestart_S();
			return;
		}

		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager)
		{
			FailFreshRestart_S();
			return;
		}

		m_iFreshRestartAttempts++;
		Print(string.Format("[AROR] Reloading the scenario fresh (attempt %1 of %2)", m_iFreshRestartAttempts, FRESH_RESTART_MAX_ATTEMPTS), LogLevel.NORMAL);
		saveManager.StartPlaythrough(SaveGameManager.GetCurrentMissionResource(), string.Empty, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The reload never happened: keep the resumed world, refuse "new" (it would sit on top of the save) and let the
	//! admin load a save instead
	protected void FailFreshRestart_S()
	{
		m_bFreshRestartPending = false;
		GetGame().GetCallqueue().Remove(TryFreshRestart_S);

		GetGame().GetCallqueue().Remove(StartFreshRestartAttempts_S);
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetOwner());
		if (gameMode)
			gameMode.GetOnGameStart().Remove(StartFreshRestartAttempts_S);

		// So a later boot does not mistake it for a restart that just happened
		if (FileIO.FileExists(RESTART_MARKER_FILE))
			FileIO.DeleteFile(RESTART_MARKER_FILE);

		m_bResumedSave = true;
		Print("[AROR] Could not reload the scenario fresh: waiting for an admin to load a playthrough", LogLevel.ERROR);

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			TryOfferSetup(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The mission's saves at boot, for CheckPlaythroughNumberUnused
	protected void FetchKnownSaves_S()
	{
		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager)
			return;

		m_KnownSavesCallback = new SaveGameObtainCallback(OnKnownSavesObtained);
		saveManager.GetSaves(SaveGameManager.GetCurrentMissionResource(), m_KnownSavesCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnKnownSavesObtained(bool success, array<SaveGame> saves, Managed context = null)
	{
		m_aKnownSaves.Clear();
		if (success && saves)
			m_aKnownSaves.Copy(saves);
	}

	//------------------------------------------------------------------------------------------------
	//! Diagnostic: a new playthrough must not take the number of an existing one, or its save points mix with that
	//! campaign's. Seen when a new campaign was started on top of a resumed save; a fresh boot should prevent it.
	protected void CheckPlaythroughNumberUnused()
	{
		SaveGameManager saveManager = SaveGameManager.Get();
		if (!saveManager)
			return;

		int number = saveManager.GetCurrentPlaythroughNumber();
		foreach (SaveGame known : m_aKnownSaves)
		{
			if (known && known.GetPlaythroughNumber() == number)
			{
				Print(string.Format("[AROR] New playthrough reuses number %1 of existing playthrough '%2'; its saves will mix", number, known.GetPlaythroughName()), LogLevel.ERROR);
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Lifecycle
	//------------------------------------------------------------------------------------------------
	override void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);
		TryOfferSetup(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Covers an admin logging in after joining
	override void OnPlayerRoleChange(int playerId, EPlayerRole roleFlags)
	{
		super.OnPlayerRoleChange(playerId, roleFlags);
		TryOfferSetup(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! A rejoining admin gets the setup screen again
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);
		m_aSetupOfferedPlayerIds.RemoveItem(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;

		if (!Replication.IsServer())
			return;

		SaveGameManager saveManager = SaveGameManager.Get();
		if (saveManager)
			LogSaveState();

		// Launching the mission always starts fresh with the setup screen. A save is restored only when the admin
		// chose it there (RequestLoad_S). If the game resumed a save by itself (a server resuming its last save,
		// Continue from the menu), drop it and reload the scenario once, so no saved entities or characters return.
		bool saveActive = saveManager && saveManager.GetActiveSave() != null;
		bool loadRequested = ConsumeMarker(LOAD_MARKER_FILE);
		bool restarted = ConsumeMarker(RESTART_MARKER_FILE);
		if (saveActive && loadRequested)
		{
			m_bPlaythroughReady = true;
			saveManager.SetSavingAllowed(true);
			StartAutosaveTimer();
			GetGame().GetCallqueue().CallLater(EnsureGarage_S, ENSURE_GARAGE_DELAY_MS);
			Print("[AROR] Active save found: restoring the playthrough chosen in setup", LogLevel.NORMAL);
		}
		else
		{
			if (saveManager)
				saveManager.SetSavingAllowed(false);

			if (saveActive && !restarted)
			{
				BeginFreshRestart_S();
			}
			else if (saveActive)
			{
				// The restart did not clear it: setup is still offered, but "new" is refused (see RequestNewPlaythrough_S)
				m_bResumedSave = true;
				Print("[AROR] A resumed save is still active: waiting for an admin to load a playthrough", LogLevel.WARNING);
			}
			else
			{
				Print("[AROR] No active save: waiting for an admin to create or load a playthrough", LogLevel.NORMAL);
			}

			FetchKnownSaves_S();
		}

		// Staging spawns that registered before this component (enabled only while no playthrough exists)
		if (s_aPendingStagingSpawns)
		{
			foreach (SCR_SpawnPoint spawnPoint : s_aPendingStagingSpawns)
			{
				if (spawnPoint)
					AddStagingSpawn(spawnPoint);
			}

			s_aPendingStagingSpawns.Clear();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;

		// Timers and retries must not outlive the game mode
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
		{
			callQueue.Remove(Autosave_S);
			callQueue.Remove(TryManualSave_S);
			callQueue.Remove(DeleteNextPruned_S);
			callQueue.Remove(EnsureGarage_S);
			callQueue.Remove(StartFreshRestartAttempts_S);
			callQueue.Remove(TryFreshRestart_S);
		}

		super.OnDelete(owner);
	}
}
