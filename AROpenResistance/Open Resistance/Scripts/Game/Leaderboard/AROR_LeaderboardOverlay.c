//! Client: the leaderboard as one compact scoreboard in the middle of the screen, Arma 3 style. Its keybind
//! (AROR_ToggleLeaderboard, P; F8 as a spare) fades it in and out; it closes by itself when a menu or the map opens.
//! One plain table: rank, player, kills per category, assists and score, best score first. The local player is
//! marked with an accent bar and green name, and is always listed, even below the cut. A one-line footer says what
//! each kill is worth. Drawn with its own copy of the phone's pooled widget layout, placed straight on the screen.
class AROR_LeaderboardOverlay : AROR_PhonePooledApp
{
	protected static const ResourceName LAYOUT = "{6AB5C0D7B1A4C3E2}UI/Layouts/Phone/AROR_BlackMarket.layout";
	protected static const int Z_ORDER = 900;	//!< Under the notifications overlay (1000)
	protected static const float FADE_SECONDS = 0.15;
	protected static const int MAX_ROWS = 12;

	// App units
	protected static const float BOARD_WIDTH = 1140;
	protected static const float PAD = 28;
	protected static const float TITLE_HEIGHT = 64;
	protected static const float HEADER_HEIGHT = 40;
	protected static const float ROW_HEIGHT = 36;
	protected static const float FOOTER_HEIGHT = 52;
	protected static const float RANK_X = 28;
	protected static const float NAME_X = 84;
	protected static const float FIRST_STAT_CENTRE = 500;	//!< Centre of the first kill column
	protected static const float STAT_STEP = 96;			//!< Between kill column centres
	protected static const float ASSISTS_CENTRE = 990;
	protected static const float SCORE_CENTRE = 1078;
	protected static const float HEADER_SIZE = 15;
	protected static const float ROW_SIZE = 20;

	protected Widget m_wResource;
	protected bool m_bOpen;
	protected float m_fShown;	//!< 0 = hidden, 1 = fully shown

	protected ref array<AROR_LeaderboardRow> m_aShown = {};
	protected ref array<int> m_aShownRanks = {};

	//------------------------------------------------------------------------------------------------
	protected static Color C(int token)
	{
		return AROR_UIColors.Get(token);
	}

