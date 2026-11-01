//! The campaign setup screen (see AROR_SetupMenu), drawn immediate-mode from the shared pooled-widget layout on a
//! 1700 x 900 canvas, as the admin menu is. Screens:
//! - START: New campaign or Load campaign.
//! - LOAD: the saved campaigns sent by the server, one card per playthrough (loads its newest save), each expandable
//!   to pick any of its saves.
//! - New campaign, in steps: FACTIONS (an army per role and the content packs whose mods are loaded), SETTINGS (the
//!   campaign settings by category), NAME (the resistance name), then the HQ is placed on the map.
//! The choices are kept in the setup component's AROR_SetupDraft; the server checks them all again.
class AROR_SetupApp : AROR_PhonePooledApp
{
	static const float CANVAS_WIDTH = 1700;
	static const float CANVAS_HEIGHT = 900;

	static const int SCREEN_START = 0;
	static const int SCREEN_LOAD = 1;
	static const int SCREEN_FACTIONS = 2;
	static const int SCREEN_SETTINGS = 3;
	static const int SCREEN_NAME = 4;

	protected static const ResourceName LAYOUT = "{6AB5C0D7B1A4C3E2}UI/Layouts/Phone/AROR_BlackMarket.layout";

	protected static const float LEFT = 40;
	protected static const float CONTENT_WIDTH = 1620;
	protected static const float CONTENT_TOP = 190;
	protected static const float FOOTER_Y = 830;

	protected static const float LOAD_LIST_HEIGHT = 610;	//!< Space for campaign cards and save rows on one page
	protected static const float GROUP_ROW_HEIGHT = 86;
	protected static const float GROUP_ROW_STEP = 98;
	protected static const float SAVE_ROW_HEIGHT = 52;
	protected static const float SAVE_ROW_STEP = 60;
	protected static const float SAVE_ROW_INDENT = 60;
	protected static const float EXPAND_WIDTH = 220;
	protected static const int PACK_ROWS = 4;
	protected static const int SETTING_ROWS = 7;

	protected Widget m_wResource;
	protected AROR_SetupMenu m_Menu;
	protected EditBoxWidget m_wName;
	protected bool m_bNameShown;
	protected bool m_bNameFilled;

	protected int m_iScreen = SCREEN_START;
	protected string m_sMessage;
	protected ref array<AROR_EFactionRole> m_aRoles = {};

	// Load: one row per save from the server, grouped by playthrough (each group's saves together, newest first)
	protected ref array<string> m_aSaves = {};
	protected ref array<int> m_aGroupFirst = {};	//!< Index of each group's first (newest) save
	protected ref array<int> m_aGroupCount = {};
	protected ref array<int> m_aExpandedGroups = {};
	protected ref array<int> m_aLoadRows = {};	//!< Visible rows: group g as -(g + 1), a single save by its index
	protected ref array<int> m_aLoadPageStarts = {};	//!< First visible row of each page
	protected int m_iSavePage;
	protected int m_iSelectedSave = -1;	//!< The save LOAD CAMPAIGN loads (a group's newest, or a picked one)
	protected bool m_bSelectedSingleSave;	//!< A save picked from an expanded group rather than the group itself
	protected bool m_bWaitingForList;

	// New campaign
	protected int m_iOpenRole = -1;	//!< Role whose army dropdown is open, or -1
	protected int m_iPackPage;
	protected int m_iCategory;
	protected int m_iSettingPage;

