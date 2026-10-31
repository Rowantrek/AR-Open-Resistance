//! HICOMM screens (see ARCHITECTURE.md -> High Command)
enum AROR_EHiCommScreen
{
	LOGIN,		//!< Clearance check: only Resistance Leaders get past it
	COMMAND,	//!< Resistance name and leaders (the host delegates here)
	STRATEGIC,	//!< Strategic Command: map and orders only
	UNITS		//!< Unit Management: raise squads, review the order of battle
}

//! Passes HICOMM clicks, with their screen position, to the app
class AROR_HiCommClickHandler : ScriptedWidgetEventHandler
{
	protected AROR_HiCommApp m_App;

	//------------------------------------------------------------------------------------------------
	void AROR_HiCommClickHandler(AROR_HiCommApp app)
	{
		m_App = app;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App)
			m_App.OnClick(w, x, y);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		return m_App && m_App.OnMouseDown(w, x, y, button);
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		return m_App && m_App.OnMouseUp(w, x, y, button);
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		return m_App && m_App.OnMouseWheel(w, x, y, wheel);
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_App)
			m_App.OnMouseLeave(w);

		return false;
	}
}

//! Last values pushed to one pooled widget, so unchanged properties are not set again every frame
class AROR_HiCommWidgetCache
{
	float m_fX = -100000;
	float m_fY;
	float m_fWidth;
	float m_fHeight;
	int m_iFontSize = -1;
	int m_iColor;
	bool m_bColorSet;
	int m_iBold = -1;
	int m_iVisible = -1;

	// Text layout inputs (wrapping and measuring are redone only when these change)
	string m_sRaw;
	float m_fWrapWidth = -1;
	float m_fWrapSize = -1;
	int m_iCentred = -1;
	int m_iLongest;
	int m_iLines = 1;
}

//! HICOMM: the Resistance Leaders' landscape phone app, in a phosphor-terminal style. Client only; every action is a
//! request to AROR_HighCommandComponent on the server, and everything shown comes from replicated state.
//! The layout (AROR_PhoneMenu.layout) declares fixed pools of panels, texts, buttons and map labels; each frame the
//! current screen takes what it needs from the pools (in landscape screen pixels, 1700 x 900, times the phone scale)
//! and the rest are hidden. Buttons carry an action string for that frame.
class AROR_HiCommApp
{
	protected static const float WIDTH = 1700;
	protected static const float HEIGHT = 900;

	protected static const int PANEL_POOL = 24;
	protected static const int TEXT_POOL = 80;
	protected static const int BUTTON_POOL = 40;
	protected static const int MAP_LABEL_POOL = 128;

	protected static const string BUTTON_PREFIX = "HiCommBtn";

	// Button looks
	protected static const int STYLE_NORMAL = 0;	//!< Own / available
	protected static const int STYLE_SELECTED = 1;
	protected static const int STYLE_PRIMARY = 2;	//!< Amber call to action
	protected static const int STYLE_DISABLED = 3;	//!< Not clickable
	protected static const int STYLE_DANGER = 4;
	protected static const int STYLE_MUTED = 5;	//!< Someone else's (view only)
	protected static const int STYLE_PANEL = 6;	//!< Read-out box (tooltips, order preview); not clickable

	//! Content is drawn at this fraction of the phone screen, centred, so nothing touches the screen edges
	protected static const float CONTENT_SCALE = 0.95;

	// Strategic Command (app units): the map fills the body; panels float over it
	protected static const float MAP_LEFT = 16;
	protected static const float MAP_TOP = 96;
	protected static const float MAP_WIDTH = 1668;
	protected static const float MAP_HEIGHT = 790;
	protected static const float PANEL_MARGIN = 10;
	protected static const float OOB_WIDTH = 260;
	protected static const float SELECTION_WIDTH = 300;
	protected static const float SELECTION_HEIGHT = 432;
	protected static const float HELP_WIDTH = 270;
	protected static const float SLIDER_HEIGHT = 240;

	//! Location icons over the map (the in-game map's icons, tinted by controlling faction)
	protected static const int ICON_POOL = 64;
	protected static const float ICON_SIZE = 22;

	// Map palette (packed RGB, see AROR_HiCommMap.Rgb)
	protected static const int PAL_GREEN = 0x86F0AC;
	protected static const int PAL_GREEN_DIM = 0x4E966A;
	protected static const int PAL_AMBER = 0xF6BA40;
	protected static const int PAL_RED = 0xF05C52;
	protected static const int PAL_OCCUPIERS = 0x6096F0;
	protected static const int PAL_INVADERS = 0xF6963C;
	protected static const int PAL_MOVE = 0x7CD8F0;	//!< Move orders

	//! Spacing (m) between squads given one Move point together
	protected static const float MOVE_SPACING = 35;

	//! Squads closer than this on screen (pixels) are drawn as one stack
	protected static const float SQUAD_CLUSTER_PIXELS = 22;

	//! Phosphor-skinned game map: our map config (colour overrides on the base map) and the tint multiplied over
	//! everything the map draws. The tint darkens the map's land, whose colour the configs do not expose.
	protected static const ResourceName GAME_MAP_CONFIG = "{6AB4E0F1A2C0A001}Configs/Map/AROR_HiCommMap.conf";

	//! Map: pick radius and drag threshold (pixels), order ping length (ms), keyboard pan (px/s), wheel zoom step
	protected static const float PICK_RADIUS = 18;
	protected static const float ORDER_PICK_RADIUS = 60;	//!< Attack/defend clicks snap to a location this close (pixels)
	protected static const float DRAG_THRESHOLD = 6;
	protected static const int PING_MS = 900;
	protected static const float PAN_SPEED = 700;
	protected static const float WHEEL_ZOOM = 1.2;

	//! Rough width of a character as a fraction of the font size, used to centre and right-align text
	protected static const float CHAR_WIDTH_FACTOR = 0.58;
	protected static const float WRAP_CHAR_FACTOR = 0.62;	//!< Slightly wider when wrapping, so lines never overrun

	protected Widget m_wRoot;
	protected ImageWidget m_wBackground;
	protected CanvasWidget m_wMapCanvas;
	protected CanvasWidget m_wMapOverlay;
	protected Widget m_wMapClip;	//!< Clipping frame the base canvas slides in
	protected Widget m_wGameMapRoot;	//!< Holds the real game map (base-game Map.layout)
	protected bool m_bUseGameMap = true;	//!< Base game map (default for now, untinted) or the procedural tactical view
	protected bool m_bGameMapOpen;
	protected Widget m_wMapButton;

	protected ref array<ImageWidget> m_aPanels = {};
	protected ref array<RichTextWidget> m_aTexts = {};
	protected ref array<string> m_aTextCache = {};
	protected ref array<ImageWidget> m_aButtonBackgrounds = {};
	protected ref array<RichTextWidget> m_aButtonLabels = {};
	protected ref array<string> m_aButtonLabelCache = {};
	protected ref array<Widget> m_aButtons = {};
	protected ref array<string> m_aButtonActions = {};
	protected ref array<RichTextWidget> m_aMapLabels = {};
	protected ref array<string> m_aMapLabelCache = {};
	protected ref array<ImageWidget> m_aMapIcons = {};
	protected ref array<ResourceName> m_aMapIconCache = {};

	// Property caches, one per pooled widget (see AROR_HiCommWidgetCache)
	protected ref array<ref AROR_HiCommWidgetCache> m_aPanelCaches = {};
	protected ref array<ref AROR_HiCommWidgetCache> m_aTextCaches = {};
	protected ref array<ref AROR_HiCommWidgetCache> m_aButtonBgCaches = {};
	protected ref array<ref AROR_HiCommWidgetCache> m_aButtonLabelCaches = {};
	protected ref array<ref AROR_HiCommWidgetCache> m_aButtonCaches = {};
	protected ref array<ref AROR_HiCommWidgetCache> m_aMapLabelCaches = {};
	protected ref array<ref AROR_HiCommWidgetCache> m_aMapIconCaches = {};

	// This frame's location icons (canvas pixels)
	protected ref array<ResourceName> m_aIconTextures = {};
	protected ref array<vector> m_aIconPositions = {};
	protected ref array<ref Color> m_aIconColors = {};

	// Floating panels this frame (app units: x, y, width, height per panel)
	protected ref array<float> m_aPanelRects = {};
	protected bool m_bShowOrderOfBattle = true;
	protected bool m_bShowHelp;
	protected float m_fOobBottom;
	protected float m_fRightEdge;	//!< Right edge of free map space (left of the orders panel)
	protected float m_fLegendLeft;
	protected float m_fSliderX;	//!< Zoom slider track (app units)
	protected float m_fSliderTop;
	protected bool m_bSliderDrag;

	protected int m_iPanelCount;
	protected int m_iTextCount;
	protected int m_iButtonCount;
	protected int m_iMapLabelCount;
	protected int m_iMapIconCount;

	protected ref AROR_HiCommMap m_Map;
	protected ref AROR_HiCommClickHandler m_ClickHandler;

	// Palette
	protected ref Color m_Background = AROR_HiCommMap.Srgb(5, 11, 8, 1);
	protected ref Color m_PanelFill = AROR_HiCommMap.Srgb(9, 19, 13, 1);
	protected ref Color m_Border = AROR_HiCommMap.Srgb(44, 92, 64, 1);
	protected ref Color m_Green = AROR_HiCommMap.Srgb(134, 240, 172, 1);
	protected ref Color m_GreenDim = AROR_HiCommMap.Srgb(78, 150, 106, 1);
	protected ref Color m_GreenFaint = AROR_HiCommMap.Srgb(48, 92, 66, 1);
	protected ref Color m_Amber = AROR_HiCommMap.Srgb(246, 186, 64, 1);
	protected ref Color m_Red = AROR_HiCommMap.Srgb(240, 92, 82, 1);
	protected ref Color m_Dark = AROR_HiCommMap.Srgb(6, 12, 8, 1);
	protected ref Color m_Occupiers = AROR_HiCommMap.Srgb(96, 150, 240, 1);
	protected ref Color m_Invaders = AROR_HiCommMap.Srgb(246, 150, 60, 1);
	protected ref Color m_ButtonFill = AROR_HiCommMap.Srgb(20, 44, 30, 1);
	protected ref Color m_ButtonDisabled = AROR_HiCommMap.Srgb(14, 24, 18, 1);
	protected ref Color m_ButtonDanger = AROR_HiCommMap.Srgb(70, 22, 20, 1);
	protected ref Color m_ButtonMuted = AROR_HiCommMap.Srgb(12, 24, 17, 1);
	protected ref Color m_ButtonPanel = AROR_HiCommMap.Srgb(4, 12, 8, 0.93);

	// State (reset each time the phone opens)
	protected AROR_EHiCommScreen m_eScreen = AROR_EHiCommScreen.LOGIN;
	protected AROR_EHiCommScreen m_eLastScreen = -1;
	protected int m_iLoginStart;
	protected ref AROR_HiCommGlobe m_Globe = new AROR_HiCommGlobe();
	protected ref array<int> m_aSelected = {};	//!< Selected squad IDs
	protected AROR_EHighCommandOrder m_eSelectedOrder = AROR_EHighCommandOrder.MOVE;
	protected string m_sSelectedTarget;

	// Map input (screen pixels unless noted)
	protected bool m_bLeftDown;
	protected bool m_bRightDown;
	protected bool m_bDragging;
	protected int m_iPressX;
	protected int m_iPressY;
	protected int m_iLastMouseX;
	protected int m_iLastMouseY;
	protected int m_iLastFrameMs;
	protected int m_iLastMapClickMs;
	protected vector m_vCursor;	//!< Canvas pixels
	protected bool m_bCursorInside;
	protected string m_sHoverLocation;
	protected int m_iHoverSquad = -1;
	protected vector m_vPing;
	protected int m_iPingTime;
	protected int m_iPingColor;	//!< Packed RGB
	protected vector m_vMoveTarget;	//!< Planned Move point (world)
	protected bool m_bHasMoveTarget;

	// Map: geometry this frame (app units) and what this frame's overlay put where (canvas pixels)
	protected float m_fScale = 1;
	protected float m_fInsetX;	//!< Content offset from the screen corner (scaled pixels)
	protected float m_fInsetY;
	protected float m_fMapX;
	protected float m_fMapY;
	protected float m_fMapWidth;
	protected float m_fMapHeight;
	protected ref array<int> m_aHitSquadIds = {};
	protected ref array<vector> m_aHitSquadPositions = {};
	protected ref array<string> m_aHitLocationIds = {};
	protected ref array<vector> m_aHitLocationPositions = {};
	protected ref array<string> m_aLabelTexts = {};
	protected ref array<vector> m_aLabelPositions = {};
	protected ref array<ref Color> m_aLabelColors = {};
	protected ref array<float> m_aLabelSizes = {};

