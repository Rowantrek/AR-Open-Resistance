[ComponentEditorProps(category: "OpenResistance/Admin", description: "Admin/testing menu (Ctrl+T). Client only. Attach to the game mode entity.")]
class AROR_AdminMenuComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Admin/testing tools, opened with Ctrl+T by administrators; Workbench permits local testing.
//! The interface is the full-screen AROR_AdminMenu (AROR_AdminApp draws it); this component holds the server-side
//! actions and the two map-picking tools that need the base-game map open:
//! - Teleport: the clicked position teleports the local player there.
//! - Capture: the admin chooses a side, then every click on the map gives the nearest location to that side, until
//!   the map is closed (the admin menu then reopens on its Locations page).
//! The hotkey is read directly from the keyboard to avoid overriding the base-game input config.
class AROR_AdminMenuComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", desc: "Menu Available: administrators can open Ctrl+T and request debug actions. Workbench permits local testing.")]
	protected bool m_bMenuAvailable;

	//! Wait for the server to grant Game Master before opening the editor locally
	protected static const int GAME_MASTER_OPEN_DELAY_MS = 1000;

	//! Pause between the map closing and the admin menu reopening (the menu manager is busy closing the map)
	protected static const int REOPEN_DELAY_MS = 200;

	//! A map click captures the nearest location only if it is this close (metres) or inside the location's radius
	protected static const float CAPTURE_PICK_DISTANCE = 300;

	protected static const int PICK_NONE = 0;
	protected static const int PICK_TELEPORT = 1;
	protected static const int PICK_CAPTURE = 2;

	protected static const string INPUT_CONTEXT = "AROR_Context";
	protected static const string INPUT_ACTION = "AROR_OpenAdminMenu";
	protected static const int TOGGLE_COOLDOWN_MS = 250;

	protected static AROR_AdminMenuComponent s_Instance;

	protected int m_iLastToggleMs = -TOGGLE_COOLDOWN_MS;
	protected bool m_bActionDown;
	protected bool m_bAltDown;
	protected int m_iPickMode;
	protected AROR_EFactionRole m_eCaptureRole;

	//------------------------------------------------------------------------------------------------
	//! Local UI policy; server actions independently authorize their requester.
	protected bool CanUseLocalTools()
	{
		if (!m_bMenuAvailable)
			return false;
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		return player && AROR_AdminAccess.IsAdministrator(player.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Client: true when this player may use the admin tools (the menu asks before it draws anything)
	bool CanOpenMenu()
	{
		return CanUseLocalTools();
	}

	//! Polled every frame on the client while enabled.
	protected void CheckHotkey()
	{
		// Shares the phone's input context, which the phone component activates every frame
		InputManager inputManager = GetGame().GetInputManager();
		inputManager.ActivateContext(INPUT_CONTEXT);

		// Third route: poll the action's value in case the listener is never called
		bool actionDown = inputManager.GetActionValue(INPUT_ACTION) > 0;
		if (actionDown != m_bActionDown)
		{
			m_bActionDown = actionDown;
			Print(string.Format("[AROR] Admin menu key action %1", actionDown), LogLevel.NORMAL);
			if (actionDown)
				ToggleAdminMenu();
		}

		// F10: same toggle without Ctrl, on a key the base game leaves free
		bool altDown = inputManager.GetActionValue("AROR_OpenAdminMenuAlt") > 0;
		if (altDown != m_bAltDown)
		{
			m_bAltDown = altDown;
			Print(string.Format("[AROR] Admin menu F10 action %1", altDown), LogLevel.NORMAL);
			if (altDown)
				ToggleAdminMenu();
		}

		if (!Debug.KeyState(KeyCode.KC_T))
			return;

		if (!Debug.KeyState(KeyCode.KC_LCONTROL) && !Debug.KeyState(KeyCode.KC_RCONTROL))
			return;

		Debug.ClearKey(KeyCode.KC_T);
		ToggleAdminMenu();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAdminAction(float value = 0.0, EActionTrigger reason = 0)
	{
		Print("[AROR] Admin menu listener fired", LogLevel.NORMAL);
		ToggleAdminMenu();
	}

	//------------------------------------------------------------------------------------------------
	protected void ToggleAdminMenu()
	{
		// The input action and the keyboard check can both fire for one press
		int now = System.GetTickCount();
		if (now - m_iLastToggleMs < TOGGLE_COOLDOWN_MS)
			return;

		m_iLastToggleMs = now;

		// Ctrl+T again closes the menu
		AROR_AdminMenu open = AROR_AdminMenu.GetInstance();
		if (open)
		{
			open.Close();
			return;
		}

		// Not while another menu is open or a position is being picked
		if (m_iPickMode != PICK_NONE || GetGame().GetMenuManager().IsAnyMenuOpen())
			return;

		OpenAdminMenu();
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenAdminMenu()
	{
		if (!CanUseLocalTools())
			return;

		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.AROR_AdminMenu);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: add money to a faction's funds, or (selfMoney) to the requesting player's own money
	void GiveMoney_S(int playerId, AROR_EFactionRole role, bool selfMoney, int amount)
	{
		if (!Replication.IsServer() || !m_bMenuAvailable || !AROR_AdminAccess.IsAdministrator(playerId))
			return;

		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy)
			return;

		if (selfMoney)
			economy.AddPlayerMoney_S(playerId, amount);
		else
			economy.AddFactionFunds_S(role, amount);

		Print(string.Format("[AROR] Admin: player %1 gave %2 to %3", playerId, amount, GetMoneyTargetName(role, selfMoney)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: run an income payment now instead of waiting for the timer
	void PayIncomeNow_S(int playerId)
	{
		if (!Replication.IsServer() || !m_bMenuAvailable || !AROR_AdminAccess.IsAdministrator(playerId))
			return;

		AROR_IncomeComponent income = AROR_IncomeComponent.GetInstance();
		if (!income)
		{
			Print("[AROR] Admin: no AROR_IncomeComponent on the game mode", LogLevel.WARNING);
			return;
		}

		Print(string.Format("[AROR] Admin: player %1 triggered an income payment", playerId), LogLevel.NORMAL);
		income.PayIncome_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: add one of every Resistance preset item (gear, equipment, weapons, accessories,
	//! ammunition, vehicle ammunition — not units or vehicles) to Resistance Storage
	void FillResistanceStock_S(int playerId)
	{
		if (!Replication.IsServer() || !m_bMenuAvailable || !AROR_AdminAccess.IsAdministrator(playerId))
			return;

		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.GetInstance();
		if (!presets || !storage)
		{
			Print("[AROR] Admin: cannot fill Resistance stock (missing presets or storage)", LogLevel.WARNING);
			return;
		}

		AROR_FactionPreset preset = presets.GetPreset(AROR_EFactionRole.RESISTANCE);
		if (!preset)
		{
			Print("[AROR] Admin: Resistance has no faction preset", LogLevel.WARNING);
			return;
		}

		array<ResourceName> prefabs = {};
		CollectResistanceStockPrefabs(preset, prefabs);
		storage.AddMany_S(prefabs, 1);
		Print(string.Format("[AROR] Admin: player %1 filled Resistance stock from preset '%2' (%3 prefabs)", playerId, preset.GetId(), prefabs.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Item categories only (excludes units and vehicles)
	protected static void CollectResistanceStockPrefabs(notnull AROR_FactionPreset preset, notnull array<ResourceName> outPrefabs)
	{
		CollectCategoryPrefabs(preset, AROR_EFactionPresetCategory.GEAR, outPrefabs);
		CollectCategoryPrefabs(preset, AROR_EFactionPresetCategory.EQUIPMENT, outPrefabs);
		CollectCategoryPrefabs(preset, AROR_EFactionPresetCategory.WEAPONS, outPrefabs);
		CollectCategoryPrefabs(preset, AROR_EFactionPresetCategory.WEAPON_ACCESSORIES, outPrefabs);
		CollectCategoryPrefabs(preset, AROR_EFactionPresetCategory.AMMUNITION, outPrefabs);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectCategoryPrefabs(notnull AROR_FactionPreset preset, AROR_EFactionPresetCategory category, notnull array<ResourceName> outPrefabs)
	{
		array<AROR_FactionPresetEntry> entries = {};
		preset.GetAllEntries(category, entries);
		foreach (AROR_FactionPresetEntry entry : entries)
		{
			ResourceName prefab = entry.GetPrefab();
			if (!prefab.IsEmpty())
				outPrefabs.Insert(prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetMoneyTargetName(AROR_EFactionRole role, bool selfMoney)
	{
		if (selfMoney)
			return "themselves";

		return typename.EnumToString(AROR_EFactionRole, role);
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest location to the entity that has support (towns and cities)
	static AROR_SettlementComponent FindNearestSupport(IEntity entity)
	{
		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (!entity || !locationManager)
			return null;

		array<AROR_LocationComponent> locations = {};
		locationManager.GetLocations(locations);

		AROR_SettlementComponent nearest;
		float nearestDistance = float.MAX;
		foreach (AROR_LocationComponent location : locations)
		{
			if (!location)
				continue;

			AROR_SettlementComponent support = AROR_SettlementComponent.Cast(location.GetOwner().FindComponent(AROR_SettlementComponent));
			if (!support)
				continue;

			float distance = vector.DistanceXZ(location.GetCenter(), entity.GetOrigin());
			if (distance < nearestDistance)
			{
				nearestDistance = distance;
				nearest = support;
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: open the local editor once the server has granted Game Master
	protected void OpenGameMaster()
	{
		if (!CanUseLocalTools())
			return;
		if (SCR_EditorManagerEntity.CanOpenInstance())
			SCR_EditorManagerEntity.OpenInstance();
		else
			Print("[AROR] Admin: Game Master granted but the editor cannot be opened yet", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: unlock the Game Master (Edit) editor mode for the requesting player
	void GrantGameMaster_S(int playerId)
	{
		if (!Replication.IsServer() || !m_bMenuAvailable || !AROR_AdminAccess.IsAdministrator(playerId))
			return;

		SCR_EditorManagerCore editorCore = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (!editorCore)
		{
			Print("[AROR] Admin: Game Master unavailable (no editor manager core)", LogLevel.WARNING);
			return;
		}

		SCR_EditorManagerEntity editorManager = editorCore.GetEditorManager(playerId);
		if (!editorManager)
		{
			Print(string.Format("[AROR] Admin: no editor manager for player %1", playerId), LogLevel.WARNING);
			return;
		}

		// Adding the mode is not enough: opening is a separate permission
		editorManager.AddEditorModes(EEditorModeAccess.BASE, EEditorMode.EDIT);
		editorManager.SetCanOpen(true, EEditorCanOpen.SCRIPT);
		editorManager.SetCurrentMode(EEditorMode.EDIT);
		Print(string.Format("[AROR] Admin: Game Master granted to player %1", playerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a headline and a notification for everyone (they queue), and a hint for the requesting player
	void SendTestNotifications_S(int playerId)
	{
		if (!Replication.IsServer() || !m_bMenuAvailable || !AROR_AdminAccess.IsAdministrator(playerId))
			return;

		Color resistanceColor;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (roles && roles.GetFaction(AROR_EFactionRole.RESISTANCE))
			resistanceColor = roles.GetFaction(AROR_EFactionRole.RESISTANCE).GetFactionColor();

		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.NOTIFICATION, "Test notification from the admin menu", string.Empty, resistanceColor, "Admin test"));
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.HEADLINE, "Test headline", string.Empty, resistanceColor));
		AROR_NotificationManagerComponent.SendHint_S(playerId, "Test hint: only you can see this");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: adjust support of the town or city nearest to the requesting player
	void AdjustNearestSupport_S(int playerId, AROR_EFactionRole role, float delta)
	{
		if (!Replication.IsServer() || !m_bMenuAvailable || !AROR_AdminAccess.IsAdministrator(playerId))
			return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		AROR_SettlementComponent support = FindNearestSupport(player);
		if (!support)
			return;

		support.AddSupport_S(role, delta);
		Print(string.Format("[AROR] Admin: %1 support at '%2' %3 by %4 (player %5)", typename.EnumToString(AROR_EFactionRole, role), support.GetLocation().GetName(), "changed", delta, playerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives a location to a side. Towns and cities go through support (the side gets 100%), military sites
	//! change hands directly, ignoring the flip cooldown and strongholds. Locations with fixed control (the Resistance
	//! HQ, the support corridor) are refused. The requesting player gets a hint with the outcome.
	void CaptureLocation_S(int playerId, string locationId, AROR_EFactionRole role)
	{
		if (!Replication.IsServer() || !m_bMenuAvailable || !AROR_AdminAccess.IsAdministrator(playerId))
			return;

		if (!AROR_FactionRoles.IsMilitary(role))
			return;

		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent location;
		if (locations)
			location = locations.GetLocationById(locationId);

		if (!location)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Capture failed: that location no longer exists.");
			return;
		}

		string sideName = AROR_FactionRoles.GetDisplayName(role);
		if (location.IsControlFixed())
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("%1 cannot change hands: its control is fixed.", location.GetName()));
			return;
		}

		if (location.GetControllingRole() == role)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("%1 is already held by %2.", location.GetName(), sideName));
			return;
		}

		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(location.GetOwner().FindComponent(AROR_SettlementComponent));
		if (settlement)
			settlement.SetSupport_S(role, 100);

		// Support may already have flipped it; otherwise (military sites, or a blocked flip) force it
		location.ForceControllingRole_S(role);
		AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("%1 is now held by %2.", location.GetName(), sideName), AROR_UIColors.Get(AROR_UIColors.REBEL));
		Print(string.Format("[AROR] Admin: player %1 gave '%2' to %3", playerId, location.GetName(), typename.EnumToString(AROR_EFactionRole, role)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the admin menu once it has closed; handled next frame, outside the menu manager's close sequence
	void QueueAction(string tag)
	{
		GetGame().GetCallqueue().CallLater(HandleAction, 0, false, tag);
	}

	//------------------------------------------------------------------------------------------------
	//! Actions that need the admin menu closed first. Everything else the menu requests directly.
	protected void HandleAction(string tag)
	{
		if (!CanUseLocalTools())
			return;

		if (tag == "teleport")
		{
			StartPicking(PICK_TELEPORT);
			return;
		}

		if (tag.IndexOf("capture:") == 0)
		{
			m_eCaptureRole = tag.Substring(8, tag.Length() - 8).ToInt();
			StartPicking(PICK_CAPTURE);
			return;
		}

		if (tag == "gamemaster")
		{
			SCR_PlayerController gmController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (gmController)
				gmController.AROR_RequestAdminGameMaster();

			// Give the server time to grant access, then open the editor locally
			GetGame().GetCallqueue().CallLater(OpenGameMaster, GAME_MASTER_OPEN_DELAY_MS);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Map picking (teleport, capture)
	//------------------------------------------------------------------------------------------------
	protected void StartPicking(int mode)
	{
		if (!CanUseLocalTools())
			return;
		if (m_iPickMode != PICK_NONE)
			return;

		m_iPickMode = mode;
		AROR_LocationMapIconsComponent.SetInfoPopupSuppressed(true);
		SCR_MapEntity.GetOnSelection().Insert(OnMapSelection);
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpened);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClosed);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);

		// Esc is not available in Workbench, so M closes the map too
		GetGame().GetCallqueue().CallLater(CheckMapCloseKey, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Polled while a map pick is active: M closes the map (and with it ends the pick)
	protected void CheckMapCloseKey()
	{
		if (!Debug.KeyState(KeyCode.KC_M))
			return;

		Debug.ClearKey(KeyCode.KC_M);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapOpened(MapConfiguration config)
	{
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpened);

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
			mapEntity.ZoomOut();

		if (m_iPickMode == PICK_CAPTURE)
			AROR_NotificationManagerComponent.ShowHint(string.Format("Capture for %1: click locations on the map. M or Esc to finish.",AROR_FactionRoles.GetDisplayName(m_eCaptureRole)));
		else if (m_iPickMode == PICK_TELEPORT)
			AROR_NotificationManagerComponent.ShowHint("Teleport: click a position on the map.");
	}

	//------------------------------------------------------------------------------------------------
	//! The selection event gives screen coordinates; read the world position under the map cursor
	protected void OnMapSelection(vector screenPosition)
	{
		if (!CanUseLocalTools())
		{
			StopPicking();
			return;
		}
		if (m_iPickMode == PICK_NONE)
			return;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return;

		float worldX, worldZ;
		mapEntity.GetMapCursorWorldPosition(worldX, worldZ);

		if (m_iPickMode == PICK_CAPTURE)
		{
			// The map stays open for the next click
			CaptureAt(Vector(worldX, 0, worldZ));
			return;
		}

		vector target = Vector(worldX, GetGame().GetWorld().GetSurfaceY(worldX, worldZ), worldZ);
		StopPicking();
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);

		// The owning client moves its own character; movement replicates to the server
		if (!SCR_Global.TeleportLocalPlayer(target))
			Print(string.Format("[AROR] Teleport to %1 failed (not spawned?)", target), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to give the location nearest to the map click to the chosen side
	protected void CaptureAt(vector position)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return;

		AROR_LocationComponent location = locations.GetNearestLocation(position);
		if (!location || (!location.IsPositionInside(position) && vector.DistanceXZ(location.GetCenter(), position) > CAPTURE_PICK_DISTANCE))
		{
			AROR_NotificationManagerComponent.ShowHint("No location there. Click closer to one.");
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.AROR_RequestAdminCapture(location.GetId(), m_eCaptureRole);
	}

	//------------------------------------------------------------------------------------------------
	//! Closing the map cancels a teleport; after capturing it brings the admin menu back
	protected void OnMapClosed(MapConfiguration config)
	{
		bool reopen = m_iPickMode == PICK_CAPTURE;
		StopPicking();
		if (reopen)
			GetGame().GetCallqueue().CallLater(OpenAdminMenu, REOPEN_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopPicking()
	{
		m_iPickMode = PICK_NONE;
		GetGame().GetCallqueue().Remove(CheckMapCloseKey);
		AROR_LocationMapIconsComponent.SetInfoPopupSuppressed(false);
		SCR_MapEntity.GetOnSelection().Remove(OnMapSelection);
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpened);
		SCR_MapEntity.GetOnMapClose().Remove(OnMapClosed);
	}

	//------------------------------------------------------------------------------------------------
	static AROR_AdminMenuComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		// The server instance handles admin requests sent through the player controller
		s_Instance = this;

		if (!m_bMenuAvailable || RplSession.Mode() == RplMode.Dedicated)
			return;

		Print("[AROR] Admin menu hotkey ready (input action + polling)", LogLevel.NORMAL);
		GetGame().GetInputManager().AddActionListener(INPUT_ACTION, EActionTrigger.DOWN, OnAdminAction);
		GetGame().GetCallqueue().CallLater(CheckHotkey, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckHotkey);

		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.RemoveActionListener(INPUT_ACTION, EActionTrigger.DOWN, OnAdminAction);

		GetGame().GetCallqueue().Remove(HandleAction);
		GetGame().GetCallqueue().Remove(OpenGameMaster);
		GetGame().GetCallqueue().Remove(OpenAdminMenu);
		StopPicking();

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
