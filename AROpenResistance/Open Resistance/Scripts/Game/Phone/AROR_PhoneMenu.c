//! Forwards button clicks inside the phone to the menu, by widget name
class AROR_PhoneButtonHandler : ScriptedWidgetEventHandler
{
	protected AROR_PhoneMenu m_Menu;

	//------------------------------------------------------------------------------------------------
	void AROR_PhoneButtonHandler(AROR_PhoneMenu menu)
	{
		m_Menu = menu;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Menu)
			m_Menu.OnPhoneButton(w.GetName(), w);

		return true;
	}
}

//! Screens the phone can show
enum AROR_EPhoneApp
{
	HOME,
	BANK,	//!< Portrait: account holder, personal money and Resistance funds
	BANK_TRANSFER,	//!< Portrait: transfer personal money to a player or Resistance funds
	TEST,	//!< Landscape test app
	REBINTEL,	//!< Portrait: public enemy readiness and crackdown summary
	HICOMM,	//!< Landscape: Resistance high command (leaders only, see AROR_HiCommApp)
	OPS,		//!< Landscape: Resistance mission board (see AROR_OpsApp)
	MARS,		//!< Landscape: response debugger
	BLACK_MARKET,	//!< Landscape: items shop paid by import facilities held (see AROR_BlackMarketApp)
	IMPORTS		//!< Landscape: vehicle shop, one click per vehicle (see AROR_ImportsApp)
}

//! The phone (see ARCHITECTURE.md -> Phone). A menu (preset AROR_PhoneMenu, registered in chimeraMenus.conf), so it
//! gets a cursor and stops the character while open. It slides up from the bottom when opened; opening an app that
//! needs landscape turns the whole phone 90 degrees and scales it up, with the frame staying on. Esc closes it.
//! Layout (AROR_PhoneMenu.layout) only declares the widgets; everything is placed in code in screen pixels of the
//! textures times the current scale.
//! Rotation: an image widget rotates its picture inside its own unrotated box and clips what falls outside, so the
//! phone is drawn from square textures (AROR_PhoneComposite_*) with the frame and wallpaper baked together and centred
//! on a transparent canvas larger than the phone's diagonal. The landscape composite is the phone already lying on
//! its side (camera on the left) with the landscape wallpaper upright. App content is hidden while the phone turns.
class AROR_PhoneMenu : ChimeraMenuBase
{
	protected static const ResourceName PHONE_PORTRAIT = "{A0A140B61ED630E4}UI/Textures/Phone/AROR_PhoneComposite_Portrait.edds";
	protected static const ResourceName PHONE_LANDSCAPE = "{CD038A3912020FD8}UI/Textures/Phone/AROR_PhoneComposite_Landscape.edds";
	protected static const ResourceName SIGNAL_ICON = "{8FA134FBB4808586}UI/Textures/Phone/Status/Signal_4.edds";
	protected static const ResourceName BATTERY_ICON = "{58C96C9C611F79F7}UI/Textures/Phone/Status/Battery_100.edds";
	protected static const ResourceName BANK_ICON = "{FDFC772914D7F6C2}UI/Textures/Phone/Apps/Bank/AROR_Icon_Bank.edds";
	protected static const ResourceName BANK_SCREEN = "{C1EA87F984125838}UI/Textures/Phone/Apps/Bank/AROR_Bank_AppHome.edds";

	// Texture geometry (pixels): the square composite canvas, and the frame and screen cutout centred in it
	protected static const float CANVAS_SIZE = 2240;
	protected static const float FRAME_WIDTH = 1000;
	protected static const float FRAME_HEIGHT = 2000;
	protected static const float SCREEN_WIDTH = 900;
	protected static const float SCREEN_HEIGHT = 1700;

	// Home button on the frame's bottom bezel (portrait), centred this far below the canvas centre
	protected static const float HOME_BUTTON_WIDTH = 160;
	protected static const float HOME_BUTTON_HEIGHT = 54;
	protected static const float HOME_BUTTON_OFFSET = 925;

	// Status bar strip at the top of the screen (portrait)
	protected static const float STATUS_BAR_HEIGHT = 70;
	protected static const float STATUS_ICON_SIZE = 40;
	protected static const float STATUS_MARGIN = 40;

	// Home screen app tiles
	protected static const float TILE_TOP = 140;
	protected static const float TILE_LEFT = 70;
	protected static const float TILE_SIZE = 180;
	protected static const float TILE_SPACING = 60;
	protected static const float TILE_CORNER_FRACTION = 0.22;	//!< Corner radius as a fraction of the tile, matching the Bank icon art
	protected static const float TILE_ROW_SPACING = 110;	//!< From the bottom of one tile row to the top of the next (room for labels)

	// Home quick-action dock (4 slots along the bottom of the portrait screen)
	protected static const int QUICK_SLOT_COUNT = 4;
	protected static const float QUICK_DOCK_BOTTOM = 80;	//!< Gap above the bottom of the screen cutout
	protected static const float QUICK_SLOT_SIZE = 160;
	protected static const float QUICK_SLOT_GAP = 30;

	// Bank screen rectangles (screen pixels of AROR_Bank_AppHome)
	protected static const float BANK_NAME_X = 150;
	protected static const float BANK_NAME_Y = 416;
	protected static const float BANK_NAME_WIDTH = 670;
	protected static const float BANK_NAME_HEIGHT = 100;
	protected static const float BANK_BALANCE_X = 100;
	protected static const float BANK_PERSONAL_Y = 640;
	protected static const float BANK_RESISTANCE_Y = 970;
	protected static const float BANK_BALANCE_WIDTH = 700;
	protected static const float BANK_BALANCE_HEIGHT = 110;
	protected static const float BANK_TRANSFER_X = 60;
	protected static const float BANK_TRANSFER_Y = 1480;
	protected static const float BANK_TRANSFER_WIDTH = 780;
	protected static const float BANK_TRANSFER_HEIGHT = 120;
	protected static const float BANK_NAME_FONT = 46;
	protected static const float BANK_BALANCE_FONT = 80;

	//! Rough width of a bold character as a fraction of the font size, used to shrink long balances to fit
	protected static const float CHAR_WIDTH_FACTOR = 0.6;

	//! Rotation of the portrait phone when landscape (top of the phone to the left, matching the landscape composite)
	protected static const float TURN_ANGLE = -90;

	// On-screen size: portrait by screen height, landscape as large as fits
	protected static const float PORTRAIT_HEIGHT_FRACTION = 0.82;
	protected static const float LANDSCAPE_WIDTH_FRACTION = 0.92;
	protected static const float LANDSCAPE_HEIGHT_FRACTION = 0.9;

	protected static const float OPEN_SECONDS = 0.25;
	protected static const float TURN_SECONDS = 0.45;
	protected static const float CONTENT_FADE_SECONDS = 0.15;
	protected static const float DIM_OPACITY = 0.5;

	protected static AROR_PhoneMenu s_Instance;

	protected ImageWidget m_wDim;
	protected ImageWidget m_wPhonePortrait;
	protected ImageWidget m_wPhoneLandscape;
	protected Widget m_wHomeButton;

	protected Widget m_wStatusBar;
	protected RichTextWidget m_wClock;
	protected ImageWidget m_wSignal;
	protected ImageWidget m_wBattery;

