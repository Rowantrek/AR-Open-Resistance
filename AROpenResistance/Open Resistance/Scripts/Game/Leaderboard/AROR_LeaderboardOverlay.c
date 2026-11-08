//! Client: the leaderboard as one drop-down scoreboard, Arma 3 style. Its keybind (AROR_ToggleLeaderboard, P; F8 as
//! a spare) slides it down from the top of the screen and back up; it closes by itself when a menu or the map opens.
//! One table holds everything: rank and player, kills per category (infantry first) under each category's icon,
//! assists and score, best score first. The local player is highlighted and always listed, even below the cut.
//! Category icons are the game's own catalogue pictures (the ones Game Master shows) of what the enemy armies field,
//! else of a vehicle of that type from an enabled content pack, so a modded army or pack such as WCS shows its own.
//! Drawn with its own copy of the phone's pooled widget layout, placed straight on the screen.
class AROR_LeaderboardOverlay : AROR_PhonePooledApp
{
	protected static const ResourceName LAYOUT = "{6AB5C0D7B1A4C3E2}UI/Layouts/Phone/AROR_BlackMarket.layout";
	protected static const int Z_ORDER = 900;	//!< Under the notifications overlay (1000)
	protected static const float SLIDE_SECONDS = 0.18;
	protected static const float TOP_MARGIN = 16;	//!< Screen pixels
	protected static const int MAX_ROWS = 12;

	// App units (the pooled layout's 1700-wide canvas)
	protected static const float HEADER_TOP = 64;
	protected static const float HEADER_HEIGHT = 86;
	protected static const float ROWS_TOP = 156;
	protected static const float ROW_HEIGHT = 40;
	protected static const float ROW_STEP = 44;
	protected static const float FOOTER_HEIGHT = 48;
	protected static const float NAME_X = 24;
	protected static const float CATEGORY_LEFT = 660;
	protected static const float CATEGORY_WIDTH = 150;
	protected static const float ASSISTS_X = 1410;
	protected static const float ASSISTS_WIDTH = 140;
	protected static const float SCORE_X = 1550;
	protected static const float SCORE_WIDTH = 140;

	protected Widget m_wResource;
	protected bool m_bOpen;
	protected float m_fShown;	//!< 0 = up out of sight, 1 = fully down

	protected ref array<ResourceName> m_aIcons = {};
	protected string m_sIconKey;
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
	//! Closed and fully retracted (and so already hidden)
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
			m_fShown = Math.Min(1, m_fShown + timeSlice / SLIDE_SECONDS);
		else
			m_fShown = Math.Max(0, m_fShown - timeSlice / SLIDE_SECONDS);

