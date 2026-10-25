[ComponentEditorProps(category: "OpenResistance/Admin", description: "Admin/testing menu (Ctrl+T). Client only. Attach to the game mode entity.")]
class AROR_AdminMenuComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Admin/testing menu, opened with Ctrl+T. Currently available to every player (switch off with Enabled).
//! Teleport: opens the base-game map menu; the clicked position teleports the local player there.
//! The hotkey is read directly from the keyboard to avoid overriding the base-game input config.
class AROR_AdminMenuComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", desc: "Menu available (currently to every player). Turn off for public servers.")]
	protected bool m_bEnabled;

	[Attribute("{5BB4D0ED53225DDC}Configs/UI/AROR_SetupDialogs.conf", UIWidgets.ResourceNamePicker, desc: "Dialog presets (uses the 'aror_admin' preset)", params: "conf")]
	protected ResourceName m_sDialogPresets;

	// Dialog presets: a dialog fits about four buttons in a row, so tools are split into sub-menus
	protected static const string PRESET_ADMIN = "aror_admin";
	protected static const string PRESET_MORE = "aror_admin_more";
	protected static const string PRESET_SUPPORT = "aror_admin_support";
	protected static const string PRESET_ECONOMY = "aror_admin_economy";
	protected static const string PRESET_FUNDS = "aror_admin_funds";

	//! Money added per admin button press
	protected static const int MONEY_STEP = 1000;

	//! Reopen a dialog after the server has applied a change, so it shows the new values
	protected static const int REFRESH_DELAY_MS = 300;

	//! Support change per admin button press (percentage points)
	protected static const float SUPPORT_STEP = 10;

	//! Wait for the server to grant Game Master before opening the editor locally
	protected static const int GAME_MASTER_OPEN_DELAY_MS = 1000;

	protected static AROR_AdminMenuComponent s_Instance;

	protected bool m_bPickingTeleport;

	//------------------------------------------------------------------------------------------------
	//! Polled every frame on the client while enabled
	protected void CheckHotkey()
	{
		if (!Debug.KeyState(KeyCode.KC_T))
			return;

		if (!Debug.KeyState(KeyCode.KC_LCONTROL) && !Debug.KeyState(KeyCode.KC_RCONTROL))
			return;

		Debug.ClearKey(KeyCode.KC_T);

		// Not while another dialog is open or a position is being picked
		if (m_bPickingTeleport || SCR_ConfigurableDialogUi.GetCurrentDialog())
			return;

		OpenMainDialog();
	}

	//------------------------------------------------------------------------------------------------
	//! Opens a preset; an empty message keeps the preset's own
	protected void OpenDialog(string preset, string message = string.Empty)
	{
		AROR_AdminDialog dialog = new AROR_AdminDialog(this);
		SCR_ConfigurableDialogUi.CreateFromPreset(m_sDialogPresets, preset, dialog);
		if (!message.IsEmpty())
			dialog.SetMessage(message);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenMainDialog()
	{
		OpenDialog(PRESET_ADMIN);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenMoreDialog()
	{
		OpenDialog(PRESET_MORE);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenSupportDialog()
	{
		OpenDialog(PRESET_SUPPORT, GetNearestSupportText());
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenEconomyDialog()
	{
		OpenDialog(PRESET_ECONOMY, GetEconomyText());
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenFundsDialog()
	{
		OpenDialog(PRESET_FUNDS, GetEconomyText());
	}

	//------------------------------------------------------------------------------------------------
	//! Client: faction funds and the local player's money (shown in the economy dialog)
	protected string GetEconomyText()
	{
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy)
			return "No AROR_EconomyComponent on the game mode.";

		return string.Format("Funds: Resistance %1 | Occupiers %2 | Invaders %3\nResistance manpower: %4\nYour money: %5",
			economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE),
			economy.GetFactionFunds(AROR_EFactionRole.OCCUPIERS),
			economy.GetFactionFunds(AROR_EFactionRole.INVADERS),
			economy.GetManpower(),
			economy.GetLocalPlayerMoney());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: add money to a faction's funds, or (selfMoney) to the requesting player's own money
	void GiveMoney_S(int playerId, AROR_EFactionRole role, bool selfMoney, int amount)
	{
		if (!Replication.IsServer() || !m_bEnabled)
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
		if (!Replication.IsServer() || !m_bEnabled)
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
	protected static string GetMoneyTargetName(AROR_EFactionRole role, bool selfMoney)
	{
		if (selfMoney)
			return "themselves";

		return typename.EnumToString(AROR_EFactionRole, role);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: support of the town or city nearest to the local player (shown in the dialog)
	protected string GetNearestSupportText()
	{
		AROR_SettlementComponent support = FindNearestSupport(GetGame().GetPlayerController().GetControlledEntity());
		if (!support)
			return "No town or city nearby.";

		return string.Format("Nearest: %1\nResistance %2 | Occupiers %3 | Invaders %4",
			support.GetLocation().GetName(),
			Math.Round(support.GetSupport(AROR_EFactionRole.RESISTANCE)),
			Math.Round(support.GetSupport(AROR_EFactionRole.OCCUPIERS)),
			Math.Round(support.GetSupport(AROR_EFactionRole.INVADERS)));
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
		if (SCR_EditorManagerEntity.CanOpenInstance())
			SCR_EditorManagerEntity.OpenInstance();
		else
			Print("[AROR] Admin: Game Master granted but the editor cannot be opened yet", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: unlock the Game Master (Edit) editor mode for the requesting player
	void GrantGameMaster_S(int playerId)
	{
		if (!Replication.IsServer() || !m_bEnabled)
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
		if (!Replication.IsServer() || !m_bEnabled)
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
		if (!Replication.IsServer() || !m_bEnabled)
			return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		AROR_SettlementComponent support = FindNearestSupport(player);
		if (!support)
			return;

		support.AddSupport_S(role, delta);
		Print(string.Format("[AROR] Admin: %1 support at '%2' %3 by %4 (player %5)", typename.EnumToString(AROR_EFactionRole, role), support.GetLocation().GetName(), "changed", delta, playerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the dialog once it has closed; handled next frame, outside the menu manager's close sequence
	void QueueAction(string tag)
	{
		GetGame().GetCallqueue().CallLater(HandleAction, 0, false, tag);
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleAction(string tag)
	{
		if (tag == "teleport")
		{
			StartTeleportPicking();
			return;
		}

		if (tag == "gamemaster")
		{
			SCR_PlayerController gmController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (gmController)
				gmController.AROR_RequestAdminGameMaster();

			// Give the server time to grant access, then open the editor locally
			GetGame().GetCallqueue().CallLater(OpenGameMaster, GAME_MASTER_OPEN_DELAY_MS);
			return;
		}

		if (tag == "notifications")
		{
			SCR_PlayerController notificationsController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (notificationsController)
				notificationsController.AROR_RequestAdminTestNotifications();

			return;
		}

		// Navigation between the sub-menus
		if (tag == "open_main")
		{
			OpenMainDialog();
			return;
		}

		if (tag == "open_more")
		{
			OpenMoreDialog();
			return;
		}

		if (tag == "open_support")
		{
			OpenSupportDialog();
			return;
		}

		if (tag == "open_economy")
		{
			OpenEconomyDialog();
			return;
		}

		if (tag == "open_funds")
		{
			OpenFundsDialog();
			return;
		}

		if (tag.IndexOf("money_") == 0)
		{
			HandleMoneyAction(tag);
			return;
		}

		if (tag == "income_now")
		{
			SCR_PlayerController incomeController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (incomeController)
				incomeController.AROR_RequestAdminPayIncome();

			GetGame().GetCallqueue().CallLater(OpenEconomyDialog, REFRESH_DELAY_MS);
			return;
		}

		AROR_EFactionRole role;
		if (tag == "support_resistance")
			role = AROR_EFactionRole.RESISTANCE;
		else if (tag == "support_occupiers")
			role = AROR_EFactionRole.OCCUPIERS;
		else if (tag == "support_invaders")
			role = AROR_EFactionRole.INVADERS;
		else
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (playerController)
			playerController.AROR_RequestAdminSupport(role, SUPPORT_STEP);

		GetGame().GetCallqueue().CallLater(OpenSupportDialog, REFRESH_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Economy and faction funds buttons; the dialog they came from reopens with the updated balances
	protected void HandleMoneyAction(string tag)
	{
		AROR_EFactionRole role;
		bool selfMoney;
		if (tag == "money_resistance")
			role = AROR_EFactionRole.RESISTANCE;
		else if (tag == "money_occupiers")
			role = AROR_EFactionRole.OCCUPIERS;
		else if (tag == "money_invaders")
			role = AROR_EFactionRole.INVADERS;
		else if (tag == "money_self")
			selfMoney = true;
		else
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (playerController)
			playerController.AROR_RequestAdminMoney(role, selfMoney, MONEY_STEP);

		if (selfMoney)
			GetGame().GetCallqueue().CallLater(OpenEconomyDialog, REFRESH_DELAY_MS);
		else
			GetGame().GetCallqueue().CallLater(OpenFundsDialog, REFRESH_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	// Teleport
	//------------------------------------------------------------------------------------------------
	protected void StartTeleportPicking()
	{
		if (m_bPickingTeleport)
			return;

		m_bPickingTeleport = true;
		AROR_LocationMapIconsComponent.SetInfoPopupSuppressed(true);
		SCR_MapEntity.GetOnSelection().Insert(OnMapSelection);
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpened);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClosed);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapOpened(MapConfiguration config)
	{
		SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpened);

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
			mapEntity.ZoomOut();
	}

	//------------------------------------------------------------------------------------------------
	//! The selection event gives screen coordinates; read the world position under the map cursor
	protected void OnMapSelection(vector screenPosition)
	{
		if (!m_bPickingTeleport)
			return;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return;

		float worldX, worldZ;
		mapEntity.GetMapCursorWorldPosition(worldX, worldZ);
		vector target = Vector(worldX, GetGame().GetWorld().GetSurfaceY(worldX, worldZ), worldZ);

		StopTeleportPicking();
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);

		// The owning client moves its own character; movement replicates to the server
		if (!SCR_Global.TeleportLocalPlayer(target))
			Print(string.Format("[AROR] Teleport to %1 failed (not spawned?)", target), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Closing the map without clicking cancels the teleport
	protected void OnMapClosed(MapConfiguration config)
	{
		StopTeleportPicking();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopTeleportPicking()
	{
		m_bPickingTeleport = false;
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

		if (!m_bEnabled || RplSession.Mode() == RplMode.Dedicated)
			return;

		GetGame().GetCallqueue().CallLater(CheckHotkey, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckHotkey);
		GetGame().GetCallqueue().Remove(HandleAction);
		GetGame().GetCallqueue().Remove(OpenGameMaster);
		GetGame().GetCallqueue().Remove(OpenEconomyDialog);
		GetGame().GetCallqueue().Remove(OpenFundsDialog);
		GetGame().GetCallqueue().Remove(OpenSupportDialog);
		StopTeleportPicking();

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