	protected Widget m_wHome;
	protected CanvasWidget m_wHomeTileCanvas;	//!< Rounded backgrounds of the drawn (non-Bank) app tiles
	protected ref array<ref CanvasWidgetCommand> m_aTileCommands = {};
	protected ImageWidget m_wBankTile;
	protected RichTextWidget m_wBankTileLabel;
	protected Widget m_wBankTileButton;
	protected ImageWidget m_wTestTile;
	protected RichTextWidget m_wTestTileGlyph;
	protected RichTextWidget m_wTestTileLabel;
	protected Widget m_wTestTileButton;
	protected ImageWidget m_wRebIntelTile;
	protected RichTextWidget m_wRebIntelTileGlyph;
	protected RichTextWidget m_wRebIntelTileLabel;
	protected Widget m_wRebIntelTileButton;
	protected ref AROR_RebIntelApp m_RebIntel;
	protected ImageWidget m_wHiCommTile;
	protected RichTextWidget m_wHiCommTileGlyph;
	protected CanvasWidget m_wHiCommTileCanvas;	//!< The tile's icon: a small spinning green globe
	protected ref AROR_HiCommGlobe m_HiCommTileGlobe = new AROR_HiCommGlobe();
	protected RichTextWidget m_wHiCommTileLabel;
	protected Widget m_wHiCommTileButton;
	protected ref AROR_HiCommApp m_HiComm;
	protected ImageWidget m_wOpsTile;
	protected CanvasWidget m_wOpsTileCanvas;	//!< The tile's icon: the OPS crest
	protected ref AROR_OpsCrest m_OpsTileCrest = new AROR_OpsCrest();
	protected RichTextWidget m_wOpsTileLabel;
	protected Widget m_wOpsTileButton;
	protected ref AROR_OpsApp m_Ops;
	protected ref AROR_MarsApp m_Mars;
	protected ref AROR_BlackMarketApp m_BlackMarket;
	protected RichTextWidget m_wBlackMarketTileGlyph;	//!< Home tile widgets live in the Black Market's own layout
	protected RichTextWidget m_wBlackMarketTileLabel;
	protected Widget m_wBlackMarketTileButton;
	protected ref AROR_ImportsApp m_Imports;
	protected RichTextWidget m_wImportsTileGlyph;	//!< Imports uses its own copy of the same layout, so its tile widgets are separate
	protected RichTextWidget m_wImportsTileLabel;
	protected Widget m_wImportsTileButton;

	//! Home quick dock: slot 0 = garage store; 1–2 empty placeholders; 3 = save campaign (admins, drawn on HomeTileCanvas)
	protected ImageWidget m_wQuickSlot0;
	protected RichTextWidget m_wQuickSlot0Glyph;
	protected Widget m_wQuickSlot0Button;
	protected ImageWidget m_wQuickSlot1;
	protected Widget m_wQuickSlot1Button;
	protected ImageWidget m_wQuickSlot2;
	protected Widget m_wQuickSlot2Button;
	protected ImageWidget m_wQuickSlot3;
	protected Widget m_wQuickSlot3Button;
	protected bool m_bManualSaveAvailable;	//!< Set when the phone opens: an admin with a campaign running

	protected Widget m_wBankApp;
	protected ImageWidget m_wBankBackground;
	protected RichTextWidget m_wBankName;
	protected RichTextWidget m_wBankPersonal;
	protected RichTextWidget m_wBankResistance;
	protected Widget m_wBankTransferButton;
	protected ref AROR_BankTransferApp m_BankTransfer;

	protected Widget m_wTestApp;
	protected ImageWidget m_wTestPanel;
	protected RichTextWidget m_wTestTitle;
	protected RichTextWidget m_wTestBody;
	protected ImageWidget m_wTestBackBackground;
	protected RichTextWidget m_wTestBackLabel;
	protected Widget m_wTestBackButton;

	protected ref AROR_PhoneButtonHandler m_ButtonHandler;

	protected AROR_EPhoneApp m_eApp;

	//! 0 = off screen below, 1 = fully open
	protected float m_fOpen;
	protected bool m_bClosing;

	//! True while the open hotkey is held; starts true so the press that opened the phone does not close it
	protected bool m_bHotkeyDown = true;

	//! 0 = portrait, 1 = landscape
	protected float m_fTurn;
	protected float m_fTurnTarget;

	//! Fades the current screen's content in after switching apps or turning
	protected float m_fContentFade;

	//------------------------------------------------------------------------------------------------
	static AROR_PhoneMenu GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Slide the phone away, then close the menu
	void RequestClose()
	{
		m_bClosing = true;
	}

