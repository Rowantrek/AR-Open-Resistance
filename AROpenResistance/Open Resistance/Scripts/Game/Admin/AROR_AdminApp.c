//! The admin menu's screen (see AROR_AdminMenu), drawn immediate-mode from the shared pooled-widget layout on a
//! 1700 x 900 canvas: a top bar, a navigation column (Locations, Economy, Support, Tools) and a content area of cards.
//! Money, income, stock and support actions are requests to the server and keep the menu open (values update live);
//! teleport, capture-on-map and Game Master close the menu first because they need the world or the map.
class AROR_AdminApp : AROR_PhonePooledApp
{
	static const float CANVAS_WIDTH = 1700;
	static const float CANVAS_HEIGHT = 900;

	protected static const ResourceName LAYOUT = "{6AB5C0D7B1A4C3E2}UI/Layouts/Phone/AROR_BlackMarket.layout";

	protected static const int TAB_LOCATIONS = 0;
	protected static const int TAB_ECONOMY = 1;
	protected static const int TAB_SUPPORT = 2;
	protected static const int TAB_TOOLS = 3;

	protected static const float NAV_LEFT = 40;
	protected static const float NAV_WIDTH = 270;
	protected static const float CONTENT_LEFT = 340;
	protected static const float CONTENT_WIDTH = 1320;
	protected static const float CONTENT_TOP = 110;

	//! Money added per button press
	protected static const int MONEY_STEP = 1000;

	//! Support change per button press (percentage points)
	protected static const float SUPPORT_STEP = 10;

	protected Widget m_wResource;
	protected AROR_AdminMenu m_Menu;
	protected int m_iTab;
	protected ref array<AROR_EFactionRole> m_aRoles = {};

