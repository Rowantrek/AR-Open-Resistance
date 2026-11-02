//! OPS screens (see ARCHITECTURE.md -> Missions)
enum AROR_EOpsScreen
{
	LOGIN,	//!< Field tablet sign-in: boot log, callsign and clearance
	BOARD	//!< Operations board: filters, mission list, dossier with area map, network log
}

//! OPS: the Resistance's mission board, a landscape phone app styled as a rugged field tablet (phosphor green on
//! near-black, scanlines). Every Resistance player can browse the board; only Resistance Leaders accept or abort
//! operations. Client only: actions are requests to AROR_MissionBoardComponent on the server, and everything shown
//! comes from its replicated snapshot. Drawn from widget pools (see AROR_PhonePooledApp) plus a crest canvas, a
//! scanline canvas and a terrain map (AROR_HiCommMap) for the dossier's area view.
class AROR_OpsApp : AROR_PhonePooledApp
{
	protected static const int PANEL_POOL = 120;
	protected static const int TEXT_POOL = 120;
	protected static const int BUTTON_POOL = 28;

	//! Content is drawn at this fraction of the phone screen, centred, so nothing touches the screen edges
	protected static const float CONTENT_SCALE = 0.96;

	// The sign-in column (app units 590-1110 x 98-730) is drawn enlarged about its centre
	protected static const float LOGIN_ZOOM = 1.32;
	protected static const float LOGIN_CENTRE_X = 850;
	protected static const float LOGIN_CENTRE_Y = 414;

	// Board geometry (app units)
	protected static const float TOP_BAR = 32;
	protected static const float BODY_TOP = 170;
	protected static const float BODY_BOTTOM = 888;
	protected static const float LIST_LEFT = 16;
	protected static const float LIST_WIDTH = 540;
	protected static const float ROW_HEIGHT = 66;
	protected static const float LOG_TOP = 742;
	protected static const float DOSSIER_LEFT = 584;
	protected static const float DOSSIER_RIGHT = 1684;
	protected static const float MAP_WIDTH = 400;
	protected static const float MAP_HEIGHT = 244;
	protected static const float ACTIONS_TOP = 806;

	protected static const int VERIFY_MS = 700;
	protected static const int ABORT_CONFIRM_MS = 4000;
	protected static const float MAP_VIEW_METRES = 2400;	//!< Area shown around a single objective
	protected static const float MAP_CENTRE_SMOOTHING = 8;

	// Faction colours on the area map (packed RGB, as HICOMM)
	protected static const int PAL_OCCUPIERS = 0x6096F0;
	protected static const int PAL_INVADERS = 0xF6963C;
	protected static const int PAL_RESISTANCE = 0x9FD36B;
	protected static const int PAL_AMBER = 0xE6A83A;

	protected CanvasWidget m_wCrest;
	protected CanvasWidget m_wScanlines;
	protected Widget m_wMapClip;
	protected CanvasWidget m_wMapCanvas;
	protected CanvasWidget m_wMapOverlay;

	protected ref AROR_OpsCrest m_Crest = new AROR_OpsCrest();
	protected ref AROR_HiCommMap m_Map;
	protected ref array<ref CanvasWidgetCommand> m_aScanlines = {};
	protected float m_fScanlineWidth;
	protected float m_fScanlineHeight;

	// Palette (the OPS tablet's): screen, panels, lines, phosphor, amber, red, text
	protected ref Color m_Screen = AROR_HiCommMap.Srgb(11, 15, 10, 1);
	protected ref Color m_PanelFill = AROR_HiCommMap.Srgb(17, 23, 16, 1);
	protected ref Color m_Line = AROR_HiCommMap.Srgb(38, 50, 31, 1);
	protected ref Color m_LineHi = AROR_HiCommMap.Srgb(60, 77, 48, 1);
	protected ref Color m_Phos = AROR_HiCommMap.Srgb(159, 211, 107, 1);
	protected ref Color m_PhosDim = AROR_HiCommMap.Srgb(95, 127, 70, 1);
	protected ref Color m_PhosFaint = AROR_HiCommMap.Srgb(43, 58, 34, 1);
	protected ref Color m_Amber = AROR_HiCommMap.Srgb(230, 168, 58, 1);
	protected ref Color m_Red = AROR_HiCommMap.Srgb(224, 87, 63, 1);
	protected ref Color m_Text = AROR_HiCommMap.Srgb(207, 220, 192, 1);
	protected ref Color m_Muted = AROR_HiCommMap.Srgb(124, 138, 110, 1);
	protected ref Color m_PhosFill = AROR_HiCommMap.Srgb(159, 211, 107, 0.08);
	protected ref Color m_AmberFill = AROR_HiCommMap.Srgb(230, 168, 58, 0.08);
	protected ref Color m_RedFill = AROR_HiCommMap.Srgb(224, 87, 63, 0.1);
	protected ref Color m_DisabledFill = AROR_HiCommMap.Srgb(159, 211, 107, 0.02);
	protected ref Color m_Selected = AROR_HiCommMap.Srgb(159, 211, 107, 0.09);
	protected ref Color m_StatusFill = AROR_HiCommMap.Srgb(230, 168, 58, 0.07);
	protected ref Color m_Clear = Color.FromRGBA(0, 0, 0, 0);

	// State (reset each time the phone opens)
	protected AROR_EOpsScreen m_eScreen = AROR_EOpsScreen.LOGIN;
	protected AROR_EOpsScreen m_eLastScreen = -1;
	protected int m_iLoginStart;
	protected int m_iVerifyStart;
	protected string m_sCategory;			//!< Empty for all
	protected int m_iSelectedId = -1;
	protected int m_iMapFocusId = -1;
	protected vector m_vMapFocus;
	protected vector m_vMapCentre;
	protected int m_iAbortArmedId = -1;
	protected int m_iAbortArmedAt;
	protected int m_iLastFrameMs;