	//------------------------------------------------------------------------------------------------
	//! A button inside the phone was clicked
	void OnPhoneButton(string buttonName, Widget w = null)
	{
		if (m_bClosing || !IsSettled())
			return;

		if (buttonName == "BankTileButton")
			OpenApp(AROR_EPhoneApp.BANK);
		else if (buttonName == "TestTileButton")
			OpenApp(AROR_EPhoneApp.MARS);
		else if (buttonName == "RebIntelTileButton")
			OpenApp(AROR_EPhoneApp.REBINTEL);
		else if (buttonName == "HiCommTileButton")
			OpenApp(AROR_EPhoneApp.HICOMM);
		else if (buttonName == "OpsTileButton")
			OpenApp(AROR_EPhoneApp.OPS);
		else if (buttonName == "BlackMarketTileButton")
		{
			// The Imports app is a second copy of the Black Market layout, so both tiles share this name
			if (w && w == m_wImportsTileButton)
				OpenApp(AROR_EPhoneApp.IMPORTS);
			else
				OpenApp(AROR_EPhoneApp.BLACK_MARKET);
		}
		else if (buttonName == "QuickSlot0Button")
		{
			// Store first so the look ray still has a character/camera, then close the phone
			AROR_GarageStoreAPI.TryStoreLookedAtVehicle();
			RequestClose();
		}
		else if (buttonName == "QuickSlot3Button" && m_bManualSaveAvailable)
		{
			// The server checks admin rights again and answers with a hint
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (playerController)
				playerController.AROR_RequestManualSave();

			RequestClose();
		}
		else if (buttonName == "BankTransferButton")
		{
			OpenApp(AROR_EPhoneApp.BANK_TRANSFER);
			return;
		}
		else if (buttonName == "BankTransferBackButton")
		{
			OpenApp(AROR_EPhoneApp.BANK);
			return;
		}
		else if (m_BankTransfer && m_BankTransfer.OnButton(buttonName))
			return;
		else if (m_RebIntel && m_RebIntel.OnButton(buttonName))
			return;
		else if (buttonName == "AppBackButton" || buttonName == "HomeButton")
			OpenApp(AROR_EPhoneApp.HOME);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: transfer RPC succeeded; clear the amount field on the Transfer screen
	void OnBankTransferSucceeded()
	{
		if (m_BankTransfer)
			m_BankTransfer.OnTransferSucceeded();
	}

	//------------------------------------------------------------------------------------------------
	//! Switch screens; turns the phone when the new screen needs the other orientation
	protected void OpenApp(AROR_EPhoneApp app)
	{
		if (app == AROR_EPhoneApp.MARS && !m_Mars)
			return;
		if (app == AROR_EPhoneApp.BLACK_MARKET && !m_BlackMarket)
			return;
		if (app == AROR_EPhoneApp.IMPORTS && !m_Imports)
			return;
		if (app == m_eApp)
			return;

		m_eApp = app;
		m_fContentFade = 0;

		if (IsLandscapeApp(app))
			m_fTurnTarget = 1;
		else
			m_fTurnTarget = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsLandscapeApp(AROR_EPhoneApp app)
	{
		return app == AROR_EPhoneApp.TEST || app == AROR_EPhoneApp.HICOMM || app == AROR_EPhoneApp.OPS || app == AROR_EPhoneApp.MARS || app == AROR_EPhoneApp.BLACK_MARKET || app == AROR_EPhoneApp.IMPORTS;
	}

	//------------------------------------------------------------------------------------------------
	//! Not turning between orientations
	protected bool IsSettled()
	{
		return m_fTurn == m_fTurnTarget;
	}

	//------------------------------------------------------------------------------------------------
	//! Esc closes the phone from any screen
	protected void OnBack(float value = 0.0, EActionTrigger reason = 0)
	{
		RequestClose();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		s_Instance = this;

		// The menu manager sizes the root to the screen; it has no slot to set
		Widget root = GetRootWidget();

		m_wDim = ImageWidget.Cast(root.FindAnyWidget("Dim"));
		m_wPhonePortrait = ImageWidget.Cast(root.FindAnyWidget("PhonePortrait"));
		m_wPhoneLandscape = ImageWidget.Cast(root.FindAnyWidget("PhoneLandscape"));
		m_wHomeButton = root.FindAnyWidget("HomeButton");

		m_wStatusBar = root.FindAnyWidget("StatusBar");
		m_wClock = RichTextWidget.Cast(root.FindAnyWidget("Clock"));
		m_wSignal = ImageWidget.Cast(root.FindAnyWidget("Signal"));
		m_wBattery = ImageWidget.Cast(root.FindAnyWidget("Battery"));

		m_wHome = root.FindAnyWidget("Home");
		m_wHomeTileCanvas = CanvasWidget.Cast(root.FindAnyWidget("HomeTileCanvas"));
		m_wBankTile = ImageWidget.Cast(root.FindAnyWidget("BankTile"));
		m_wBankTileLabel = RichTextWidget.Cast(root.FindAnyWidget("BankTileLabel"));
		m_wBankTileButton = root.FindAnyWidget("BankTileButton");
		m_wTestTile = ImageWidget.Cast(root.FindAnyWidget("TestTile"));
		m_wTestTileGlyph = RichTextWidget.Cast(root.FindAnyWidget("TestTileGlyph"));
		m_wTestTileLabel = RichTextWidget.Cast(root.FindAnyWidget("TestTileLabel"));
		m_wTestTileButton = root.FindAnyWidget("TestTileButton");
		m_wRebIntelTile = ImageWidget.Cast(root.FindAnyWidget("RebIntelTile"));
		m_wRebIntelTileGlyph = RichTextWidget.Cast(root.FindAnyWidget("RebIntelTileGlyph"));
		m_wRebIntelTileLabel = RichTextWidget.Cast(root.FindAnyWidget("RebIntelTileLabel"));
		m_wRebIntelTileButton = root.FindAnyWidget("RebIntelTileButton");
		m_RebIntel = new AROR_RebIntelApp();
		bool rebIntelReady = m_RebIntel.Init(root);
		m_wHiCommTile = ImageWidget.Cast(root.FindAnyWidget("HiCommTile"));
		m_wHiCommTileGlyph = RichTextWidget.Cast(root.FindAnyWidget("HiCommTileGlyph"));
		m_wHiCommTileCanvas = CanvasWidget.Cast(root.FindAnyWidget("HiCommTileCanvas"));
		m_wHiCommTileLabel = RichTextWidget.Cast(root.FindAnyWidget("HiCommTileLabel"));
		m_wHiCommTileButton = root.FindAnyWidget("HiCommTileButton");
		m_HiComm = new AROR_HiCommApp();
		bool hiCommReady = m_HiComm.Init(root);
		m_wOpsTile = ImageWidget.Cast(root.FindAnyWidget("OpsTile"));
		m_wOpsTileCanvas = CanvasWidget.Cast(root.FindAnyWidget("OpsTileCanvas"));
		m_wOpsTileLabel = RichTextWidget.Cast(root.FindAnyWidget("OpsTileLabel"));
		m_wOpsTileButton = root.FindAnyWidget("OpsTileButton");
		m_Ops = new AROR_OpsApp();
		bool opsReady = m_Ops.Init(root);
		m_Mars = new AROR_MarsApp();
		if (!m_Mars.Init(root))
		{
			m_Mars.Shutdown();
			m_Mars = null;
			Print("[AROR MARS] Separate app layout unavailable; other phone apps remain available", LogLevel.WARNING);
		}
		m_BlackMarket = new AROR_BlackMarketApp();
		if (m_BlackMarket.Init(root))
		{
			Widget market = m_BlackMarket.GetResource();
			m_wBlackMarketTileGlyph = RichTextWidget.Cast(market.FindAnyWidget("BlackMarketTileGlyph"));
			m_wBlackMarketTileLabel = RichTextWidget.Cast(market.FindAnyWidget("BlackMarketTileLabel"));
			m_wBlackMarketTileButton = market.FindAnyWidget("BlackMarketTileButton");
		}
		else
		{
			m_BlackMarket.Shutdown();
			m_BlackMarket = null;
			Print("[AROR] Black Market app layout unavailable; other phone apps remain available", LogLevel.WARNING);
		}

		m_Imports = new AROR_ImportsApp();
		if (m_Imports.Init(root))
		{
			Widget imports = m_Imports.GetResource();
			m_wImportsTileGlyph = RichTextWidget.Cast(imports.FindAnyWidget("BlackMarketTileGlyph"));
			m_wImportsTileLabel = RichTextWidget.Cast(imports.FindAnyWidget("BlackMarketTileLabel"));
			m_wImportsTileButton = imports.FindAnyWidget("BlackMarketTileButton");
		}
		else
		{
			m_Imports.Shutdown();
			m_Imports = null;
			Print("[AROR] Imports app layout unavailable; other phone apps remain available", LogLevel.WARNING);
		}

		m_wQuickSlot0 = ImageWidget.Cast(root.FindAnyWidget("QuickSlot0"));
		m_wQuickSlot0Glyph = RichTextWidget.Cast(root.FindAnyWidget("QuickSlot0Glyph"));
		m_wQuickSlot0Button = root.FindAnyWidget("QuickSlot0Button");
		m_wQuickSlot1 = ImageWidget.Cast(root.FindAnyWidget("QuickSlot1"));
		m_wQuickSlot1Button = root.FindAnyWidget("QuickSlot1Button");
		m_wQuickSlot2 = ImageWidget.Cast(root.FindAnyWidget("QuickSlot2"));
		m_wQuickSlot2Button = root.FindAnyWidget("QuickSlot2Button");
		m_wQuickSlot3 = ImageWidget.Cast(root.FindAnyWidget("QuickSlot3"));
		m_wQuickSlot3Button = root.FindAnyWidget("QuickSlot3Button");

		m_wBankApp = root.FindAnyWidget("BankApp");
		m_wBankBackground = ImageWidget.Cast(root.FindAnyWidget("BankBackground"));
		m_wBankName = RichTextWidget.Cast(root.FindAnyWidget("BankName"));
		m_wBankPersonal = RichTextWidget.Cast(root.FindAnyWidget("BankPersonal"));
		m_wBankResistance = RichTextWidget.Cast(root.FindAnyWidget("BankResistance"));
		// Prefer the Bank home hit-target (not a similarly named widget elsewhere)
		if (m_wBankApp)
			m_wBankTransferButton = m_wBankApp.FindAnyWidget("BankTransferButton");
		if (!m_wBankTransferButton)
			m_wBankTransferButton = root.FindAnyWidget("BankTransferButton");
		m_BankTransfer = new AROR_BankTransferApp();
		bool bankTransferReady = m_BankTransfer.Init(root);

		m_wTestApp = root.FindAnyWidget("App");
		m_wTestPanel = ImageWidget.Cast(root.FindAnyWidget("AppPanel"));
		m_wTestTitle = RichTextWidget.Cast(root.FindAnyWidget("AppTitle"));
		m_wTestBody = RichTextWidget.Cast(root.FindAnyWidget("AppBody"));
		m_wTestBackBackground = ImageWidget.Cast(root.FindAnyWidget("AppBackBackground"));
		m_wTestBackLabel = RichTextWidget.Cast(root.FindAnyWidget("AppBackLabel"));
		m_wTestBackButton = root.FindAnyWidget("AppBackButton");

		// Checked in groups: one long condition exceeds the script compiler's expression limit
		bool ready = m_wDim && m_wPhonePortrait && m_wPhoneLandscape && m_wStatusBar && m_wHome && m_wBankApp && m_wTestApp;
		ready = ready && m_wBankTransferButton && bankTransferReady;
		ready = ready && rebIntelReady && m_wRebIntelTile && m_wRebIntelTileGlyph && m_wRebIntelTileLabel && m_wRebIntelTileButton;
		ready = ready && hiCommReady && m_wHiCommTile && m_wHiCommTileGlyph && m_wHiCommTileCanvas && m_wHiCommTileLabel && m_wHiCommTileButton;
		ready = ready && opsReady && m_wOpsTile && m_wOpsTileCanvas && m_wOpsTileLabel && m_wOpsTileButton;
		ready = ready && m_wQuickSlot0 && m_wQuickSlot0Glyph && m_wQuickSlot0Button;
		ready = ready && m_wQuickSlot1 && m_wQuickSlot1Button && m_wQuickSlot2 && m_wQuickSlot2Button && m_wQuickSlot3 && m_wQuickSlot3Button;
		if (!ready)
		{
			Print("[AROR] Phone layout is missing widgets", LogLevel.ERROR);
			Close();
			return;
		}

		m_bManualSaveAvailable = CanManualSave();
		ApplyStyle();

		m_ButtonHandler = new AROR_PhoneButtonHandler(this);
		// Empty quick slots keep buttons for layout but are not registered (not clickable)
		array<Widget> buttons = {m_wBankTileButton, m_wTestTileButton, m_wRebIntelTileButton, m_wHiCommTileButton, m_wOpsTileButton, m_wQuickSlot0Button, m_wBankTransferButton, m_wTestBackButton, m_wHomeButton};
		if (m_bManualSaveAvailable)
			buttons.Insert(m_wQuickSlot3Button);
		m_BankTransfer.CollectButtons(buttons);
		if (m_wBlackMarketTileButton)
		{
			m_wBlackMarketTileButton.SetColor(Color.FromRGBA(0, 0, 0, 0));
			buttons.Insert(m_wBlackMarketTileButton);
		}
		if (m_wImportsTileButton)
		{
			m_wImportsTileButton.SetColor(Color.FromRGBA(0, 0, 0, 0));
			buttons.Insert(m_wImportsTileButton);
		}
		m_RebIntel.CollectButtons(buttons);
		foreach (Widget button : buttons)
		{
			if (button)
				button.AddHandler(m_ButtonHandler);
		}

		GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, OnBack);

		m_eApp = AROR_EPhoneApp.HOME;
		m_fOpen = 0;
		m_fTurn = 0;
		m_fTurnTarget = 0;
		m_fContentFade = 0;
		m_bClosing = false;
		UpdateClock();
		UpdateLayout();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetInputManager().RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnBack);

		if (s_Instance == this)
			s_Instance = null;
		m_RebIntel = null;

		// HICOMM may have the game map open inside the phone
		if (m_HiComm)
			m_HiComm.Shutdown();

		m_HiComm = null;
		m_Ops = null;
		if (m_Mars)
			m_Mars.Shutdown();
		m_Mars = null;
		if (m_BlackMarket)
			m_BlackMarket.Shutdown();
		m_BlackMarket = null;
		if (m_Imports)
			m_Imports.Shutdown();
		m_Imports = null;
		m_BankTransfer = null;

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		if (!m_wPhonePortrait)
			return;

		// The hotkey closes the phone. Read here as well as in AROR_PhoneComponent: while a menu is open the game
		// may leave our input context out, so keep it active and watch for the press from inside the menu.
		InputManager inputManager = GetGame().GetInputManager();
		inputManager.ActivateContext("AROR_Context");
		bool hotkeyDown = inputManager.GetActionValue("AROR_OpenPhone") > 0 || inputManager.GetActionValue("AROR_OpenPhoneAlt") > 0;
		if (hotkeyDown != m_bHotkeyDown)
		{
			m_bHotkeyDown = hotkeyDown;
			Print(string.Format("[AROR] Phone menu sees hotkey %1", hotkeyDown), LogLevel.NORMAL);
			if (hotkeyDown)
				RequestClose();
		}

		// Esc is also read straight from the keyboard, in case the MenuBack action does not reach this menu
		if (Debug.KeyState(KeyCode.KC_ESCAPE))
		{
			Debug.ClearKey(KeyCode.KC_ESCAPE);
			RequestClose();
		}

		float openTarget = 1;
		if (m_bClosing)
			openTarget = 0;

		m_fOpen = MoveTowards(m_fOpen, openTarget, tDelta / OPEN_SECONDS);
		if (m_bClosing && m_fOpen <= 0)
		{
			Close();
			return;
		}

		m_fTurn = MoveTowards(m_fTurn, m_fTurnTarget, tDelta / TURN_SECONDS);

		if (IsSettled())
			m_fContentFade = MoveTowards(m_fContentFade, 1, tDelta / CONTENT_FADE_SECONDS);

		UpdateClock();
		UpdateLayout();
	}