	//------------------------------------------------------------------------------------------------
	bool Init()
	{
		m_wResource = GetGame().GetWorkspace().CreateWidgets(LAYOUT);
		if (!m_wResource || !InitPools(m_wResource, "BM", 96, 120, 64, 16, 16))
			return false;

		// The shared layout's Black Market extras are not part of the scoreboard
		array<string> unused = {"BMSearch", "BMWheel", "BlackMarketTileGlyph", "BlackMarketTileLabel", "BlackMarketTileButton"};
		foreach (string widgetName : unused)
		{
			Widget extra = m_wResource.FindAnyWidget(widgetName);
			if (extra)
				extra.SetVisible(false);
		}

		m_wBackground.SetVisible(false);
		m_wResource.SetZOrder(Z_ORDER);
		SetVisible(false, 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Shutdown()
	{
		if (m_wResource)
			m_wResource.RemoveFromHierarchy();

		m_wResource = null;
	}

	//------------------------------------------------------------------------------------------------
	void Toggle()
	{
		if (!m_bOpen && IsBlocked())
			return;

		m_bOpen = !m_bOpen;
	}

	//------------------------------------------------------------------------------------------------
	//! Closed and fully faded out (and so already hidden)
	bool IsIdle()
	{
		return !m_bOpen && m_fShown <= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Menus (pause, inventory, phone, deploy) and the map take the screen
	protected static bool IsBlocked()
	{
		if (GetGame().GetMenuManager().IsAnyMenuOpen())
			return true;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		return mapEntity && mapEntity.IsOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame on the client
	void Update(float timeSlice)
	{
		if (m_bOpen && IsBlocked())
			m_bOpen = false;

		if (m_bOpen)
			m_fShown = Math.Min(1, m_fShown + timeSlice / FADE_SECONDS);
		else
			m_fShown = Math.Max(0, m_fShown - timeSlice / FADE_SECONDS);

		AROR_LeaderboardComponent board = AROR_LeaderboardComponent.GetInstance();
		bool visible = m_fShown > 0 && board != null;
		SetVisible(visible, m_fShown);
		if (visible)
			Draw(board);
	}

	//------------------------------------------------------------------------------------------------
	protected void Draw(notnull AROR_LeaderboardComponent board)
	{
		array<ref AROR_LeaderboardRow> rows = board.GetRows();
		PickRows(rows);
		int rowCount = m_aShown.Count();
		if (rowCount < 1)
			rowCount = 1;

		float rowsTop = TITLE_HEIGHT + HEADER_HEIGHT;
		float height = rowsTop + rowCount * ROW_HEIGHT + FOOTER_HEIGHT;

		// Centred on the screen, about half its width
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float screenWidth = workspace.DPIUnscale(workspace.GetWidth());
		float screenHeight = workspace.DPIUnscale(workspace.GetHeight());
		float scale = Math.Min(screenWidth * 0.55 / BOARD_WIDTH, screenHeight * 0.85 / height);
		BeginFrame((screenWidth - BOARD_WIDTH * scale) * 0.5, (screenHeight - height * scale) * 0.5, scale, 1);

		// Plate and title
		Panel(0, 0, BOARD_WIDTH, height, C(AROR_UIColors.PLATE_STRONG));
		Panel(0, 0, BOARD_WIDTH, 3, C(AROR_UIColors.REBEL));
		Text(PAD, 16, 600, 40, 28, "SCOREBOARD", C(AROR_UIColors.TEXT), true);
		string players = string.Format("%1 PLAYERS", rows.Count());
		if (rows.Count() == 1)
			players = "1 PLAYER";
		TextRight(BOARD_WIDTH - PAD, 24, 16, players, C(AROR_UIColors.TEXT_MUTED), true);

		// Column headings
		float headerY = TITLE_HEIGHT + (HEADER_HEIGHT - HEADER_SIZE * 1.5) * 0.5;
		Panel(PAD, TITLE_HEIGHT, BOARD_WIDTH - PAD * 2, 1, C(AROR_UIColors.LINE));
		Text(RANK_X, headerY, 50, 24, HEADER_SIZE, "#", C(AROR_UIColors.TEXT_MUTED), true);
		Text(NAME_X, headerY, 300, 24, HEADER_SIZE, "PLAYER", C(AROR_UIColors.TEXT_MUTED), true);
		for (int c = 0; c < AROR_LeaderboardComponent.GetCategoryCount(); c++)
		{
			TextCentre(FIRST_STAT_CENTRE + c * STAT_STEP, headerY, HEADER_SIZE, GetShortName(c), C(AROR_UIColors.TEXT_MUTED), true);
		}

		TextCentre(ASSISTS_CENTRE, headerY, HEADER_SIZE, "ASSISTS", C(AROR_UIColors.TEXT_MUTED), true);
		TextCentre(SCORE_CENTRE, headerY, HEADER_SIZE, "SCORE", C(AROR_UIColors.TEXT_MUTED), true);
		Panel(PAD, rowsTop - 1, BOARD_WIDTH - PAD * 2, 1, C(AROR_UIColors.LINE));

		// Players
		if (m_aShown.IsEmpty())
			Text(NAME_X, rowsTop + 6, 1000, 28, 18, "No kills yet. Kill enemy soldiers and destroy their vehicles to get on the board.", C(AROR_UIColors.TEXT_MUTED));

		int localId = GetLocalPlayerId();
		float textOffset = (ROW_HEIGHT - ROW_SIZE * 1.5) * 0.5;
		foreach (int r, AROR_LeaderboardRow row : m_aShown)
		{
			float y = rowsTop + r * ROW_HEIGHT;
			bool isLocal = localId > 0 && row.m_iPlayerId == localId;

			// Every other row slightly raised, so a row reads straight across
			if (r % 2 == 1)
				Panel(PAD, y, BOARD_WIDTH - PAD * 2, ROW_HEIGHT, C(AROR_UIColors.SURFACE_RAISED));
			if (isLocal)
				Panel(PAD - 10, y + 4, 4, ROW_HEIGHT - 8, C(AROR_UIColors.REBEL));

			// You in green; offline players muted
			int nameColor = AROR_UIColors.TEXT;
			if (isLocal)
				nameColor = AROR_UIColors.REBEL;
			else if (row.m_iPlayerId <= 0)
				nameColor = AROR_UIColors.TEXT_MUTED;

			float textY = y + textOffset;
			Text(RANK_X, textY, 50, 30, ROW_SIZE, m_aShownRanks[r].ToString(), C(AROR_UIColors.TEXT_MUTED));
			Text(NAME_X, textY, 340, 30, ROW_SIZE, Clip(row.m_sName, 24), C(nameColor), isLocal);
			for (int k = 0; k < AROR_LeaderboardComponent.GetCategoryCount(); k++)
			{
				TextCentre(FIRST_STAT_CENTRE + k * STAT_STEP, textY, ROW_SIZE, row.m_aKills[k].ToString(), C(CountColor(row.m_aKills[k])));
			}

			TextCentre(ASSISTS_CENTRE, textY, ROW_SIZE, row.m_iAssists.ToString(), C(CountColor(row.m_iAssists)));
			TextCentre(SCORE_CENTRE, textY, ROW_SIZE, row.m_iScore.ToString(), C(AROR_UIColors.TEXT), true);
		}

		// Footer: what things are worth, and how to close
		float footerTop = rowsTop + rowCount * ROW_HEIGHT;
		Panel(PAD, footerTop + 6, BOARD_WIDTH - PAD * 2, 1, C(AROR_UIColors.LINE));
		Text(PAD, footerTop + 18, 900, 24, 14, GetPointsLegend(board), C(AROR_UIColors.TEXT_FAINT));
		TextRight(BOARD_WIDTH - PAD, footerTop + 18, 14, "P  CLOSE", C(AROR_UIColors.TEXT_FAINT), true);
		EndFrame();
	}

	//------------------------------------------------------------------------------------------------
	//! Zeros fade back so the kills that happened stand out
	protected static int CountColor(int count)
	{
		if (count == 0)
			return AROR_UIColors.TEXT_FAINT;

		return AROR_UIColors.TEXT;
	}

	//------------------------------------------------------------------------------------------------
	//! The best MAX_ROWS players; the local player takes the last row when ranked below them
	protected void PickRows(notnull array<ref AROR_LeaderboardRow> rows)
	{
		m_aShown.Clear();
		m_aShownRanks.Clear();
		int localId = GetLocalPlayerId();
		int localIndex = -1;
		foreach (int i, AROR_LeaderboardRow row : rows)
		{
			if (localId > 0 && row.m_iPlayerId == localId)
				localIndex = i;

			if (m_aShown.Count() < MAX_ROWS)
			{
				m_aShown.Insert(row);
				m_aShownRanks.Insert(i + 1);
			}
		}

		if (localIndex >= MAX_ROWS)
		{
			m_aShown[MAX_ROWS - 1] = rows[localIndex];
			m_aShownRanks[MAX_ROWS - 1] = localIndex + 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "POINTS PER KILL  INF 1 · LIGHT 3 · ... · ASSISTS (YOUR AI) 50%"
	protected static string GetPointsLegend(notnull AROR_LeaderboardComponent board)
	{
		string legend = "POINTS PER KILL ";
		for (int c = 0; c < AROR_LeaderboardComponent.GetCategoryCount(); c++)
		{
			if (c > 0)
				legend += "  ·";

			legend += string.Format("  %1 %2", GetShortName(c), board.GetPoints(c));
		}

		int sharePercent = Math.Round(board.GetAssistShare() * 100);
		legend += string.Format("  ·  ASSISTS (YOUR AI) %1 PERCENT", sharePercent);
		return legend;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetShortName(int category)
	{
		switch (category)
		{
			case AROR_ELeaderboardCategory.INFANTRY:
				return "INF";
			case AROR_ELeaderboardCategory.LIGHT_VEHICLE:
				return "LIGHT";
			case AROR_ELeaderboardCategory.APC_IFV:
				return "APC";
			case AROR_ELeaderboardCategory.ARMOUR:
				return "ARMOUR";
			case AROR_ELeaderboardCategory.AIRCRAFT:
				return "AIR";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Clip(string text, int length)
	{
		if (text.Length() <= length)
			return text;

		return text.Substring(0, length - 1) + ".";
	}
}