	//------------------------------------------------------------------------------------------------
	bool Init(Widget parent, AROR_AdminMenu menu)
	{
		m_Menu = menu;
		m_wResource = GetGame().GetWorkspace().CreateWidgets(LAYOUT, parent);
		if (!m_wResource || !InitPools(m_wResource, "BM", 96, 120, 64, 16))
			return false;

		m_wBackground.SetColor(C(AROR_UIColors.SURFACE));
		AROR_FactionRoles.GetMilitary(m_aRoles);
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
	void Layout(float left, float top, float scale)
	{
		BeginFrame(left, top, scale, 1);
		DrawTopBar();
		DrawNav();

		switch (m_iTab)
		{
			case TAB_LOCATIONS: DrawLocations(); break;
			case TAB_ECONOMY: DrawEconomy(); break;
			case TAB_SUPPORT: DrawSupport(); break;
			case TAB_TOOLS: DrawTools(); break;
		}

		EndFrame();
	}

	//------------------------------------------------------------------------------------------------
	// Frame
	//------------------------------------------------------------------------------------------------
	protected void DrawTopBar()
	{
		Text(NAV_LEFT, 12, 600, 22, 15, "ADMIN TOOLS  -  TESTING AND CAMPAIGN CONTROL", C(AROR_UIColors.TEXT_FAINT), true);
		Text(NAV_LEFT, 32, 700, 50, 38, "ADMINISTRATION", C(AROR_UIColors.TEXT), true);

		string player = GetPlayerName(GetLocalPlayerId());
		TextRight(CANVAS_WIDTH - NAV_LEFT - 200, 18, 14, "SIGNED IN AS", C(AROR_UIColors.TEXT_MUTED), true);
		TextRight(CANVAS_WIDTH - NAV_LEFT - 200, 38, 24, Upper(player), C(AROR_UIColors.REBEL), true);
		Panel(CANVAS_WIDTH - NAV_LEFT - 170, 20, 171, 46, C(AROR_UIColors.LINE_STRONG));
		Button(CANVAS_WIDTH - NAV_LEFT - 169, 21, 169, 44, "CLOSE  (ESC)", "close", C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT), 16);
		Panel(0, 84, CANVAS_WIDTH, 1, C(AROR_UIColors.LINE));
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawNav()
	{
		array<string> labels = {"LOCATIONS", "ECONOMY", "SUPPORT", "TOOLS"};
		array<string> hints = {"Capture points on the map", "Funds, income, stock", "Town and city support", "Teleport, Game Master"};
		foreach (int tab, string label : labels)
		{
			float y = CONTENT_TOP + tab * 92;
			bool selected = tab == m_iTab;
			int fill = AROR_UIColors.SURFACE_RAISED;
			int text = AROR_UIColors.TEXT;
			if (selected)
			{
				fill = AROR_UIColors.REBEL_DIM;
				text = AROR_UIColors.REBEL;
				Panel(NAV_LEFT, y, 5, 80, C(AROR_UIColors.REBEL));
			}

			// Panel + texts + an invisible click area: a button's own background would cover the texts
			Panel(NAV_LEFT + 5, y, NAV_WIDTH - 5, 80, C(fill));
			Text(NAV_LEFT + 28, y + 14, NAV_WIDTH - 40, 30, 24, label, C(text), true);
			Text(NAV_LEFT + 28, y + 46, NAV_WIDTH - 40, 24, 15, hints[tab], C(AROR_UIColors.TEXT_MUTED));
			HitArea(NAV_LEFT + 5, y, NAV_WIDTH - 5, 80, "tab:" + tab);
		}

		Text(NAV_LEFT, 800, NAV_WIDTH, 60, 14, "Every action is checked again on the server: only administrators can use them.", C(AROR_UIColors.TEXT_FAINT));
	}

	//------------------------------------------------------------------------------------------------
	protected void Card(float x, float y, float width, float height, string title)
	{
		Panel(x, y, width, height, C(AROR_UIColors.LINE));
		Panel(x + 1, y + 1, width - 2, height - 2, C(AROR_UIColors.SURFACE_RAISED));
		Text(x + 24, y + 16, width - 48, 28, 20, title, C(AROR_UIColors.TEXT_MUTED), true);
		Panel(x, y + 56, width, 1, C(AROR_UIColors.LINE));
	}

	//------------------------------------------------------------------------------------------------
	protected void ActionButton(float x, float y, float width, float height, string label, string action, bool primary = false)
	{
		if (primary)
		{
			Button(x, y, width, height, label, action, C(AROR_UIColors.REBEL), C(AROR_UIColors.REBEL_INK), 18);
			return;
		}

		Panel(x - 1, y - 1, width + 2, height + 2, C(AROR_UIColors.LINE_STRONG));
		Button(x, y, width, height, label, action, C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT), 18);
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
	protected static string RoleName(AROR_EFactionRole role)
	{
		return Upper(AROR_FactionRoles.GetDisplayName(role));
	}

	//------------------------------------------------------------------------------------------------
	// Locations
	//------------------------------------------------------------------------------------------------
	protected void DrawLocations()
	{
		Card(CONTENT_LEFT, CONTENT_TOP, CONTENT_WIDTH, 380, "CAPTURE ON MAP");
		Text(CONTENT_LEFT + 24, CONTENT_TOP + 72, CONTENT_WIDTH - 48, 70, 19, "Choose a side. The map opens and every click gives the nearest location to that side. Towns and cities get 100% support for the side; military sites change hands at once, ignoring the flip cooldown. Close the map (Esc) when you are done and this menu comes back.", C(AROR_UIColors.TEXT));

		array<int> sites = {};
		array<int> settlements = {};
		CountTerritory(sites, settlements);

		float buttonWidth = (CONTENT_WIDTH - 48 - 24 * (m_aRoles.Count() - 1)) / m_aRoles.Count();
		foreach (int i, AROR_EFactionRole role : m_aRoles)
		{
			float x = CONTENT_LEFT + 24 + i * (buttonWidth + 24);
			float y = CONTENT_TOP + 176;
			Panel(x, y, buttonWidth, 8, RoleColor(role));
			ActionButton(x, y + 8, buttonWidth, 100, "CAPTURE FOR " + RoleName(role), "capture:" + role);
			Text(x, y + 124, buttonWidth, 24, 16, string.Format("Holds %1 military sites, %2 towns and cities", sites[i], settlements[i]), C(AROR_UIColors.TEXT_MUTED));
		}

		Card(CONTENT_LEFT, CONTENT_TOP + 404, CONTENT_WIDTH, 232, "TERRITORY");
		float rowY = CONTENT_TOP + 404 + 76;
		Text(CONTENT_LEFT + 24, rowY, 300, 22, 14, "SIDE", C(AROR_UIColors.TEXT_FAINT), true);
		Text(CONTENT_LEFT + 460, rowY, 300, 22, 14, "MILITARY SITES", C(AROR_UIColors.TEXT_FAINT), true);
		Text(CONTENT_LEFT + 760, rowY, 300, 22, 14, "TOWNS AND CITIES", C(AROR_UIColors.TEXT_FAINT), true);
		foreach (int row, AROR_EFactionRole territoryRole : m_aRoles)
		{
			float y = rowY + 32 + row * 40;
			Panel(CONTENT_LEFT + 24, y + 4, 6, 24, RoleColor(territoryRole));
			Text(CONTENT_LEFT + 44, y, 400, 30, 22, RoleName(territoryRole), C(AROR_UIColors.TEXT), true);
			Text(CONTENT_LEFT + 460, y, 200, 30, 22, sites[row].ToString(), C(AROR_UIColors.TEXT), true);
			Text(CONTENT_LEFT + 760, y, 200, 30, 22, settlements[row].ToString(), C(AROR_UIColors.TEXT), true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Locations held per side (same order as m_aRoles): military sites, and towns and cities
	protected void CountTerritory(notnull array<int> outSites, notnull array<int> outSettlements)
	{
		foreach (AROR_EFactionRole role : m_aRoles)
		{
			outSites.Insert(0);
			outSettlements.Insert(0);
		}

		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		foreach (AROR_LocationComponent location : all)
		{
			if (!location)
				continue;

			int index = m_aRoles.Find(location.GetControllingRole());
			if (index < 0)
				continue;

			AROR_ELocationType type = location.GetLocationType();
			if (type == AROR_ELocationType.TOWN || type == AROR_ELocationType.CITY)
				outSettlements[index] = outSettlements[index] + 1;
			else if (AROR_CaptureLocationAction.IsCapturableType(type))
				outSites[index] = outSites[index] + 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	// Economy
	//------------------------------------------------------------------------------------------------
	protected void DrawEconomy()
	{
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy)
		{
			Card(CONTENT_LEFT, CONTENT_TOP, CONTENT_WIDTH, 160, "ECONOMY");
			Text(CONTENT_LEFT + 24, CONTENT_TOP + 80, CONTENT_WIDTH - 48, 30, 20, "No AROR_EconomyComponent on the game mode.", C(AROR_UIColors.WARN), true);
			return;
		}

		int stockLines;
		AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.GetInstance();
		if (storage)
		{
			array<ResourceName> prefabs = {};
			array<int> quantities = {};
			storage.GetEntries(prefabs, quantities);
			stockLines = prefabs.Count();
		}

		float leftWidth = 600;
		Card(CONTENT_LEFT, CONTENT_TOP, leftWidth, 440, "BALANCES");
		float y = CONTENT_TOP + 76;
		foreach (AROR_EFactionRole role : m_aRoles)
		{
			Panel(CONTENT_LEFT + 24, y + 4, 6, 24, RoleColor(role));
			Text(CONTENT_LEFT + 44, y, 300, 30, 20, RoleName(role) + " FUNDS", C(AROR_UIColors.TEXT_MUTED), true);
			TextRight(CONTENT_LEFT + leftWidth - 24, y, 24, AROR_ImportShop.Money(economy.GetFactionFunds(role)), C(AROR_UIColors.TEXT), true);
			y += 48;
		}

		Panel(CONTENT_LEFT, y + 4, leftWidth, 1, C(AROR_UIColors.LINE));
		y += 24;
		Text(CONTENT_LEFT + 24, y, 300, 30, 20, "RESISTANCE MANPOWER", C(AROR_UIColors.TEXT_MUTED), true);
		TextRight(CONTENT_LEFT + leftWidth - 24, y, 24, economy.GetManpower().ToString(), C(AROR_UIColors.TEXT), true);
		y += 48;
		Text(CONTENT_LEFT + 24, y, 300, 30, 20, "YOUR MONEY", C(AROR_UIColors.TEXT_MUTED), true);
		TextRight(CONTENT_LEFT + leftWidth - 24, y, 24, AROR_ImportShop.Money(economy.GetLocalPlayerMoney()), C(AROR_UIColors.REBEL), true);
		y += 48;
		Text(CONTENT_LEFT + 24, y, 300, 30, 20, "RESISTANCE STOCK LINES", C(AROR_UIColors.TEXT_MUTED), true);
		TextRight(CONTENT_LEFT + leftWidth - 24, y, 24, stockLines.ToString(), C(AROR_UIColors.TEXT), true);

		float rightLeft = CONTENT_LEFT + leftWidth + 24;
		float rightWidth = CONTENT_WIDTH - leftWidth - 24;
		Card(rightLeft, CONTENT_TOP, rightWidth, 300, "ADD " + AROR_ImportShop.Money(MONEY_STEP));
		float buttonWidth = (rightWidth - 48 - 16) * 0.5;
		foreach (int i, AROR_EFactionRole moneyRole : m_aRoles)
		{
			float x = rightLeft + 24 + Math.Mod(i, 2) * (buttonWidth + 16);
			float buttonY = CONTENT_TOP + 76 + (i / 2) * 76;
			ActionButton(x, buttonY, buttonWidth, 60, RoleName(moneyRole) + " FUNDS", "money:" + moneyRole);
		}

		int selfIndex = m_aRoles.Count();
		ActionButton(rightLeft + 24 + Math.Mod(selfIndex, 2) * (buttonWidth + 16), CONTENT_TOP + 76 + (selfIndex / 2) * 76, buttonWidth, 60, "YOUR MONEY", "money:self");

		Card(rightLeft, CONTENT_TOP + 324, rightWidth, 116 + 80, "ACTIONS");
		ActionButton(rightLeft + 24, CONTENT_TOP + 324 + 76, rightWidth - 48, 56, "PAY INCOME NOW", "income");
		ActionButton(rightLeft + 24, CONTENT_TOP + 324 + 76 + 68, rightWidth - 48, 56, "FILL RESISTANCE STOCK FROM PRESET", "stock");
	}

	//------------------------------------------------------------------------------------------------
	// Support
	//------------------------------------------------------------------------------------------------
	protected void DrawSupport()
	{
		Card(CONTENT_LEFT, CONTENT_TOP, CONTENT_WIDTH, 520, "NEAREST TOWN OR CITY");
		IEntity entity = GetGame().GetPlayerController().GetControlledEntity();
		AROR_SettlementComponent support = AROR_AdminMenuComponent.FindNearestSupport(entity);
		if (!support)
		{
			Text(CONTENT_LEFT + 24, CONTENT_TOP + 80, CONTENT_WIDTH - 48, 60, 20, "No town or city near your character. Spawn in, or move closer to one, then come back.", C(AROR_UIColors.WARN), true);
			return;
		}

		Text(CONTENT_LEFT + 24, CONTENT_TOP + 76, 800, 40, 32, Upper(support.GetLocation().GetName()), C(AROR_UIColors.TEXT), true);
		Text(CONTENT_LEFT + 24, CONTENT_TOP + 120, CONTENT_WIDTH - 48, 24, 16, "Support changes by 10 points per press; the other sides share the rest. A side above 50% takes the town.", C(AROR_UIColors.TEXT_MUTED));

		foreach (int i, AROR_EFactionRole role : m_aRoles)
		{
			float y = CONTENT_TOP + 180 + i * 100;
			float value = support.GetSupport(role);
			Panel(CONTENT_LEFT + 24, y + 6, 6, 56, RoleColor(role));
			Text(CONTENT_LEFT + 44, y, 400, 30, 24, RoleName(role), C(AROR_UIColors.TEXT), true);
			Text(CONTENT_LEFT + 44, y + 34, 400, 24, 16, string.Format("%1%% support", Math.Round(value)), C(AROR_UIColors.TEXT_MUTED));

			// Meter: 100% = 500 units wide
			Panel(CONTENT_LEFT + 460, y + 20, 500, 20, C(AROR_UIColors.LINE));
			Panel(CONTENT_LEFT + 460, y + 20, Math.Max(value * 5, 1), 20, RoleColor(role));
			int step = SUPPORT_STEP;
			ActionButton(CONTENT_LEFT + 1000, y, 130, 60, string.Format("-%1", step), string.Format("support:%1:%2", role, -step));
			ActionButton(CONTENT_LEFT + 1150, y, 130, 60, string.Format("+%1", step), string.Format("support:%1:%2", role, step));
		}
	}

	//------------------------------------------------------------------------------------------------
	// Tools
	//------------------------------------------------------------------------------------------------
	protected void DrawTools()
	{
		DrawTool(0, "TELEPORT", "Opens the map. Click a position to move your character there.", "TELEPORT ON MAP", "teleport");
		DrawTool(1, "GAME MASTER", "Unlocks the Game Master editor for you and opens it.", "OPEN GAME MASTER", "gamemaster");
		DrawTool(2, "TEST NOTIFICATIONS", "Sends a headline and a notification to everyone and a hint to you, to check the notification styles.", "SEND TEST NOTIFICATIONS", "notifications");
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTool(int index, string title, string description, string label, string action)
	{
		float y = CONTENT_TOP + index * 200;
		Card(CONTENT_LEFT, y, CONTENT_WIDTH, 176, title);
		Text(CONTENT_LEFT + 24, y + 76, 760, 80, 19, description, C(AROR_UIColors.TEXT));
		ActionButton(CONTENT_LEFT + CONTENT_WIDTH - 24 - 440, y + 84, 440, 64, label, action, true);
	}

	//------------------------------------------------------------------------------------------------
	// Input
	//------------------------------------------------------------------------------------------------
	override protected void DoAction(string action)
	{
		if (!m_Menu)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (action == "close")
			m_Menu.Close();
		else if (action.IndexOf("tab:") == 0)
			m_iTab = action.Substring(4, action.Length() - 4).ToInt();
		else if (action.IndexOf("capture:") == 0 || action == "teleport" || action == "gamemaster")
			m_Menu.CloseWith(action);
		else if (!controller)
			return;
		else if (action == "notifications")
			controller.AROR_RequestAdminTestNotifications();
		else if (action == "income")
			controller.AROR_RequestAdminPayIncome();
		else if (action == "stock")
			controller.AROR_RequestAdminFillResistanceStock();
		else if (action == "money:self")
			controller.AROR_RequestAdminMoney(AROR_EFactionRole.RESISTANCE, true, MONEY_STEP);
		else if (action.IndexOf("money:") == 0)
			controller.AROR_RequestAdminMoney(action.Substring(6, action.Length() - 6).ToInt(), false, MONEY_STEP);
		else if (action.IndexOf("support:") == 0)
			RequestSupport(controller, action.Substring(8, action.Length() - 8));
	}

	//------------------------------------------------------------------------------------------------
	//! "<role>:<delta>"
	protected void RequestSupport(notnull SCR_PlayerController controller, string request)
	{
		int split = request.IndexOf(":");
		if (split < 1)
			return;

		AROR_EFactionRole role = request.Substring(0, split).ToInt();
		float delta = request.Substring(split + 1, request.Length() - split - 1).ToFloat();
		controller.AROR_RequestAdminSupport(role, delta);
	}

	//------------------------------------------------------------------------------------------------
	protected static Color C(int token)
	{
		return AROR_UIColors.Get(token);
	}
}