	//------------------------------------------------------------------------------------------------
	//! Positions every frame: slide-in, rotation and scale, then the settled content
	protected void UpdateLayout()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float screenWidth = workspace.DPIUnscale(workspace.GetWidth());
		float screenHeight = workspace.DPIUnscale(workspace.GetHeight());

		float turn = SmoothStep(m_fTurn);
		float open = EaseOutCubic(m_fOpen);

		float portraitScale = screenHeight * PORTRAIT_HEIGHT_FRACTION / FRAME_HEIGHT;
		float landscapeScale = Math.Min(screenWidth * LANDSCAPE_WIDTH_FRACTION / FRAME_HEIGHT, screenHeight * LANDSCAPE_HEIGHT_FRACTION / FRAME_WIDTH);
		float scale = Math.Lerp(portraitScale, landscapeScale, turn);

		float angle = TURN_ANGLE * turn;

		float centerX = screenWidth * 0.5;
		float centerY = screenHeight * 0.5 + (1 - open) * screenHeight;

		Place(m_wDim, 0, 0, screenWidth, screenHeight);
		m_wDim.SetOpacity(open * DIM_OPACITY);

		// Both composites turn together; the landscape one fades in over the portrait one. The frame is identical in
		// both, so it stays solid throughout the fade. The landscape composite is drawn already lying on its side, so
		// it is offset by a quarter turn: lined up with the portrait one at the start, at 0 degrees once landscape.
		float canvas = CANVAS_SIZE * scale;
		PlaceRotated(m_wPhonePortrait, centerX, centerY, canvas, canvas, angle);
		PlaceRotated(m_wPhoneLandscape, centerX, centerY, canvas, canvas, angle - TURN_ANGLE);
		m_wPhonePortrait.SetVisible(m_fTurn < 1);
		m_wPhoneLandscape.SetVisible(m_fTurn > 0);
		m_wPhoneLandscape.SetOpacity(turn);

		// Content is only shown once the phone has settled in an orientation
		bool portraitSettled = IsSettled() && m_fTurn <= 0;
		bool landscapeSettled = IsSettled() && m_fTurn >= 1;
		bool showHome = portraitSettled && m_eApp == AROR_EPhoneApp.HOME;
		bool showBank = portraitSettled && m_eApp == AROR_EPhoneApp.BANK;
		bool showBankTransfer = portraitSettled && m_eApp == AROR_EPhoneApp.BANK_TRANSFER;
		bool showTest = landscapeSettled && m_eApp == AROR_EPhoneApp.TEST;
		bool showRebIntel = portraitSettled && m_eApp == AROR_EPhoneApp.REBINTEL;
		bool showHiComm = landscapeSettled && m_eApp == AROR_EPhoneApp.HICOMM;
		bool showOps = landscapeSettled && m_eApp == AROR_EPhoneApp.OPS;

		m_wStatusBar.SetVisible(portraitSettled);
		m_wHome.SetVisible(showHome);
		m_wBankApp.SetVisible(showBank);
		m_wTestApp.SetVisible(showTest);