	//------------------------------------------------------------------------------------------------
	bool Init(Widget parent, AROR_SetupMenu menu)
	{
		m_Menu = menu;
		m_wResource = GetGame().GetWorkspace().CreateWidgets(LAYOUT, parent);
		// Every pool in the layout is registered, even unused ones, so EndFrame hides them (an empty picture draws white)
		if (!m_wResource || !InitPools(m_wResource, "BM", 96, 120, 64, 16, 16))
			return false;

		// The layout's search box serves as the name field
		m_wName = EditBoxWidget.Cast(m_wResource.FindAnyWidget("BMSearch"));
		if (!m_wName)
			return false;

		m_wName.SetColor(C(AROR_UIColors.TEXT));
		m_wName.SetPlaceholderText("Name your resistance");
		m_wName.SetVisible(false);
		// The shared layout also carries the Black Market's phone home tile; it has no place here
		array<string> tileNames = {"BlackMarketTileGlyph", "BlackMarketTileLabel", "BlackMarketTileButton"};
		foreach (string tileName : tileNames)
		{
			Widget tile = m_wResource.FindAnyWidget(tileName);
			if (tile)
				tile.SetVisible(false);
		}

		m_wBackground.SetColor(C(AROR_UIColors.SURFACE));
		AROR_FactionRoles.GetAll(m_aRoles);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Shutdown()
	{
		if (m_wResource)
			m_wResource.RemoveFromHierarchy();

		m_wResource = null;
		m_Menu = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Shows what the setup component holds: its screen, message and (on the save list) the saves
	void SyncFromSetup()
	{
		AROR_SetupUIComponent setup = AROR_SetupUIComponent.GetInstance();
		if (!setup)
			return;

		m_iScreen = setup.GetMenuScreen();
		m_sMessage = setup.GetMenuMessage();
		m_bWaitingForList = false;
		m_bNameFilled = false;
		if (m_iScreen == SCREEN_LOAD)
		{
			m_aSaves.Copy(setup.GetSaveLabels());
			BuildSaveGroups();
			m_aExpandedGroups.Clear();
			m_iSavePage = 0;
			m_bSelectedSingleSave = false;
			m_iSelectedSave = -1;
			if (!m_aSaves.IsEmpty())
				m_iSelectedSave = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Esc: one screen back; the start screen stays
	void Back()
	{
		if (m_iOpenRole >= 0)
		{
			m_iOpenRole = -1;
			return;
		}

		if (m_iScreen == SCREEN_LOAD || m_iScreen == SCREEN_FACTIONS)
			GoTo(SCREEN_START);
		else if (m_iScreen == SCREEN_SETTINGS)
			GoTo(SCREEN_FACTIONS);
		else if (m_iScreen == SCREEN_NAME)
			GoTo(SCREEN_SETTINGS);
	}

	//------------------------------------------------------------------------------------------------
	protected void GoTo(int screen)
	{
		AROR_SetupUIComponent setup = AROR_SetupUIComponent.GetInstance();
		if (setup)
		{
			setup.SetMenuScreen(screen);
			m_sMessage = setup.GetMenuMessage();
		}

		m_iScreen = screen;
		m_bWaitingForList = false;
		m_bNameFilled = false;
		m_iOpenRole = -1;
		m_iPackPage = 0;
		m_iSettingPage = 0;
	}

	//------------------------------------------------------------------------------------------------
	void Layout(float left, float top, float scale)
	{
		BeginFrame(left, top, scale, 1);
		bool nameShown;
		switch (m_iScreen)
		{
			case SCREEN_START: DrawStart(); break;
			case SCREEN_LOAD: DrawLoad(); break;
			case SCREEN_FACTIONS: DrawFactions(); break;
			case SCREEN_SETTINGS: DrawSettings(); break;
			case SCREEN_NAME: DrawName(); nameShown = true; break;
		}

		ShowName(nameShown);
		EndFrame();
	}

	//------------------------------------------------------------------------------------------------
	// Frame
	//------------------------------------------------------------------------------------------------
	protected void DrawHeader(string kicker, string title, string subtitle)
	{
		Text(LEFT, 24, 900, 24, 16, kicker, C(AROR_UIColors.REBEL), true);
		Text(LEFT, 48, 1000, 60, 46, title, C(AROR_UIColors.TEXT), true);
		Panel(0, 112, CANVAS_WIDTH, 1, C(AROR_UIColors.LINE));
		Text(LEFT, 126, CONTENT_WIDTH, 28, 19, subtitle, C(AROR_UIColors.TEXT_MUTED));

		int messageColor = AROR_UIColors.WARN;
		string message = m_sMessage;
		if (m_bWaitingForList)
		{
			message = "Requesting saved campaigns from the server...";
			messageColor = AROR_UIColors.TEXT_MUTED;
		}

		Text(LEFT, 154, CONTENT_WIDTH, 26, 18, message, C(messageColor), true);
	}

	//------------------------------------------------------------------------------------------------
	//! New campaign steps across the top right: done, current, still to come
	protected void DrawSteps(int current)
	{
		array<string> labels = {"ARMIES", "SETTINGS", "NAME", "HQ"};
		float stepWidth = 150;
		float x0 = CANVAS_WIDTH - LEFT - labels.Count() * (stepWidth + 8) + 8;
		foreach (int i, string label : labels)
		{
			float x = x0 + i * (stepWidth + 8);
			int fill = AROR_UIColors.SURFACE_RAISED;
			int text = AROR_UIColors.TEXT_FAINT;
			if (i == current)
			{
				fill = AROR_UIColors.REBEL_DIM;
				text = AROR_UIColors.REBEL;
			}
			else if (i < current)
			{
				text = AROR_UIColors.TEXT_MUTED;
			}

			Panel(x, 40, stepWidth, 52, C(fill));
			if (i == current)
				Panel(x, 88, stepWidth, 4, C(AROR_UIColors.REBEL));

			Text(x + 16, 46, stepWidth - 24, 20, 14, string.Format("STEP %1", i + 1), C(text), true);
			Text(x + 16, 64, stepWidth - 24, 26, 19, label, C(text), true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawFooterRule()
	{
		Panel(0, FOOTER_Y - 16, CANVAS_WIDTH, 1, C(AROR_UIColors.LINE));
	}

	//------------------------------------------------------------------------------------------------
	//! Primary: rebel fill. Secondary: outlined. Disabled: faint and not clickable.
	protected void ActionButton(float x, float y, float width, float height, string label, string action, bool primary = false, bool enabled = true, float size = 18)
	{
		if (!enabled)
		{
			Panel(x - 1, y - 1, width + 2, height + 2, C(AROR_UIColors.LINE));
			Button(x, y, width, height, label, string.Empty, C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT_FAINT), size);
			return;
		}

		if (primary)
		{
			Button(x, y, width, height, label, action, C(AROR_UIColors.REBEL), C(AROR_UIColors.REBEL_INK), size);
			return;
		}

		Panel(x - 1, y - 1, width + 2, height + 2, C(AROR_UIColors.LINE_STRONG));
		Button(x, y, width, height, label, action, C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT), size);
	}

	//------------------------------------------------------------------------------------------------
	protected void Card(float x, float y, float width, float height)
	{
		CardColoured(x, y, width, height, AROR_UIColors.LINE, AROR_UIColors.SURFACE_RAISED);
	}

	//------------------------------------------------------------------------------------------------
	protected void CardColoured(float x, float y, float width, float height, int border, int fill)
	{
		Panel(x, y, width, height, C(border));
		Panel(x + 1, y + 1, width - 2, height - 2, C(fill));
	}

	//------------------------------------------------------------------------------------------------
	//! A small section heading with an optional note on the right
	protected void SectionLabel(float y, string label, string note = string.Empty)
	{
		Text(LEFT, y, 800, 22, 15, label, C(AROR_UIColors.TEXT_FAINT), true);
		if (!note.IsEmpty())
			TextRight(CANVAS_WIDTH - LEFT, y, 15, note, C(AROR_UIColors.TEXT_FAINT), true);
	}

	//------------------------------------------------------------------------------------------------
	// Start
	//------------------------------------------------------------------------------------------------
	protected void DrawStart()
	{
		DrawHeader("EVERON  -  CAMPAIGN SETUP", "OPEN RESISTANCE", "Start a new insurgency or pick up a saved one. Only the server admin sees this screen.");

		array<string> names = {"NEW CAMPAIGN", "LOAD CAMPAIGN"};
		array<string> bodies = {
			"Choose the army for each side and the content packs in play, set up the campaign, name your resistance and place its HQ on the map.",
			"Continue a saved campaign on this map from where it was left."
		};
		array<string> actions = {"START SETUP  >", "BROWSE SAVES  >"};
		array<string> tags = {"new", "load"};

		float cardWidth = (CONTENT_WIDTH - 40) / 2;
		foreach (int i, string cardName : names)
		{
			float x = LEFT + i * (cardWidth + 40);
			float y = 240;
			Card(x, y, cardWidth, 380);
			Panel(x + 1, y + 1, cardWidth - 2, 6, C(AROR_UIColors.REBEL));
			Text(x + 40, y + 44, 100, 28, 22, string.Format("0%1", i + 1), C(AROR_UIColors.TEXT_MUTED), true);
			Text(x + 40, y + 80, cardWidth - 80, 60, 48, cardName, C(AROR_UIColors.TEXT), true);
			Text(x + 40, y + 160, cardWidth - 80, 130, 22, bodies[i], C(AROR_UIColors.TEXT_MUTED));
			Text(x + 40, y + 316, cardWidth - 80, 30, 22, actions[i], C(AROR_UIColors.REBEL), true);
			HitArea(x, y, cardWidth, 380, tags[i]);
		}

		DrawFooterRule();
		Text(LEFT, FOOTER_Y + 12, CONTENT_WIDTH, 24, 16, "OPEN RESISTANCE  -  AN INSURGENCY CAMPAIGN", C(AROR_UIColors.TEXT_FAINT));
	}

	//------------------------------------------------------------------------------------------------
	// Load
	//------------------------------------------------------------------------------------------------
	protected void DrawLoad()
	{
		DrawHeader("EVERON  -  CAMPAIGN SETUP", "LOAD CAMPAIGN", LoadSubtitle());

		BuildLoadRows();
		int pages = m_aLoadPageStarts.Count();
		m_iSavePage = Math.ClampInt(m_iSavePage, 0, pages - 1);
		int firstRow = m_aLoadPageStarts[m_iSavePage];
		int endRow = m_aLoadRows.Count();
		if (m_iSavePage + 1 < pages)
			endRow = m_aLoadPageStarts[m_iSavePage + 1];

		float y = CONTENT_TOP + 10;
		for (int r = firstRow; r < endRow; r++)
		{
			int row = m_aLoadRows[r];
			if (row < 0)
			{
				DrawSaveGroup(-row - 1, y);
				y += GROUP_ROW_STEP;
			}
			else
			{
				DrawSingleSave(row, y);
				y += SAVE_ROW_STEP;
			}
		}

		DrawFooterRule();
		ActionButton(LEFT, FOOTER_Y, 220, 52, "BACK", "start");
		if (pages > 1)
		{
			ActionButton(580, FOOTER_Y, 140, 52, "PREV", "savepage:-1", false, m_iSavePage > 0, 18);
			TextCentre(850, FOOTER_Y + 14, 18, string.Format("PAGE %1 OF %2", m_iSavePage + 1, pages), C(AROR_UIColors.TEXT_MUTED), true);
			ActionButton(980, FOOTER_Y, 140, 52, "NEXT", "savepage:1", false, m_iSavePage < pages - 1, 18);
		}

		ActionButton(CANVAS_WIDTH - LEFT - 300, FOOTER_Y, 300, 52, "LOAD CAMPAIGN", "loadsave", true, m_iSelectedSave >= 0);
	}

	//------------------------------------------------------------------------------------------------
	//! A playthrough's card: the resistance name, its newest save and how many it has; the toggle on the right
	//! lists every save
	protected void DrawSaveGroup(int group, float y)
	{
		int newest = m_aGroupFirst[group];
		string groupKey, groupName, saveName, date;
		bool autosave, compatible;
		ParseSave(m_aSaves[newest], groupKey, groupName, saveName, autosave, date, compatible);

		bool selected = !m_bSelectedSingleSave && m_iSelectedSave == newest;
		int border = AROR_UIColors.LINE;
		int fill = AROR_UIColors.SURFACE_RAISED;
		int nameColor = AROR_UIColors.TEXT;
		if (selected)
		{
			border = AROR_UIColors.REBEL;
			fill = AROR_UIColors.REBEL_DIM;
			nameColor = AROR_UIColors.REBEL;
		}

		string count = "1 save";
		if (m_aGroupCount[group] != 1)
			count = string.Format("%1 saves", m_aGroupCount[group]);

		float textWidth = CONTENT_WIDTH - EXPAND_WIDTH - 220;
		CardColoured(LEFT, y, CONTENT_WIDTH, GROUP_ROW_HEIGHT, border, fill);
		Text(LEFT + 28, y + 12, textWidth, 36, 28, Upper(groupName), C(nameColor), true);
		Text(LEFT + 28, y + 50, textWidth, 26, 18, "Last saved " + date + "  -  " + count, C(AROR_UIColors.TEXT_MUTED));
		if (!compatible)
			TextRight(LEFT + CONTENT_WIDTH - EXPAND_WIDTH - 28, y + 30, 20, "INCOMPATIBLE", C(AROR_UIColors.WARN), true);

		HitArea(LEFT, y, CONTENT_WIDTH - EXPAND_WIDTH, GROUP_ROW_HEIGHT, "group:" + group);

		string toggle = "SHOW ALL  V";
		if (m_aExpandedGroups.Contains(group))
			toggle = "HIDE  ^";

		float toggleLeft = LEFT + CONTENT_WIDTH - EXPAND_WIDTH;
		Panel(toggleLeft, y + 16, 1, GROUP_ROW_HEIGHT - 32, C(AROR_UIColors.LINE));
		TextCentre(toggleLeft + EXPAND_WIDTH * 0.5, y + 30, 18, toggle, C(AROR_UIColors.REBEL), true);
		HitArea(toggleLeft, y, EXPAND_WIDTH, GROUP_ROW_HEIGHT, "expand:" + group);
	}

	//------------------------------------------------------------------------------------------------
	//! One save of an expanded playthrough: autosave or manual, when, and the name it was saved under
	protected void DrawSingleSave(int index, float y)
	{
		string groupKey, groupName, saveName, date;
		bool autosave, compatible;
		ParseSave(m_aSaves[index], groupKey, groupName, saveName, autosave, date, compatible);

		bool selected = m_bSelectedSingleSave && m_iSelectedSave == index;
		int border = AROR_UIColors.LINE;
		int fill = AROR_UIColors.SURFACE;
		int nameColor = AROR_UIColors.TEXT;
		if (selected)
		{
			border = AROR_UIColors.REBEL;
			fill = AROR_UIColors.REBEL_DIM;
			nameColor = AROR_UIColors.REBEL;
		}

		string kind = "SAVE";
		int kindColor = AROR_UIColors.REBEL;
		if (autosave)
		{
			kind = "AUTOSAVE";
			kindColor = AROR_UIColors.TEXT_MUTED;
		}

		float x = LEFT + SAVE_ROW_INDENT;
		float width = CONTENT_WIDTH - SAVE_ROW_INDENT;
		CardColoured(x, y, width, SAVE_ROW_HEIGHT, border, fill);
		Text(x + 24, y + 15, 180, 26, 16, kind, C(kindColor), true);
		Text(x + 200, y + 12, width - 440, 30, 20, date + "   " + Upper(saveName), C(nameColor));
		if (!compatible)
			TextRight(x + width - 24, y + 15, 18, "INCOMPATIBLE", C(AROR_UIColors.WARN), true);

		HitArea(x, y, width, SAVE_ROW_HEIGHT, "pick:" + index);
	}

	//------------------------------------------------------------------------------------------------
	protected string LoadSubtitle()
	{
		int count = m_aGroupFirst.Count();
		if (count == 0)
			return "No saved campaigns on this map yet. Go back and start a new one.";

		if (count == 1)
			return "1 saved campaign on this map. Loading it picks up its latest save; show all to pick an older one.";

		return string.Format("%1 saved campaigns on this map, latest first. Loading one picks up its latest save; show all to pick an older one.", count);
	}

	//------------------------------------------------------------------------------------------------
	//! Groups the server's rows: the rows of one playthrough arrive together, newest first
	protected void BuildSaveGroups()
	{
		m_aGroupFirst.Clear();
		m_aGroupCount.Clear();

		string lastKey;
		foreach (int index, string row : m_aSaves)
		{
			string groupKey, groupName, saveName, date;
			bool autosave, compatible;
			ParseSave(row, groupKey, groupName, saveName, autosave, date, compatible);

			if (index == 0 || groupKey != lastKey)
			{
				m_aGroupFirst.Insert(index);
				m_aGroupCount.Insert(0);
				lastKey = groupKey;
			}

			int last = m_aGroupCount.Count() - 1;
			m_aGroupCount[last] = m_aGroupCount[last] + 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The visible rows (cards plus the saves of expanded ones) and where each page starts
	protected void BuildLoadRows()
	{
		m_aLoadRows.Clear();
		foreach (int group, int first : m_aGroupFirst)
		{
			m_aLoadRows.Insert(-group - 1);
			if (!m_aExpandedGroups.Contains(group))
				continue;

			for (int i = first, groupEnd = first + m_aGroupCount[group]; i < groupEnd; i++)
			{
				m_aLoadRows.Insert(i);
			}
		}

		m_aLoadPageStarts.Clear();
		float used = LOAD_LIST_HEIGHT;
		foreach (int rowIndex, int row : m_aLoadRows)
		{
			float step = SAVE_ROW_STEP;
			if (row < 0)
				step = GROUP_ROW_STEP;

			if (used + step > LOAD_LIST_HEIGHT)
			{
				m_aLoadPageStarts.Insert(rowIndex);
				used = 0;
			}

			used += step;
		}

		if (m_aLoadPageStarts.IsEmpty())
			m_aLoadPageStarts.Insert(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Group of a save index, or -1
	protected int GroupOfSave(int index)
	{
		foreach (int group, int first : m_aGroupFirst)
		{
			if (index >= first && index < first + m_aGroupCount[group])
				return group;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! "group key|group name|save name|autosave (1/0)|date|compatible (1/0)" from the server
	protected static void ParseSave(string row, out string groupKey, out string groupName, out string saveName, out bool autosave, out string date, out bool compatible)
	{
		array<string> parts = {};
		row.Split(AROR_SetupMenu.FIELD_SEPARATOR, parts, false);
		groupKey = row;
		groupName = row;
		saveName = row;
		autosave = false;
		date = string.Empty;
		compatible = true;
		if (parts.Count() < 6)
			return;

		groupKey = parts[0];
		groupName = parts[1];
		saveName = parts[2];
		autosave = parts[3] == "1";
		date = parts[4];
		compatible = parts[5] == "1";
	}

	//------------------------------------------------------------------------------------------------
	// New campaign, step 1: armies and content packs
	//------------------------------------------------------------------------------------------------
	protected void DrawFactions()
	{
		DrawHeader("NEW CAMPAIGN  -  STEP 1 OF 4", "ARMIES AND CONTENT", "Choose the army each side fields and the content packs in play. Both are saved with the campaign.");
		DrawSteps(0);

		AROR_SetupDraft draft = GetDraft();
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!draft)
			return;

		SectionLabel(CONTENT_TOP, "ARMIES");
		float cardWidth = (CONTENT_WIDTH - 20 * (m_aRoles.Count() - 1)) / m_aRoles.Count();
		foreach (int i, AROR_EFactionRole role : m_aRoles)
		{
			float x = LEFT + i * (cardWidth + 20);
			float y = CONTENT_TOP + 28;
			Card(x, y, cardWidth, 190);
			Panel(x + 1, y + 1, cardWidth - 2, 6, RoleColor(role));
			Text(x + 24, y + 24, cardWidth - 48, 34, 26, Upper(AROR_FactionRoles.GetDisplayName(role)), C(AROR_UIColors.TEXT), true);
			Text(x + 24, y + 66, cardWidth - 48, 20, 13, "ARMY", C(AROR_UIColors.TEXT_FAINT), true);

			array<AROR_FactionPreset> rolePresets = {};
			if (presets)
				presets.GetPresetsForRole(role, rolePresets);

			if (rolePresets.IsEmpty())
			{
				Text(x + 24, y + 88, cardWidth - 48, 60, 18, "No army available for this side; the default is used.", C(AROR_UIColors.WARN));
				continue;
			}

			// Dropdown field: the chosen army; clicking it opens the list (drawn last, over everything)
			int current = FindPresetIndex(rolePresets, draft.GetPresetId(role));
			int fieldBorder = AROR_UIColors.LINE_STRONG;
			if (m_iOpenRole == role)
				fieldBorder = AROR_UIColors.REBEL;

			CardColoured(x + 24, y + 90, cardWidth - 48, 56, fieldBorder, AROR_UIColors.SURFACE);
			Text(x + 44, y + 103, cardWidth - 120, 32, 22, Upper(rolePresets[current].GetName()), C(AROR_UIColors.REBEL), true);
			TextRight(x + cardWidth - 44, y + 104, 20, "V", C(AROR_UIColors.TEXT_MUTED), true);
			HitArea(x + 24, y + 90, cardWidth - 48, 56, "dropdown:" + role);

			string countText = "1 ARMY AVAILABLE";
			if (rolePresets.Count() != 1)
				countText = string.Format("%1 ARMIES AVAILABLE", rolePresets.Count());

			Text(x + 24, y + 156, cardWidth - 48, 22, 14, countText, C(AROR_UIColors.TEXT_FAINT), true);
		}

		DrawPacks(draft);

		DrawFooterRule();
		ActionButton(LEFT, FOOTER_Y, 220, 52, "BACK", "start");
		bool packChosen = draft.CountEnabledPacks() > 0;
		if (!packChosen)
			Text(LEFT + 260, FOOTER_Y + 14, 1000, 26, 18, "Turn on at least one content pack: nothing can be bought or fielded without one.", C(AROR_UIColors.WARN), true);

		ActionButton(CANVAS_WIDTH - LEFT - 260, FOOTER_Y, 260, 52, "NEXT  >", "goto:" + SCREEN_SETTINGS, true, packChosen);

		if (m_iOpenRole >= 0)
			DrawArmyDropdown(draft, presets, cardWidth);
	}

	//------------------------------------------------------------------------------------------------
	//! The open role's armies as a list under its field. Drawn last from buttons, which sit above every panel and
	//! text, behind a full-screen catcher so a click anywhere else closes it.
	protected void DrawArmyDropdown(notnull AROR_SetupDraft draft, AROR_FactionPresetsComponent presets, float cardWidth)
	{
		int roleIndex = m_aRoles.Find(m_iOpenRole);
		array<AROR_FactionPreset> rolePresets = {};
		if (presets)
			presets.GetPresetsForRole(m_iOpenRole, rolePresets);

		if (roleIndex < 0 || rolePresets.IsEmpty())
		{
			m_iOpenRole = -1;
			return;
		}

		HitArea(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, "dropdown:close");

		float listX = LEFT + roleIndex * (cardWidth + 20) + 24;
		float listY = CONTENT_TOP + 28 + 90 + 58;
		float listWidth = cardWidth - 48;
		float rowHeight = 50;
		string chosen = draft.GetPresetId(m_iOpenRole);
		Button(listX - 1, listY - 1, listWidth + 2, rolePresets.Count() * rowHeight + 1, string.Empty, string.Empty, C(AROR_UIColors.LINE_STRONG), C(AROR_UIColors.LINE_STRONG), 10);
		foreach (int i, AROR_FactionPreset preset : rolePresets)
		{
			int fill = AROR_UIColors.SURFACE_RAISED;
			int text = AROR_UIColors.TEXT;
			if (preset.GetId() == chosen)
			{
				fill = AROR_UIColors.REBEL_DIM;
				text = AROR_UIColors.REBEL;
			}

			Button(listX, listY + i * rowHeight, listWidth, rowHeight - 1, Upper(preset.GetName()), "pickpreset:" + m_iOpenRole + ":" + i, C(fill), C(text), 19, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawPacks(notnull AROR_SetupDraft draft)
	{
		array<AROR_ContentPack> packs = {};
		AROR_SetupDraft.GetAvailablePacks(packs);

		int pages = Math.Max(1, (packs.Count() + PACK_ROWS - 1) / PACK_ROWS);
		m_iPackPage = Math.ClampInt(m_iPackPage, 0, pages - 1);
		string note;
		if (pages > 1)
			note = string.Format("PAGE %1 OF %2", m_iPackPage + 1, pages);

		float top = CONTENT_TOP + 250;
		SectionLabel(top, "CONTENT PACKS", note);
		Text(LEFT, top + 24, CONTENT_WIDTH, 24, 16, "What can be bought and fielded, per content source. Only packs whose mods are loaded on this server are listed.", C(AROR_UIColors.TEXT_MUTED));

		if (packs.IsEmpty())
		{
			Text(LEFT, top + 64, CONTENT_WIDTH, 26, 19, "No content packs are available.", C(AROR_UIColors.WARN), true);
			return;
		}

		float rowsTop = top + 58;
		float toggleX = LEFT + CONTENT_WIDTH - 24 - 140;
		for (int r = 0; r < PACK_ROWS; r++)
		{
			int index = m_iPackPage * PACK_ROWS + r;
			if (index >= packs.Count())
				break;

			AROR_ContentPack pack = packs[index];
			bool enabled = draft.IsPackEnabled(pack.GetId());
			float y = rowsTop + r * 62;
			Card(LEFT, y, CONTENT_WIDTH, 54);
			Text(LEFT + 24, y + 14, 900, 30, 22, pack.GetName(), C(AROR_UIColors.TEXT), true);

			string defaultText = "OFF BY DEFAULT";
			if (pack.IsEnabledByDefault())
				defaultText = "ON BY DEFAULT";

			TextRight(toggleX - 24, y + 18, 15, defaultText, C(AROR_UIColors.TEXT_FAINT), true);
			if (enabled)
				Button(toggleX, y + 9, 140, 36, "ON", "pack:" + index, C(AROR_UIColors.REBEL), C(AROR_UIColors.REBEL_INK), 17);
			else
				ActionButton(toggleX, y + 9, 140, 36, "OFF", "pack:" + index, false, true, 17);
		}

		if (pages > 1)
		{
			float pagerY = rowsTop + PACK_ROWS * 62;
			ActionButton(LEFT + CONTENT_WIDTH - 216, pagerY, 100, 36, "PREV", "packpage:-1", false, m_iPackPage > 0, 16);
			ActionButton(LEFT + CONTENT_WIDTH - 100, pagerY, 100, 36, "NEXT", "packpage:1", false, m_iPackPage < pages - 1, 16);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static int FindPresetIndex(notnull array<AROR_FactionPreset> rolePresets, string presetId)
	{
		foreach (int i, AROR_FactionPreset preset : rolePresets)
		{
			if (preset.GetId() == presetId)
				return i;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	// New campaign, step 2: settings
	//------------------------------------------------------------------------------------------------
	protected void DrawSettings()
	{
		DrawHeader("NEW CAMPAIGN  -  STEP 2 OF 4", "CAMPAIGN SETTINGS", "How this campaign plays. Settings are saved with the campaign.");
		DrawSteps(1);

		AROR_SetupDraft draft = GetDraft();
		array<AROR_CampaignSettingCategory> categories = {};
		AROR_CampaignSettingsComponent campaignSettings = AROR_CampaignSettingsComponent.GetInstance();
		if (campaignSettings)
			campaignSettings.GetCategories(categories);

		if (!draft || categories.IsEmpty())
		{
			Card(LEFT, CONTENT_TOP + 20, CONTENT_WIDTH, 200);
			Text(LEFT + 40, CONTENT_TOP + 70, CONTENT_WIDTH - 80, 40, 30, "NO CAMPAIGN SETTINGS YET", C(AROR_UIColors.TEXT), true);
			Text(LEFT + 40, CONTENT_TOP + 120, CONTENT_WIDTH - 80, 60, 20, "Settings appear here as they are added. Continue to name your resistance.", C(AROR_UIColors.TEXT_MUTED));
			DrawSettingsFooter();
			return;
		}

		m_iCategory = Math.ClampInt(m_iCategory, 0, categories.Count() - 1);

		// Categories down the left
		float navWidth = 300;
		foreach (int c, AROR_CampaignSettingCategory category : categories)
		{
			float navY = CONTENT_TOP + c * 72;
			int navFill = AROR_UIColors.SURFACE_RAISED;
			int navText = AROR_UIColors.TEXT;
			if (c == m_iCategory)
			{
				navFill = AROR_UIColors.REBEL_DIM;
				navText = AROR_UIColors.REBEL;
				Panel(LEFT, navY, 5, 62, C(AROR_UIColors.REBEL));
			}

			Panel(LEFT + 5, navY, navWidth - 5, 62, C(navFill));
			Text(LEFT + 28, navY + 16, navWidth - 40, 30, 22, Upper(category.GetName()), C(navText), true);
			HitArea(LEFT + 5, navY, navWidth - 5, 62, "category:" + c);
		}

		// The selected category's settings on the right
		array<AROR_CampaignSetting> settings = {};
		categories[m_iCategory].GetSettings(settings);
		float x = LEFT + navWidth + 24;
		float width = CONTENT_WIDTH - navWidth - 24;
		if (settings.IsEmpty())
		{
			Text(x, CONTENT_TOP + 16, width, 28, 20, "This category has no settings.", C(AROR_UIColors.TEXT_MUTED));
			DrawSettingsFooter();
			return;
		}

		int pages = Math.Max(1, (settings.Count() + SETTING_ROWS - 1) / SETTING_ROWS);
		m_iSettingPage = Math.ClampInt(m_iSettingPage, 0, pages - 1);
		for (int r = 0; r < SETTING_ROWS; r++)
		{
			int index = m_iSettingPage * SETTING_ROWS + r;
			if (index >= settings.Count())
				break;

			AROR_CampaignSetting setting = settings[index];
			string value = draft.GetSettingValue(setting);
			float y = CONTENT_TOP + r * 84;
			Card(x, y, width, 74);
			Text(x + 24, y + 10, width - 360, 30, 22, setting.GetName(), C(AROR_UIColors.TEXT), true);
			Text(x + 24, y + 42, width - 360, 24, 16, setting.GetDescription(), C(AROR_UIColors.TEXT_MUTED));

			string action = "setting:" + m_iCategory + ":" + index;
			float controlX = x + width - 24 - 300;
			if (AROR_CampaignSettingBool.Cast(setting))
			{
				if (value == "1")
					Button(controlX + 160, y + 17, 140, 40, setting.FormatValue(value), action + ":1", C(AROR_UIColors.REBEL), C(AROR_UIColors.REBEL_INK), 17);
				else
					ActionButton(controlX + 160, y + 17, 140, 40, setting.FormatValue(value), action + ":1", false, true, 17);

				continue;
			}

			ActionButton(controlX, y + 17, 52, 40, "-", action + ":-1", false, true, 22);
			TextCentre(controlX + 150, y + 25, 18, Upper(setting.FormatValue(value)), C(AROR_UIColors.REBEL), true);
			ActionButton(controlX + 248, y + 17, 52, 40, "+", action + ":1", false, true, 22);
		}

		if (pages > 1)
		{
			float pagerY = CONTENT_TOP + SETTING_ROWS * 84;
			TextRight(x + width - 240, pagerY + 8, 15, string.Format("PAGE %1 OF %2", m_iSettingPage + 1, pages), C(AROR_UIColors.TEXT_FAINT), true);
			ActionButton(x + width - 216, pagerY, 100, 36, "PREV", "settingpage:-1", false, m_iSettingPage > 0, 16);
			ActionButton(x + width - 100, pagerY, 100, 36, "NEXT", "settingpage:1", false, m_iSettingPage < pages - 1, 16);
		}

		DrawSettingsFooter();
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSettingsFooter()
	{
		DrawFooterRule();
		ActionButton(LEFT, FOOTER_Y, 220, 52, "BACK", "goto:" + SCREEN_FACTIONS);
		ActionButton(CANVAS_WIDTH - LEFT - 260, FOOTER_Y, 260, 52, "NEXT  >", "goto:" + SCREEN_NAME, true);
	}

	//------------------------------------------------------------------------------------------------
	// New campaign, step 3: name
	//------------------------------------------------------------------------------------------------
	protected void DrawName()
	{
		DrawHeader("NEW CAMPAIGN  -  STEP 3 OF 4", "NAME YOUR RESISTANCE", "The name your fighters go by. It also names the campaign in the save list and cannot be changed later.");
		DrawSteps(2);

		AROR_SetupDraft draft = GetDraft();
		if (!draft)
			return;

		// Fill the field once per visit, then follow what the admin types
		if (!m_bNameFilled)
		{
			m_wName.SetText(draft.GetName());
			m_bNameFilled = true;
		}

		string typed = m_wName.GetText();
		draft.SetName(typed);
		string reason;
		bool valid = AROR_ResistanceName.Validate(typed, reason);

		float fieldWidth = 820;
		SectionLabel(CONTENT_TOP + 10, "RESISTANCE NAME");
		int border = AROR_UIColors.LINE_STRONG;
		if (!typed.IsEmpty() && !valid)
			border = AROR_UIColors.WARN;

		CardColoured(LEFT, CONTENT_TOP + 36, fieldWidth, 68, border, AROR_UIColors.SURFACE_RAISED);
		int countColor = AROR_UIColors.TEXT_FAINT;
		if (typed.Length() > AROR_ResistanceName.MAX_LENGTH)
			countColor = AROR_UIColors.WARN;

		TextRight(LEFT + fieldWidth, CONTENT_TOP + 112, 16, string.Format("%1 / %2", typed.Length(), AROR_ResistanceName.MAX_LENGTH), C(countColor), true);
		Text(LEFT, CONTENT_TOP + 112, fieldWidth - 120, 24, 16, string.Format("Up to %1 characters: letters A-Z, digits 0-9 and single spaces.", AROR_ResistanceName.MAX_LENGTH), C(AROR_UIColors.TEXT_MUTED));

		if (valid)
			Text(LEFT, CONTENT_TOP + 146, fieldWidth, 26, 19, "NAME OK", C(AROR_UIColors.REBEL), true);
		else if (!typed.IsEmpty())
			Text(LEFT, CONTENT_TOP + 146, fieldWidth, 26, 19, Upper(reason), C(AROR_UIColors.WARN), true);

		Text(LEFT, CONTENT_TOP + 220, fieldWidth, 90, 19, "Next, the map opens: click where the Resistance HQ should stand. It must be on land, outside every town and military site, on a clear 10 x 10 m pad.", C(AROR_UIColors.TEXT_MUTED));

		DrawSummary(draft, LEFT + fieldWidth + 60, CONTENT_TOP + 10, CONTENT_WIDTH - fieldWidth - 60);

		DrawFooterRule();
		ActionButton(LEFT, FOOTER_Y, 220, 52, "BACK", "goto:" + SCREEN_SETTINGS);
		ActionButton(CANVAS_WIDTH - LEFT - 260, FOOTER_Y, 260, 52, "PLACE HQ  >", "hq", true, valid);
	}

	//------------------------------------------------------------------------------------------------
	//! What the campaign will be created with
	protected void DrawSummary(notnull AROR_SetupDraft draft, float x, float y, float width)
	{
		Card(x, y, width, 560);
		Text(x + 24, y + 18, width - 48, 24, 16, "CAMPAIGN SUMMARY", C(AROR_UIColors.TEXT_FAINT), true);
		Panel(x, y + 54, width, 1, C(AROR_UIColors.LINE));

		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		foreach (int i, AROR_EFactionRole role : m_aRoles)
		{
			float rowY = y + 74 + i * 44;
			Panel(x + 24, rowY + 4, 6, 24, RoleColor(role));
			Text(x + 44, rowY, 220, 30, 20, Upper(AROR_FactionRoles.GetDisplayName(role)), C(AROR_UIColors.TEXT_MUTED), true);

			string armyName = "DEFAULT";
			AROR_FactionPreset preset;
			if (presets)
				preset = presets.FindPreset(draft.GetPresetId(role));
			if (preset)
				armyName = Upper(preset.GetName());

			Text(x + 260, rowY, width - 284, 30, 20, armyName, C(AROR_UIColors.TEXT), true);
		}

		float packsY = y + 74 + m_aRoles.Count() * 44 + 20;
		Text(x + 24, packsY, width - 48, 22, 15, "CONTENT PACKS", C(AROR_UIColors.TEXT_FAINT), true);
		array<AROR_ContentPack> packs = {};
		AROR_SetupDraft.GetAvailablePacks(packs);
		string packNames;
		foreach (AROR_ContentPack pack : packs)
		{
			if (!draft.IsPackEnabled(pack.GetId()))
				continue;

			if (!packNames.IsEmpty())
				packNames += ", ";
			packNames += pack.GetName();
		}

		if (packNames.IsEmpty())
			packNames = "None";

		Text(x + 24, packsY + 26, width - 48, 80, 19, packNames, C(AROR_UIColors.TEXT));

		int changed = CountChangedSettings(draft);
		string settingsText = "All on their defaults";
		if (changed == 1)
			settingsText = "1 changed from its default";
		else if (changed > 1)
			settingsText = string.Format("%1 changed from their defaults", changed);

		Text(x + 24, packsY + 116, width - 48, 22, 15, "SETTINGS", C(AROR_UIColors.TEXT_FAINT), true);
		Text(x + 24, packsY + 142, width - 48, 28, 19, settingsText, C(AROR_UIColors.TEXT));
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountChangedSettings(notnull AROR_SetupDraft draft)
	{
		AROR_CampaignSettingsComponent campaignSettings = AROR_CampaignSettingsComponent.GetInstance();
		if (!campaignSettings)
			return 0;

		array<AROR_CampaignSetting> settings = {};
		campaignSettings.GetAllSettings(settings);
		int changed;
		foreach (AROR_CampaignSetting setting : settings)
		{
			if (draft.GetSettingValue(setting) != setting.GetDefaultValue())
				changed++;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! The name field is a real edit box over the canvas, only on the name step
	protected void ShowName(bool show)
	{
		if (m_bNameShown != show)
		{
			m_bNameShown = show;
			m_wName.SetVisible(show);
		}

		if (show)
			Place(m_wName, (LEFT + 24) * m_fScale, (CONTENT_TOP + 46) * m_fScale, (820 - 48) * m_fScale, 48 * m_fScale);
	}

	//------------------------------------------------------------------------------------------------
	// Actions
	//------------------------------------------------------------------------------------------------
	override protected void DoAction(string action)
	{
		AROR_SetupUIComponent setup = AROR_SetupUIComponent.GetInstance();
		if (!setup || !m_Menu)
			return;

		AROR_SetupDraft draft = setup.GetDraft();
		array<string> parts = {};
		action.Split(":", parts, false);
		string verb = parts[0];

		if (verb == "start")
		{
			GoTo(SCREEN_START);
		}
		else if (verb == "new")
		{
			setup.StartNewCampaign();
			GoTo(SCREEN_FACTIONS);
		}
		else if (verb == "load")
		{
			m_sMessage = string.Empty;
			m_bWaitingForList = true;
			setup.RequestSaveList();
		}
		else if (verb == "goto" && parts.Count() > 1)
		{
			GoTo(parts[1].ToInt());
		}
		else if (verb == "group" && parts.Count() > 1 && m_aGroupFirst.IsIndexValid(parts[1].ToInt()))
		{
			m_iSelectedSave = m_aGroupFirst[parts[1].ToInt()];
			m_bSelectedSingleSave = false;
		}
		else if (verb == "pick" && parts.Count() > 1)
		{
			m_iSelectedSave = parts[1].ToInt();
			m_bSelectedSingleSave = true;
		}
		else if (verb == "expand" && parts.Count() > 1)
		{
			int group = parts[1].ToInt();
			if (m_aExpandedGroups.Contains(group))
			{
				m_aExpandedGroups.RemoveItem(group);
				// A picked save hidden by collapsing falls back to its group
				if (m_bSelectedSingleSave && GroupOfSave(m_iSelectedSave) == group)
				{
					m_iSelectedSave = m_aGroupFirst[group];
					m_bSelectedSingleSave = false;
				}
			}
			else if (m_aGroupFirst.IsIndexValid(group))
			{
				m_aExpandedGroups.Insert(group);
			}
		}
		else if (verb == "savepage" && parts.Count() > 1)
		{
			BuildLoadRows();
			m_iSavePage = Math.ClampInt(m_iSavePage + parts[1].ToInt(), 0, m_aLoadPageStarts.Count() - 1);
		}
		else if (verb == "loadsave" && m_aSaves.IsIndexValid(m_iSelectedSave))
		{
			m_Menu.CloseWith(AROR_SetupUIComponent.DIALOG_LOAD, "load", m_iSelectedSave);
		}
		else if (verb == "dropdown" && parts.Count() > 1)
		{
			int role = -1;
			if (parts[1] != "close" && parts[1].ToInt() != m_iOpenRole)
				role = parts[1].ToInt();

			m_iOpenRole = role;
		}
		else if (verb == "pickpreset" && parts.Count() > 2)
		{
			PickPreset(draft, parts[1].ToInt(), parts[2].ToInt());
			m_iOpenRole = -1;
		}
		else if (verb == "pack" && parts.Count() > 1)
		{
			array<AROR_ContentPack> packs = {};
			AROR_SetupDraft.GetAvailablePacks(packs);
			int packIndex = parts[1].ToInt();
			if (packs.IsIndexValid(packIndex))
				draft.SetPackEnabled(packs[packIndex].GetId(), !draft.IsPackEnabled(packs[packIndex].GetId()));
		}
		else if (verb == "packpage" && parts.Count() > 1)
		{
			m_iPackPage += parts[1].ToInt();
		}
		else if (verb == "category" && parts.Count() > 1)
		{
			m_iCategory = parts[1].ToInt();
			m_iSettingPage = 0;
		}
		else if (verb == "setting" && parts.Count() > 3)
		{
			StepSetting(draft, parts[1].ToInt(), parts[2].ToInt(), parts[3].ToInt());
		}
		else if (verb == "settingpage" && parts.Count() > 1)
		{
			m_iSettingPage += parts[1].ToInt();
		}
		else if (verb == "hq")
		{
			string reason;
			if (AROR_ResistanceName.Validate(draft.GetName(), reason))
				m_Menu.CloseWith(AROR_SetupUIComponent.DIALOG_MAIN, "hq", 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The army at index in the role's list becomes its choice
	protected void PickPreset(AROR_SetupDraft draft, AROR_EFactionRole role, int index)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!draft || !presets)
			return;

		array<AROR_FactionPreset> rolePresets = {};
		presets.GetPresetsForRole(role, rolePresets);
		if (rolePresets.IsIndexValid(index))
			draft.SetPresetId(role, rolePresets[index].GetId());
	}

	//------------------------------------------------------------------------------------------------
	protected void StepSetting(AROR_SetupDraft draft, int categoryIndex, int settingIndex, int direction)
	{
		AROR_CampaignSettingsComponent campaignSettings = AROR_CampaignSettingsComponent.GetInstance();
		if (!draft || !campaignSettings)
			return;

		array<AROR_CampaignSettingCategory> categories = {};
		campaignSettings.GetCategories(categories);
		if (!categories.IsIndexValid(categoryIndex))
			return;

		array<AROR_CampaignSetting> settings = {};
		categories[categoryIndex].GetSettings(settings);
		if (!settings.IsIndexValid(settingIndex))
			return;

		AROR_CampaignSetting setting = settings[settingIndex];
		draft.SetSettingValue(setting, setting.StepValue(draft.GetSettingValue(setting), direction));
	}

	//------------------------------------------------------------------------------------------------
	// Helpers
	//------------------------------------------------------------------------------------------------
	protected static AROR_SetupDraft GetDraft()
	{
		AROR_SetupUIComponent setup = AROR_SetupUIComponent.GetInstance();
		if (!setup)
			return null;

		return setup.GetDraft();
	}

	//------------------------------------------------------------------------------------------------
	protected Color RoleColor(AROR_EFactionRole role)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (roles && roles.GetFaction(role))
			return roles.GetFaction(role).GetFactionColor();

		return C(AROR_UIColors.TEXT);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color C(int token)
	{
		return AROR_UIColors.Get(token);
	}
}