	//------------------------------------------------------------------------------------------------
	bool Init(Widget phoneRoot)
	{
		m_wRoot = phoneRoot.FindAnyWidget("HiCommApp");
		m_wBackground = ImageWidget.Cast(phoneRoot.FindAnyWidget("HiCommBackground"));
		m_wMapCanvas = CanvasWidget.Cast(phoneRoot.FindAnyWidget("HiCommMapCanvas"));
		m_wMapClip = phoneRoot.FindAnyWidget("HiCommMapClip");
		m_wGameMapRoot = phoneRoot.FindAnyWidget("HiCommGameMap");
		m_wMapOverlay = CanvasWidget.Cast(phoneRoot.FindAnyWidget("HiCommMapOverlay"));
		m_wMapButton = phoneRoot.FindAnyWidget("HiCommMapButton");
		if (!m_wRoot || !m_wBackground || !m_wMapCanvas || !m_wMapClip || !m_wMapOverlay || !m_wMapButton)
			return false;

		// The layout's old rename field is no longer used (the name is chosen at campaign setup)
		Widget nameEdit = phoneRoot.FindAnyWidget("HiCommNameEdit");
		if (nameEdit)
			nameEdit.SetVisible(false);

		m_ClickHandler = new AROR_HiCommClickHandler(this);
		m_wMapButton.AddHandler(m_ClickHandler);
		m_wMapButton.SetColor(AROR_HiCommMap.Srgb(0, 0, 0, 0));
		m_wBackground.SetColor(m_Background);
		m_Map = new AROR_HiCommMap(m_wMapCanvas, m_wMapOverlay);

		for (int i = 0; i < PANEL_POOL; i++)
		{
			ImageWidget panel = ImageWidget.Cast(phoneRoot.FindAnyWidget("HiCommPanel" + i));
			if (!panel)
				return false;

			m_aPanels.Insert(panel);
			m_aPanelCaches.Insert(new AROR_HiCommWidgetCache());
		}

		for (int i = 0; i < TEXT_POOL; i++)
		{
			RichTextWidget text = RichTextWidget.Cast(phoneRoot.FindAnyWidget("HiCommText" + i));
			if (!text)
				return false;

			m_aTexts.Insert(text);
			m_aTextCache.Insert(string.Empty);
			m_aTextCaches.Insert(new AROR_HiCommWidgetCache());
		}

		for (int i = 0; i < BUTTON_POOL; i++)
		{
			ImageWidget background = ImageWidget.Cast(phoneRoot.FindAnyWidget("HiCommBtnBg" + i));
			RichTextWidget label = RichTextWidget.Cast(phoneRoot.FindAnyWidget("HiCommBtnLabel" + i));
			Widget button = phoneRoot.FindAnyWidget(BUTTON_PREFIX + i);
			if (!background || !label || !button)
				return false;

			button.AddHandler(m_ClickHandler);
			button.SetColor(AROR_HiCommMap.Srgb(0, 0, 0, 0));
			m_aButtonBackgrounds.Insert(background);
			m_aButtonLabels.Insert(label);
			m_aButtonLabelCache.Insert(string.Empty);
			m_aButtonBgCaches.Insert(new AROR_HiCommWidgetCache());
			m_aButtonLabelCaches.Insert(new AROR_HiCommWidgetCache());
			m_aButtonCaches.Insert(new AROR_HiCommWidgetCache());
			m_aButtons.Insert(button);
			m_aButtonActions.Insert(string.Empty);
		}

		for (int i = 0; i < MAP_LABEL_POOL; i++)
		{
			RichTextWidget mapLabel = RichTextWidget.Cast(phoneRoot.FindAnyWidget("HiCommMapLabel" + i));
			if (!mapLabel)
				return false;

			m_aMapLabels.Insert(mapLabel);
			m_aMapLabelCache.Insert(string.Empty);
			m_aMapLabelCaches.Insert(new AROR_HiCommWidgetCache());
		}

		for (int i = 0; i < ICON_POOL; i++)
		{
			ImageWidget mapIcon = ImageWidget.Cast(phoneRoot.FindAnyWidget("HiCommMapIcon" + i));
			if (!mapIcon)
				return false;

			m_aMapIcons.Insert(mapIcon);
			m_aMapIconCache.Insert(string.Empty);
			m_aMapIconCaches.Insert(new AROR_HiCommWidgetCache());
		}

		SetVisible(false, 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void SetVisible(bool visible, float opacity)
	{
		if (!visible)
			CloseGameMap();

		m_wRoot.SetVisible(visible);
		m_wRoot.SetOpacity(opacity);
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while shown: left/top is the landscape screen's corner, scale the phone's current scale
	void Layout(float left, float top, float scale)
	{
		// The app and its background fill the whole screen; the content is inset from the edges
		m_fScale = scale * CONTENT_SCALE;
		m_fInsetX = WIDTH * (scale - m_fScale) * 0.5;
		m_fInsetY = HEIGHT * (scale - m_fScale) * 0.5;
		PlaceRaw(m_wRoot, left, top, WIDTH * scale, HEIGHT * scale);
		PlaceRaw(m_wBackground, 0, 0, WIDTH * scale, HEIGHT * scale);

		m_iPanelCount = 0;
		m_iTextCount = 0;
		m_iButtonCount = 0;
		m_iMapLabelCount = 0;
		m_iMapIconCount = 0;

		// Losing clearance (revoked, host changed) drops back to the login screen
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (m_eScreen != AROR_EHiCommScreen.LOGIN && (!highCommand || !highCommand.IsLeader(GetLocalPlayerId())))
			m_eScreen = AROR_EHiCommScreen.LOGIN;

		bool showMap = m_eScreen == AROR_EHiCommScreen.STRATEGIC;

		// The login sequence replays each time the login screen is shown
		if (m_eScreen == AROR_EHiCommScreen.LOGIN && m_eLastScreen != AROR_EHiCommScreen.LOGIN)
			m_iLoginStart = System.GetTickCount();

		m_eLastScreen = m_eScreen;

		if (m_eScreen == AROR_EHiCommScreen.LOGIN)
		{
			DrawLogin(highCommand);
		}
		else
		{
			DrawTopBar();
			if (m_eScreen == AROR_EHiCommScreen.COMMAND)
				DrawCommand(highCommand);
			else if (m_eScreen == AROR_EHiCommScreen.STRATEGIC)
				DrawStrategic(highCommand);
			else
				DrawUnits(highCommand);
		}

		if (!showMap)
			CloseGameMap();

		bool gameMapShown = showMap && m_Map.UsesGameMap();
		if (m_wGameMapRoot)
			m_wGameMapRoot.SetVisible(showMap && m_bGameMapOpen);

		m_wMapClip.SetVisible(showMap && !gameMapShown);
		m_wMapCanvas.SetVisible(showMap);
		m_wMapOverlay.SetVisible(showMap || m_eScreen == AROR_EHiCommScreen.LOGIN);
		m_wMapButton.SetVisible(showMap);
		if (!showMap)
		{
			m_bLeftDown = false;
			m_bRightDown = false;
			m_bDragging = false;
		}
		HideUnused();
	}

	//------------------------------------------------------------------------------------------------
	// Screens
	//------------------------------------------------------------------------------------------------
	protected void DrawLogin(AROR_HighCommandComponent highCommand)
	{
		int now = System.GetTickCount();
		float elapsed = (now - m_iLoginStart) / 1000.0;
		int localId = GetLocalPlayerId();
		string operatorName = Upper(GetPlayerName(localId));

		// Header strip: identity on the left, uplink status on the right
		Text(40, 22, 400, 52, 40, "HICOMM", m_Green, true);
		Text(42, 72, 600, 24, 16, "GLOBAL RESISTANCE NETWORK  //  SECURE TERMINAL", m_GreenDim);
		TextRight(1660, 30, 16, "NODE EVR-01   //   UPLINK AES-256   //   " + GetClockText(), m_GreenDim);
		TextRight(1660, 56, 16, string.Format("LATENCY %1 MS   //   RELAYS 9 ONLINE", 38 + Math.Mod(now / 700, 9)), m_GreenFaint);
		Panel(40, 104, 1620, 1, m_Border);

		// Left: the rotating relay network
		float globeLeft = 40;
		float globeTop = 120;
		float globeSize = 740;
		Place(m_wMapOverlay, globeLeft * m_fScale, globeTop * m_fScale, globeSize * m_fScale, globeSize * m_fScale);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float pixels = workspace.DPIScale(globeSize * m_fScale);
		m_Globe.Draw(m_wMapOverlay, pixels, pixels, elapsed, workspace.DPIScale(m_fScale));

		// Right: connection log typed out line by line, then the login
		Box(860, 140, 800, 700);
		Text(890, 160, 740, 30, 20, "// SECURE UPLINK", m_GreenDim, true);

		array<string> lines = {};
		array<ref Color> colors = {};
		AddLogLine(lines, colors, "> HICOMM 2.6 // resistance command network", m_GreenDim);
		AddLogLine(lines, colors, "> locating relay node ......... EVR-01 [EVERON]", m_GreenDim);
		AddLogLine(lines, colors, "> negotiating uplink .......... AES-256 OK", m_GreenDim);
		AddLogLine(lines, colors, "> routing via 9 relays ........ OK", m_GreenDim);
		AddLogLine(lines, colors, "> verifying operator .......... " + operatorName, m_Green);

		bool leader = highCommand && highCommand.IsLeader(localId);
		if (!highCommand)
			AddLogLine(lines, colors, "> network ...................... OFFLINE", m_Red);
		else if (highCommand.IsHost(localId))
			AddLogLine(lines, colors, "> clearance .................... RESISTANCE LEADER (HOST)", m_Amber);
		else if (leader)
			AddLogLine(lines, colors, "> clearance .................... RESISTANCE LEADER", m_Amber);
		else
			AddLogLine(lines, colors, "> clearance .................... DENIED", m_Red);

		if (leader)
			AddLogLine(lines, colors, "> secure channel established. welcome, " + operatorName + ".", m_Green);
		else if (highCommand)
			AddLogLine(lines, colors, "> only a Resistance Leader may log in. ask the host to delegate you.", m_Red);

		// Type the log: each line starts once the previous one has finished
		float charactersPerSecond = 70;
		float lineStart = 0.3;
		bool typing = false;
		foreach (int i, string line : lines)
		{
			float shown = (elapsed - lineStart) * charactersPerSecond;
			if (shown <= 0)
			{
				typing = true;
				break;
			}

			string visible = line;
			if (shown < line.Length())
			{
				int count = Math.Floor(shown);
				visible = line.Substring(0, count);
				typing = true;
			}

			// Blinking cursor on the line being typed
			if (visible.Length() < line.Length() && Math.Mod(now / 400, 2) == 0)
				visible += "_";

			Text(890, 210 + i * 40, 740, 34, 18, visible, colors[i]);
			if (visible.Length() < line.Length())
				break;

			lineStart += line.Length() / charactersPerSecond + 0.18;
		}

		if (typing)
		{
			Text(890, 740, 740, 30, 16, "ESTABLISHING CONNECTION...", m_GreenFaint);
			return;
		}

		if (!highCommand)
			return;

		if (leader)
			Button(890, 740, 740, 74, "LOG IN", "login", STYLE_PRIMARY, 30);
		else
			Button(890, 740, 740, 74, "ACCESS DENIED", string.Empty, STYLE_DISABLED, 30);

		Text(40, 872, 900, 22, 14, "RESISTANCE  //  " + Upper(highCommand.GetResistanceName()), m_GreenFaint);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddLogLine(array<string> lines, array<ref Color> colors, string line, Color color)
	{
		lines.Insert(line);
		colors.Insert(color);
	}

	//------------------------------------------------------------------------------------------------
	//! In-game time of day, HH:MM
	protected static string GetClockText()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world || !world.GetTimeAndWeatherManager())
			return "--:--";

		int hours, minutes, seconds;
		world.GetTimeAndWeatherManager().GetHoursMinutesSeconds(hours, minutes, seconds);
		return hours.ToString(2) + ":" + minutes.ToString(2);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTopBar()
	{
		Text(30, 22, 200, 60, 48, "HICOMM", m_Green, true);

		TabButton(240, "COMMAND", AROR_EHiCommScreen.COMMAND, 190);
		TabButton(440, "STRATEGIC COMMAND", AROR_EHiCommScreen.STRATEGIC, 320);
		TabButton(770, "UNIT MANAGEMENT", AROR_EHiCommScreen.UNITS, 290);

		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		string funds = "$ -";
		string manpower = "-";
		if (economy)
		{
			funds = AROR_EconomyComponent.FormatMoney(economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE));
			manpower = economy.GetManpower().ToString();
		}

		TextRight(1675, 24, 22, "CR " + funds + "    MP " + manpower, m_Amber);
		Panel(30, 92, 1645, 2, m_Border);
	}

	//------------------------------------------------------------------------------------------------
	protected void TabButton(float x, string label, AROR_EHiCommScreen screen, float width)
	{
		int style = STYLE_NORMAL;
		if (m_eScreen == screen)
			style = STYLE_SELECTED;

		Button(x, 24, width, 54, label, "screen:" + screen, style, 22);
	}

	//------------------------------------------------------------------------------------------------
	//! Resistance identity and leaders
	protected void DrawCommand(AROR_HighCommandComponent highCommand)
	{
		int localId = GetLocalPlayerId();
		bool isHost = highCommand.IsHost(localId);

		// Left: the resistance (its name is chosen at campaign setup)
		Box(30, 110, 790, 770);
		Header(55, 125, "// THE RESISTANCE");
		Text(55, 175, 740, 70, 52, Upper(highCommand.GetResistanceName()), m_Amber, true);

		// Campaign summary
		int held, total;
		CountSites(held, total);
		Panel(55, 410, 740, 2, m_Border);
		Stat(55, 430, "SITES HELD", string.Format("%1 / %2", held, total));
		array<AROR_HighCommandSquadInfo> squads = {};
		int squadCount = highCommand.GetSquads(squads);
		Stat(55, 480, "SQUADS IN THE FIELD", squadCount.ToString());
		Stat(55, 530, "YOUR SQUADS", string.Format("%1 / %2", highCommand.CountSquadsOwnedBy(localId), highCommand.GetMaxSquadsPerLeader()));

		string clearance = "RESISTANCE LEADER";
		if (isHost)
			clearance = "RESISTANCE LEADER (HOST)";

		Stat(55, 580, "YOUR CLEARANCE", clearance);
		Text(55, 650, 740, 120, 20, "Strategic Command gives orders to your squads. Unit Management raises new squads with Resistance funds and manpower. Squads are refunded in full when the campaign ends.", m_GreenFaint);
		Button(55, 800, 240, 60, "LOG OUT", "logout", STYLE_NORMAL, 22);

		// Right: leaders
		Box(850, 110, 825, 770);
		Header(875, 125, "// RESISTANCE LEADERS");
		if (isHost)
			Text(875, 165, 780, 34, 20, "Delegate players to give them their own HICOMM command.", m_GreenDim);
		else
			Text(875, 165, 780, 34, 20, "The host appoints Resistance Leaders.", m_GreenDim);

		Text(875, 215, 300, 30, 20, "PLAYER", m_GreenFaint);
		Text(1180, 215, 200, 30, 20, "ROLE", m_GreenFaint);
		Text(1340, 215, 120, 30, 20, "SQUADS", m_GreenFaint);

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		float y = 255;
		foreach (int playerId : playerIds)
		{
			if (y > 820)
				break;

			Color rowColor = m_GreenDim;
			string role = "-";
			if (highCommand.IsHost(playerId))
			{
				role = "HOST";
				rowColor = m_Amber;
			}
			else if (highCommand.IsDelegate(playerId))
			{
				role = "LEADER";
				rowColor = m_Green;
			}

			string name = Upper(GetPlayerName(playerId));
			if (playerId == localId)
				name += " (YOU)";

			Text(875, y + 8, 300, 36, 22, name, rowColor);
			Text(1180, y + 8, 160, 36, 22, role, rowColor);
			Text(1340, y + 8, 100, 36, 22, highCommand.CountSquadsOwnedBy(playerId).ToString(), rowColor);

			if (isHost && !highCommand.IsHost(playerId))
			{
				if (highCommand.IsDelegate(playerId))
					Button(1460, y, 190, 48, "REVOKE", string.Format("leader:%1:0", playerId), STYLE_DANGER, 20);
				else
					Button(1460, y, 190, 48, "DELEGATE", string.Format("leader:%1:1", playerId), STYLE_NORMAL, 20);
			}

			Panel(875, y + 54, 775, 1, m_GreenFaint);
			y += 60;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Raise squads; the order of battle with strengths and orders
	protected void DrawUnits(AROR_HighCommandComponent highCommand)
	{
		int localId = GetLocalPlayerId();
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		int funds, manpower;
		if (economy)
		{
			funds = economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE);
			manpower = economy.GetManpower();
		}

		int ownedCount = highCommand.CountSquadsOwnedBy(localId);
		int maximum = highCommand.GetMaxSquadsPerLeader();

		// Left: catalogue
		Box(30, 110, 790, 770);
		Header(55, 125, "// RAISE SQUADS");
		Text(55, 165, 740, 34, 20, "Squads form up at the HQ. Cost: Resistance funds AND manpower.", m_GreenDim);

		array<AROR_HighCommandSquadType> types = {};
		highCommand.GetSquadTypes(types);
		float y = 215;
		foreach (int index, AROR_HighCommandSquadType type : types)
		{
			if (y > 700)
				break;

			Panel(55, y, 740, 110, m_Border);
			Panel(57, y + 2, 736, 106, m_PanelFill);
			Text(80, y + 14, 420, 44, 32, Upper(type.m_sName), m_Green, true);
			Text(80, y + 62, 420, 30, 20, string.Format("%1 MEN  //  CALLSIGN %2", type.GetSize(), type.m_sCallsign), m_GreenDim);

			bool affordable = funds >= type.m_iFundsCost && manpower >= type.m_iManpowerCost;
			Color costColor = m_Amber;
			if (!affordable)
				costColor = m_Red;

			Text(470, y + 20, 160, 30, 22, AROR_EconomyComponent.FormatMoney(type.m_iFundsCost), costColor);
			Text(470, y + 56, 160, 30, 22, type.m_iManpowerCost.ToString() + " MP", costColor);

			if (affordable && ownedCount < maximum)
				Button(620, y + 25, 155, 60, "RAISE", "raise:" + index, STYLE_PRIMARY, 24);
			else
				Button(620, y + 25, 155, 60, "RAISE", string.Empty, STYLE_DISABLED, 24);

			y += 125;
		}

		if (types.IsEmpty())
			Text(55, 220, 740, 60, 22, "No squad types are configured on AROR_HighCommandComponent.", m_Red);

		Panel(55, 790, 740, 1, m_Border);
		Text(55, 805, 740, 30, 22, string.Format("AVAILABLE  %1  //  %2 MP", AROR_EconomyComponent.FormatMoney(funds), manpower), m_Amber);
		Text(55, 838, 740, 30, 20, string.Format("YOUR SQUADS  %1 / %2", ownedCount, maximum), m_GreenDim);

		// Right: order of battle
		Box(850, 110, 825, 770);
		Header(875, 125, "// ORDER OF BATTLE");
		Text(875, 170, 200, 30, 18, "SQUAD", m_GreenFaint);
		Text(1035, 170, 120, 30, 18, "MEN", m_GreenFaint);
		Text(1130, 170, 300, 30, 18, "ORDER", m_GreenFaint);
		Text(1440, 170, 220, 30, 18, "COMMANDER", m_GreenFaint);

		array<AROR_HighCommandSquadInfo> squads = {};
		highCommand.GetSquads(squads);
		y = 205;
		for (int pass = 0; pass < 2; pass++)
		{
			foreach (AROR_HighCommandSquadInfo squad : squads)
			{
				bool own = highCommand.CanCommand(localId, squad.m_iOwnerId);
				if ((pass == 0) != own || y > 830)
					continue;

				Color rowColor = m_GreenFaint;
				if (own)
					rowColor = m_Green;

				Text(875, y, 160, 32, 22, squad.m_sCallsign, rowColor, own);
				Text(1035, y, 100, 32, 22, string.Format("%1/%2", squad.m_iAlive, squad.m_iSize), rowColor);
				Text(1130, y, 310, 32, 22, LongOrder(squad), rowColor);
				Text(1440, y, 220, 32, 22, OwnerTag(squad.m_iOwnerId), rowColor);
				Panel(875, y + 38, 775, 1, m_GreenFaint);
				y += 46;
			}
		}

		if (squads.IsEmpty())
			Text(875, 210, 780, 40, 22, "No squads in the field.", m_GreenDim);
	}

	//------------------------------------------------------------------------------------------------
	// Strategic Command
	//------------------------------------------------------------------------------------------------
	//! Orders only: the map fills the screen; the order of battle and the orders panel float over it.
	//! The orders panel only appears while squads are selected.
	protected void DrawStrategic(AROR_HighCommandComponent highCommand)
	{
		int localId = GetLocalPlayerId();
		array<AROR_HighCommandSquadInfo> squads = {};
		highCommand.GetSquads(squads);
		PruneSelection(highCommand);

		m_fMapX = MAP_LEFT;
		m_fMapY = MAP_TOP;
		m_fMapWidth = MAP_WIDTH;
		m_fMapHeight = MAP_HEIGHT;
		Panel(m_fMapX - 2, m_fMapY - 2, m_fMapWidth + 4, m_fMapHeight + 4, m_Border);
		Place(m_wMapClip, m_fMapX * m_fScale, m_fMapY * m_fScale, m_fMapWidth * m_fScale, m_fMapHeight * m_fScale);
		Place(m_wMapOverlay, m_fMapX * m_fScale, m_fMapY * m_fScale, m_fMapWidth * m_fScale, m_fMapHeight * m_fScale);
		Place(m_wMapButton, m_fMapX * m_fScale, m_fMapY * m_fScale, m_fMapWidth * m_fScale, m_fMapHeight * m_fScale);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		vector min, max;
		highCommand.GetMapBounds(min, max);
		m_Map.SetView(min, max, workspace.DPIScale(m_fMapWidth * m_fScale), workspace.DPIScale(m_fMapHeight * m_fScale));

		LayoutPanels(highCommand, squads, localId);
		HandleMapInput();
		m_Map.UpdateBase();

		// The base layer is drawn with a margin and slides inside the clipping frame as the view pans
		// Terrain view: the real game map under the overlay (the procedural map shows until it is ready)
		if (m_bUseGameMap && m_wGameMapRoot)
		{
			Place(m_wGameMapRoot, m_fMapX * m_fScale, m_fMapY * m_fScale, m_fMapWidth * m_fScale, m_fMapHeight * m_fScale);
			OpenGameMap();
		}
		else
		{
			CloseGameMap();
		}

		float baseX, baseY, baseWidth, baseHeight;
		m_Map.GetBaseRect(baseX, baseY, baseWidth, baseHeight);
		PlaceRaw(m_wMapCanvas, workspace.DPIUnscale(baseX), workspace.DPIUnscale(baseY), workspace.DPIUnscale(baseWidth), workspace.DPIUnscale(baseHeight));
		DrawMapOverlay(highCommand, squads, localId);
		DrawMapLabels();
		DrawMapHud(highCommand);

		if (m_bShowOrderOfBattle)
			DrawOrderOfBattle(highCommand, squads, localId);
		else
			Button(m_fMapX + PANEL_MARGIN, m_fMapY + PANEL_MARGIN, 210, 40, "ORDER OF BATTLE  +", "oob", STYLE_NORMAL, 15);

		if (!m_aSelected.IsEmpty())
			DrawSelection(highCommand, localId);
		else if (m_bShowHelp)
			DrawHelp();
	}

	//------------------------------------------------------------------------------------------------
	//! Where the floating panels go this frame (app units); the map hides labels and ignores clicks under them
	protected void LayoutPanels(AROR_HighCommandComponent highCommand, array<AROR_HighCommandSquadInfo> squads, int localId)
	{
		m_aPanelRects.Clear();
		float mapRight = m_fMapX + m_fMapWidth;
		float mapBottom = m_fMapY + m_fMapHeight;

		m_fOobBottom = m_fMapY + PANEL_MARGIN + 40;
		if (m_bShowOrderOfBattle)
		{
			int rows = Math.Max(squads.Count(), 1);
			m_fOobBottom = Math.Min(m_fMapY + PANEL_MARGIN + 52 + rows * 50 + 96, mapBottom - 120);
			AddPanelRect(m_fMapX + PANEL_MARGIN, m_fMapY + PANEL_MARGIN, OOB_WIDTH, m_fOobBottom - m_fMapY - PANEL_MARGIN);
		}

		m_fRightEdge = mapRight - PANEL_MARGIN;
		if (!m_aSelected.IsEmpty())
		{
			float height = SELECTION_HEIGHT;
			if (CountCommandable(highCommand, localId) == 0)
				height = 230;

			AddPanelRect(mapRight - PANEL_MARGIN - SELECTION_WIDTH, m_fMapY + PANEL_MARGIN, SELECTION_WIDTH, height);
			m_fRightEdge = mapRight - PANEL_MARGIN * 2 - SELECTION_WIDTH;
		}
		else if (m_bShowHelp)
		{
			AddPanelRect(mapRight - PANEL_MARGIN - HELP_WIDTH - 56, m_fMapY + PANEL_MARGIN + 52, HELP_WIDTH, 300);
		}

		// Zoom slider, on its own small panel below the map buttons
		m_fSliderX = m_fRightEdge - 34;
		m_fSliderTop = m_fMapY + PANEL_MARGIN + 110;
		AddPanelRect(m_fSliderX - 10, m_fSliderTop - 56, 48, SLIDER_HEIGHT + 112);

		// The legend sits right of the order of battle when it reaches down that far
		m_fLegendLeft = m_fMapX + PANEL_MARGIN;
		if (m_bShowOrderOfBattle && m_fOobBottom > mapBottom - 120)
			m_fLegendLeft = m_fMapX + PANEL_MARGIN * 2 + OOB_WIDTH;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPanelRect(float x, float y, float width, float height)
	{
		m_aPanelRects.Insert(x);
		m_aPanelRects.Insert(y);
		m_aPanelRects.Insert(width);
		m_aPanelRects.Insert(height);
	}

	//------------------------------------------------------------------------------------------------
	//! True if a map canvas point lies under a floating panel
	protected bool IsUnderPanel(vector canvasPoint)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float x = m_fMapX + workspace.DPIUnscale(canvasPoint[0]) / m_fScale;
		float y = m_fMapY + workspace.DPIUnscale(canvasPoint[1]) / m_fScale;
		for (int i = 0; i < m_aPanelRects.Count(); i += 4)
		{
			if (x >= m_aPanelRects[i] && x <= m_aPanelRects[i] + m_aPanelRects[i + 2] && y >= m_aPanelRects[i + 1] && y <= m_aPanelRects[i + 1] + m_aPanelRects[i + 3])
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! App units -> map canvas pixels
	protected vector AppToCanvas(float x, float y)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		return Vector(workspace.DPIScale((x - m_fMapX) * m_fScale), workspace.DPIScale((y - m_fMapY) * m_fScale), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Floating order of battle: every squad, own first (click selects, shift adds)
	protected void DrawOrderOfBattle(AROR_HighCommandComponent highCommand, array<AROR_HighCommandSquadInfo> squads, int localId)
	{
		float left = m_fMapX + PANEL_MARGIN;
		float top = m_fMapY + PANEL_MARGIN;
		float inner = OOB_WIDTH - 24;
		Text(left + 12, top + 12, OOB_WIDTH - 70, 26, 17, "// ORDER OF BATTLE", m_GreenDim, true);
		Button(left + OOB_WIDTH - 48, top + 8, 38, 32, "-", "oob", STYLE_NORMAL, 20);

		float y = top + 52;
		float last = m_fOobBottom - 96;
		for (int pass = 0; pass < 2; pass++)
		{
			foreach (AROR_HighCommandSquadInfo squad : squads)
			{
				bool own = highCommand.CanCommand(localId, squad.m_iOwnerId);
				if ((pass == 0) != own || y + 44 > last)
					continue;

				int style = STYLE_MUTED;
				if (m_aSelected.Contains(squad.m_iId))
					style = STYLE_SELECTED;
				else if (own)
					style = STYLE_NORMAL;

				string detail = ShortOrder(squad);
				if (!own)
					detail = OwnerTag(squad.m_iOwnerId);

				int maxDetail = 24;
				if (detail.Length() > maxDetail)
					detail = detail.Substring(0, maxDetail - 2) + "..";

				string label = string.Format("%1   %2/%3", squad.m_sCallsign, squad.m_iAlive, squad.m_iSize) + "\n" + detail;
				Button(left + 12, y, inner, 46, label, "squad:" + squad.m_iId, style, 16, false);
				y += 52;
			}
		}

		if (squads.IsEmpty())
			Text(left + 12, top + 54, inner, 48, 15, "No squads yet. Raise one in Unit Management.", m_GreenDim);

		float half = (inner - 8) * 0.5;
		int allStyle = STYLE_NORMAL;
		if (AllOwnSelected(highCommand, squads, localId))
			allStyle = STYLE_SELECTED;

		int noneStyle = STYLE_NORMAL;
		if (m_aSelected.IsEmpty())
			noneStyle = STYLE_SELECTED;

		Button(left + 12, m_fOobBottom - 88, half, 36, "ALL MINE", "selall", allStyle, 14);
		Button(left + 20 + half, m_fOobBottom - 88, half, 36, "NONE", "selnone", noneStyle, 14);

		int held, total;
		CountSites(held, total);
		Text(left + 12, m_fOobBottom - 42, inner, 26, 14, string.Format("SITES HELD  %1 / %2", held, total), m_GreenDim);
	}

	//------------------------------------------------------------------------------------------------
	//! Floating orders panel for the selection: who, order buttons, target, execute
	protected void DrawSelection(AROR_HighCommandComponent highCommand, int localId)
	{
		float left = m_fMapX + m_fMapWidth - PANEL_MARGIN - SELECTION_WIDTH;
		float top = m_fMapY + PANEL_MARGIN;
		float x = left + 16;
		float width = SELECTION_WIDTH - 32;

		Button(left + SELECTION_WIDTH - 48, top + 10, 36, 34, "X", "selnone", STYLE_NORMAL, 16);

		int commandable = CountCommandable(highCommand, localId);
		if (m_aSelected.Count() == 1)
		{
			AROR_HighCommandSquadInfo squad = highCommand.GetSquadInfo(m_aSelected[0]);
			AROR_HighCommandSquadType type = highCommand.GetSquadType(squad.m_iType);
			string typeName = "SQUAD";
			if (type)
				typeName = Upper(type.m_sName);

			Text(x, top + 10, width - 44, 46, 36, squad.m_sCallsign, m_Amber, true);
			Text(x, top + 60, width, 24, 17, string.Format("%1  %2/%3", typeName, squad.m_iAlive, squad.m_iSize), m_Green);
			Text(x, top + 86, width, 24, 16, LongOrder(squad), m_GreenDim);
			Text(x, top + 110, width, 24, 16, "CMDR " + OwnerTag(squad.m_iOwnerId), m_GreenDim);
		}
		else
		{
			string callsigns;
			foreach (int i, int squadId : m_aSelected)
			{
				AROR_HighCommandSquadInfo selected = highCommand.GetSquadInfo(squadId);
				if (!selected)
					continue;

				if (i > 0)
					callsigns += "  ";

				callsigns += selected.m_sCallsign;
			}

			Text(x, top + 10, width - 44, 46, 34, string.Format("%1 SQUADS", m_aSelected.Count()), m_Amber, true);
			Text(x, top + 60, width, 72, 16, callsigns, m_Green);
		}

		Panel(x, top + 142, width, 1, m_Border);

		if (commandable == 0)
		{
			Text(x, top + 156, width, 90, 17, "VIEW ONLY: none of the selected squads are under your command.", m_Red);
			return;
		}

		// Move / Attack / Defend need a target; Hold stands the squads by where they are, at once
		float half = (width - 10) * 0.5;
		OrderButton(x, top + 156, half, "MOVE", AROR_EHighCommandOrder.MOVE);
		OrderButton(x + half + 10, top + 156, half, "ATTACK", AROR_EHighCommandOrder.ATTACK);
		OrderButton(x, top + 204, half, "DEFEND", AROR_EHighCommandOrder.DEFEND);
		int holdStyle = STYLE_NORMAL;
		if (AllSelectedHolding(highCommand, localId))
			holdStyle = STYLE_SELECTED;

		Button(x + half + 10, top + 204, half, 42, "HOLD", "hold", holdStyle, 18);

		// Target: what the chosen order will act on
		string targetText;
		if (m_eSelectedOrder == AROR_EHighCommandOrder.MOVE)
		{
			if (m_bHasMoveTarget)
			{
				targetText = "GRID " + AROR_HighCommandComponent.GridReference(m_vMoveTarget);
				Text(x, top + 262, width, 32, 24, targetText, m_Amber, true);
				Text(x, top + 294, width, 24, 15, "Move point. Left-click to change.", m_GreenDim);
			}
			else
			{
				Text(x, top + 262, width, 56, 16, "Right-click to move there now, or left-click to plan it.", m_GreenDim);
			}
		}
		else
		{
			AROR_LocationComponent target = FindLocation(m_sSelectedTarget);
			if (target)
			{
				targetText = Upper(target.GetName());
				Text(x, top + 262, width, 32, 22, targetText, LabelColor(target.GetControllingRole()), true);
				Text(x, top + 294, width, 24, 15, string.Format("%1  //  %2", TypeName(target.GetLocationType()), RoleName(target.GetControllingRole())), m_GreenDim);
			}
			else
			{
				Text(x, top + 262, width, 56, 16, "Right-click near a location to order now, or left-click to plan it.", m_GreenDim);
			}
		}

		string problem = GetPlannedOrderProblem(highCommand);
		string who = string.Format("%1 SQUADS", commandable);
		if (m_aSelected.Count() == 1)
			who = highCommand.GetSquadInfo(m_aSelected[0]).m_sCallsign;

		if (problem.IsEmpty())
			Text(x, top + 330, width, 26, 16, string.Format("> %1 %2 %3", who, ShortVerb(m_eSelectedOrder), targetText), m_Green);

		Button(x, top + 366, 92, 52, "CLEAR", "clear", STYLE_NORMAL, 18);
		if (problem.IsEmpty())
			Button(x + 102, top + 366, width - 102, 52, "EXECUTE", "execute", STYLE_PRIMARY, 24);
		else
			Button(x + 102, top + 366, width - 102, 52, "EXECUTE", string.Empty, STYLE_DISABLED, 24);
	}

	//------------------------------------------------------------------------------------------------
	//! Controls cheat sheet, toggled with the ? button
	protected void DrawHelp()
	{
		float left = m_fMapX + m_fMapWidth - PANEL_MARGIN - HELP_WIDTH - 56;	// left of the zoom slider
		float top = m_fMapY + PANEL_MARGIN + 52;
		Header(left + 12, top + 10, "// CONTROLS");
		Text(left + 12, top + 44, HELP_WIDTH - 24, 250, 14, "LMB  select squad / set target\nDRAG  box-select squads\nSHIFT  add to selection\nRMB location  attack / defend\nRMB ground  move there\nHOLD  stand by in place\nRMB DRAG / WASD  pan\nWHEEL / Q E  zoom\nHOVER  intel", m_GreenDim);
	}

	//------------------------------------------------------------------------------------------------
	//! Why the planned order cannot be executed yet, or empty if it can
	protected string GetPlannedOrderProblem(AROR_HighCommandComponent highCommand)
	{
		if (m_eSelectedOrder == AROR_EHighCommandOrder.MOVE)
		{
			if (!m_bHasMoveTarget)
				return "Pick a point on the map to move to";

			return highCommand.GetMoveProblem(m_vMoveTarget);
		}

		return AROR_HighCommandComponent.GetOrderTargetProblem(m_eSelectedOrder, FindLocation(m_sSelectedTarget));
	}

	//------------------------------------------------------------------------------------------------
	protected void OrderButton(float x, float y, float width, string label, AROR_EHighCommandOrder order)
	{
		int style = STYLE_NORMAL;
		if (m_eSelectedOrder == order)
			style = STYLE_SELECTED;

		Button(x, y, width, 42, label, "order:" + order, style, 18);
	}

	//------------------------------------------------------------------------------------------------
	// Map: the real game map (terrain view)
	//------------------------------------------------------------------------------------------------
	//! Opens the base game's map inside HICOMM with our phosphor config. The view switches over once the map has
	//! initialised (its open event); until then the procedural map stays visible.
	protected void OpenGameMap()
	{
		if (m_bGameMapOpen)
			return;

		SCR_MapEntity gameMap = SCR_MapEntity.GetMapInstance();
		if (!gameMap || gameMap.IsOpen())
			return;

		// Our normal-map location icons would otherwise attach to this map too
		AROR_LocationMapIconsComponent.SetSuppressed(true);
		MapConfiguration config = gameMap.SetupMapConfig(EMapEntityMode.PLAIN, GAME_MAP_CONFIG, m_wGameMapRoot);
		if (!config)
		{
			AROR_LocationMapIconsComponent.SetSuppressed(false);
			Print("[AROR] HICOMM could not set up the game map config; using the tactical map", LogLevel.WARNING);
			m_bUseGameMap = false;
			return;
		}

		SCR_MapEntity.GetOnMapOpen().Insert(OnGameMapOpened);
		gameMap.OpenMap(config);
		m_bGameMapOpen = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnGameMapOpened(MapConfiguration config)
	{
		SCR_MapEntity.GetOnMapOpen().Remove(OnGameMapOpened);
		SCR_MapEntity gameMap = SCR_MapEntity.GetMapInstance();
		if (!m_bGameMapOpen || !gameMap)
			return;

		// Plain base map for now; a phosphor recolour is to be revisited
		if (gameMap.GetMapWidget())
			gameMap.GetMapWidget().SetColor(Color.FromRGBA(255, 255, 255, 255));

		gameMap.ZoomOut();
		m_Map.SetGameMap(gameMap);
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseGameMap()
	{
		if (!m_bGameMapOpen)
			return;

		m_bGameMapOpen = false;
		SCR_MapEntity.GetOnMapOpen().Remove(OnGameMapOpened);
		m_Map.SetGameMap(null);
		SCR_MapEntity gameMap = SCR_MapEntity.GetMapInstance();
		if (gameMap && gameMap.IsOpen())
			gameMap.CloseMap();

		AROR_LocationMapIconsComponent.SetSuppressed(false);
	}

	//------------------------------------------------------------------------------------------------
	//! The phone is closing: release the game map
	void Shutdown()
	{
		CloseGameMap();
	}

	//------------------------------------------------------------------------------------------------
	// Map: input
	//------------------------------------------------------------------------------------------------
	//! Mouse drag (pan / box select), keyboard pan and zoom, hover
	protected void HandleMapInput()
	{
		int now = System.GetTickCount();
		float deltaTime = Math.Clamp((now - m_iLastFrameMs) / 1000.0, 0, 0.1);
		m_iLastFrameMs = now;

		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);
		vector cursor = ToCanvas(mouseX, mouseY);

		// Zoom slider: dragging sets the zoom target continuously (log scale, top = closest)
		if (m_bSliderDrag)
		{
			float appY = m_fMapY + GetGame().GetWorkspace().DPIUnscale(cursor[1]) / m_fScale;
			float position = 1 - Math.Clamp((appY - m_fSliderTop) / SLIDER_HEIGHT, 0, 1);
			float zoom = Math.Pow(m_Map.GetMaxZoomLevel(), position);
			m_Map.SetTargetZoom(zoom, m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);
		}

		m_Map.Animate(deltaTime);
		m_vCursor = cursor;
		m_bCursorInside = cursor[0] >= 0 && cursor[1] >= 0 && cursor[0] <= m_Map.GetWidth() && cursor[1] <= m_Map.GetHeight() && !IsUnderPanel(cursor);

		float moved = Math.Sqrt((mouseX - m_iPressX) * (mouseX - m_iPressX) + (mouseY - m_iPressY) * (mouseY - m_iPressY));
		if ((m_bLeftDown || m_bRightDown) && !m_bDragging && moved > GetGame().GetWorkspace().DPIScale(DRAG_THRESHOLD))
			m_bDragging = true;

		if (m_bRightDown && m_bDragging)
			m_Map.Pan(mouseX - m_iLastMouseX, mouseY - m_iLastMouseY);

		m_iLastMouseX = mouseX;
		m_iLastMouseY = mouseY;

		// Keyboard: arrows/WASD pan, Q/E zoom about the centre
		float step = PAN_SPEED * deltaTime * GetGame().GetWorkspace().DPIScale(1);
		float panX, panY;
		if (Debug.KeyState(KeyCode.KC_LEFT) || Debug.KeyState(KeyCode.KC_A))
			panX += step;

		if (Debug.KeyState(KeyCode.KC_RIGHT) || Debug.KeyState(KeyCode.KC_D))
			panX -= step;

		if (Debug.KeyState(KeyCode.KC_UP) || Debug.KeyState(KeyCode.KC_W))
			panY += step;

		if (Debug.KeyState(KeyCode.KC_DOWN) || Debug.KeyState(KeyCode.KC_S))
			panY -= step;

		if (panX != 0 || panY != 0)
			m_Map.Pan(panX, panY);

		float zoomRate = 1 + deltaTime * 2.5;
		if (Debug.KeyState(KeyCode.KC_E))
			m_Map.ZoomAt(zoomRate, m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);

		if (Debug.KeyState(KeyCode.KC_Q))
			m_Map.ZoomAt(1 / zoomRate, m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);
	}

	//------------------------------------------------------------------------------------------------
	//! Screen pixels -> map canvas pixels
	protected vector ToCanvas(float x, float y)
	{
		float screenX, screenY;
		m_wMapButton.GetScreenPos(screenX, screenY);
		return Vector(x - screenX, y - screenY, 0);
	}

	//------------------------------------------------------------------------------------------------
	bool OnMouseDown(Widget w, int x, int y, int button)
	{
		if (w != m_wMapButton || !m_wRoot.IsVisible())
			return false;

		if (button == 0 && IsOnSlider(ToCanvas(x, y)))
		{
			m_bSliderDrag = true;
			return true;
		}

		// Clicks on a floating panel's background are not map clicks
		if (IsUnderPanel(ToCanvas(x, y)))
			return false;

		m_iPressX = x;
		m_iPressY = y;
		m_iLastMouseX = x;
		m_iLastMouseY = y;
		m_bDragging = false;
		if (button == 0)
			m_bLeftDown = true;
		else
			m_bRightDown = true;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Up can arrive on any HICOMM widget (the cursor may have left the map while dragging)
	bool OnMouseUp(Widget w, int x, int y, int button)
	{
		if (m_bSliderDrag && button == 0)
		{
			m_bSliderDrag = false;
			return true;
		}

		bool wasDragging = m_bDragging;
		bool handled;
		if (button == 0 && m_bLeftDown)
		{
			m_bLeftDown = false;
			m_iLastMapClickMs = System.GetTickCount();
			if (wasDragging)
				BoxSelect(ToCanvas(m_iPressX, m_iPressY), ToCanvas(x, y));
			else
				MapLeftClick(ToCanvas(x, y));

			handled = true;
		}
		else if (button != 0 && m_bRightDown)
		{
			m_bRightDown = false;
			if (!wasDragging && button == 1)
				MapRightClick(ToCanvas(x, y));

			handled = true;
		}

		m_bDragging = false;
		return handled;
	}

	//------------------------------------------------------------------------------------------------
	bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		if (w != m_wMapButton || wheel == 0 || IsUnderPanel(ToCanvas(x, y)))
			return false;

		vector anchor = ToCanvas(x, y);
		if (wheel > 0)
			m_Map.ZoomAt(WHEEL_ZOOM, anchor[0], anchor[1]);
		else
			m_Map.ZoomAt(1 / WHEEL_ZOOM, anchor[0], anchor[1]);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Leaving the map ends a drag (its button-up may never arrive)
	void OnMouseLeave(Widget w)
	{
		if (w != m_wMapButton)
			return;

		m_bRightDown = false;
		if (m_bLeftDown && m_bDragging)
			m_bLeftDown = false;

		m_bDragging = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Left click: a squad selects it (shift adds), a location becomes the target; open ground becomes the Move point
	//! while MOVE is the chosen order, otherwise it clears the selection
	protected void MapLeftClick(vector point)
	{
		bool additive = IsShiftDown();
		int squadId = PickSquad(point);
		if (squadId >= 0)
		{
			if (additive)
				ToggleSelected(squadId);
			else
				SelectOnly(squadId);

			return;
		}

		// With squads selected, a left click plans the chosen order (EXECUTE sends it)
		if (m_aSelected.IsEmpty())
			return;

		if (m_eSelectedOrder == AROR_EHighCommandOrder.MOVE)
		{
			// The exact point, so squads can be sent up to a building or a treeline, not only to locations
			m_vMoveTarget = m_Map.ScreenToWorld(point[0], point[1]);
			m_bHasMoveTarget = true;
			return;
		}

		string locationId = PickLocationFor(point, m_eSelectedOrder);
		if (locationId.IsEmpty())
		{
			AROR_NotificationManagerComponent.ShowHint(NoTargetHint(m_eSelectedOrder));
			return;
		}

		SetTarget(locationId);
	}

	//------------------------------------------------------------------------------------------------
	//! Right click sends the chosen order at once: MOVE to the exact point, ATTACK the nearest enemy-held location,
	//! DEFEND the nearest Resistance-held one. It never silently turns into a different order.
	protected void MapRightClick(vector point)
	{
		if (m_aSelected.IsEmpty())
			return;

		if (m_eSelectedOrder == AROR_EHighCommandOrder.MOVE)
		{
			IssueOrder(AROR_EHighCommandOrder.MOVE, string.Empty, m_Map.ScreenToWorld(point[0], point[1]));
			return;
		}

		string locationId = PickLocationFor(point, m_eSelectedOrder);
		AROR_LocationComponent target = FindLocation(locationId);
		if (!target)
		{
			AROR_NotificationManagerComponent.ShowHint(NoTargetHint(m_eSelectedOrder));
			return;
		}

		SetTarget(locationId);
		IssueOrder(m_eSelectedOrder, locationId, target.GetCenter());
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest location the order can target (attack: not ours; defend: ours), within a generous radius
	protected string PickLocationFor(vector point, AROR_EHighCommandOrder order)
	{
		float best = GetGame().GetWorkspace().DPIScale(ORDER_PICK_RADIUS);
		string picked;
		foreach (int i, vector locationPoint : m_aHitLocationPositions)
		{
			AROR_LocationComponent location = FindLocation(m_aHitLocationIds[i]);
			if (!location)
				continue;

			string problem = AROR_HighCommandComponent.GetOrderTargetProblem(order, location);
			if (!problem.IsEmpty())
				continue;

			float distance = vector.Distance(point, locationPoint);
			if (distance < best)
			{
				best = distance;
				picked = m_aHitLocationIds[i];
			}
		}

		return picked;
	}

	//------------------------------------------------------------------------------------------------
	protected static string NoTargetHint(AROR_EHighCommandOrder order)
	{
		if (order == AROR_EHighCommandOrder.DEFEND)
			return "No Resistance-held location there. Click closer to one, or choose MOVE.";

		return "No enemy-held location there. Click closer to one, or choose MOVE.";
	}

	//------------------------------------------------------------------------------------------------
	protected void BoxSelect(vector from, vector to)
	{
		float left = Math.Min(from[0], to[0]);
		float right = Math.Max(from[0], to[0]);
		float top = Math.Min(from[1], to[1]);
		float bottom = Math.Max(from[1], to[1]);

		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (!highCommand)
			return;

		if (!IsShiftDown())
			m_aSelected.Clear();

		int localId = GetLocalPlayerId();
		foreach (int i, vector point : m_aHitSquadPositions)
		{
			if (point[0] < left || point[0] > right || point[1] < top || point[1] > bottom)
				continue;

			AROR_HighCommandSquadInfo squad = highCommand.GetSquadInfo(m_aHitSquadIds[i]);
			if (squad && highCommand.CanCommand(localId, squad.m_iOwnerId) && !m_aSelected.Contains(squad.m_iId))
				m_aSelected.Insert(squad.m_iId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int PickSquad(vector point)
	{
		float best = GetGame().GetWorkspace().DPIScale(PICK_RADIUS);
		int picked = -1;
		foreach (int i, vector squadPoint : m_aHitSquadPositions)
		{
			float distance = vector.Distance(point, squadPoint);
			if (distance < best)
			{
				best = distance;
				picked = m_aHitSquadIds[i];
			}
		}

		return picked;
	}

	//------------------------------------------------------------------------------------------------
	protected string PickLocation(vector point)
	{
		float best = GetGame().GetWorkspace().DPIScale(PICK_RADIUS * 1.4);
		string picked;
		foreach (int i, vector locationPoint : m_aHitLocationPositions)
		{
			float distance = vector.Distance(point, locationPoint);
			if (distance < best)
			{
				best = distance;
				picked = m_aHitLocationIds[i];
			}
		}

		return picked;
	}

	//------------------------------------------------------------------------------------------------
	//! Location the planned attack/defend order will act on
	protected void SetTarget(string locationId)
	{
		m_sSelectedTarget = locationId;
		m_bHasMoveTarget = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the order for every selected squad under our command; the server re-checks each one.
	//! Attack/defend use locationId; move uses position, spreading several squads around it; hold needs neither.
	protected void IssueOrder(AROR_EHighCommandOrder order, string locationId, vector position)
	{
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!highCommand || !controller)
			return;

		string problem;
		if (order == AROR_EHighCommandOrder.MOVE)
		{
			problem = highCommand.GetMoveProblem(position);
		}
		else if (order != AROR_EHighCommandOrder.HOLD)
		{
			AROR_LocationComponent target = FindLocation(locationId);
			problem = AROR_HighCommandComponent.GetOrderTargetProblem(order, target);
			if (target)
				position = target.GetCenter();
		}

		if (!problem.IsEmpty())
		{
			AROR_NotificationManagerComponent.ShowHint(problem);
			return;
		}

		int localId = GetLocalPlayerId();
		int sent;
		foreach (int squadId : m_aSelected)
		{
			AROR_HighCommandSquadInfo squad = highCommand.GetSquadInfo(squadId);
			if (!squad || !highCommand.CanCommand(localId, squad.m_iOwnerId))
				continue;

			// Several squads moving together take spaced points (a spiral round the target) instead of stacking
			vector destination = position;
			if (order == AROR_EHighCommandOrder.MOVE && sent > 0)
			{
				float angle = sent * 2.4;
				float distance = MOVE_SPACING * Math.Sqrt(sent);
				vector spread = Vector(position[0] + Math.Cos(angle) * distance, 0, position[2] + Math.Sin(angle) * distance);
				string spreadProblem = highCommand.GetMoveProblem(spread);
				if (spreadProblem.IsEmpty())
					destination = spread;
			}

			controller.AROR_RequestHiCommOrder(squadId, order, locationId, destination);
			sent++;
		}

		if (sent == 0)
		{
			AROR_NotificationManagerComponent.ShowHint("None of the selected squads are under your command");
			return;
		}

		// Map acknowledgement ping (hold has no single point)
		if (order != AROR_EHighCommandOrder.HOLD)
		{
			m_vPing = position;
			m_iPingTime = System.GetTickCount();
			m_iPingColor = PAL_GREEN;
			if (order == AROR_EHighCommandOrder.ATTACK)
				m_iPingColor = PAL_RED;
			else if (order == AROR_EHighCommandOrder.MOVE)
				m_iPingColor = PAL_MOVE;
		}

		m_sSelectedTarget = string.Empty;
		m_bHasMoveTarget = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectOnly(int squadId)
	{
		m_aSelected.Clear();
		m_aSelected.Insert(squadId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ToggleSelected(int squadId)
	{
		if (m_aSelected.Contains(squadId))
			m_aSelected.RemoveItem(squadId);
		else
			m_aSelected.Insert(squadId);
	}

	//------------------------------------------------------------------------------------------------
	protected void PruneSelection(AROR_HighCommandComponent highCommand)
	{
		for (int i = m_aSelected.Count() - 1; i >= 0; i--)
		{
			if (!highCommand.GetSquadInfo(m_aSelected[i]))
				m_aSelected.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int CountCommandable(AROR_HighCommandComponent highCommand, int localId)
	{
		int count;
		foreach (int squadId : m_aSelected)
		{
			AROR_HighCommandSquadInfo squad = highCommand.GetSquadInfo(squadId);
			if (squad && highCommand.CanCommand(localId, squad.m_iOwnerId))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Every selected squad under our command is standing by (HOLD shows as active)
	protected bool AllSelectedHolding(AROR_HighCommandComponent highCommand, int localId)
	{
		int holding;
		foreach (int squadId : m_aSelected)
		{
			AROR_HighCommandSquadInfo squad = highCommand.GetSquadInfo(squadId);
			if (!squad || !highCommand.CanCommand(localId, squad.m_iOwnerId))
				continue;

			if (squad.m_eOrder != AROR_EHighCommandOrder.HOLD)
				return false;

			holding++;
		}

		return holding > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The selection is exactly our own squads (ALL MINE shows as active)
	protected bool AllOwnSelected(AROR_HighCommandComponent highCommand, array<AROR_HighCommandSquadInfo> squads, int localId)
	{
		int own;
		foreach (AROR_HighCommandSquadInfo squad : squads)
		{
			if (!highCommand.CanCommand(localId, squad.m_iOwnerId))
				continue;

			if (!m_aSelected.Contains(squad.m_iId))
				return false;

			own++;
		}

		return own > 0 && own == m_aSelected.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsShiftDown()
	{
		return Debug.KeyState(KeyCode.KC_LSHIFT) || Debug.KeyState(KeyCode.KC_RSHIFT);
	}

	//------------------------------------------------------------------------------------------------
	// Map: drawing
	//------------------------------------------------------------------------------------------------
	//! Everything that moves, redrawn every frame on the overlay canvas
	protected void DrawMapOverlay(AROR_HighCommandComponent highCommand, array<AROR_HighCommandSquadInfo> squads, int localId)
	{
		m_aHitSquadIds.Clear();
		m_aHitSquadPositions.Clear();
		m_aHitLocationIds.Clear();
		m_aHitLocationPositions.Clear();
		m_aLabelTexts.Clear();
		m_aLabelPositions.Clear();
		m_aLabelColors.Clear();
		m_aLabelSizes.Clear();
		m_aIconTextures.Clear();
		m_aIconPositions.Clear();
		m_aIconColors.Clear();

		int now = System.GetTickCount();
		float unit = GetGame().GetWorkspace().DPIScale(1);
		float zoom = m_Map.GetZoom();
		float width = m_Map.GetWidth();
		float height = m_Map.GetHeight();
		float markerScale = Math.Min(1 + (zoom - 1) * 0.06, 1.5) * unit;

		m_Map.BeginOverlay();

		// Cursor crosshair
		if (m_bCursorInside)
		{
			Color crosshair = AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 0.12);
			m_Map.DrawLine(Vector(0, m_vCursor[1], 0), Vector(width, m_vCursor[1], 0), crosshair, 1);
			m_Map.DrawLine(Vector(m_vCursor[0], 0, 0), Vector(m_vCursor[0], height, 0), crosshair, 1);
		}

		// The game map draws its own grid and grid labels
		if (!m_Map.UsesGameMap())
			DrawGridLabels(width, height);
		DrawLocations(highCommand, markerScale, zoom, width, height);
		DrawMissions(markerScale, width, height);
		DrawSquads(highCommand, squads, localId, markerScale, zoom, unit);

		// Order acknowledgement ping
		int pingAge = now - m_iPingTime;
		if (m_iPingTime > 0 && pingAge < PING_MS)
		{
			float progress = pingAge / (PING_MS * 1.0);
			vector pingPoint = m_Map.WorldToScreen(m_vPing);
			m_Map.DrawCircle(pingPoint, (8 + 34 * progress) * unit, AROR_HiCommMap.Rgb(m_iPingColor, 1 - progress), false, 2.5, 28);
			m_Map.DrawCircle(pingPoint, (4 + 18 * progress) * unit, AROR_HiCommMap.Rgb(m_iPingColor, (1 - progress) * 0.6), false, 1.5, 20);
		}

		// Box selection
		if (m_bLeftDown && m_bDragging)
		{
			vector from = ToCanvas(m_iPressX, m_iPressY);
			array<float> box = {from[0], from[1], m_vCursor[0], from[1], m_vCursor[0], m_vCursor[1], from[0], m_vCursor[1]};
			m_Map.DrawPolygon(box, AROR_HiCommMap.Rgb(PAL_AMBER, 0.08));
			m_Map.DrawPolyline(box, AROR_HiCommMap.Rgb(PAL_AMBER, 0.9), 1.5, true);
		}

		DrawScaleBar(unit);
		DrawLegend();
		m_Map.DrawFrameCorners(AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 0.6));

		// Floating panel backgrounds, last so they cover the map (their text and buttons are widgets above)
		Color panelFill = AROR_HiCommMap.Srgb(5, 12, 8, 0.94);
		Color panelBorder = AROR_HiCommMap.Rgb(AROR_HiCommMap.GRID, 1);
		for (int i = 0; i < m_aPanelRects.Count(); i += 4)
		{
			vector topLeft = AppToCanvas(m_aPanelRects[i], m_aPanelRects[i + 1]);
			vector bottomRight = AppToCanvas(m_aPanelRects[i] + m_aPanelRects[i + 2], m_aPanelRects[i + 1] + m_aPanelRects[i + 3]);
			array<float> panel = {topLeft[0], topLeft[1], bottomRight[0], topLeft[1], bottomRight[0], bottomRight[1], topLeft[0], bottomRight[1]};
			m_Map.DrawPolygon(panel, panelFill);
			m_Map.DrawPolyline(panel, panelBorder, 1.5, true);
		}

		DrawZoomSlider();
		m_Map.CommitOverlay();
	}

	//------------------------------------------------------------------------------------------------
	//! Continuous zoom slider (log scale): ticks at 1x, 2x, 4x, 8x, a faint marker at the target and the handle
	//! at the current zoom. Drawn over its own small panel.
	protected void DrawZoomSlider()
	{
		float pixel = GetGame().GetWorkspace().DPIScale(m_fScale);
		float trackX = m_fSliderX + 14;
		vector top = AppToCanvas(trackX, m_fSliderTop);
		vector bottom = AppToCanvas(trackX, m_fSliderTop + SLIDER_HEIGHT);
		m_Map.DrawLine(top, bottom, AROR_HiCommMap.Rgb(AROR_HiCommMap.GRID, 1), 3 * pixel);

		array<float> ticks = {1, 2, 4, 8};
		foreach (float tick : ticks)
		{
			vector point = AppToCanvas(trackX, SliderY(tick));
			m_Map.DrawLine(point + Vector(-7 * pixel, 0, 0), point + Vector(7 * pixel, 0, 0), AROR_HiCommMap.Rgb(PAL_GREEN_DIM, 1), 1.5 * pixel);
		}

		vector target = AppToCanvas(trackX, SliderY(m_Map.GetTargetZoom()));
		m_Map.DrawSquare(target, 6 * pixel, AROR_HiCommMap.Rgb(PAL_AMBER, 0.35), false, 1.5 * pixel);

		vector handle = AppToCanvas(trackX, SliderY(m_Map.GetZoom()));
		array<float> knob = {handle[0] - 11 * pixel, handle[1] - 5 * pixel, handle[0] + 11 * pixel, handle[1] - 5 * pixel, handle[0] + 11 * pixel, handle[1] + 5 * pixel, handle[0] - 11 * pixel, handle[1] + 5 * pixel};
		m_Map.DrawPolygon(knob, AROR_HiCommMap.Rgb(PAL_AMBER, 1));
	}

	//------------------------------------------------------------------------------------------------
	//! Slider position (app units) of a zoom level
	protected float SliderY(float zoom)
	{
		float position = Math.Log2(Math.Max(zoom, 1)) / Math.Log2(Math.Max(m_Map.GetMaxZoomLevel(), 1.01));
		return m_fSliderTop + (1 - Math.Clamp(position, 0, 1)) * SLIDER_HEIGHT;
	}

	//------------------------------------------------------------------------------------------------
	//! True if a map canvas point is on the zoom slider (with a little slack around the track)
	protected bool IsOnSlider(vector canvasPoint)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float x = m_fMapX + workspace.DPIUnscale(canvasPoint[0]) / m_fScale;
		float y = m_fMapY + workspace.DPIUnscale(canvasPoint[1]) / m_fScale;
		return x >= m_fSliderX - 10 && x <= m_fSliderX + 38 && y >= m_fSliderTop - 14 && y <= m_fSliderTop + SLIDER_HEIGHT + 14;
	}

	//------------------------------------------------------------------------------------------------
	//! Grid references along the top and left edges, matching the base layer's adaptive spacing
	protected void DrawGridLabels(float width, float height)
	{
		float spacing = m_Map.GetGridSpacing();
		float minX, minZ, maxX, maxZ;
		m_Map.GetVisibleBounds(minX, minZ, maxX, maxZ);
		Color color = AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 0.55);

		for (float x = Math.Ceil(minX / spacing) * spacing; x <= maxX; x += spacing)
		{
			float screenX = m_Map.WorldToScreen(Vector(x, 0, 0))[0];
			if (screenX > 40)
				AddMapLabel(GridReference(x, spacing), Vector(screenX + 3, 4, 0), color, 12);
		}

		for (float z = Math.Ceil(minZ / spacing) * spacing; z <= maxZ; z += spacing)
		{
			float screenY = m_Map.WorldToScreen(Vector(0, 0, z))[1];
			if (screenY > 24 && screenY < height - 20)
				AddMapLabel(GridReference(z, spacing), Vector(6, screenY + 2, 0), color, 12);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every location with the in-game map's icon for its type (town, city, outpost, base, airport...), tinted by the
	//! controlling faction's colour as on the in-game map. Icons are widgets placed over the canvas.
	protected void DrawLocations(AROR_HighCommandComponent highCommand, float markerScale, float zoom, float width, float height)
	{
		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (!locationManager)
			return;

		array<AROR_LocationComponent> locations = {};
		locationManager.GetLocations(locations);
		float metres = m_Map.PixelsPerMetre();

		foreach (AROR_LocationComponent location : locations)
		{
			if (!location)
				continue;

			vector point = m_Map.WorldToScreen(location.GetCenter());
			if (point[0] < -40 || point[1] < -40 || point[0] > width + 40 || point[1] > height + 40)
				continue;

			AROR_EFactionRole role = location.GetControllingRole();
			Color color = FactionColor(role);
			AROR_ELocationType type = location.GetLocationType();

			// Zoomed in: the location's area
			if (zoom >= 2.5 && type != AROR_ELocationType.HQ)
				m_Map.DrawCircle(point, location.GetRadius() * metres, new Color(color.R(), color.G(), color.B(), 0.35), false, 1.2, 36);

			ResourceName icon = location.GetMapIcon();
			if (icon.IsEmpty())
			{
				// No icon authored (e.g. the offshore support corridor): a hollow diamond
				m_Map.DrawDiamond(point, 7 * markerScale, LabelColor(role), false);
			}
			else
			{
				// Same symbol as the location map: a military frame (AROR_MapFrameIcon), or the holder's flag for a stronghold
				if (location.GetOwner().FindComponent(AROR_StrongholdComponent))
					DrawStrongholdFlag(point, 22 * markerScale, role, color);
				else
					DrawFrameSymbol(point, 22 * markerScale, type, role, color);
			}

			string id = location.GetId();
			m_aHitLocationIds.Insert(id);
			m_aHitLocationPositions.Insert(point);

			if (id == m_sHoverLocation)
				m_Map.DrawCircle(point, 14 * markerScale, AROR_HiCommMap.Rgb(PAL_AMBER, 0.7), false, 1.5, 24);

			// Names: important sites always, everything once zoomed in
			bool important = type == AROR_ELocationType.HQ || type == AROR_ELocationType.AIRPORT || type == AROR_ELocationType.MILITARY_BASE || type == AROR_ELocationType.CITY;
			if (zoom >= 1.6 || important || id == m_sSelectedTarget || id == m_sHoverLocation)
			{
				string name = Upper(location.GetName());
				if (id == m_sSelectedTarget)
					name = "OBJ " + name;

				AddMapLabel(name, point + Vector(13 * markerScale, -8 * markerScale, 0), LabelColor(role, 0.9), 13);
			}
		}

		// Chosen target
		AROR_LocationComponent target = FindLocation(m_sSelectedTarget);
		if (target)
			m_Map.DrawReticle(m_Map.WorldToScreen(target.GetCenter()), 13 * markerScale, AROR_HiCommMap.Rgb(PAL_AMBER, 1));
	}

	//------------------------------------------------------------------------------------------------
	//! A location's military-frame symbol centred on its map point: frame by owner, pale fill, glyph by type.
	//! Line widths scale with the symbol, matching the 40-pixel location map icon.
	//! The national flag of the army holding a stronghold, 3:2 and centred on the point
	protected void DrawStrongholdFlag(vector point, float size, AROR_EFactionRole owner, Color color)
	{
		string presetId;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets)
			presetId = presets.GetPresetId(owner);
		float height = size * 2 / 3;
		array<ref CanvasWidgetCommand> commands = {};
		AROR_ArmyFlag.Build(commands, point[0] - size * 0.5, point[1] - height * 0.5, size, height, presetId, color, AROR_MapFrameIcon.KEYLINE_WIDTH * size / 40);
		foreach (CanvasWidgetCommand command : commands)
		{
			PolygonDrawCommand polygon = PolygonDrawCommand.Cast(command);
			if (polygon)
				m_Map.DrawPolygon(polygon.m_Vertices, Color.FromInt(polygon.m_iColor));
			LineDrawCommand line = LineDrawCommand.Cast(command);
			if (line)
				m_Map.DrawPolyline(line.m_Vertices, Color.FromInt(line.m_iColor), line.m_fWidth, line.m_bShouldEnclose);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawFrameSymbol(vector point, float size, AROR_ELocationType type, AROR_EFactionRole owner, Color color)
	{
		float left = point[0] - size * 0.5;
		float top = point[1] - size * 0.5;
		float lineScale = size / 40;
		Color stroke = AROR_MapFrameIcon.StrokeColor(color);

		array<float> frame = {};
		AROR_MapFrameIcon.FrameOutline(owner, left, top, size, frame);
		m_Map.DrawPolygon(frame, AROR_MapFrameIcon.FillColor(stroke));
		m_Map.DrawPolyline(frame, AROR_MapFrameIcon.KeylineColor(), AROR_MapFrameIcon.KEYLINE_WIDTH * lineScale, true);
		m_Map.DrawPolyline(frame, stroke, AROR_MapFrameIcon.BORDER_WIDTH * lineScale, true);

		array<ref array<float>> strokes = {};
		AROR_MapFrameIcon.GlyphStrokes(type, owner, left, top, size, strokes);
		foreach (array<float> strokeLine : strokes)
		{
			m_Map.DrawPolyline(strokeLine, stroke, AROR_MapFrameIcon.GLYPH_WIDTH * lineScale, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Active OPS missions: an amber diamond with the codename at the objective, a dashed line to any destination
	protected void DrawMissions(float markerScale, float width, float height)
	{
		AROR_MissionBoardComponent board = AROR_MissionBoardComponent.GetInstance();
		if (!board)
			return;

		array<AROR_MissionInfo> missions = {};
		board.GetMissions(missions);
		Color amber = AROR_HiCommMap.Rgb(PAL_AMBER, 1);
		foreach (AROR_MissionInfo mission : missions)
		{
			if (!mission.m_bActive)
				continue;

			vector point = m_Map.WorldToScreen(mission.m_vPosition);
			if (mission.m_bHasDestination)
			{
				vector destination = m_Map.WorldToScreen(mission.m_vDestination);
				m_Map.DrawDashedLine(point, destination, AROR_HiCommMap.Rgb(PAL_AMBER, 0.6), 1.5, 6, 5);
				m_Map.DrawSquare(destination, 5 * markerScale, AROR_HiCommMap.Rgb(PAL_GREEN, 0.9), false);
			}

			if (point[0] < -40 || point[1] < -40 || point[0] > width + 40 || point[1] > height + 40)
				continue;

			if (mission.m_fAreaRadius > 0)
				m_Map.DrawCircle(point, mission.m_fAreaRadius * m_Map.PixelsPerMetre(), AROR_HiCommMap.Rgb(PAL_AMBER, 0.5), false, 1.2, 32);

			m_Map.DrawDiamond(point, 7 * markerScale, AROR_HiCommMap.Srgb(4, 10, 7, 0.8), true);
			m_Map.DrawDiamond(point, 7 * markerScale, amber, false, 2);
			m_Map.DrawDiamond(point, 2.5 * markerScale, amber, true);
			AddMapLabel(mission.m_sCodename, point + Vector(12 * markerScale, 6 * markerScale, 0), amber, 13);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Squads with their current orders, and the planned order for the selection
	protected void DrawSquads(AROR_HighCommandComponent highCommand, array<AROR_HighCommandSquadInfo> squads, int localId, float markerScale, float zoom, float unit)
	{
		AROR_LocationComponent plannedTarget = FindLocation(m_sSelectedTarget);
		vector plannedPoint;
		if (m_bHasMoveTarget)
		{
			plannedTarget = null;
			plannedPoint = m_Map.WorldToScreen(m_vMoveTarget);
		}
		else if (plannedTarget)
		{
			plannedPoint = m_Map.WorldToScreen(plannedTarget.GetCenter());
		}

		// Order lines first, under every symbol
		foreach (AROR_HighCommandSquadInfo squad : squads)
		{
			vector point = m_Map.WorldToScreen(squad.m_vPosition);
			AROR_LocationComponent target = FindLocation(squad.m_sTargetId);
			if (squad.m_eOrder == AROR_EHighCommandOrder.MOVE)
			{
				// Move: cyan dashed route to a cross at the destination
				vector movePoint = m_Map.WorldToScreen(squad.m_vTarget);
				m_Map.DrawDashedLine(point, movePoint, AROR_HiCommMap.Rgb(PAL_MOVE, 0.85), 2 * unit, 6 * unit, 5 * unit);
				DrawCross(movePoint, 6 * unit, AROR_HiCommMap.Rgb(PAL_MOVE, 1));
			}
			else if (squad.m_eOrder == AROR_EHighCommandOrder.HOLD)
			{
				// Hold: the area the squad stands by in
				vector holdPoint = m_Map.WorldToScreen(squad.m_vTarget);
				float holdRadius = Math.Max(30 * m_Map.PixelsPerMetre(), 10 * unit);
				m_Map.DrawCircle(holdPoint, holdRadius, AROR_HiCommMap.Rgb(PAL_GREEN, 0.5), false, 1.5, 24);
			}
			else if (target && squad.m_eOrder != AROR_EHighCommandOrder.NONE)
			{
				vector targetPoint = m_Map.WorldToScreen(target.GetCenter());
				if (squad.m_eOrder == AROR_EHighCommandOrder.ATTACK)
				{
					m_Map.DrawGlowLine(point, targetPoint, AROR_HiCommMap.Rgb(PAL_RED, 0.85), AROR_HiCommMap.Rgb(PAL_RED, 0.15), 2 * unit);
					m_Map.DrawArrowHead(point, targetPoint, 11 * unit, AROR_HiCommMap.Rgb(PAL_RED, 0.95));
				}
				else
				{
					m_Map.DrawDashedLine(point, targetPoint, AROR_HiCommMap.Rgb(PAL_GREEN, 0.8), 2 * unit, 8 * unit, 6 * unit);
					m_Map.DrawArrowHead(point, targetPoint, 9 * unit, AROR_HiCommMap.Rgb(PAL_GREEN, 0.9));
				}
			}

			bool planning = (plannedTarget || m_bHasMoveTarget) && m_aSelected.Contains(squad.m_iId) && highCommand.CanCommand(localId, squad.m_iOwnerId);
			if (planning)
			{
				m_Map.DrawDashedLine(point, plannedPoint, AROR_HiCommMap.Rgb(PAL_AMBER, 0.9), 2.5 * unit, 10 * unit, 6 * unit);
				m_Map.DrawArrowHead(point, plannedPoint, 12 * unit, AROR_HiCommMap.Rgb(PAL_AMBER, 1));
			}
		}

		if (m_bHasMoveTarget)
			DrawCross(plannedPoint, 9 * unit, AROR_HiCommMap.Rgb(PAL_AMBER, 1));

		// Squads close together on screen form a stack: each keeps a dot at its real position, and their symbols
		// sit in a column above the group on a leader line, so overlapping squads stay readable and clickable
		array<vector> points = {};
		foreach (AROR_HighCommandSquadInfo squad : squads)
		{
			points.Insert(m_Map.WorldToScreen(squad.m_vPosition));
		}

		float clusterRadius = SQUAD_CLUSTER_PIXELS * unit;
		array<bool> placed = {};
		placed.Resize(squads.Count());
		Color fill = AROR_HiCommMap.Srgb(6, 16, 10, 0.92);
		foreach (int first, AROR_HighCommandSquadInfo firstSquad : squads)
		{
			if (placed[first])
				continue;

			array<int> stack = {};
			vector anchor;
			foreach (int other, AROR_HighCommandSquadInfo otherSquad : squads)
			{
				if (!placed[other] && vector.Distance(points[first], points[other]) <= clusterRadius)
				{
					placed[other] = true;
					stack.Insert(other);
					anchor = anchor + points[other];
				}
			}

			anchor = anchor * (1.0 / stack.Count());

			// Leader line from the group's position up to the bottom of the column
			float stem = 24 * markerScale;
			float row = 24 * markerScale;
			vector column = anchor - Vector(0, stem, 0);
			vector columnTop = column - Vector(0, row * (stack.Count() - 1), 0);
			m_Map.DrawLine(anchor, column + Vector(0, 7 * markerScale, 0), AROR_HiCommMap.Rgb(PAL_GREEN, 0.7), 1.5 * unit);

			// A stack gets a spine joining its symbols (drawn first, under them)
			if (stack.Count() > 1)
				m_Map.DrawLine(column, columnTop, AROR_HiCommMap.Rgb(PAL_GREEN, 0.35), 1 * unit);

			foreach (int position, int index : stack)
			{
				AROR_HighCommandSquadInfo squad = squads[index];
				bool selected = m_aSelected.Contains(squad.m_iId);
				bool own = highCommand.CanCommand(localId, squad.m_iOwnerId);

				int packed = PAL_GREEN_DIM;
				if (selected)
					packed = PAL_AMBER;
				else if (own)
					packed = PAL_GREEN;

				Color color = AROR_HiCommMap.Rgb(packed, 1);

				// Real position
				m_Map.DrawCircle(points[index], 3 * markerScale, color, true, 1, 10);

				vector symbol = column - Vector(0, row * position, 0);
				if (squad.m_iId == m_iHoverSquad)
					m_Map.DrawCircle(symbol, 16 * markerScale, AROR_HiCommMap.Rgb(packed, 0.3), true, 1, 20);

				m_Map.DrawUnit(symbol, 9 * markerScale, color, fill, 1, selected);
				m_aHitSquadIds.Insert(squad.m_iId);
				m_aHitSquadPositions.Insert(symbol);

				string label = squad.m_sCallsign;
				if (zoom >= 2 || stack.Count() > 1)
					label += string.Format("  %1/%2", squad.m_iAlive, squad.m_iSize);

				AddMapLabel(label, symbol + Vector(14 * markerScale, -8 * markerScale, 0), AROR_HiCommMap.Rgb(packed, 1), 14);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Bottom-right scale bar (left of the orders panel) with a round length of about 80-160 pixels
	protected void DrawScaleBar(float unit)
	{
		array<float> lengths = {25, 50, 100, 200, 500, 1000, 2000, 5000};
		float metres = m_Map.PixelsPerMetre();
		float length = lengths[0];
		foreach (float candidate : lengths)
		{
			if (candidate * metres <= 160 * unit)
				length = candidate;
		}

		float pixels = length * metres;
		vector corner = AppToCanvas(m_fRightEdge - 14, m_fMapY + m_fMapHeight - 30);
		float right = corner[0];
		float y = corner[1];
		Color color = AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 0.85);
		m_Map.DrawLine(Vector(right - pixels, y, 0), Vector(right, y, 0), color, 2);
		m_Map.DrawLine(Vector(right - pixels, y - 6 * unit, 0), Vector(right - pixels, y + 2 * unit, 0), color, 2);
		m_Map.DrawLine(Vector(right, y - 6 * unit, 0), Vector(right, y + 2 * unit, 0), color, 2);

		string text = string.Format("%1 M", length);
		if (length >= 1000)
			text = string.Format("%1 KM", length / 1000);

		AddMapLabel(text, Vector(right - pixels, y - 24 * unit, 0), color, 13);
	}

	//------------------------------------------------------------------------------------------------
	//! Bottom-left legend: who holds a site (faction colours, as on the in-game map) and what order lines mean.
	//! Laid out in app units so it scales with the phone.
	protected void DrawLegend()
	{
		float left = m_fLegendLeft;
		float top = m_fMapY + m_fMapHeight - PANEL_MARGIN - 58;
		vector topLeft = AppToCanvas(left, top);
		vector bottomRight = AppToCanvas(left + 400, top + 58);
		array<float> box = {topLeft[0], topLeft[1], bottomRight[0], topLeft[1], bottomRight[0], bottomRight[1], topLeft[0], bottomRight[1]};
		m_Map.DrawPolygon(box, AROR_HiCommMap.Srgb(4, 10, 7, 0.9));
		m_Map.DrawPolyline(box, AROR_HiCommMap.Rgb(AROR_HiCommMap.GRID, 0.9), 1, true);

		float pixel = GetGame().GetWorkspace().DPIScale(m_fScale);	// canvas pixels per app unit
		Color label = AROR_HiCommMap.Rgb(PAL_GREEN_DIM, 1);

		// Row 1: site colours
		array<AROR_EFactionRole> roles = {AROR_EFactionRole.RESISTANCE, AROR_EFactionRole.OCCUPIERS, AROR_EFactionRole.INVADERS};
		array<string> names = {"RESISTANCE", "OCCUPIERS", "INVADERS"};
		foreach (int i, AROR_EFactionRole role : roles)
		{
			vector swatch = AppToCanvas(left + 16 + i * 108, top + 17);
			m_Map.DrawSquare(swatch, 5 * pixel, FactionColor(role), true);
			AddMapLabel(names[i], swatch + Vector(10 * pixel, -9 * pixel, 0), LabelColor(role), 12);
		}

		// Row 2: squads and orders
		vector unitPoint = AppToCanvas(left + 18, top + 41);
		m_Map.DrawUnit(unitPoint, 7 * pixel, AROR_HiCommMap.Rgb(PAL_GREEN, 1), AROR_HiCommMap.Srgb(6, 16, 10, 0.92), 0, false);
		AddMapLabel("SQUAD", unitPoint + Vector(12 * pixel, -9 * pixel, 0), label, 12);

		array<int> orderColors = {PAL_RED, PAL_GREEN, PAL_MOVE};
		array<string> orderNames = {"ATTACK", "DEFEND", "MOVE"};
		foreach (int i, int orderColor : orderColors)
		{
			vector from = AppToCanvas(left + 96 + i * 78, top + 41);
			m_Map.DrawLine(from, from + Vector(16 * pixel, 0, 0), AROR_HiCommMap.Rgb(orderColor, 1), 2);
			AddMapLabel(orderNames[i], from + Vector(20 * pixel, -9 * pixel, 0), label, 12);
		}

		vector roadFrom = AppToCanvas(left + 330, top + 41);
		m_Map.DrawLine(roadFrom, roadFrom + Vector(16 * pixel, 0, 0), AROR_HiCommMap.Rgb(AROR_HiCommMap.ROAD, 0.85), 2);
		AddMapLabel("ROAD", roadFrom + Vector(20 * pixel, -9 * pixel, 0), label, 12);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddMapLabel(string text, vector canvasPoint, Color color, float size)
	{
		m_aLabelTexts.Insert(text);
		m_aLabelPositions.Insert(canvasPoint);
		m_aLabelColors.Insert(color);
		m_aLabelSizes.Insert(size);
	}

	//------------------------------------------------------------------------------------------------
	//! Labels from this frame's overlay, placed over the canvas (canvas pixels -> app units)
	protected void DrawMapLabels()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		foreach (int i, string text : m_aLabelTexts)
		{
			if (m_iMapLabelCount >= MAP_LABEL_POOL)
				break;

			// Keep labels inside the map and off the floating panels; flip long ones to the left near the edge
			vector point = m_aLabelPositions[i];
			float labelWidth = workspace.DPIScale(text.Length() * m_aLabelSizes[i] * CHAR_WIDTH_FACTOR * m_fScale);
			float labelHeight = workspace.DPIScale(m_aLabelSizes[i] * 1.3 * m_fScale);
			if (point[0] < 0 || point[1] < 0 || point[0] > m_Map.GetWidth() || point[1] + labelHeight > m_Map.GetHeight())
				continue;

			if (IsUnderPanel(point) || IsUnderPanel(point + Vector(labelWidth, 0, 0)))
				continue;

			float labelX = point[0];
			if (labelX + labelWidth > m_Map.GetWidth() - 4)
				labelX = Math.Max(m_Map.GetWidth() - 4 - labelWidth, 0);

			RichTextWidget label = m_aMapLabels[m_iMapLabelCount];
			AROR_HiCommWidgetCache cache = m_aMapLabelCaches[m_iMapLabelCount];
			if (m_aMapLabelCache[m_iMapLabelCount] != text)
			{
				label.SetText(text);
				m_aMapLabelCache[m_iMapLabelCount] = text;
			}

			CachedColor(label, cache, m_aLabelColors[i]);
			CachedFontSize(label, cache, m_aLabelSizes[i] * m_fScale);
			CachedPlace(label, cache, m_fMapX * m_fScale + workspace.DPIUnscale(labelX), m_fMapY * m_fScale + workspace.DPIUnscale(point[1]), workspace.DPIUnscale(labelWidth) + 8, 24 * m_fScale);
			CachedVisible(label, cache, true);
			m_iMapLabelCount++;
		}

		// Location icons, centred on their point (white textures tinted with the faction colour)
		m_iMapIconCount = 0;
		float iconSize = ICON_SIZE * m_fScale * Math.Min(1 + (m_Map.GetZoom() - 1) * 0.05, 1.4);
		foreach (int i, ResourceName texture : m_aIconTextures)
		{
			if (m_iMapIconCount >= ICON_POOL)
				break;

			vector point = m_aIconPositions[i];
			if (point[0] < 0 || point[1] < 0 || point[0] > m_Map.GetWidth() || point[1] > m_Map.GetHeight() || IsUnderPanel(point))
				continue;

			ImageWidget icon = m_aMapIcons[m_iMapIconCount];
			AROR_HiCommWidgetCache cache = m_aMapIconCaches[m_iMapIconCount];
			if (m_aMapIconCache[m_iMapIconCount] != texture)
			{
				icon.LoadImageTexture(0, texture);
				m_aMapIconCache[m_iMapIconCount] = texture;
			}

			CachedColor(icon, cache, m_aIconColors[i]);
			CachedPlace(icon, cache, m_fMapX * m_fScale + workspace.DPIUnscale(point[0]) - iconSize * 0.5, m_fMapY * m_fScale + workspace.DPIUnscale(point[1]) - iconSize * 0.5, iconSize, iconSize);
			CachedVisible(icon, cache, true);
			m_iMapIconCount++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Map controls, zoom/grid readout, cursor grid reference and the hover tooltip (pooled widgets over the canvas)
	protected void DrawMapHud(AROR_HighCommandComponent highCommand)
	{
		// Map controls along the top, left of the orders panel when it is open
		float right = m_fRightEdge;
		float top = m_fMapY + PANEL_MARGIN;
		int helpStyle = STYLE_NORMAL;
		if (m_bShowHelp)
			helpStyle = STYLE_SELECTED;

		if (m_aSelected.IsEmpty())
		{
			Button(right - 40, top, 40, 40, "?", "help", helpStyle, 18);
			right -= 46;
		}

		Button(m_fSliderX - 6, m_fSliderTop - 50, 40, 36, "+", "zoomin", STYLE_NORMAL, 20);
		Button(m_fSliderX - 6, m_fSliderTop + SLIDER_HEIGHT + 14, 40, 36, "-", "zoomout", STYLE_NORMAL, 20);

		Button(right - 56, top, 56, 40, "HQ", "hq", STYLE_NORMAL, 15);
		Button(right - 118, top, 56, 40, "SEL", "centresel", STYLE_NORMAL, 15);
		Button(right - 180, top, 56, 40, "FIT", "fit", STYLE_NORMAL, 15);

		// Read-out along the bottom strip, right of the legend
		string readout = string.Format("ZOOM %1X", Math.Round(m_Map.GetZoom() * 10) / 10) + "  //  GRID " + SpacingText(m_Map.GetGridSpacing());
		if (m_bCursorInside)
		{
			vector world = m_Map.ScreenToWorld(m_vCursor[0], m_vCursor[1]);
			readout += "  //  CURSOR " + GridReference(world[0], 100) + " " + GridReference(world[2], 100);
		}

		Text(m_fLegendLeft + 414, m_fMapY + m_fMapHeight - 38, 560, 26, 14, readout, m_GreenDim);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_sHoverLocation = string.Empty;
		m_iHoverSquad = -1;
		if (!m_bCursorInside)
			return;

		// Hover tooltip: squads first, then locations
		string tooltip;
		m_iHoverSquad = PickSquad(m_vCursor);
		if (m_iHoverSquad >= 0)
		{
			AROR_HighCommandSquadInfo squad = highCommand.GetSquadInfo(m_iHoverSquad);
			if (squad)
				tooltip = string.Format("%1   %2/%3 MEN", squad.m_sCallsign, squad.m_iAlive, squad.m_iSize) + "\n" + LongOrder(squad) + "\nCMDR " + OwnerTag(squad.m_iOwnerId);
		}
		else
		{
			m_sHoverLocation = PickLocation(m_vCursor);
			AROR_LocationComponent location = FindLocation(m_sHoverLocation);
			if (location)
			{
				tooltip = Upper(location.GetName()) + "\n" + TypeName(location.GetLocationType()) + "  //  " + RoleName(location.GetControllingRole());
				AROR_LocationComponent hq = AROR_HighCommandComponent.FindHQ();
				if (hq && hq != location)
					tooltip += string.Format("\n%1 KM FROM HQ", Math.Round(vector.DistanceXZ(hq.GetCenter(), location.GetCenter()) / 100) / 10);
				else
					tooltip += "\nRESISTANCE HQ";
			}
		}

		if (tooltip.IsEmpty())
			return;

		float tipX = m_fMapX + workspace.DPIUnscale(m_vCursor[0]) / m_fScale + 18;
		float tipY = m_fMapY + workspace.DPIUnscale(m_vCursor[1]) / m_fScale + 18;
		tipX = Math.Min(tipX, m_fRightEdge - 330);
		tipY = Math.Min(tipY, m_fMapY + m_fMapHeight - 96);
		Button(tipX, tipY, 320, 86, tooltip, string.Empty, STYLE_PANEL, 15, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Arma-style grid digits: km (2), hectometres (3) or decametres (4), depending on the spacing
	protected static string GridReference(float metres, float spacing)
	{
		int value = Math.Floor(Math.Max(metres, 0) / 1000);
		int digits = 2;
		if (spacing < 100)
		{
			value = Math.Floor(Math.Max(metres, 0) / 10);
			digits = 4;
		}
		else if (spacing < 1000)
		{
			value = Math.Floor(Math.Max(metres, 0) / 100);
			digits = 3;
		}

		return value.ToString(digits);
	}

	//------------------------------------------------------------------------------------------------
	protected static string SpacingText(float spacing)
	{
		if (spacing >= 1000)
			return string.Format("%1 KM", spacing / 1000);

		return string.Format("%1 M", spacing);
	}

	//------------------------------------------------------------------------------------------------
	// Input
	//------------------------------------------------------------------------------------------------
	void OnClick(Widget w, int x, int y)
	{
		if (!w || !m_wRoot.IsVisible())
			return;

		// Map clicks are handled on button up; this is the fallback if a button swallows the up event
		if (w == m_wMapButton)
		{
			if (m_bLeftDown && System.GetTickCount() - m_iLastMapClickMs > 150)
				OnMouseUp(w, x, y, 0);

			return;
		}

		string name = w.GetName();
		if (name.IndexOf(BUTTON_PREFIX) != 0)
			return;

		int index = name.Substring(BUTTON_PREFIX.Length(), name.Length() - BUTTON_PREFIX.Length()).ToInt();
		if (!m_aButtonActions.IsIndexValid(index) || index >= m_iButtonCount)
			return;

		DoAction(m_aButtonActions[index]);
	}

	//------------------------------------------------------------------------------------------------
	protected void DoAction(string action)
	{
		if (action.IsEmpty())
			return;

		array<string> parts = {};
		action.Split(":", parts, false);
		string verb = parts[0];

		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!highCommand || !controller)
			return;

		if (verb == "login")
		{
			if (highCommand.IsLeader(GetLocalPlayerId()))
				m_eScreen = AROR_EHiCommScreen.COMMAND;
		}
		else if (verb == "logout")
		{
			m_eScreen = AROR_EHiCommScreen.LOGIN;
		}
		else if (verb == "screen" && parts.Count() > 1)
		{
			m_eScreen = parts[1].ToInt();
		}
		else if (verb == "squad" && parts.Count() > 1)
		{
			if (IsShiftDown())
				ToggleSelected(parts[1].ToInt());
			else
				SelectOnly(parts[1].ToInt());
		}
		else if (verb == "selall")
		{
			m_aSelected.Clear();
			array<AROR_HighCommandSquadInfo> squads = {};
			highCommand.GetSquads(squads);
			foreach (AROR_HighCommandSquadInfo squad : squads)
			{
				if (highCommand.CanCommand(GetLocalPlayerId(), squad.m_iOwnerId))
					m_aSelected.Insert(squad.m_iId);
			}
		}
		else if (verb == "selnone")
		{
			m_aSelected.Clear();
		}
		else if (verb == "oob")
		{
			m_bShowOrderOfBattle = !m_bShowOrderOfBattle;
		}
		else if (verb == "mapmode")
		{
			m_bUseGameMap = !m_bUseGameMap;
		}
		else if (verb == "help")
		{
			m_bShowHelp = !m_bShowHelp;
		}
		else if (verb == "order" && parts.Count() > 1)
		{
			// Move targets a point, the others a location: switching drops the other kind of target
			m_eSelectedOrder = parts[1].ToInt();
			if (m_eSelectedOrder == AROR_EHighCommandOrder.MOVE)
				m_sSelectedTarget = string.Empty;
			else
				m_bHasMoveTarget = false;
		}
		else if (verb == "clear")
		{
			m_sSelectedTarget = string.Empty;
			m_bHasMoveTarget = false;
		}
		else if (verb == "execute")
		{
			string problem = GetPlannedOrderProblem(highCommand);
			if (!m_aSelected.IsEmpty() && problem.IsEmpty())
				IssueOrder(m_eSelectedOrder, m_sSelectedTarget, m_vMoveTarget);
		}
		else if (verb == "hold")
		{
			if (!m_aSelected.IsEmpty())
				IssueOrder(AROR_EHighCommandOrder.HOLD, string.Empty, vector.Zero);
		}
		else if (verb == "zoomin")
		{
			m_Map.ZoomAt(WHEEL_ZOOM * WHEEL_ZOOM, m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);
		}
		else if (verb == "zoomout")
		{
			m_Map.ZoomAt(1 / (WHEEL_ZOOM * WHEEL_ZOOM), m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);
		}
		else if (verb == "fit")
		{
			m_Map.Fit();
		}
		else if (verb == "hq")
		{
			AROR_LocationComponent hq = AROR_HighCommandComponent.FindHQ();
			if (hq)
			{
				m_Map.CentreOn(hq.GetCenter());
				if (m_Map.GetTargetZoom() < 3)
					m_Map.SetTargetZoom(3, m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);
			}
		}
		else if (verb == "centresel")
		{
			CentreOnSelection(highCommand);
		}
		else if (verb == "raise" && parts.Count() > 1)
		{
			controller.AROR_RequestHiCommRaise(parts[1].ToInt());
		}
		else if (verb == "leader" && parts.Count() > 2)
		{
			controller.AROR_RequestHiCommSetLeader(parts[1].ToInt(), parts[2].ToInt() != 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CentreOnSelection(AROR_HighCommandComponent highCommand)
	{
		vector sum;
		int count;
		foreach (int squadId : m_aSelected)
		{
			AROR_HighCommandSquadInfo squad = highCommand.GetSquadInfo(squadId);
			if (!squad)
				continue;

			sum = sum + squad.m_vPosition;
			count++;
		}

		if (count > 0)
			m_Map.CentreOn(sum * (1.0 / count));
	}

	//------------------------------------------------------------------------------------------------
	// Pooled widgets (app units: landscape screen pixels)
	//------------------------------------------------------------------------------------------------
	protected void Panel(float x, float y, float width, float height, Color color)
	{
		if (m_iPanelCount >= PANEL_POOL)
			return;

		ImageWidget panel = m_aPanels[m_iPanelCount];
		AROR_HiCommWidgetCache cache = m_aPanelCaches[m_iPanelCount];
		CachedColor(panel, cache, color);
		CachedPlace(panel, cache, x * m_fScale, y * m_fScale, width * m_fScale, height * m_fScale);
		CachedVisible(panel, cache, true);
		m_iPanelCount++;
	}

	//------------------------------------------------------------------------------------------------
	//! Bordered panel
	protected void Box(float x, float y, float width, float height)
	{
		Panel(x, y, width, height, m_Border);
		Panel(x + 2, y + 2, width - 4, height - 4, m_PanelFill);
	}

	//------------------------------------------------------------------------------------------------
	protected void Text(float x, float y, float width, float height, float size, string content, Color color, bool bold = false)
	{
		if (m_iTextCount >= TEXT_POOL)
			return;

		int index = m_iTextCount;
		RichTextWidget text = m_aTexts[index];
		AROR_HiCommWidgetCache cache = m_aTextCaches[index];

		// Wrapping is only redone when the text or its box changes
		if (cache.m_sRaw != content || cache.m_fWrapWidth != width || cache.m_fWrapSize != size)
		{
			cache.m_sRaw = content;
			cache.m_fWrapWidth = width;
			cache.m_fWrapSize = size;
			string wrapped = Wrap(content, width, size);
			if (m_aTextCache[index] != wrapped)
			{
				text.SetText(wrapped);
				m_aTextCache[index] = wrapped;
			}
		}

		CachedColor(text, cache, color);
		CachedBold(text, cache, bold);
		CachedFontSize(text, cache, size * m_fScale);
		CachedPlace(text, cache, x * m_fScale, y * m_fScale, width * m_fScale, height * m_fScale);
		CachedVisible(text, cache, true);
		m_iTextCount++;
	}

	//------------------------------------------------------------------------------------------------
	//! Single line ending at right (estimated width)
	protected void TextRight(float right, float y, float size, string content, Color color)
	{
		float width = content.Length() * size * WRAP_CHAR_FACTOR;
		Text(right - width, y, width + size, size * 1.5, size, content, color);
	}

	//------------------------------------------------------------------------------------------------
	protected void Header(float x, float y, string content)
	{
		Text(x, y, 600, 34, 22, content, m_GreenDim, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void Stat(float x, float y, string label, string value)
	{
		Text(x, y, 340, 34, 22, label, m_GreenDim);
		Text(x + 340, y, 400, 34, 22, value, m_Green, true);
	}

	//------------------------------------------------------------------------------------------------
	//! action empty = not clickable
	protected void Button(float x, float y, float width, float height, string label, string action, int style, float size, bool centred = true)
	{
		if (m_iButtonCount >= BUTTON_POOL)
			return;

		Color fill = m_ButtonFill;
		Color labelColor = m_Green;
		if (style == STYLE_SELECTED)
		{
			fill = m_Green;
			labelColor = m_Dark;
		}
		else if (style == STYLE_PRIMARY)
		{
			fill = m_Amber;
			labelColor = m_Dark;
		}
		else if (style == STYLE_DISABLED)
		{
			fill = m_ButtonDisabled;
			labelColor = m_GreenFaint;
			action = string.Empty;
		}
		else if (style == STYLE_DANGER)
		{
			fill = m_ButtonDanger;
			labelColor = m_Red;
		}
		else if (style == STYLE_MUTED)
		{
			fill = m_ButtonMuted;
			labelColor = m_GreenDim;
		}
		else if (style == STYLE_PANEL)
		{
			fill = m_ButtonPanel;
			labelColor = m_Green;
			action = string.Empty;
		}

		int index = m_iButtonCount;
		AROR_HiCommWidgetCache labelCache = m_aButtonLabelCaches[index];

		// Read-outs wrap to the box; multi-line labels ("\n") are measured by their longest line.
		// Both are only recomputed when the label or box changes.
		int centredValue = 0;
		if (centred)
			centredValue = 1;

		if (labelCache.m_sRaw != label || labelCache.m_fWrapWidth != width || labelCache.m_fWrapSize != size || labelCache.m_iCentred != centredValue)
		{
			labelCache.m_sRaw = label;
			labelCache.m_fWrapWidth = width;
			labelCache.m_fWrapSize = size;
			labelCache.m_iCentred = centredValue;
			string shown = label;
			if (!centred)
				shown = Wrap(label, width - 28, size);

			array<string> lines = {};
			shown.Split("\n", lines, false);
			int longest;
			foreach (string line : lines)
			{
				longest = Math.Max(longest, line.Length());
			}

			labelCache.m_iLongest = longest;
			labelCache.m_iLines = Math.Max(lines.Count(), 1);
			if (m_aButtonLabelCache[index] != shown)
			{
				m_aButtonLabels[index].SetText(shown);
				m_aButtonLabelCache[index] = shown;
			}
		}

		ImageWidget background = m_aButtonBackgrounds[index];
		AROR_HiCommWidgetCache backgroundCache = m_aButtonBgCaches[index];
		CachedColor(background, backgroundCache, fill);
		CachedPlace(background, backgroundCache, x * m_fScale, y * m_fScale, width * m_fScale, height * m_fScale);
		CachedVisible(background, backgroundCache, true);

		RichTextWidget text = m_aButtonLabels[index];
		CachedColor(text, labelCache, labelColor);
		CachedBold(text, labelCache, style != STYLE_PANEL);
		CachedFontSize(text, labelCache, size * m_fScale);
		float labelX = x + 14;
		if (centred)
			labelX = x + Math.Max((width - labelCache.m_iLongest * size * CHAR_WIDTH_FACTOR) * 0.5, 8);

		float labelHeight = size * 1.3 * labelCache.m_iLines;
		CachedPlace(text, labelCache, labelX * m_fScale, (y + (height - labelHeight) * 0.5) * m_fScale, (width - (labelX - x)) * m_fScale, (labelHeight + size * 0.2) * m_fScale);
		CachedVisible(text, labelCache, true);

		Widget button = m_aButtons[index];
		AROR_HiCommWidgetCache buttonCache = m_aButtonCaches[index];
		CachedPlace(button, buttonCache, x * m_fScale, y * m_fScale, width * m_fScale, height * m_fScale);
		CachedVisible(button, buttonCache, !action.IsEmpty());
		m_aButtonActions[index] = action;
		m_iButtonCount++;
	}

	//------------------------------------------------------------------------------------------------
	protected void HideUnused()
	{
		for (int i = m_iPanelCount; i < PANEL_POOL; i++)
		{
			CachedVisible(m_aPanels[i], m_aPanelCaches[i], false);
		}

		for (int i = m_iTextCount; i < TEXT_POOL; i++)
		{
			CachedVisible(m_aTexts[i], m_aTextCaches[i], false);
		}

		for (int i = m_iButtonCount; i < BUTTON_POOL; i++)
		{
			CachedVisible(m_aButtonBackgrounds[i], m_aButtonBgCaches[i], false);
			CachedVisible(m_aButtonLabels[i], m_aButtonLabelCaches[i], false);
			CachedVisible(m_aButtons[i], m_aButtonCaches[i], false);
			m_aButtonActions[i] = string.Empty;
		}

		for (int i = m_iMapLabelCount; i < MAP_LABEL_POOL; i++)
		{
			CachedVisible(m_aMapLabels[i], m_aMapLabelCaches[i], false);
		}

		for (int i = m_iMapIconCount; i < ICON_POOL; i++)
		{
			CachedVisible(m_aMapIcons[i], m_aMapIconCaches[i], false);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Cached widget setters: pooled widgets are re-laid out every frame, but engine calls (especially font
	// size, which relayouts text) are only made when a value actually changed
	//------------------------------------------------------------------------------------------------
	protected void CachedPlace(Widget widget, AROR_HiCommWidgetCache cache, float x, float y, float width, float height)
	{
		x += m_fInsetX;
		y += m_fInsetY;
		if (Math.AbsFloat(x - cache.m_fX) < 0.05 && Math.AbsFloat(y - cache.m_fY) < 0.05 && Math.AbsFloat(width - cache.m_fWidth) < 0.05 && Math.AbsFloat(height - cache.m_fHeight) < 0.05)
			return;

		cache.m_fX = x;
		cache.m_fY = y;
		cache.m_fWidth = width;
		cache.m_fHeight = height;
		PlaceRaw(widget, x, y, width, height);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CachedColor(Widget widget, AROR_HiCommWidgetCache cache, Color color)
	{
		int packed = color.PackToInt();
		if (cache.m_bColorSet && cache.m_iColor == packed)
			return;

		cache.m_bColorSet = true;
		cache.m_iColor = packed;
		widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CachedFontSize(TextWidget text, AROR_HiCommWidgetCache cache, float size)
	{
		int fontSize = Math.Max(1, Math.Round(size));
		if (cache.m_iFontSize == fontSize)
			return;

		cache.m_iFontSize = fontSize;
		text.SetExactFontSize(fontSize);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CachedBold(TextWidget text, AROR_HiCommWidgetCache cache, bool bold)
	{
		int value = 0;
		if (bold)
			value = 1;

		if (cache.m_iBold == value)
			return;

		cache.m_iBold = value;
		text.SetBold(bold);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CachedVisible(Widget widget, AROR_HiCommWidgetCache cache, bool visible)
	{
		int value = 0;
		if (visible)
			value = 1;

		if (cache.m_iVisible == value)
			return;

		cache.m_iVisible = value;
		widget.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	// Data helpers
	//------------------------------------------------------------------------------------------------
	protected static int GetLocalPlayerId()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return 0;

		return controller.GetPlayerId();
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetPlayerName(int playerId)
	{
		string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (name.IsEmpty())
			return "UNKNOWN";

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Inserts line breaks so each line fits width (app units) at size, by estimated character width;
	//! existing line breaks are kept. The layout's text widgets do not wrap on their own.
	protected static string Wrap(string content, float width, float size)
	{
		int maxChars = Math.Max(Math.Floor(width / (size * WRAP_CHAR_FACTOR)), 4);
		if (content.Length() <= maxChars)
			return content;

		array<string> paragraphs = {};
		content.Split("\n", paragraphs, false);
		string result;
		foreach (int p, string paragraph : paragraphs)
		{
			if (p > 0)
				result += "\n";

			array<string> words = {};
			paragraph.Split(" ", words, false);
			string line;
			foreach (int w, string word : words)
			{
				if (w == 0)
					line = word;
				else if (line.Length() + 1 + word.Length() <= maxChars)
					line += " " + word;
				else
				{
					result += line + "\n";
					line = word;
				}
			}

			result += line;
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! string.ToUpper changes the string in place; this returns an upper-case copy
	protected static string Upper(string text)
	{
		text.ToUpper();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string OwnerTag(int ownerId)
	{
		if (ownerId == 0)
			return "UNASSIGNED";

		return Upper(GetPlayerName(ownerId));
	}

	//------------------------------------------------------------------------------------------------
	protected static AROR_LocationComponent FindLocation(string locationId)
	{
		if (locationId.IsEmpty())
			return null;

		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (!locationManager)
			return null;

		return locationManager.GetLocationById(locationId);
	}

	//------------------------------------------------------------------------------------------------
	//! Flag-capturable military sites held by the Resistance, out of all of them
	protected static void CountSites(out int held, out int total)
	{
		held = 0;
		total = 0;
		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (!locationManager)
			return;

		array<AROR_LocationComponent> locations = {};
		locationManager.GetLocations(locations);
		foreach (AROR_LocationComponent location : locations)
		{
			if (!location)
				continue;

			AROR_ELocationType type = location.GetLocationType();
			if (type == AROR_ELocationType.HQ || type == AROR_ELocationType.SUPPORT_CORRIDOR)
				continue;

			total++;
			if (location.GetControllingRole() == AROR_EFactionRole.RESISTANCE)
				held++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The role's faction colour, as the in-game map tints its location icons
	protected Color FactionColor(AROR_EFactionRole role)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (roles && roles.GetFaction(role))
			return roles.GetFaction(role).GetFactionColor();

		if (role == AROR_EFactionRole.OCCUPIERS)
			return m_Occupiers;

		if (role == AROR_EFactionRole.INVADERS)
			return m_Invaders;

		return m_Green;
	}

	//------------------------------------------------------------------------------------------------
	//! Faction colour lifted towards white so text stays readable on the dark map
	protected Color LabelColor(AROR_EFactionRole role, float alpha = 1)
	{
		Color color = FactionColor(role);
		float lift = 0.5;
		return new Color(color.R() + (1 - color.R()) * lift, color.G() + (1 - color.G()) * lift, color.B() + (1 - color.B()) * lift, alpha);
	}

	//------------------------------------------------------------------------------------------------
	//! Map colour of a role, packed for AROR_HiCommMap.Rgb
	protected static int RolePacked(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.OCCUPIERS)
			return PAL_OCCUPIERS;

		if (role == AROR_EFactionRole.INVADERS)
			return PAL_INVADERS;

		return PAL_GREEN;
	}

	//------------------------------------------------------------------------------------------------
	protected static string RoleName(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.OCCUPIERS)
			return "OCCUPIERS";

		if (role == AROR_EFactionRole.INVADERS)
			return "INVADERS";

		return "RESISTANCE";
	}

	//------------------------------------------------------------------------------------------------
	protected static string TypeName(AROR_ELocationType type)
	{
		string name = typename.EnumToString(AROR_ELocationType, type);
		name.Replace("_", " ");
		return name;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ShortVerb(AROR_EHighCommandOrder order)
	{
		if (order == AROR_EHighCommandOrder.ATTACK)
			return "ATK";

		if (order == AROR_EHighCommandOrder.DEFEND)
			return "DEF";

		if (order == AROR_EHighCommandOrder.MOVE)
			return "MOVE";

		return "HOLD";
	}

	//------------------------------------------------------------------------------------------------
	protected static string TargetName(AROR_HighCommandSquadInfo squad)
	{
		AROR_LocationComponent target = FindLocation(squad.m_sTargetId);
		if (!target)
			return string.Empty;

		return Upper(target.GetName());
	}

	//------------------------------------------------------------------------------------------------
	protected static string ShortOrder(AROR_HighCommandSquadInfo squad)
	{
		if (!squad.m_bFormed)
			return "FORMING";

		if (squad.m_eOrder == AROR_EHighCommandOrder.NONE)
			return "AT HQ";

		if (squad.m_eOrder == AROR_EHighCommandOrder.MOVE || squad.m_eOrder == AROR_EHighCommandOrder.HOLD)
			return ShortVerb(squad.m_eOrder) + " " + AROR_HighCommandComponent.GridReference(squad.m_vTarget);

		return ShortVerb(squad.m_eOrder) + " " + TargetName(squad);
	}

	//------------------------------------------------------------------------------------------------
	protected static string LongOrder(AROR_HighCommandSquadInfo squad)
	{
		if (!squad.m_bFormed)
			return "FORMING UP AT HQ";

		if (squad.m_eOrder == AROR_EHighCommandOrder.ATTACK)
			return "ATTACKING " + TargetName(squad);

		if (squad.m_eOrder == AROR_EHighCommandOrder.DEFEND)
			return "DEFENDING " + TargetName(squad);

		if (squad.m_eOrder == AROR_EHighCommandOrder.MOVE)
			return "MOVING TO GRID " + AROR_HighCommandComponent.GridReference(squad.m_vTarget);

		if (squad.m_eOrder == AROR_EHighCommandOrder.HOLD)
			return "STANDING BY AT GRID " + AROR_HighCommandComponent.GridReference(squad.m_vTarget);

		return "HOLDING AT HQ";
	}

	//------------------------------------------------------------------------------------------------
	//! Destination marker for move orders
	protected void DrawCross(vector centre, float half, Color color)
	{
		m_Map.DrawLine(centre + Vector(-half, -half, 0), centre + Vector(half, half, 0), color, 2);
		m_Map.DrawLine(centre + Vector(-half, half, 0), centre + Vector(half, -half, 0), color, 2);
	}

	//------------------------------------------------------------------------------------------------
	//! Places a content widget (screen-relative, already scaled), offset by the content inset
	protected void Place(Widget widget, float x, float y, float width, float height)
	{
		PlaceRaw(widget, x + m_fInsetX, y + m_fInsetY, width, height);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlaceRaw(Widget widget, float x, float y, float width, float height)
	{
		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, width, height);
	}
}