		float contentOpacity = SmoothStep(m_fContentFade);
		m_wHome.SetOpacity(contentOpacity);
		m_wBankApp.SetOpacity(contentOpacity);
		m_wTestApp.SetOpacity(contentOpacity);
		m_BankTransfer.SetVisible(showBankTransfer, contentOpacity);
		m_RebIntel.SetVisible(showRebIntel, contentOpacity);
		m_HiComm.SetVisible(showHiComm, contentOpacity);
		m_Ops.SetVisible(showOps, contentOpacity);
		bool showMars = landscapeSettled && m_eApp == AROR_EPhoneApp.MARS;
		if (m_Mars)
			m_Mars.SetVisible(showMars, contentOpacity);
		bool showBlackMarket = landscapeSettled && m_eApp == AROR_EPhoneApp.BLACK_MARKET;
		if (m_BlackMarket)
			m_BlackMarket.SetVisible(showBlackMarket, contentOpacity);
		bool showImports = landscapeSettled && m_eApp == AROR_EPhoneApp.IMPORTS;
		if (m_Imports)
			m_Imports.SetVisible(showImports, contentOpacity);
		array<Widget> marketTile = {m_wBlackMarketTileGlyph, m_wBlackMarketTileLabel, m_wBlackMarketTileButton, m_wImportsTileGlyph, m_wImportsTileLabel, m_wImportsTileButton};
		foreach (Widget marketTileWidget : marketTile)
		{
			if (marketTileWidget)
			{
				marketTileWidget.SetVisible(showHome);
				marketTileWidget.SetOpacity(contentOpacity);
			}
		}

		// Home button: an invisible click area over the frame's home button (bottom bezel in portrait, right in landscape)
		if (m_wHomeButton)
		{
			m_wHomeButton.SetVisible(portraitSettled || landscapeSettled);
			float buttonCentre = (HOME_BUTTON_OFFSET - HOME_BUTTON_HEIGHT * 0.5) * scale;
			if (landscapeSettled)
				Place(m_wHomeButton, centerX + buttonCentre, centerY - HOME_BUTTON_WIDTH * scale * 0.5, HOME_BUTTON_HEIGHT * scale, HOME_BUTTON_WIDTH * scale);
			else
				Place(m_wHomeButton, centerX - HOME_BUTTON_WIDTH * scale * 0.5, centerY + buttonCentre, HOME_BUTTON_WIDTH * scale, HOME_BUTTON_HEIGHT * scale);
		}

		float portraitLeft = centerX - SCREEN_WIDTH * scale * 0.5;
		float portraitTop = centerY - SCREEN_HEIGHT * scale * 0.5;

		if (portraitSettled)
			LayoutStatusBar(portraitLeft, portraitTop, scale);

		if (showHome)
			LayoutHome(portraitLeft, portraitTop, scale);

		if (showBank)
			LayoutBank(portraitLeft, portraitTop, scale);
		if (showBankTransfer)
			m_BankTransfer.Layout(portraitLeft, portraitTop, scale);
		if (showRebIntel)
			m_RebIntel.Layout(portraitLeft, portraitTop, scale);

		if (showTest)
			LayoutTestApp(centerX - SCREEN_HEIGHT * scale * 0.5, centerY - SCREEN_WIDTH * scale * 0.5, scale);

		if (showHiComm)
			m_HiComm.Layout(centerX - SCREEN_HEIGHT * scale * 0.5, centerY - SCREEN_WIDTH * scale * 0.5, scale);

