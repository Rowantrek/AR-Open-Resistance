[ComponentEditorProps(category: "OpenResistance/GameFlow", description: "Admin setup UI (new / load campaign). Client only. Attach to the game mode entity next to AROR_GameFlowComponent.")]
class AROR_SetupUIComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Client-side setup UI for the server admin (see ARCHITECTURE.md -> Game Flow -> Setup UI).
//! Reacts to AROR_GameFlowComponent client events and sends requests through the AROR_ methods on SCR_PlayerController.
//! The setup menu (AROR_SetupMenu / AROR_SetupApp) shows the start screen, the save list and the new campaign steps
//! (armies and content packs, settings, name); the choices are kept here in an AROR_SetupDraft. Placing the HQ closes
//! the menu and opens the base-game map: the admin clicks the position, a dialog confirms it and the whole draft is
//! sent to the server. Backing out of the map, or the server refusing, reopens the menu on the name step.
//! Menu and dialog actions arrive through QueueDialogAction once they have closed (see AROR_SetupDialogs.c).
class AROR_SetupUIComponent : SCR_BaseGameModeComponent
{
	[Attribute("{5BB4D0ED53225DDC}Configs/UI/AROR_SetupDialogs.conf", UIWidgets.ResourceNamePicker, desc: "Setup dialog presets", params: "conf")]
	protected ResourceName m_sDialogPresets;

	static const string DIALOG_MAIN = "main";
	static const string DIALOG_HQ_CONFIRM = "hq_confirm";
	static const string DIALOG_LOAD = "load";

	protected static const string PRESET_HQ_CONFIRM = "aror_setup_hq_confirm";

	protected AROR_GameFlowComponent m_GameFlow;
	protected bool m_bPickingHQ;

	//! The new campaign being set up
	protected ref AROR_SetupDraft m_Draft = new AROR_SetupDraft();

	//! True from sending a new campaign request until the server answers, so a refusal returns to the name step
	protected bool m_bAwaitingNewResult;

	protected static AROR_SetupUIComponent s_Instance;

	// State the setup menu shows (it reads it when it opens): screen, save list and any message from the server
	protected int m_iMenuScreen = AROR_SetupApp.SCREEN_START;
	protected ref array<string> m_aSaveLabels = {};
	protected string m_sMenuMessage;
	protected vector m_vPickedPosition;

	//------------------------------------------------------------------------------------------------
	static AROR_SetupUIComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Which setup menu screen to show (AROR_SetupApp.SCREEN_*)
	int GetMenuScreen()
	{
		return m_iMenuScreen;
	}

