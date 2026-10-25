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
//! Server: validates admin requests, spawns the Resistance HQ, starts or loads playthroughs.
//! Clients: receive setup messages through the AROR_ methods on SCR_PlayerController and raise the events below for the UI.
class AROR_GameFlowComponent : SCR_BaseGameModeStateComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Resistance HQ prefab spawned at the chosen position", params: "et")]
	protected ResourceName m_sHQPrefab;

	[Attribute("100", UIWidgets.Slider, desc: "Minimum distance in metres between the HQ and the edge (radius) of any location", params: "0 5000 10")]
	protected float m_fHQMinDistanceFromLocation;


	[Attribute("1", desc: "Players can move at the staging spawn while the admin sets up the campaign")]
	protected bool m_bAllowControlsInSetup;

	[Attribute("8", UIWidgets.Slider, desc: "Players are moved to random points within this radius of the HQ when it is created", params: "0 50 1")]
	protected float m_fHQArrivalRadius;

	[Attribute("1", desc: "Workbench only: treat every player as admin so setup can be tested", category: "Workbench")]
	protected bool m_bWorkbenchEveryoneIsAdmin;

	[Attribute("0", desc: "Workbench only: skip the setup screen and create a new playthrough with the HQ at the position below", category: "Workbench")]
	protected bool m_bWorkbenchAutoNewPlaythrough;

	[Attribute("0 0 0", UIWidgets.Coords, desc: "Workbench only: HQ position used by the auto new playthrough", category: "Workbench")]
	protected vector m_vWorkbenchHQPosition;

	protected static AROR_GameFlowComponent s_Instance;

	//! Server: staging spawns that initialised before the game flow existed
	protected static ref array<SCR_SpawnPoint> s_aPendingStagingSpawns;

	//! Server: staging spawn points (enabled only until a playthrough exists)
	protected ref array<SCR_SpawnPoint> m_aStagingSpawns = {};

	//! Server: true once a playthrough is created or loaded; lets the game mode leave pre-game
	protected bool m_bPlaythroughReady;

	//! Server: players already offered the setup screen (join and role change can both trigger it)
	protected ref array<int> m_aSetupOfferedPlayerIds = {};

	//! Server: saves last sent to the admin, newest first; the client refers to them by index
	protected ref array<SaveGame> m_aListedSaves = {};
	protected int m_iSaveListRequesterId;
	protected ref SaveGameObtainCallback m_SaveListCallback;
	protected ref SaveGameOperationCallback m_InitialSaveCallback;

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
		if (world && world.GetSurfaceY(position[0], position[2]) <= 0)
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

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Server: requests from the admin (arrive through SCR_PlayerController RPCs)
	//------------------------------------------------------------------------------------------------
	void RequestNewPlaythrough_S(int playerId, vector hqPosition, string playthroughName)
	{
		if (!CanAcceptSetupRequest(playerId))
			return;

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

		SaveGameManager saveManager = SaveGameManager.Get();
		if (saveManager)
		{
			// TODO: verify that transition = false starts the playthrough without reloading the world
			saveManager.StartPlaythrough(SaveGameManager.GetCurrentMissionResource(), playthroughName, false);
			saveManager.SetSavingAllowed(true);

			LogSaveState();

			// Initial save point so new and loaded playthroughs share the same restore path
			m_InitialSaveCallback = new SaveGameOperationCallback(OnInitialSaveCompleted);
			saveManager.RequestSavePoint(ESaveGameType.SCRIPTED, "Start", 0, m_InitialSaveCallback);
		}

		m_bPlaythroughReady = true;
		DisableStagingSpawns();
		MovePlayersToHQ(hq.GetOrigin());

		SendHQEstablishedHeadline(hq);

		Print(string.Format("[AROR] New playthrough '%1' started, HQ at %2", playthroughName, hq.GetOrigin()), LogLevel.NORMAL);
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
		saveManager.Load(m_aListedSaves[saveIndex], true);
	}

	//------------------------------------------------------------------------------------------------
	// Server internals
	//------------------------------------------------------------------------------------------------
	protected bool CanAcceptSetupRequest(int playerId)
	{
		if (!Replication.IsServer())
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
		if (!Replication.IsServer() || m_bPlaythroughReady || !IsAdmin(playerId))
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

		if (success && saves)
		{
			foreach (SaveGame save : saves)
			{
				InsertNewestFirst(save);
			}
		}

		array<string> labels = {};
		foreach (SaveGame save : m_aListedSaves)
		{
			labels.Insert(FormatSaveLabel(save));
		}

		SCR_PlayerController playerController = GetPlayerController(m_iSaveListRequesterId);
		if (playerController)
			playerController.AROR_SendSaveList_S(labels);
	}

	//------------------------------------------------------------------------------------------------
	protected void InsertNewestFirst(SaveGame save)
	{
		if (!save)
			return;

		int created = save.GetSavePointCreatedUnix();
		for (int i = 0, count = m_aListedSaves.Count(); i < count; i++)
		{
			if (created > m_aListedSaves[i].GetSavePointCreatedUnix())
			{
				m_aListedSaves.InsertAt(save, i);
				return;
			}
		}

		m_aListedSaves.Insert(save);
	}

	//------------------------------------------------------------------------------------------------
	protected string FormatSaveLabel(SaveGame save)
	{
		int year, month, day, hour, minute, second;
		save.GetSavePointCreatedLocalDateTime(year, month, day, hour, minute, second);

		string label = string.Format("%1 - %2 (%3-%4-%5 %6:%7)", save.GetPlaythroughName(), save.GetSavePointName(), year, month.ToString(2), day.ToString(2), hour.ToString(2), minute.ToString(2));

		if (!save.IsSavePointGameVersionCompatible() || !save.AreSavePointAddonsCompatible())
			label += " [incompatible]";

		return label;
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
	protected void OnInitialSaveCompleted(bool success, Managed context = null)
	{
		if (success)
			Print("[AROR] Initial save point created", LogLevel.NORMAL);
		else
			Print("[AROR] Failed to create the initial save point", LogLevel.ERROR);
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

		// TODO: verify that a loaded save is already active at this point
		if (saveManager && saveManager.GetActiveSave())
		{
			m_bPlaythroughReady = true;
			Print("[AROR] Active save found: restoring playthrough, skipping setup", LogLevel.NORMAL);
		}
		else
		{
			if (saveManager)
				saveManager.SetSavingAllowed(false);

			Print("[AROR] No active save: waiting for an admin to create or load a playthrough", LogLevel.NORMAL);
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

		super.OnDelete(owner);
	}
}