		AROR_LeaderboardComponent board = AROR_LeaderboardComponent.GetInstance();
		bool visible = m_fShown > 0 && board != null;
		SetVisible(visible, 1);
		if (visible)
			Draw(board);
	}

	//------------------------------------------------------------------------------------------------
	protected void Draw(notnull AROR_LeaderboardComponent board)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float screenWidth = workspace.DPIUnscale(workspace.GetWidth());
		float screenHeight = workspace.DPIUnscale(workspace.GetHeight());
		float scale = Math.Min(screenWidth * 0.8 / WIDTH, screenHeight * 0.92 / HEIGHT);

		array<ref AROR_LeaderboardRow> rows = board.GetRows();
		PickRows(rows);
		int rowCount = Math.Max(m_aShown.Count(), 1);
		float height = ROWS_TOP + rowCount * ROW_STEP + FOOTER_HEIGHT;

		// Slides down from above the screen, easing out
		float eased = 1 - Math.Pow(1 - m_fShown, 3);
		float top = TOP_MARGIN - (height * scale + TOP_MARGIN) * (1 - eased);
		BeginFrame((screenWidth - WIDTH * scale) * 0.5, top, scale, 1);
		RefreshIcons(board);

		// Plate, title bar
		Panel(0, 0, WIDTH, height, C(AROR_UIColors.PLATE_STRONG));
		Panel(0, 0, WIDTH, 4, C(AROR_UIColors.REBEL));
		Text(NAME_X, 14, 700, 44, 30, "LEADERBOARD", C(AROR_UIColors.TEXT), true);
		TextRight(WIDTH - NAME_X, 22, 17, string.Format("%1 PLAYERS  ·  P TO CLOSE", rows.Count()), C(AROR_UIColors.TEXT_MUTED), true);

		// Column headings: each category's icon over its name
		float labelY = HEADER_TOP + HEADER_HEIGHT - 30;
		Panel(0, HEADER_TOP, WIDTH, HEADER_HEIGHT, C(AROR_UIColors.SURFACE));
		Text(NAME_X, labelY, 500, 24, 15, "#     PLAYER", C(AROR_UIColors.TEXT_MUTED), true);
		for (int c = 0; c < AROR_LeaderboardComponent.GetCategoryCount(); c++)
		{
			float columnX = CATEGORY_LEFT + c * CATEGORY_WIDTH;
			DrawIcon(c, columnX + 24, HEADER_TOP + 6, CATEGORY_WIDTH - 48, 46);
			TextCentre(columnX + CATEGORY_WIDTH * 0.5, labelY, 15, GetShortName(c), C(AROR_UIColors.TEXT_MUTED), true);
		}

		TextCentre(ASSISTS_X + ASSISTS_WIDTH * 0.5, HEADER_TOP + 16, 20, "HC + SQ", C(AROR_UIColors.TEXT_FAINT), true);
		TextCentre(ASSISTS_X + ASSISTS_WIDTH * 0.5, labelY, 15, "ASSISTS", C(AROR_UIColors.TEXT_MUTED), true);
		TextCentre(SCORE_X + SCORE_WIDTH * 0.5, labelY, 15, "SCORE", C(AROR_UIColors.REBEL), true);

		// Players
		if (m_aShown.IsEmpty())
			Text(NAME_X, ROWS_TOP + 6, 1500, 32, 21, "No kills yet. Kill enemy soldiers and destroy their vehicles to get on the board.", C(AROR_UIColors.TEXT_MUTED));

		int localId = GetLocalPlayerId();
		foreach (int r, AROR_LeaderboardRow row : m_aShown)
		{
			float y = ROWS_TOP + r * ROW_STEP;
			bool isLocal = localId > 0 && row.m_iPlayerId == localId;
			if (isLocal)
			{
				Panel(8, y, WIDTH - 16, ROW_HEIGHT, C(AROR_UIColors.REBEL_DIM));
				Panel(8, y, 4, ROW_HEIGHT, C(AROR_UIColors.REBEL));
			}
			else
			{
				Panel(8, y, WIDTH - 16, ROW_HEIGHT, C(AROR_UIColors.SURFACE_RAISED));
			}

			// Offline players are listed in muted text
			int nameColor = AROR_UIColors.TEXT_MUTED;
			if (row.m_iPlayerId > 0)
				nameColor = AROR_UIColors.TEXT;

			float textY = y + (ROW_HEIGHT - 31) * 0.5;
			Text(NAME_X, textY, 620, 32, 21, string.Format("%1     %2", m_aShownRanks[r], Clip(row.m_sName, 30)), C(nameColor), true);
			for (int k = 0; k < AROR_LeaderboardComponent.GetCategoryCount(); k++)
			{
				int kills = row.m_aKills[k];
				int killColor = AROR_UIColors.TEXT;
				if (kills == 0)
					killColor = AROR_UIColors.TEXT_FAINT;

				TextCentre(CATEGORY_LEFT + k * CATEGORY_WIDTH + CATEGORY_WIDTH * 0.5, textY, 21, kills.ToString(), C(killColor), false);
			}

			int assistColor = AROR_UIColors.TEXT;
			if (row.m_iAssists == 0)
				assistColor = AROR_UIColors.TEXT_FAINT;

			TextCentre(ASSISTS_X + ASSISTS_WIDTH * 0.5, textY, 21, row.m_iAssists.ToString(), C(assistColor), false);
			TextCentre(SCORE_X + SCORE_WIDTH * 0.5, textY, 23, row.m_iScore.ToString(), C(AROR_UIColors.REBEL), true);
		}

		// Footer: what things are worth
		float footerY = ROWS_TOP + rowCount * ROW_STEP + 14;
		Text(NAME_X, footerY, 700, 24, 15, "ASSISTS: KILLS BY YOUR HICOMM SQUADS AND SQUAD AI", C(AROR_UIColors.TEXT_FAINT), true);
		TextRight(WIDTH - NAME_X, footerY, 15, GetPointsLegend(board), C(AROR_UIColors.TEXT_FAINT), true);
		EndFrame();
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
	protected static string GetPointsLegend(notnull AROR_LeaderboardComponent board)
	{
		string legend = "POINTS";
		for (int c = 0; c < AROR_LeaderboardComponent.GetCategoryCount(); c++)
		{
			legend += string.Format("   %1 %2", GetShortName(c), board.GetPoints(c));
		}

		int sharePercent = Math.Round(board.GetAssistShare() * 100);
		legend += string.Format("   ASSIST %1 PERCENT", sharePercent);
		return legend;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetShortName(int category)
	{
		switch (category)
		{
			case AROR_ELeaderboardCategory.INFANTRY:
				return "INFANTRY";
			case AROR_ELeaderboardCategory.LIGHT_VEHICLE:
				return "LIGHT";
			case AROR_ELeaderboardCategory.APC_IFV:
				return "APC / IFV";
			case AROR_ELeaderboardCategory.ARMOUR:
				return "ARMOUR";
			case AROR_ELeaderboardCategory.AIRCRAFT:
				return "AIRCRAFT";
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

	//------------------------------------------------------------------------------------------------
	// Icons
	//------------------------------------------------------------------------------------------------
	//! A category's picture fitted in the box, or its name when the game has no picture for it
	protected void DrawIcon(int category, float x, float y, float width, float height)
	{
		ResourceName image;
		if (m_aIcons.IsIndexValid(category))
			image = m_aIcons[category];

		if (!image.IsEmpty())
		{
			Picture(x, y, width, height, image);
			return;
		}

		TextCentre(x + width * 0.5, y + height * 0.5 - 14, 18, GetShortName(category), C(AROR_UIColors.TEXT_FAINT), true);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the category pictures again when the enemy armies or the content packs change
	protected void RefreshIcons(notnull AROR_LeaderboardComponent board)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
		string key;
		if (presets)
			key = presets.GetPresetId(AROR_EFactionRole.OCCUPIERS) + "/" + presets.GetPresetId(AROR_EFactionRole.INVADERS);

		if (packs)
			key += "/" + packs.GetRevision();

		if (key == m_sIconKey && m_aIcons.Count() == AROR_LeaderboardComponent.GetCategoryCount())
			return;

		m_sIconKey = key;
		m_aIcons.Clear();
		for (int c = 0; c < AROR_LeaderboardComponent.GetCategoryCount(); c++)
		{
			m_aIcons.Insert(FindIcon(c, presets, packs, board));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName FindIcon(int category, AROR_FactionPresetsComponent presets, AROR_ContentPacksComponent packs, notnull AROR_LeaderboardComponent board)
	{
		array<AROR_EFactionRole> roles = {AROR_EFactionRole.OCCUPIERS, AROR_EFactionRole.INVADERS};
		ResourceName image;
		if (category == AROR_ELeaderboardCategory.INFANTRY)
		{
			if (!presets)
				return image;

			foreach (AROR_EFactionRole role : roles)
			{
				AROR_FactionPreset preset = presets.GetPreset(role);
				AROR_FactionPresetUnit unit;
				if (preset)
					unit = preset.GetUnit(AROR_EUnitType.RIFLEMAN);

				if (unit)
					image = AROR_PrefabInfo.GetCharacterImage(unit.GetPrefab());

				if (!image.IsEmpty())
					return image;
			}

			return image;
		}

		array<int> types = {};
		GetIconTypes(category, types);
		if (presets)
		{
			foreach (AROR_EFactionRole army : roles)
			{
				AROR_FactionPreset armyPreset = presets.GetPreset(army);
				if (!armyPreset)
					continue;

				foreach (int type : types)
				{
					array<AROR_FactionPresetEntry> entries = {};
					armyPreset.GetEntries(AROR_EFactionPresetCategory.VEHICLES, type, entries);
					foreach (AROR_FactionPresetEntry entry : entries)
					{
						image = GetVehicleIcon(entry.GetPrefab(), category, board);
						if (!image.IsEmpty())
							return image;
					}
				}
			}
		}

		if (!packs)
			return image;

		// Neither army fields one (vanilla armies have no tanks): any vehicle of the type an enabled pack offers
		array<AROR_ContentPackEntry> offered = {};
		packs.GetEnabledEntries(AROR_EFactionPresetCategory.VEHICLES, offered);
		foreach (int packType : types)
		{
			foreach (AROR_ContentPackEntry offer : offered)
			{
				if (offer.GetTypeValue() != packType)
					continue;

				image = GetVehicleIcon(offer.GetPrefab(), category, board);
				if (!image.IsEmpty())
					return image;
			}
		}

		return image;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle's picture, unless a leaderboard rule puts it in another category (a JLTV is no light vehicle)
	protected static ResourceName GetVehicleIcon(ResourceName prefab, int category, notnull AROR_LeaderboardComponent board)
	{
		int ruled = board.GetRuleCategory(prefab);
		if (ruled >= 0 && ruled != category)
			return string.Empty;

		return AROR_PrefabInfo.GetImage(prefab, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Preset vehicle types to take a category's picture from, most typical first
	protected static void GetIconTypes(int category, notnull array<int> outTypes)
	{
		switch (category)
		{
			case AROR_ELeaderboardCategory.LIGHT_VEHICLE:
				outTypes.Insert(AROR_EVehicleType.ARMED_CAR);
				outTypes.Insert(AROR_EVehicleType.ARMED_TRUCK);
				outTypes.Insert(AROR_EVehicleType.MOTOR_CAR);
				break;
			case AROR_ELeaderboardCategory.APC_IFV:
				outTypes.Insert(AROR_EVehicleType.IFV);
				outTypes.Insert(AROR_EVehicleType.APC);
				break;
			case AROR_ELeaderboardCategory.ARMOUR:
				outTypes.Insert(AROR_EVehicleType.TANK);
				break;
			case AROR_ELeaderboardCategory.AIRCRAFT:
				outTypes.Insert(AROR_EVehicleType.HELI_ATTACK);
				outTypes.Insert(AROR_EVehicleType.HELI_TRANSPORT);
				break;
		}
	}
}