	//------------------------------------------------------------------------------------------------
	//! The menu moved to another screen; a message from the server only shows on the screen it arrived for
	void SetMenuScreen(int screen)
	{
		if (screen != m_iMenuScreen)
			m_sMenuMessage = string.Empty;

		m_iMenuScreen = screen;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetSaveLabels()
	{
		return m_aSaveLabels;
	}

	//------------------------------------------------------------------------------------------------
	//! The last message from the server (e.g. why a request was refused)
	string GetMenuMessage()
	{
		return m_sMenuMessage;
	}

	//------------------------------------------------------------------------------------------------
	AROR_SetupDraft GetDraft()
	{
		return m_Draft;
	}

	//------------------------------------------------------------------------------------------------
	//! New campaign chosen on the start screen: start over from the defaults on the first step
	void StartNewCampaign()
	{
		m_Draft.Reset();
		SetMenuScreen(AROR_SetupApp.SCREEN_FACTIONS);
	}

	//------------------------------------------------------------------------------------------------
	void OpenMainDialog()
	{
		OpenMainDialogWithMessage(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	void OpenMainDialogWithMessage(string message)
	{
		OpenMenuAt(AROR_SetupApp.SCREEN_START, message);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the setup menu on a screen; one instance, updated in place when open
	protected void OpenMenuAt(int screen, string message)
	{
		m_iMenuScreen = screen;
		m_sMenuMessage = message;
		AROR_SetupMenu menu = AROR_SetupMenu.GetOpen();
		if (menu)
			menu.SyncFromSetup();
		else
			GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.AROR_SetupMenu);
	}

	//------------------------------------------------------------------------------------------------
	// Menu and dialog actions
	//------------------------------------------------------------------------------------------------
	//! Called by the menu or a dialog once it has closed; handled next frame, outside the menu manager's close sequence
	void QueueDialogAction(string dialogId, string tag, int value)
	{
		GetGame().GetCallqueue().CallLater(HandleDialogAction, 0, false, dialogId, tag, value);
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleDialogAction(string dialogId, string tag, int value)
	{
		if (dialogId == DIALOG_MAIN)
		{
			if (tag == "hq")
				StartHQPicking();
		}
		else if (dialogId == DIALOG_HQ_CONFIRM)
		{
			if (tag == "start")
				ConfirmHQ();
			else if (tag == "back")
				CancelHQPicking();
			// "repick": nothing, the map stays open
		}
		else if (dialogId == DIALOG_LOAD)
		{
			if (tag == "load")
				RequestLoad(value);
		}
	}

	//------------------------------------------------------------------------------------------------
	// New campaign: after the setup steps, pick the HQ on the map
	//------------------------------------------------------------------------------------------------
	protected void StartHQPicking()
	{
		if (m_bPickingHQ)
			return;

		m_bPickingHQ = true;
		AROR_LocationMapIconsComponent.SetInfoPopupSuppressed(true);
		SCR_MapEntity.GetOnSelection().Insert(OnMapSelection);
		SCR_MapEntity.GetOnMapOpen().Insert(OnHQMapOpened);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
	}

	//------------------------------------------------------------------------------------------------
	//! Show the whole map when picking the HQ
	protected void OnHQMapOpened(MapConfiguration config)
	{
		SCR_MapEntity.GetOnMapOpen().Remove(OnHQMapOpened);

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
			mapEntity.ZoomOut();
	}

	//------------------------------------------------------------------------------------------------
	//! Back from the map: the name step again, with every choice kept
	protected void CancelHQPicking()
	{
		StopHQPicking();
		OpenMenuAt(AROR_SetupApp.SCREEN_NAME, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the whole draft with the HQ position; the server checks everything again
	protected void ConfirmHQ()
	{
		StopHQPicking();

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController)
			return;

		array<string> disabledPackIds = {};
		array<string> enabledPackIds = {};
		m_Draft.GetPackOverrides(disabledPackIds, enabledPackIds);

		array<string> settingIds = {};
		array<string> settingValues = {};
		m_Draft.GetSettingValues(settingIds, settingValues);

		m_bAwaitingNewResult = true;
		playerController.AROR_RequestNewPlaythrough(m_vPickedPosition, m_Draft.GetName(), m_Draft.GetPresetIds(), disabledPackIds, enabledPackIds, settingIds, settingValues);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopHQPicking()
	{
		m_bPickingHQ = false;
		AROR_LocationMapIconsComponent.SetInfoPopupSuppressed(false);
		SCR_MapEntity.GetOnSelection().Remove(OnMapSelection);
		SCR_MapEntity.GetOnMapOpen().Remove(OnHQMapOpened);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
	}

	//------------------------------------------------------------------------------------------------
	//! The selection event gives screen coordinates, so read the world position under the map cursor instead
	protected void OnMapSelection(vector screenPosition)
	{
		if (!m_bPickingHQ || !m_GameFlow)
			return;

		// One confirm dialog at a time
		if (SCR_ConfigurableDialogUi.GetCurrentDialogTag() == PRESET_HQ_CONFIRM && SCR_ConfigurableDialogUi.GetCurrentDialog())
			return;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return;

		float worldX, worldZ;
		mapEntity.GetMapCursorWorldPosition(worldX, worldZ);
		m_vPickedPosition = Vector(worldX, 0, worldZ);

		string reason;
		bool valid = m_GameFlow.IsValidHQPosition(m_vPickedPosition, reason);

		AROR_SetupHQConfirmDialog dialog = new AROR_SetupHQConfirmDialog(this);
		SCR_ConfigurableDialogUi.CreateFromPreset(m_sDialogPresets, PRESET_HQ_CONFIRM, dialog);

		if (valid)
			dialog.SetPlacementValid(true, "Place the Resistance HQ here?");
		else
			dialog.SetPlacementValid(false, reason);
	}

	//------------------------------------------------------------------------------------------------
	// Load campaign
	//------------------------------------------------------------------------------------------------
	//! Asks the server for the save list; the answer arrives in OnSaveListReceived
	void RequestSaveList()
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (playerController)
			playerController.AROR_RequestSaveList();
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestLoad(int saveIndex)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (playerController)
			playerController.AROR_RequestLoad(saveIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSaveListReceived(array<string> saveLabels)
	{
		m_aSaveLabels.Copy(saveLabels);
		OpenMenuAt(AROR_SetupApp.SCREEN_LOAD, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Server refused a request with the reason: a new campaign returns to the name step with its choices kept (e.g.
	//! the HQ position or name was refused), anything else to the start screen
	protected void OnSetupResult(bool success, string message)
	{
		bool wasNew = m_bAwaitingNewResult;
		m_bAwaitingNewResult = false;
		if (success)
			return;

		if (wasNew)
			OpenMenuAt(AROR_SetupApp.SCREEN_NAME, message);
		else
			OpenMainDialogWithMessage(message);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || RplSession.Mode() == RplMode.Dedicated)
			return;

		s_Instance = this;

		m_GameFlow = AROR_GameFlowComponent.Cast(owner.FindComponent(AROR_GameFlowComponent));
		if (!m_GameFlow)
		{
			Print("[AROR] AROR_SetupUIComponent needs AROR_GameFlowComponent on the same entity", LogLevel.ERROR);
			return;
		}

		m_GameFlow.GetOnSetupRequested().Insert(OpenMainDialog);
		m_GameFlow.GetOnSaveListReceived().Insert(OnSaveListReceived);
		m_GameFlow.GetOnSetupResult().Insert(OnSetupResult);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;
		GetGame().GetCallqueue().Remove(HandleDialogAction);
		SCR_MapEntity.GetOnSelection().Remove(OnMapSelection);
		SCR_MapEntity.GetOnMapOpen().Remove(OnHQMapOpened);

		if (m_GameFlow)
		{
			m_GameFlow.GetOnSetupRequested().Remove(OpenMainDialog);
			m_GameFlow.GetOnSaveListReceived().Remove(OnSaveListReceived);
			m_GameFlow.GetOnSetupResult().Remove(OnSetupResult);
		}

		super.OnDelete(owner);
	}
}