	//------------------------------------------------------------------------------------------------
	bool Init(Widget phoneRoot)
	{
		if (!InitPools(phoneRoot, "Ops", PANEL_POOL, TEXT_POOL, BUTTON_POOL))
			return false;

		m_wCrest = CanvasWidget.Cast(phoneRoot.FindAnyWidget("OpsCrestCanvas"));
		m_wScanlines = CanvasWidget.Cast(phoneRoot.FindAnyWidget("OpsScanlines"));
		m_wMapClip = phoneRoot.FindAnyWidget("OpsMapClip");
		m_wMapCanvas = CanvasWidget.Cast(phoneRoot.FindAnyWidget("OpsMapCanvas"));
		m_wMapOverlay = CanvasWidget.Cast(phoneRoot.FindAnyWidget("OpsMapOverlay"));
		if (!m_wCrest || !m_wScanlines || !m_wMapClip || !m_wMapCanvas || !m_wMapOverlay)
			return false;

		m_wBackground.SetColor(m_Screen);
		m_Map = new AROR_HiCommMap(m_wMapCanvas, m_wMapOverlay);
		SetVisible(false, 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while shown: left/top is the landscape screen's corner, scale the phone's current scale
	void Layout(float left, float top, float scale)
	{
		BeginFrame(left, top, scale, CONTENT_SCALE);

		int now = System.GetTickCount();
		float deltaTime = 0;
		if (m_iLastFrameMs > 0)
			deltaTime = Math.Clamp((now - m_iLastFrameMs) / 1000.0, 0, 0.1);

		m_iLastFrameMs = now;

		// The boot log replays each time the login screen is shown
		if (m_eScreen == AROR_EOpsScreen.LOGIN && m_eLastScreen != AROR_EOpsScreen.LOGIN)
			m_iLoginStart = now;

		m_eLastScreen = m_eScreen;

		// Faint screen edge, then the bezel strip
		Frame(2, 2, WIDTH - 4, HEIGHT - 4, 1, m_Line);
		DrawBezel();

		bool showCrest;
		bool showMap;
		if (m_eScreen == AROR_EOpsScreen.LOGIN)
		{
			// The sign-in column is narrow; draw it enlarged so it fills the tablet screen
			float scaleBefore = m_fScale;
			float insetXBefore = m_fInsetX;
			float insetYBefore = m_fInsetY;
			ZoomContent(LOGIN_ZOOM, LOGIN_CENTRE_X, LOGIN_CENTRE_Y, WIDTH * 0.5, (TOP_BAR + HEIGHT) * 0.5);
			DrawLogin(now);
			m_fScale = scaleBefore;
			m_fInsetX = insetXBefore;
			m_fInsetY = insetYBefore;
			showCrest = true;
		}
		else
		{
			showMap = DrawBoard(now, deltaTime);
		}

		m_wCrest.SetVisible(showCrest);
		m_wMapClip.SetVisible(showMap);
		m_wMapOverlay.SetVisible(showMap);
		DrawScanlines(scale);
		EndFrame();
	}

	//------------------------------------------------------------------------------------------------
	// Screens
	//------------------------------------------------------------------------------------------------
	protected void DrawBezel()
	{
		Panel(28, 12, 7, 7, m_Phos);
		Text(42, 6, 400, 20, 12, "OPS-T4 FIELD TABLET", m_Muted);
		TextRight(1672, 6, 12, GetClockText() + " LOCAL", m_Muted);
	}

	//------------------------------------------------------------------------------------------------
	//! Until the frame ends or the caller restores it: app point (fromX, fromY) is drawn at (toX, toY), zoom times larger
	protected void ZoomContent(float zoom, float fromX, float fromY, float toX, float toY)
	{
		m_fInsetX += (toX - fromX * zoom) * m_fScale;
		m_fInsetY += (toY - fromY * zoom) * m_fScale;
		m_fScale *= zoom;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawLogin(int now)
	{
		float elapsed = (now - m_iLoginStart) / 1000.0;
		float left = 590;
		float width = 520;

		// Crest and title
		float crestSize = 96;
		Place(m_wCrest, left * m_fScale, 104 * m_fScale, crestSize * m_fScale, crestSize * m_fScale);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float crestPixels = workspace.DPIScale(crestSize * m_fScale);
		m_Crest.Draw(m_wCrest, crestPixels, crestPixels, elapsed, workspace.DPIScale(m_fScale));
		Text(left + 116, 98, 300, 76, 64, "OPS", m_Phos, true);
		Text(left + 120, 174, 420, 22, 14, "RESISTANCE OPERATIONS NETWORK", m_Muted);

		// Boot log, typed line by line
		AROR_MissionBoardComponent board = AROR_MissionBoardComponent.GetInstance();
		int offers;
		if (board)
		{
			array<AROR_MissionInfo> missions = {};
			board.GetMissions(missions);
			foreach (AROR_MissionInfo mission : missions)
			{
				if (!mission.m_bActive)
					offers++;
			}
		}

		array<string> labels = {"Encrypted uplink", "Cell roster", "Intel feed", "Enemy crackdown"};
		array<string> states = {"OK", "SYNCED", string.Format("%1 OPS", offers), CrackdownText()};
		array<ref Color> colors = {m_Phos, m_Phos, m_Phos, CrackdownColor()};
		if (!board)
		{
			states[2] = "OFFLINE";
			colors[2] = m_Red;
		}

		Panel(left, 228, 2, 160, m_PhosFaint);
		int shown;
		foreach (int i, string label : labels)
		{
			if (elapsed < 0.26 * (i + 1))
				break;

			Text(left + 16, 234 + i * 30, 340, 26, 16, "> " + Dots(label, 24), m_PhosDim);
			Text(left + 340, 234 + i * 30, 180, 26, 16, states[i], colors[i], true);
			shown++;
		}

		if (elapsed >= 1.3)
		{
			string prompt = "> awaiting credentials";
			if (Math.Mod(now / 500, 2) == 0)
				prompt += "_";

			Text(left + 16, 234 + shown * 30, 400, 26, 16, prompt, m_PhosDim);
		}

		if (elapsed < 1.4)
			return;

		// Identity: callsign, clearance and resistance
		int localId = GetLocalPlayerId();
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		bool leader = highCommand && highCommand.IsLeader(localId);
		string clearance = "OPERATIVE";
		Color clearanceColor = m_Phos;
		if (leader)
		{
			clearance = "LEADER";
			clearanceColor = m_Amber;
		}

		string resistance = "RESISTANCE";
		if (highCommand)
			resistance = Upper(highCommand.GetResistanceName());

		Field(left, 408, width * 0.5 - 8, "CALLSIGN", Clip(Upper(GetPlayerName(localId)), 18), m_Phos);
		Field(left + width * 0.5 + 8, 408, width * 0.5 - 8, "CLEARANCE", clearance, clearanceColor);
		Field(left, 496, width, "RESISTANCE", Clip(resistance, 34), m_Phos);

		if (leader)
			Text(left, 580, width, 22, 14, "Accepted operations are marked on every Resistance map.", m_Muted);
		else
			Text(left, 580, width, 22, 14, "View only: a Resistance Leader accepts operations.", m_Amber);

		if (m_iVerifyStart > 0)
		{
			OutlineButton(left, 612, width, 56, "VERIFYING...", string.Empty, m_Phos, 20);
			if (now - m_iVerifyStart >= VERIFY_MS)
			{
				m_iVerifyStart = 0;
				m_eScreen = AROR_EOpsScreen.BOARD;
			}
		}
		else
		{
			OutlineButton(left, 612, width, 56, "AUTHENTICATE", "login", m_Phos, 20);
		}

		// Classification strip
		for (float dash = left; dash < left + width; dash += 26)
		{
			Panel(dash, 700, 14, 1, m_Line);
		}

		Text(left, 712, width, 18, 12, "RESTRICTED  ·  RESISTANCE CELLS ONLY  ·  SESSION LOGGED", m_Muted);
	}

	//------------------------------------------------------------------------------------------------
	//! A labelled read-only field box
	protected void Field(float x, float y, float width, string label, string value, Color color)
	{
		Text(x, y, width, 18, 12, label, m_Muted, true);
		Frame(x, y + 22, width, 52, 1.5, m_LineHi);
		Panel(x + 1.5, y + 23.5, width - 3, 49, m_PanelFill);
		Text(x + 14, y + 34, width - 28, 30, 21, value, color, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the area map is shown
	protected bool DrawBoard(int now, float deltaTime)
	{
		AROR_MissionBoardComponent board = AROR_MissionBoardComponent.GetInstance();
		DrawTopBar(board);

		array<AROR_MissionInfo> missions = {};
		if (board)
			board.GetMissions(missions);

		// Active missions first, then the offers, in board order
		array<AROR_MissionInfo> ordered = {};
		foreach (AROR_MissionInfo active : missions)
		{
			if (active.m_bActive)
				ordered.Insert(active);
		}

		foreach (AROR_MissionInfo offer : missions)
		{
			if (!offer.m_bActive)
				ordered.Insert(offer);
		}

		DrawFilters(ordered);

		array<AROR_MissionInfo> visible = {};
		foreach (AROR_MissionInfo info : ordered)
		{
			if (m_sCategory.IsEmpty() || AROR_MissionTexts.Category(info.m_eType) == m_sCategory)
				visible.Insert(info);
		}

		Panel(LIST_LEFT + LIST_WIDTH + 12, BODY_TOP, 1, BODY_BOTTOM - BODY_TOP, m_Line);
		AROR_MissionInfo selected = FindSelected(visible);
		DrawList(board, visible);
		DrawLog(board);

		if (!selected)
		{
			DrawEmptyDossier(board);
			return false;
		}

		return DrawDossier(board, selected, now, deltaTime);
	}

	//------------------------------------------------------------------------------------------------
	//! The selected mission if it is visible, otherwise the first visible one (which becomes selected)
	protected AROR_MissionInfo FindSelected(array<AROR_MissionInfo> visible)
	{
		foreach (AROR_MissionInfo info : visible)
		{
			if (info.m_iId == m_iSelectedId)
				return info;
		}

		if (visible.IsEmpty())
		{
			m_iSelectedId = -1;
			return null;
		}

		m_iSelectedId = visible[0].m_iId;
		return visible[0];
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTopBar(AROR_MissionBoardComponent board)
	{
		Panel(0, TOP_BAR, WIDTH, 78, m_PanelFill);
		Panel(0, TOP_BAR + 78, WIDTH, 1.5, m_LineHi);

		Text(28, TOP_BAR + 8, 140, 60, 44, "OPS", m_Phos, true);
		string resistance = "RESISTANCE";
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (highCommand)
			resistance = Upper(highCommand.GetResistanceName());

		Text(132, TOP_BAR + 34, 700, 22, 14, Clip(Upper(GetPlayerName(GetLocalPlayerId())), 18) + "  ·  " + Clip(resistance, 30), m_Muted);

		string funds = "$ -";
		string manpower = "-";
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (economy)
		{
			funds = AROR_EconomyComponent.FormatMoney(economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE));
			manpower = economy.GetManpower().ToString();
		}

		int active;
		int maximum = 3;
		if (board)
		{
			active = board.CountActive();
			maximum = board.GetMaxActive();
		}

		Color activeColor = m_Text;
		if (active >= maximum)
			activeColor = m_Amber;

		Stat(1100, "FUNDS", funds, m_Text);
		Stat(1260, "MANPOWER", manpower, m_Text);
		Stat(1390, "ACTIVE OPS", string.Format("%1/%2", active, maximum), activeColor);
		OutlineButton(1560, TOP_BAR + 22, 112, 36, "LOG OUT", "logout", m_Muted, 13);
	}

	//------------------------------------------------------------------------------------------------
	protected void Stat(float x, string label, string value, Color color)
	{
		Text(x, TOP_BAR + 14, 200, 30, 22, value, color, true);
		Text(x, TOP_BAR + 46, 200, 18, 11, label, m_Muted, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawFilters(array<AROR_MissionInfo> missions)
	{
		array<string> categories = {string.Empty, AROR_MissionTexts.CATEGORY_LOGISTICS, AROR_MissionTexts.CATEGORY_RESCUE, AROR_MissionTexts.CATEGORY_ASSAULT, AROR_MissionTexts.CATEGORY_SABOTAGE};
		float x = LIST_LEFT;
		float y = TOP_BAR + 90;
		foreach (string category : categories)
		{
			int count;
			foreach (AROR_MissionInfo info : missions)
			{
				if (category.IsEmpty() || AROR_MissionTexts.Category(info.m_eType) == category)
					count++;
			}

			string name = "ALL";
			if (!category.IsEmpty())
				name = Upper(category);

			string label = string.Format("%1  %2", name, count);
			float width = TextWidth(label, 13) + 30;
			if (category == m_sCategory)
			{
				Button(x, y, width, 32, label, "cat:" + category, m_Phos, m_Screen, 13);
			}
			else
			{
				Button(x, y, width, 32, label, "cat:" + category, m_PanelFill, m_Muted, 13);
			}

			x += width + 8;
		}

		Panel(0, BODY_TOP - 8, WIDTH, 1, m_Line);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawList(AROR_MissionBoardComponent board, array<AROR_MissionInfo> visible)
	{
		if (visible.IsEmpty())
		{
			Text(LIST_LEFT + 18, BODY_TOP + 20, LIST_WIDTH - 36, 60, 15, "No operations here. New intel arrives every few minutes.", m_Muted);
			return;
		}

		int rows = Math.Floor((LOG_TOP - BODY_TOP - 12) / ROW_HEIGHT);
		foreach (int i, AROR_MissionInfo info : visible)
		{
			if (i >= rows)
				break;

			float y = BODY_TOP + i * ROW_HEIGHT;
			bool selected = info.m_iId == m_iSelectedId;
			if (selected)
				Panel(LIST_LEFT, y, LIST_WIDTH, ROW_HEIGHT - 1, m_Selected);

			Panel(LIST_LEFT, y, 4, ROW_HEIGHT - 1, RiskColor(info.m_iRisk, true));

			Color nameColor = m_Text;
			if (selected)
				nameColor = m_Phos;

			Text(LIST_LEFT + 18, y + 10, 360, 26, 18, Upper(AROR_MissionTexts.TypeName(info.m_eType)), nameColor, true);
			string meta = string.Format("OPS-%1  ·  %2  ·  GRID %3", info.m_iId.ToString(2), Upper(AROR_MissionTexts.Category(info.m_eType)), AROR_HighCommandComponent.GridReference(info.m_vPosition));
			Text(LIST_LEFT + 18, y + 38, 390, 20, 13, meta, m_Muted);

			string tag = "OPEN";
			Color tagColor = m_PhosDim;
			string eta = string.Format("%1 MIN", info.m_iTimeLimit / 60);
			if (info.m_bActive)
			{
				int seconds = 0;
				if (board)
					seconds = board.GetSecondsLeft(info);

				tag = "ACTIVE";
				tagColor = m_Amber;
				if (seconds < 300)
				{
					tag = "URGENT";
					tagColor = m_Red;
				}

				eta = AROR_MissionTexts.Countdown(seconds);
			}

			float right = LIST_LEFT + LIST_WIDTH - 14;
			Tag(right, y + 11, tag, tagColor);
			TextRight(right, y + 38, 13, eta, m_Muted);
			Panel(LIST_LEFT, y + ROW_HEIGHT - 1, LIST_WIDTH, 1, m_Line);
			HitArea(LIST_LEFT, y, LIST_WIDTH, ROW_HEIGHT - 1, "sel:" + info.m_iId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawLog(AROR_MissionBoardComponent board)
	{
		SectionHeader(LIST_LEFT, LOG_TOP, LIST_WIDTH, "NETWORK LOG");
		array<string> lines = {};
		if (board)
			board.GetLogLines(lines);

		if (lines.IsEmpty())
		{
			Text(LIST_LEFT, LOG_TOP + 26, LIST_WIDTH, 20, 13, "No traffic yet.", m_PhosDim);
			return;
		}

		foreach (int i, string line : lines)
		{
			if (i >= 5)
				break;

			Color color = m_PhosDim;
			if (i == 0)
				color = m_Text;

			Text(LIST_LEFT, LOG_TOP + 26 + i * 22, LIST_WIDTH, 20, 13, Clip(line, 72), color);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawEmptyDossier(AROR_MissionBoardComponent board)
	{
		string text = "Waiting for intel. Operations appear here as the network reports them.";
		if (!board)
			text = "The operations network is offline.";

		TextCentre((DOSSIER_LEFT + DOSSIER_RIGHT) * 0.5, 480, 16, text, m_Muted);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true (the area map is shown)
	protected bool DrawDossier(AROR_MissionBoardComponent board, AROR_MissionInfo info, int now, float deltaTime)
	{
		float x = DOSSIER_LEFT;
		float width = DOSSIER_RIGHT - DOSSIER_LEFT;
		string typeName = AROR_MissionTexts.TypeName(info.m_eType);
		string category = AROR_MissionTexts.Category(info.m_eType);

		// Heading
		Text(x, BODY_TOP + 2, 600, 20, 13, string.Format("OPS-%1  ·  %2", info.m_iId.ToString(2), Upper(category)), m_Muted, true);
		Text(x, BODY_TOP + 20, 900, 48, 38, Upper(typeName), m_Phos, true);
		Text(x, BODY_TOP + 70, 600, 22, 16, info.m_sCodename, m_Amber, true);
		if (info.m_bActive)
			Tag(DOSSIER_RIGHT, BODY_TOP + 6, "ACTIVE  ·  " + Clip(Upper(info.m_sLeaderName), 16), m_Amber);

		// Facts
		int seconds = 0;
		if (board)
			seconds = board.GetSecondsLeft(info);

		float factsTop = BODY_TOP + 102;
		float factHeight = 62;
		float cell = (width - 3) / 4;
		Panel(x, factsTop, width, factHeight, m_Line);
		string timeLabel = "TIME LIMIT";
		string timeValue = string.Format("%1 min", info.m_iTimeLimit / 60);
		Color timeColor = m_Text;
		if (info.m_bActive)
		{
			timeLabel = "REMAINING";
			timeValue = AROR_MissionTexts.Countdown(seconds);
			timeColor = m_Amber;
			if (seconds < 300)
				timeColor = m_Red;
		}

		Fact(x, factsTop, cell, factHeight, 0, "THREAT", AROR_MissionTexts.RiskName(info.m_iRisk), RiskColor(info.m_iRisk, false));
		Fact(x, factsTop, cell, factHeight, 1, timeLabel, timeValue, timeColor);
		Fact(x, factsTop, cell, factHeight, 2, "SQUAD SIZE", AROR_MissionTexts.SquadSize(info.m_eType, info.m_iRisk), m_Text);
		Fact(x, factsTop, cell, factHeight, 3, "GRID", AROR_HighCommandComponent.GridReference(info.m_vPosition), m_Text);

		// Status banner while the operation runs
		float columnsTop = factsTop + factHeight + 22;
		if (info.m_bActive)
		{
			Panel(x, columnsTop - 8, width, 36, m_StatusFill);
			Panel(x, columnsTop - 8, 3, 36, m_Amber);
			Text(x + 16, columnsTop - 1, width - 32, 24, 16, Clip(info.m_sProgress, 120), m_Amber, true);
			columnsTop += 44;
		}

		// Left column: situation, objectives, failure, outcome
		float leftWidth = width - MAP_WIDTH - 36;
		float y = columnsTop;
		string brief = AROR_MissionTexts.Brief(info);
		int briefLines = LineCount(brief, leftWidth, 15);
		SectionHeader(x, y, leftWidth, "SITUATION");
		y += 26;
		Text(x, y, leftWidth, briefLines * 21 + 4, 15, brief, m_Text);
		y += briefLines * 21 + 16;

		array<string> lines = {};
		AROR_MissionTexts.Objectives(info, lines);
		y = Bullets(x, y, leftWidth, "OBJECTIVES", lines, m_Phos);

		AROR_MissionTexts.Failures(info, lines);
		y = Bullets(x, y, leftWidth, "FAILURE IF", lines, m_Red);

		SectionHeader(x, y, leftWidth, "OUTCOME ON SUCCESS");
		y += 28;
		float chipX = Chip(x, y, "+" + AROR_EconomyComponent.FormatMoney(info.m_iFunds), m_Phos);
		string manpower = string.Format("+%1 MP", info.m_iManpower);
		if (info.m_eType == AROR_EMissionType.PRISONER_RESCUE)
			manpower = string.Format("UP TO +%1 MP", info.m_iManpower);

		chipX = Chip(chipX + 8, y, manpower, m_Phos);
		Chip(chipX + 8, y, "OPPOSITION: " + Upper(AROR_MissionTexts.EnemyName(info.m_eEnemyRole)), EnemyColor(info.m_eEnemyRole));

		// Right column: area map and kit
		float mapX = DOSSIER_RIGHT - MAP_WIDTH;
		SectionHeader(mapX, columnsTop, MAP_WIDTH, "AREA");
		float mapY = columnsTop + 26;
		Frame(mapX, mapY, MAP_WIDTH, MAP_HEIGHT, 1.5, m_LineHi);
		DrawAreaMap(info, mapX + 1.5, mapY + 1.5, MAP_WIDTH - 3, MAP_HEIGHT - 3, deltaTime);
		Button(mapX + MAP_WIDTH - 68, mapY + 8, 28, 28, "+", "zoomin", m_PanelFill, m_Phos, 16);
		Button(mapX + MAP_WIDTH - 36, mapY + 8, 28, 28, "-", "zoomout", m_PanelFill, m_Phos, 16);

		float captionY = mapY + MAP_HEIGHT + 4;
		Text(mapX, captionY, MAP_WIDTH - 90, 20, 13, Clip(AROR_MissionTexts.LocationName(info.m_sLocationId), 34), m_Muted);
		TextRight(DOSSIER_RIGHT, captionY, 13, AROR_HighCommandComponent.GridReference(info.m_vPosition), m_Muted);

		float kitY = captionY + 32;
		SectionHeader(mapX, kitY, MAP_WIDTH, "RECOMMENDED KIT");
		kitY += 26;
		array<string> kitNames = {};
		array<string> kitAmounts = {};
		AROR_MissionTexts.Kit(info.m_eType, kitNames, kitAmounts);
		foreach (int k, string kitName : kitNames)
		{
			if (kitY + 22 > ACTIONS_TOP - 6)
				break;

			Text(mapX, kitY, MAP_WIDTH - 60, 20, 14, kitName, m_Text);
			TextRight(DOSSIER_RIGHT, kitY, 14, kitAmounts[k], m_Muted);
			Panel(mapX, kitY + 21, MAP_WIDTH, 1, m_Line);

			kitY += 26;
		}

		DrawActions(board, info, now);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One cell of the facts strip
	protected void Fact(float x, float y, float cell, float height, int index, string label, string value, Color color)
	{
		float left = x + index * (cell + 1);
		Panel(left, y + 1, cell, height - 2, m_PanelFill);
		Text(left + 14, y + 9, cell - 28, 16, 11, label, m_Muted, true);
		Text(left + 14, y + 28, cell - 28, 26, 19, value, color, true);
	}

	//------------------------------------------------------------------------------------------------
	//! A titled bullet list; returns the y below it
	protected float Bullets(float x, float y, float width, string title, array<string> lines, Color bullet)
	{
		SectionHeader(x, y, width, title);
		y += 26;
		foreach (string line : lines)
		{
			Panel(x + 2, y + 8, 6, 6, bullet);
			Text(x + 18, y, width - 18, 22, 15, line, m_Text);
			y += 23;
		}

		return y + 12;
	}

	//------------------------------------------------------------------------------------------------
	//! Accept (offers) or abort (the running leader's own operations), with a note
	protected void DrawActions(AROR_MissionBoardComponent board, AROR_MissionInfo info, int now)
	{
		float x = DOSSIER_LEFT;
		Panel(x, ACTIONS_TOP, DOSSIER_RIGHT - x, 1, m_Line);
		float buttonY = ACTIONS_TOP + 14;
		float noteX = x + 330;

		int localId = GetLocalPlayerId();
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		bool leader = highCommand && highCommand.IsLeader(localId);

		if (!info.m_bActive)
		{
			int active;
			int maximum = 3;
			if (board)
			{
				active = board.CountActive();
				maximum = board.GetMaxActive();
			}

			if (!leader)
			{
				OutlineButton(x, buttonY, 300, 54, "ACCEPT OPERATION", string.Empty, m_Phos, 18);
				Text(noteX, buttonY + 15, 760, 24, 15, "Only a Resistance Leader can accept operations.", m_Muted);
			}
			else if (active >= maximum)
			{
				OutlineButton(x, buttonY, 300, 54, "ACCEPT OPERATION", string.Empty, m_Phos, 18);
				Text(noteX, buttonY + 15, 760, 24, 15, string.Format("The Resistance already runs %1 operations. Finish or abort one first.", maximum), m_Amber);
			}
			else
			{
				OutlineButton(x, buttonY, 300, 54, "ACCEPT OPERATION", "accept:" + info.m_iId, m_Phos, 18);
				string expires = AROR_MissionTexts.Countdown(GetSecondsLeft(board, info));
				Text(noteX, buttonY + 4, 760, 24, 15, "Accepting marks the objective on every Resistance map.", m_Muted);
				Text(noteX, buttonY + 28, 760, 22, 13, "Intel expires in " + expires, m_PhosDim);
			}

			return;
		}

		bool owner = leader && (info.m_iLeaderId == 0 || info.m_iLeaderId == localId || (highCommand && highCommand.IsHost(localId)));
		if (!owner)
		{
			Text(x, buttonY + 15, DOSSIER_RIGHT - x, 24, 15, string.Format("Run by %1. Marked on every Resistance map.", info.m_sLeaderName), m_Muted);
			return;
		}

		bool armed = m_iAbortArmedId == info.m_iId && now - m_iAbortArmedAt < ABORT_CONFIRM_MS;
		if (armed)
		{
			OutlineButton(x, buttonY, 300, 54, "CONFIRM ABORT", "abort:" + info.m_iId, m_Red, 18);
			Text(noteX, buttonY + 15, 760, 24, 15, "Click again to abort. The reward is lost; nothing else is.", m_Red);
		}
		else
		{
			OutlineButton(x, buttonY, 300, 54, "ABORT OPERATION", "abort:" + info.m_iId, m_Amber, 18);
			Text(noteX, buttonY + 15, 760, 24, 15, "Marked on every Resistance map and in HICOMM.", m_Muted);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static int GetSecondsLeft(AROR_MissionBoardComponent board, AROR_MissionInfo info)
	{
		if (!board)
			return info.m_iSeconds;

		return board.GetSecondsLeft(info);
	}

	//------------------------------------------------------------------------------------------------
	// Area map
	//------------------------------------------------------------------------------------------------
	protected void DrawAreaMap(AROR_MissionInfo info, float x, float y, float width, float height, float deltaTime)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Place(m_wMapClip, x * m_fScale, y * m_fScale, width * m_fScale, height * m_fScale);
		Place(m_wMapOverlay, x * m_fScale, y * m_fScale, width * m_fScale, height * m_fScale);

		vector min = "0 0 0";
		vector max = "12800 0 12800";
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (highCommand)
			highCommand.GetMapBounds(min, max);

		m_Map.SetView(min, max, workspace.DPIScale(width * m_fScale), workspace.DPIScale(height * m_fScale));

		// A new selection (or a moving objective) refocuses: zoom to the area, glide the centre over
		vector focus = info.m_vPosition;
		if (info.m_bHasDestination)
			focus = (info.m_vPosition + info.m_vDestination) * 0.5;

		if (m_iMapFocusId != info.m_iId)
		{
			if (m_iMapFocusId < 0)
				m_vMapCentre = focus;

			m_iMapFocusId = info.m_iId;
			float span = Math.Max(Math.Max(max[0] - min[0], max[2] - min[2]), 1);
			float metres = MAP_VIEW_METRES;
			if (info.m_bHasDestination)
				metres = Math.Max(metres, vector.DistanceXZ(info.m_vPosition, info.m_vDestination) * 1.5);

			m_Map.SetTargetZoom(span / metres, m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);
			m_Map.CentreOn(m_vMapCentre);
		}

		// Glide towards the focus; once there, leave the view alone so the terrain layer can settle
		m_vMapFocus = focus;
		if (vector.DistanceXZ(m_vMapCentre, m_vMapFocus) > 1)
		{
			float step = Math.Clamp(deltaTime * MAP_CENTRE_SMOOTHING, 0, 1);
			m_vMapCentre = m_vMapCentre + (m_vMapFocus - m_vMapCentre) * step;
			m_Map.CentreOn(m_vMapCentre);
		}

		m_Map.Animate(deltaTime);
		m_Map.UpdateBase();

		float baseX, baseY, baseWidth, baseHeight;
		m_Map.GetBaseRect(baseX, baseY, baseWidth, baseHeight);
		PlaceRaw(m_wMapCanvas, workspace.DPIUnscale(baseX), workspace.DPIUnscale(baseY), workspace.DPIUnscale(baseWidth), workspace.DPIUnscale(baseHeight));

		DrawAreaOverlay(info, x, y, width, height);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawAreaOverlay(AROR_MissionInfo info, float x, float y, float width, float height)
	{
		float unit = GetGame().GetWorkspace().DPIScale(1);
		float viewWidth = m_Map.GetWidth();
		float viewHeight = m_Map.GetHeight();
		m_Map.BeginOverlay();

		// Locations for context, in their controller's colour
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (locations)
		{
			array<AROR_LocationComponent> all = {};
			locations.GetLocations(all);
			foreach (AROR_LocationComponent location : all)
			{
				if (!location)
					continue;

				vector point = m_Map.WorldToScreen(location.GetCenter());
				if (point[0] < 0 || point[1] < 0 || point[0] > viewWidth || point[1] > viewHeight)
					continue;

				m_Map.DrawSquare(point, 3 * unit, AROR_HiCommMap.Rgb(RoleColor(location.GetControllingRole()), 0.9), true);
			}
		}

		vector target = m_Map.WorldToScreen(info.m_vPosition);
		Color amber = AROR_HiCommMap.Rgb(PAL_AMBER, 1);

		if (info.m_bHasDestination)
		{
			vector destination = m_Map.WorldToScreen(info.m_vDestination);
			m_Map.DrawDashedLine(target, destination, AROR_HiCommMap.Rgb(PAL_AMBER, 0.7), 1.5 * unit, 6 * unit, 5 * unit);
			m_Map.DrawSquare(destination, 6 * unit, AROR_HiCommMap.Rgb(PAL_RESISTANCE, 1), false, 1.5 * unit);
			MapLabel(x, y, width, height, destination, Clip(Upper(AROR_MissionTexts.LocationName(info.m_sDestinationId)), 22), m_Phos);
		}

		if (info.m_fAreaRadius > 0)
		{
			float radius = info.m_fAreaRadius * m_Map.PixelsPerMetre();
			m_Map.DrawCircle(target, radius, AROR_HiCommMap.Rgb(PAL_AMBER, 0.08), true, 1, 36);
			m_Map.DrawCircle(target, radius, AROR_HiCommMap.Rgb(PAL_AMBER, 0.75), false, 1.5 * unit, 36);
		}

		m_Map.DrawReticle(target, 14 * unit, amber);
		m_Map.DrawDiamond(target, 5 * unit, amber, true);
		MapLabel(x, y, width, height, target, info.m_sCodename, m_Amber);

		m_Map.DrawFrameCorners(AROR_HiCommMap.Rgb(PAL_RESISTANCE, 0.6));
		m_Map.CommitOverlay();
	}

	//------------------------------------------------------------------------------------------------
	//! A small label beside a map point (canvas pixels), if it is inside the map
	protected void MapLabel(float mapX, float mapY, float width, float height, vector canvasPoint, string text, Color color)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float appX = mapX + workspace.DPIUnscale(canvasPoint[0]) / m_fScale;
		float appY = mapY + workspace.DPIUnscale(canvasPoint[1]) / m_fScale;
		if (appX < mapX || appY < mapY || appX > mapX + width - 40 || appY > mapY + height - 20)
			return;

		float labelWidth = Math.Min(TextWidth(text, 12) + 12, mapX + width - appX - 12);
		Text(appX + 12, appY - 8, labelWidth, 18, 12, text, color, true);
	}

	//------------------------------------------------------------------------------------------------
	// Scanlines
	//------------------------------------------------------------------------------------------------
	//! Faint horizontal lines over the whole screen (rebuilt only when the screen size changes)
	protected void DrawScanlines(float scale)
	{
		PlaceRaw(m_wScanlines, 0, 0, WIDTH * scale, HEIGHT * scale);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float width = workspace.DPIScale(WIDTH * scale);
		float height = workspace.DPIScale(HEIGHT * scale);
		if (Math.AbsFloat(width - m_fScanlineWidth) < 1 && Math.AbsFloat(height - m_fScanlineHeight) < 1)
			return;

		m_fScanlineWidth = width;
		m_fScanlineHeight = height;
		m_aScanlines.Clear();
		Color shade = Color.FromRGBA(0, 0, 0, 46);
		int color = shade.PackToInt();
		for (float line = 0; line < height; line += 3)
		{
			array<float> vertices = {0, line, width, line, width, line + 1, 0, line + 1};
			PolygonDrawCommand stripe = new PolygonDrawCommand();
			stripe.m_iColor = color;
			stripe.m_Vertices = vertices;
			m_aScanlines.Insert(stripe);
		}

		m_wScanlines.SetDrawCommands(m_aScanlines);
	}

	//------------------------------------------------------------------------------------------------
	// Actions
	//------------------------------------------------------------------------------------------------
	override protected void DoAction(string action)
	{
		array<string> parts = {};
		action.Split(":", parts, false);
		string verb = parts[0];
		string argument;
		if (parts.Count() > 1)
			argument = parts[1];

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (verb == "login")
		{
			m_iVerifyStart = System.GetTickCount();
		}
		else if (verb == "logout")
		{
			m_eScreen = AROR_EOpsScreen.LOGIN;
			m_iMapFocusId = -1;
		}
		else if (verb == "cat")
		{
			m_sCategory = argument;
		}
		else if (verb == "sel")
		{
			m_iSelectedId = argument.ToInt();
			m_iAbortArmedId = -1;
		}
		else if (verb == "accept" && controller)
		{
			controller.AROR_RequestMissionAccept(argument.ToInt());
		}
		else if (verb == "abort" && controller)
		{
			int missionId = argument.ToInt();
			int now = System.GetTickCount();
			if (m_iAbortArmedId == missionId && now - m_iAbortArmedAt < ABORT_CONFIRM_MS)
			{
				m_iAbortArmedId = -1;
				controller.AROR_RequestMissionAbort(missionId);
			}
			else
			{
				m_iAbortArmedId = missionId;
				m_iAbortArmedAt = now;
			}
		}
		else if (verb == "zoomin")
		{
			m_Map.ZoomAt(1.5, m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);
		}
		else if (verb == "zoomout")
		{
			m_Map.ZoomAt(1 / 1.5, m_Map.GetWidth() * 0.5, m_Map.GetHeight() * 0.5);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Drawing helpers
	//------------------------------------------------------------------------------------------------
	//! Outlined button in the tablet style: a thin border, a faint fill of the same colour; empty action = disabled
	protected void OutlineButton(float x, float y, float width, float height, string label, string action, Color color, float size)
	{
		Color border = color;
		Color fill = TintOf(color);
		Color labelColor = color;
		if (action.IsEmpty())
		{
			border = m_LineHi;
			fill = m_DisabledFill;
			labelColor = m_PhosFaint;
		}

		Frame(x, y, width, height, 1.5, border);
		Button(x + 1.5, y + 1.5, width - 3, height - 3, label, action, fill, labelColor, size);
	}

	//------------------------------------------------------------------------------------------------
	protected Color TintOf(Color color)
	{
		if (color == m_Amber)
			return m_AmberFill;

		if (color == m_Red)
			return m_RedFill;

		if (color == m_PhosDim || color == m_Muted)
			return m_DisabledFill;

		return m_PhosFill;
	}

	//------------------------------------------------------------------------------------------------
	//! Small outlined tag ending at right
	protected void Tag(float right, float y, string label, Color color)
	{
		float width = TextWidth(label, 11) + 20;
		Panel(right - width, y, width, 22, TintOf(color));
		Panel(right - width, y, 2, 22, color);
		Text(right - width + 10, y + 4, width, 16, 11, label, color, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Outlined reward chip; returns its right edge
	protected float Chip(float x, float y, string label, Color color)
	{
		float width = TextWidth(label, 13) + 20;
		Panel(x, y, width, 28, m_PanelFill);
		Text(x + 10, y + 6, width, 18, 13, label, color, true);
		return x + width;
	}

	//------------------------------------------------------------------------------------------------
	//! Small heading with a rule running to the end of the column
	protected void SectionHeader(float x, float y, float width, string title)
	{
		Text(x, y, width, 18, 12, title, m_PhosDim, true);
		float titleWidth = TextWidth(title, 12) + 12;
		Panel(x + titleWidth, y + 8, Math.Max(width - titleWidth, 0), 1, m_Line);
	}

	//------------------------------------------------------------------------------------------------
	//! Stripe and threat colours: low/moderate phosphor, high amber, extreme red
	protected Color RiskColor(int risk, bool stripe)
	{
		if (risk >= AROR_Mission.RISK_EXTREME)
			return m_Red;

		if (risk == AROR_Mission.RISK_HIGH)
			return m_Amber;

		if (stripe)
			return m_PhosDim;

		return m_Phos;
	}

	//------------------------------------------------------------------------------------------------
	protected Color EnemyColor(AROR_EFactionRole role)
	{
		return AROR_HiCommMap.Rgb(RoleColor(role), 1);
	}

	//------------------------------------------------------------------------------------------------
	protected static int RoleColor(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.OCCUPIERS)
			return PAL_OCCUPIERS;

		if (role == AROR_EFactionRole.INVADERS)
			return PAL_INVADERS;

		if (role == AROR_EFactionRole.RESISTANCE)
			return PAL_RESISTANCE;

		return 0x8A9580;
	}

	//------------------------------------------------------------------------------------------------
	//! The stronger enemy's crackdown in that army's own words (the REBINTEL label tables)
	protected static string CrackdownText()
	{
		AROR_EFactionRole role;
		int level = MaxCrackdownLevel(role);
		if (level < 1)
			return "UNKNOWN";

		return AROR_ThreatLabels.ForRole(role).GetLabel(AROR_EThreatScale.CRACKDOWN, level);
	}

	//------------------------------------------------------------------------------------------------
	//! Amber once crackdown reaches 4; red stays reserved for critical alerts
	protected Color CrackdownColor()
	{
		AROR_EFactionRole role;
		if (MaxCrackdownLevel(role) >= 4)
			return m_Amber;

		return m_Phos;
	}

	//------------------------------------------------------------------------------------------------
	//! -1 without an operations manager or before the server's first reading
	protected static int MaxCrackdownLevel(out AROR_EFactionRole role)
	{
		role = AROR_EFactionRole.OCCUPIERS;
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return -1;

		AROR_OperationManagerComponent manager = AROR_OperationManagerComponent.Cast(gameMode.FindComponent(AROR_OperationManagerComponent));
		if (!manager || !manager.HasThreatState())
			return -1;

		int occupiers = manager.GetCrackdownLevel(AROR_EFactionRole.OCCUPIERS);
		int invaders = manager.GetCrackdownLevel(AROR_EFactionRole.INVADERS);
		if (invaders > occupiers)
		{
			role = AROR_EFactionRole.INVADERS;
			return invaders;
		}
		return occupiers;
	}

	//------------------------------------------------------------------------------------------------
	//! Pads a label with dots to a length, as in the tablet's boot log
	protected static string Dots(string label, int length)
	{
		string padded = label + " ";
		while (padded.Length() < length)
		{
			padded += ".";
		}

		return padded;
	}

	//------------------------------------------------------------------------------------------------
	//! Shortens text to a number of characters with an ellipsis
	protected static string Clip(string text, int length)
	{
		if (text.Length() <= length)
			return text;

		return text.Substring(0, Math.Max(length - 3, 1)) + "...";
	}
}