		if (showOps)
			m_Ops.Layout(centerX - SCREEN_HEIGHT * scale * 0.5, centerY - SCREEN_WIDTH * scale * 0.5, scale);
		if (showMars && m_Mars)
			m_Mars.Layout(centerX - SCREEN_HEIGHT * scale * 0.5, centerY - SCREEN_WIDTH * scale * 0.5, scale);
		if (showBlackMarket && m_BlackMarket)
			m_BlackMarket.Layout(centerX - SCREEN_HEIGHT * scale * 0.5, centerY - SCREEN_WIDTH * scale * 0.5, scale);
		if (showImports && m_Imports)
			m_Imports.Layout(centerX - SCREEN_HEIGHT * scale * 0.5, centerY - SCREEN_WIDTH * scale * 0.5, scale);
	}

	//------------------------------------------------------------------------------------------------
	//! Strip across the top of the portrait screen: clock on the left, signal and battery on the right
	protected void LayoutStatusBar(float left, float top, float scale)
	{
		Place(m_wStatusBar, left, top, SCREEN_WIDTH * scale, STATUS_BAR_HEIGHT * scale);

		float iconTop = (STATUS_BAR_HEIGHT - STATUS_ICON_SIZE) * 0.5 * scale;
		Place(m_wBattery, (SCREEN_WIDTH - STATUS_MARGIN - STATUS_ICON_SIZE) * scale, iconTop, STATUS_ICON_SIZE * scale, STATUS_ICON_SIZE * scale);
		Place(m_wSignal, (SCREEN_WIDTH - STATUS_MARGIN - STATUS_ICON_SIZE * 2 - 14) * scale, iconTop, STATUS_ICON_SIZE * scale, STATUS_ICON_SIZE * scale);

		SetFontSize(m_wClock, 32 * scale);
		PlaceTextLeft(m_wClock, STATUS_MARGIN * scale, STATUS_BAR_HEIGHT * 0.5 * scale);
	}

	//------------------------------------------------------------------------------------------------
	//! Portrait home screen: app tiles in a row under the status bar
	protected void LayoutHome(float left, float top, float scale)
	{
		Place(m_wHome, left, top, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale);

		// Bank
		float tileLeft = TILE_LEFT;
		Place(m_wBankTile, tileLeft * scale, TILE_TOP * scale, TILE_SIZE * scale, TILE_SIZE * scale);
		SetFontSize(m_wBankTileLabel, 32 * scale);
		CentreText(m_wBankTileLabel, (tileLeft + TILE_SIZE * 0.5) * scale, (TILE_TOP + TILE_SIZE + 34) * scale);
		Place(m_wBankTileButton, tileLeft * scale, TILE_TOP * scale, TILE_SIZE * scale, (TILE_SIZE + 60) * scale);

		// Test (landscape)
		tileLeft = TILE_LEFT + TILE_SIZE + TILE_SPACING;
		Place(m_wTestTile, tileLeft * scale, TILE_TOP * scale, TILE_SIZE * scale, TILE_SIZE * scale);
		SetFontSize(m_wTestTileGlyph, 90 * scale);
		CentreText(m_wTestTileGlyph, (tileLeft + TILE_SIZE * 0.5) * scale, (TILE_TOP + TILE_SIZE * 0.5) * scale);
		SetFontSize(m_wTestTileLabel, 32 * scale);
		CentreText(m_wTestTileLabel, (tileLeft + TILE_SIZE * 0.5) * scale, (TILE_TOP + TILE_SIZE + 34) * scale);
		Place(m_wTestTileButton, tileLeft * scale, TILE_TOP * scale, TILE_SIZE * scale, (TILE_SIZE + 60) * scale);

		// RebIntel
		tileLeft = TILE_LEFT + (TILE_SIZE + TILE_SPACING) * 2;
		Place(m_wRebIntelTile, tileLeft * scale, TILE_TOP * scale, TILE_SIZE * scale, TILE_SIZE * scale);
		SetFontSize(m_wRebIntelTileGlyph, 74 * scale);
		CentreText(m_wRebIntelTileGlyph, (tileLeft + TILE_SIZE * 0.5) * scale, (TILE_TOP + TILE_SIZE * 0.5) * scale);
		SetFontSize(m_wRebIntelTileLabel, 32 * scale);
		CentreText(m_wRebIntelTileLabel, (tileLeft + TILE_SIZE * 0.5) * scale, (TILE_TOP + TILE_SIZE + 34) * scale);
		Place(m_wRebIntelTileButton, tileLeft * scale, TILE_TOP * scale, TILE_SIZE * scale, (TILE_SIZE + 60) * scale);

		// HICOMM (landscape), first tile of the second row
		tileLeft = TILE_LEFT;
		float tileTop = TILE_TOP + TILE_SIZE + TILE_ROW_SPACING;
		Place(m_wHiCommTile, tileLeft * scale, tileTop * scale, TILE_SIZE * scale, TILE_SIZE * scale);
		float globeInset = 14;
		float globeSize = TILE_SIZE - globeInset * 2;
		Place(m_wHiCommTileCanvas, (tileLeft + globeInset) * scale, (tileTop + globeInset) * scale, globeSize * scale, globeSize * scale);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float globePixels = workspace.DPIScale(globeSize * scale);
		m_HiCommTileGlobe.Draw(m_wHiCommTileCanvas, globePixels, globePixels, System.GetTickCount() / 1000.0, workspace.DPIScale(scale) * 0.5);
		SetFontSize(m_wHiCommTileLabel, 32 * scale);
		CentreText(m_wHiCommTileLabel, (tileLeft + TILE_SIZE * 0.5) * scale, (tileTop + TILE_SIZE + 34) * scale);
		Place(m_wHiCommTileButton, tileLeft * scale, tileTop * scale, TILE_SIZE * scale, (TILE_SIZE + 60) * scale);

		// OPS (landscape), second tile of the second row
		tileLeft = TILE_LEFT + TILE_SIZE + TILE_SPACING;
		Place(m_wOpsTile, tileLeft * scale, tileTop * scale, TILE_SIZE * scale, TILE_SIZE * scale);
		float crestInset = 26;
		float crestSize = TILE_SIZE - crestInset * 2;
		Place(m_wOpsTileCanvas, (tileLeft + crestInset) * scale, (tileTop + crestInset) * scale, crestSize * scale, crestSize * scale);
		float crestPixels = workspace.DPIScale(crestSize * scale);
		m_OpsTileCrest.Draw(m_wOpsTileCanvas, crestPixels, crestPixels, System.GetTickCount() / 1000.0, workspace.DPIScale(scale) * 0.6);
		SetFontSize(m_wOpsTileLabel, 32 * scale);
		CentreText(m_wOpsTileLabel, (tileLeft + TILE_SIZE * 0.5) * scale, (tileTop + TILE_SIZE + 34) * scale);
		Place(m_wOpsTileButton, tileLeft * scale, tileTop * scale, TILE_SIZE * scale, (TILE_SIZE + 60) * scale);

		// Black Market: its tile widgets sit in the app's own layout over the whole phone, so add the screen corner
		tileLeft = TILE_LEFT + (TILE_SIZE + TILE_SPACING) * 2;
		if (m_wBlackMarketTileGlyph && m_wBlackMarketTileLabel && m_wBlackMarketTileButton)
		{
			SetFontSize(m_wBlackMarketTileGlyph, 86 * scale);
			CentreText(m_wBlackMarketTileGlyph, left + (tileLeft + TILE_SIZE * 0.5) * scale, top + (tileTop + TILE_SIZE * 0.5) * scale);
			SetFontSize(m_wBlackMarketTileLabel, 32 * scale);
			CentreText(m_wBlackMarketTileLabel, left + (tileLeft + TILE_SIZE * 0.5) * scale, top + (tileTop + TILE_SIZE + 34) * scale);
			Place(m_wBlackMarketTileButton, left + tileLeft * scale, top + tileTop * scale, TILE_SIZE * scale, (TILE_SIZE + 60) * scale);
		}

		// Imports: first tile of the third row
		float thirdRowTop = TILE_TOP + (TILE_SIZE + TILE_ROW_SPACING) * 2;
		if (m_wImportsTileGlyph && m_wImportsTileLabel && m_wImportsTileButton)
		{
			SetFontSize(m_wImportsTileGlyph, 86 * scale);
			CentreText(m_wImportsTileGlyph, left + (TILE_LEFT + TILE_SIZE * 0.5) * scale, top + (thirdRowTop + TILE_SIZE * 0.5) * scale);
			SetFontSize(m_wImportsTileLabel, 32 * scale);
			CentreText(m_wImportsTileLabel, left + (TILE_LEFT + TILE_SIZE * 0.5) * scale, top + (thirdRowTop + TILE_SIZE + 34) * scale);
			Place(m_wImportsTileButton, left + TILE_LEFT * scale, top + thirdRowTop * scale, TILE_SIZE * scale, (TILE_SIZE + 60) * scale);
		}

		DrawRoundedTiles(scale);
		LayoutQuickDock(scale);
	}

	//------------------------------------------------------------------------------------------------
	//! App icons with rounded corners like the Bank icon (its art rounds ~22% of the width); the square tile
	//! images are hidden and these shapes are drawn behind each tile's glyph and label
	protected void DrawRoundedTiles(float scale)
	{
		if (!m_wHomeTileCanvas)
			return;

		Place(m_wHomeTileCanvas, 0, 0, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale);
		float pixels = GetGame().GetWorkspace().DPIScale(scale);
		float secondRow = TILE_TOP + TILE_SIZE + TILE_ROW_SPACING;
		m_aTileCommands.Clear();
		AddRoundedTile(TILE_LEFT + TILE_SIZE + TILE_SPACING, TILE_TOP, AROR_UIColors.SURFACE_RAISED, pixels);
		AddRoundedTile(TILE_LEFT + (TILE_SIZE + TILE_SPACING) * 2, TILE_TOP, AROR_UIColors.SURFACE_RAISED, pixels);
		AddRoundedTile(TILE_LEFT, secondRow, AROR_UIColors.SURFACE, pixels);
		AddRoundedTile(TILE_LEFT + TILE_SIZE + TILE_SPACING, secondRow, AROR_UIColors.SURFACE, pixels);
		if (m_BlackMarket)
			AddRoundedTile(TILE_LEFT + (TILE_SIZE + TILE_SPACING) * 2, secondRow, AROR_UIColors.SURFACE, pixels);
		if (m_Imports)
			AddRoundedTile(TILE_LEFT, TILE_TOP + (TILE_SIZE + TILE_ROW_SPACING) * 2, AROR_UIColors.SURFACE, pixels);
		if (m_bManualSaveAvailable)
		{
			// Quick dock slot 3, in the garage slot's colours
			float rowWidth = QUICK_SLOT_COUNT * QUICK_SLOT_SIZE + (QUICK_SLOT_COUNT - 1) * QUICK_SLOT_GAP;
			float slotLeft = (SCREEN_WIDTH - rowWidth) * 0.5 + 3 * (QUICK_SLOT_SIZE + QUICK_SLOT_GAP);
			float slotTop = SCREEN_HEIGHT - QUICK_DOCK_BOTTOM - QUICK_SLOT_SIZE;
			AROR_PhoneSaveIcon.Add(m_aTileCommands, slotLeft, slotTop, QUICK_SLOT_SIZE, pixels, Color.FromRGBA(36, 72, 48, 255), Color.FromRGBA(168, 240, 196, 255));
		}
		m_wHomeTileCanvas.SetDrawCommands(m_aTileCommands);
	}

	//------------------------------------------------------------------------------------------------
	//! Save campaign in the quick dock: admins (the server checks again) once a campaign is running
	protected bool CanManualSave()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController || !AROR_AdminAccess.IsAdministrator(playerController.GetPlayerId()))
			return false;

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		return gameMode && gameMode.GetState() == SCR_EGameModeState.GAME;
	}

	//------------------------------------------------------------------------------------------------
	//! One rounded square at (x, y) screen units: fill plus a faint rebel outline so it reads on the wallpaper
	protected void AddRoundedTile(float x, float y, int fill, float pixels)
	{
		array<float> outline = {};
		int steps = 6;
		float radius = TILE_SIZE * TILE_CORNER_FRACTION;
		array<float> cornerX = {x + TILE_SIZE - radius, x + radius, x + radius, x + TILE_SIZE - radius};
		array<float> cornerY = {y + radius, y + radius, y + TILE_SIZE - radius, y + TILE_SIZE - radius};
		for (int corner = 0; corner < 4; corner++)
		{
			for (int i = 0; i <= steps; i++)
			{
				// Corners anticlockwise from the top right, each sweeping a quarter turn
				float angle = (corner * 90 + i * 90 / steps) * Math.DEG2RAD;
				outline.Insert((cornerX[corner] + Math.Cos(angle) * radius) * pixels);
				outline.Insert((cornerY[corner] - Math.Sin(angle) * radius) * pixels);
			}
		}

		PolygonDrawCommand shape = new PolygonDrawCommand();
		shape.m_iColor = AROR_UIColors.Get(fill).PackToInt();
		shape.m_Vertices = outline;
		m_aTileCommands.Insert(shape);

		LineDrawCommand edge = new LineDrawCommand();
		edge.m_iColor = AROR_UIColors.Get(AROR_UIColors.REBEL_DIM).PackToInt();
		edge.m_fWidth = 2 * pixels;
		edge.m_Vertices = outline;
		edge.m_bShouldEnclose = true;
		m_aTileCommands.Insert(edge);
	}

	//------------------------------------------------------------------------------------------------
	//! Four equal slots along the bottom of the home screen (slot 0 = garage store; 1–2 empty; 3 = save campaign)
	protected void LayoutQuickDock(float scale)
	{
		float rowWidth = QUICK_SLOT_COUNT * QUICK_SLOT_SIZE + (QUICK_SLOT_COUNT - 1) * QUICK_SLOT_GAP;
		float startLeft = (SCREEN_WIDTH - rowWidth) * 0.5;
		float slotTop = SCREEN_HEIGHT - QUICK_DOCK_BOTTOM - QUICK_SLOT_SIZE;

		array<ImageWidget> slots = {m_wQuickSlot0, m_wQuickSlot1, m_wQuickSlot2, m_wQuickSlot3};
		array<Widget> buttons = {m_wQuickSlot0Button, m_wQuickSlot1Button, m_wQuickSlot2Button, m_wQuickSlot3Button};

		for (int i = 0; i < QUICK_SLOT_COUNT; i++)
		{
			float slotLeft = startLeft + i * (QUICK_SLOT_SIZE + QUICK_SLOT_GAP);
			Place(slots[i], slotLeft * scale, slotTop * scale, QUICK_SLOT_SIZE * scale, QUICK_SLOT_SIZE * scale);
			Place(buttons[i], slotLeft * scale, slotTop * scale, QUICK_SLOT_SIZE * scale, QUICK_SLOT_SIZE * scale);
		}

		SetFontSize(m_wQuickSlot0Glyph, 64 * scale);
		CentreText(m_wQuickSlot0Glyph, (startLeft + QUICK_SLOT_SIZE * 0.5) * scale, (slotTop + QUICK_SLOT_SIZE * 0.5) * scale);
	}

	//------------------------------------------------------------------------------------------------
	//! Bank: the app screen image with the account holder and both balances written over it (live values)
	protected void LayoutBank(float left, float top, float scale)
	{
		Place(m_wBankApp, left, top, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale);
		Place(m_wBankBackground, 0, 0, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale);

		string personal = "$ -";
		string resistance = "$ -";
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (economy)
		{
			personal = AROR_EconomyComponent.FormatMoney(economy.GetLocalPlayerMoney());
			resistance = AROR_EconomyComponent.FormatMoney(economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE));
		}

		m_wBankName.SetText(GetLocalPlayerName());
		m_wBankPersonal.SetText(personal);
		m_wBankResistance.SetText(resistance);

		SetFontSize(m_wBankName, FitFontSize(GetLocalPlayerName(), BANK_NAME_FONT, BANK_NAME_WIDTH) * scale);
		PlaceTextLeft(m_wBankName, BANK_NAME_X * scale, (BANK_NAME_Y + BANK_NAME_HEIGHT * 0.5) * scale);

		SetFontSize(m_wBankPersonal, FitFontSize(personal, BANK_BALANCE_FONT, BANK_BALANCE_WIDTH) * scale);
		PlaceTextLeft(m_wBankPersonal, BANK_BALANCE_X * scale, (BANK_PERSONAL_Y + BANK_BALANCE_HEIGHT * 0.5) * scale);

		SetFontSize(m_wBankResistance, FitFontSize(resistance, BANK_BALANCE_FONT, BANK_BALANCE_WIDTH) * scale);
		PlaceTextLeft(m_wBankResistance, BANK_BALANCE_X * scale, (BANK_RESISTANCE_Y + BANK_BALANCE_HEIGHT * 0.5) * scale);

		if (m_wBankTransferButton)
		{
			m_wBankTransferButton.SetVisible(true);
			m_wBankTransferButton.SetEnabled(true);
			Place(m_wBankTransferButton, BANK_TRANSFER_X * scale, BANK_TRANSFER_Y * scale, BANK_TRANSFER_WIDTH * scale, BANK_TRANSFER_HEIGHT * scale);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Landscape test app: title, text and a Back button
	protected void LayoutTestApp(float left, float top, float scale)
	{
		float width = SCREEN_HEIGHT * scale;
		float height = SCREEN_WIDTH * scale;
		Place(m_wTestApp, left, top, width, height);

		SetFontSize(m_wTestTitle, 52 * scale);
		FrameSlot.SetPos(m_wTestTitle, 130 * scale, 80 * scale);

		SetFontSize(m_wTestBody, 30 * scale);
		FrameSlot.SetPos(m_wTestBody, 130 * scale, 170 * scale);

		float buttonLeft = 130;
		float buttonTop = SCREEN_WIDTH - 110 - 96;
		float buttonWidth = 240;
		float buttonHeight = 96;
		Place(m_wTestBackBackground, buttonLeft * scale, buttonTop * scale, buttonWidth * scale, buttonHeight * scale);

		SetFontSize(m_wTestBackLabel, 34 * scale);
		CentreText(m_wTestBackLabel, (buttonLeft + buttonWidth * 0.5) * scale, (buttonTop + buttonHeight * 0.5) * scale);

		Place(m_wTestBackButton, buttonLeft * scale, buttonTop * scale, buttonWidth * scale, buttonHeight * scale);
	}

	//------------------------------------------------------------------------------------------------
	//! Textures, colours and fixed text
	protected void ApplyStyle()
	{
		m_wPhonePortrait.LoadImageTexture(0, PHONE_PORTRAIT);
		m_wPhoneLandscape.LoadImageTexture(0, PHONE_LANDSCAPE);

		// Rotate around the widget's centre
		m_wPhonePortrait.SetPivot(0.5, 0.5);
		m_wPhoneLandscape.SetPivot(0.5, 0.5);

		m_wDim.SetColor(Color.FromRGBA(0, 0, 0, 255));

		LoadTexture(m_wSignal, SIGNAL_ICON);
		LoadTexture(m_wBattery, BATTERY_ICON);
		LoadTexture(m_wBankTile, BANK_ICON);
		LoadTexture(m_wBankBackground, BANK_SCREEN);

		// The test app draws straight over the wallpaper
		if (m_wTestPanel)
			m_wTestPanel.SetVisible(false);

		// App icons: dark rounded tiles drawn on HomeTileCanvas (see DrawRoundedTiles), glyph in rebel green.
		// The square tile images stay as layout anchors only.
		array<Widget> squareTiles = {m_wTestTile, m_wRebIntelTile, m_wHiCommTile, m_wOpsTile};
		foreach (Widget squareTile : squareTiles)
		{
			if (squareTile)
				squareTile.SetVisible(false);
		}
		SetColor(m_wTestBackBackground, Color.FromRGBA(255, 255, 255, 40));

		// Quick dock: slot 0 active (garage); 1–2 muted empty placeholders; 3 muted unless the player can save
		SetColor(m_wQuickSlot0, Color.FromRGBA(36, 72, 48, 255));
		SetColor(m_wQuickSlot1, Color.FromRGBA(28, 32, 36, 180));
		SetColor(m_wQuickSlot2, Color.FromRGBA(28, 32, 36, 180));
		SetColor(m_wQuickSlot3, Color.FromRGBA(28, 32, 36, 180));

		// Buttons only catch clicks; the visuals are the widgets underneath
		array<Widget> buttons = {m_wBankTileButton, m_wTestTileButton, m_wRebIntelTileButton, m_wHiCommTileButton, m_wOpsTileButton, m_wQuickSlot0Button, m_wQuickSlot1Button, m_wQuickSlot2Button, m_wQuickSlot3Button, m_wBankTransferButton, m_wTestBackButton, m_wHomeButton};
		foreach (Widget button : buttons)
		{
			SetColor(button, Color.FromRGBA(0, 0, 0, 0));
		}

		if (m_wQuickSlot1Button)
			m_wQuickSlot1Button.SetEnabled(false);
		if (m_wQuickSlot2Button)
			m_wQuickSlot2Button.SetEnabled(false);
		// Save campaign: its square and floppy disk are drawn on HomeTileCanvas (see DrawRoundedTiles)
		if (m_wQuickSlot3Button)
			m_wQuickSlot3Button.SetEnabled(m_bManualSaveAvailable);
		if (m_wQuickSlot3)
			m_wQuickSlot3.SetVisible(!m_bManualSaveAvailable);

		StyleText(m_wClock, "--:--", true);
		StyleText(m_wBankTileLabel, "Bank", false);
		StyleText(m_wTestTileGlyph, "M", true);
		StyleText(m_wTestTileLabel, "MARS", false);
		StyleText(m_wRebIntelTileGlyph, "RI", true);
		StyleText(m_wRebIntelTileLabel, "REBINTEL", false);
		StyleText(m_wHiCommTileGlyph, string.Empty, true);
		StyleText(m_wHiCommTileLabel, "HICOMM", false);
		StyleText(m_wOpsTileLabel, "OPS", false);
		StyleText(m_wBlackMarketTileGlyph, "$", true);
		StyleText(m_wBlackMarketTileLabel, "MARKET", false);
		if (m_wBlackMarketTileGlyph && m_wBlackMarketTileLabel)
		{
			m_wBlackMarketTileGlyph.SetColor(AROR_UIColors.Get(AROR_UIColors.REBEL));
			FrameSlot.SetSizeToContent(m_wBlackMarketTileGlyph, true);
			FrameSlot.SetSizeToContent(m_wBlackMarketTileLabel, true);
		}
		StyleText(m_wImportsTileGlyph, "V", true);
		StyleText(m_wImportsTileLabel, "IMPORTS", false);
		if (m_wImportsTileGlyph && m_wImportsTileLabel)
		{
			m_wImportsTileGlyph.SetColor(AROR_UIColors.Get(AROR_UIColors.REBEL));
			FrameSlot.SetSizeToContent(m_wImportsTileGlyph, true);
			FrameSlot.SetSizeToContent(m_wImportsTileLabel, true);
		}
		array<TextWidget> glyphs = {m_wTestTileGlyph, m_wRebIntelTileGlyph, m_wHiCommTileGlyph};
		foreach (TextWidget glyph : glyphs)
		{
			if (glyph)
				glyph.SetColor(AROR_UIColors.Get(AROR_UIColors.REBEL));
		}
		StyleText(m_wQuickSlot0Glyph, "G", true);
		if (m_wQuickSlot0Glyph)
			m_wQuickSlot0Glyph.SetColor(Color.FromRGBA(168, 240, 196, 255));
		StyleText(m_wBankName, string.Empty, true);
		StyleText(m_wBankPersonal, string.Empty, true);
		StyleText(m_wBankResistance, string.Empty, true);
		StyleText(m_wTestTitle, "Test app", true);
		StyleText(m_wTestBody, "The phone turned to landscape. Press Back to return, or Esc to close the phone.", false);
		StyleText(m_wTestBackLabel, "Back", true);

		array<Widget> texts = {m_wClock, m_wBankTileLabel, m_wTestTileGlyph, m_wTestTileLabel, m_wRebIntelTileGlyph, m_wRebIntelTileLabel, m_wHiCommTileGlyph, m_wHiCommTileLabel, m_wOpsTileLabel, m_wQuickSlot0Glyph, m_wBankName, m_wBankPersonal, m_wBankResistance, m_wTestTitle, m_wTestBody, m_wTestBackLabel};
		foreach (Widget text : texts)
		{
			if (text)
				FrameSlot.SetSizeToContent(text, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! In-game time of day
	protected void UpdateClock()
	{
		if (!m_wClock)
			return;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;

		TimeAndWeatherManagerEntity timeManager = world.GetTimeAndWeatherManager();
		if (!timeManager)
			return;

		int hours, minutes, seconds;
		timeManager.GetHoursMinutesSeconds(hours, minutes, seconds);
		m_wClock.SetText(hours.ToString(2) + ":" + minutes.ToString(2));
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetLocalPlayerName()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return string.Empty;

		return GetGame().GetPlayerManager().GetPlayerName(playerController.GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Font size that keeps the text within maxWidth (estimated from its length), up to fontSize
	protected static float FitFontSize(string text, float fontSize, float maxWidth)
	{
		int length = text.Length();
		if (length == 0)
			return fontSize;

		return Math.Min(fontSize, maxWidth / (length * CHAR_WIDTH_FACTOR));
	}

	//------------------------------------------------------------------------------------------------
	protected static void LoadTexture(ImageWidget image, ResourceName texture)
	{
		if (image)
			image.LoadImageTexture(0, texture);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StyleText(TextWidget text, string content, bool bold)
	{
		if (!text)
			return;

		text.SetText(content);
		text.SetColor(AROR_UIColors.Get(AROR_UIColors.TEXT));
		text.SetBold(bold);
		// Home and Bank text sits on wallpaper art, so it keeps a shadow (flat-only rules are for the HUD)
		text.SetShadow(1, 0xFF000000, 0.4, 1, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetFontSize(TextWidget text, float size)
	{
		if (!text)
			return;

		int fontSize = Math.Round(size);
		text.SetExactFontSize(Math.Max(fontSize, 1));
	}

	//------------------------------------------------------------------------------------------------
	//! Measured size of a size-to-content text in reference pixels (correct from the frame after a change)
	protected static void GetTextSize(TextWidget text, out float width, out float height)
	{
		text.GetScreenSize(width, height);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		width = workspace.DPIUnscale(width);
		height = workspace.DPIUnscale(height);
	}

	//------------------------------------------------------------------------------------------------
	//! Centres a size-to-content text on a point
	protected static void CentreText(TextWidget text, float x, float y)
	{
		if (!text)
			return;

		float width, height;
		GetTextSize(text, width, height);
		FrameSlot.SetPos(text, x - width * 0.5, y - height * 0.5);
	}

	//------------------------------------------------------------------------------------------------
	//! Left-aligns a size-to-content text at x, vertically centred on y
	protected static void PlaceTextLeft(TextWidget text, float x, float centreY)
	{
		if (!text)
			return;

		float width, height;
		GetTextSize(text, width, height);
		FrameSlot.SetPos(text, x, centreY - height * 0.5);
	}

	//------------------------------------------------------------------------------------------------
	//! Size and centre a widget on a point, rotated around its own centre
	protected static void PlaceRotated(ImageWidget image, float centerX, float centerY, float width, float height, float angle)
	{
		Place(image, centerX - width * 0.5, centerY - height * 0.5, width, height);
		image.SetRotation(angle);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Place(Widget widget, float x, float y, float width, float height)
	{
		if (!widget)
			return;

		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, width, height);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetColor(Widget widget, Color color)
	{
		if (widget)
			widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected static float MoveTowards(float value, float target, float step)
	{
		if (value < target)
			return Math.Min(value + step, target);

		return Math.Max(value - step, target);
	}

	//------------------------------------------------------------------------------------------------
	protected static float SmoothStep(float t)
	{
		return t * t * (3 - 2 * t);
	}

	//------------------------------------------------------------------------------------------------
	protected static float EaseOutCubic(float t)
	{
		float inverse = 1 - t;
		return 1 - inverse * inverse * inverse;
	}
}
