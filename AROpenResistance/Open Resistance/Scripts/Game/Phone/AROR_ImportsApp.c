//! Imports ("Used Imports") phone app (landscape; see AROR_Imports). Vehicles from the Resistance preset in category
//! tabs, each row with a picture and two buy buttons: faction funds (Resistance Leaders only) and personal funds.
//! There is no cart and no confirmation: every click buys one vehicle straight into the Resistance Garage, so buying
//! several means clicking several times. A popup and a sound report every purchase.
class AROR_ImportsApp : AROR_ShopAppBase
{
	protected static const float LIST_WIDTH = 1620;
	protected static const float ROW_HEIGHT = 124;
	protected static const int VISIBLE_ROWS = 4;

	protected ref array<string> m_aCategories = {};
	protected ref array<AROR_ShopOffer> m_aShown = {};
	protected string m_sShownKey;
	protected array<ref AROR_ShopOffer> m_ShownFrom;	//!< The catalogue m_aShown was built from (rebuilt when the preset changes)
	protected ref map<string, int> m_mBought = new map<string, int>();	//!< Vehicles imported this session, per prefab
	protected int m_iTab;
	protected int m_iScroll;

	//------------------------------------------------------------------------------------------------
	bool Init(Widget phoneRoot)
	{
		if (!InitShop(phoneRoot, "BM"))
			return false;

		AROR_Imports.GetCategoryNames(m_aCategories);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool IsVehicleShop()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnOpened()
	{
		m_iScroll = 0;
	}

	//------------------------------------------------------------------------------------------------
	void Layout(float left, float top, float scale)
	{
		BeginFrame(left, top, scale, 1);
		RefreshState();
		ShowSearch(false, 0, 0, 0, 0);
		DrawTopBar("USED IMPORTS", "IMPORTS");

		if (m_iFacilities < 1)
		{
			DrawNoAccess("imported");
		}
		else
		{
			BuildShown();
			if (!m_Popup)
			{
				DrawFacilityStrip(WIDTH - MARGIN * 2, true);
				DrawTabs();
			}

			DrawList();
		}

		DrawPopup();
		EndFrame();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildShown()
	{
		array<ref AROR_ShopOffer> catalogue = AROR_Imports.GetCatalogue();
		string key = string.Format("%1", m_iTab);
		if (key == m_sShownKey && catalogue == m_ShownFrom)
			return;

		m_sShownKey = key;
		m_ShownFrom = catalogue;
		m_aShown.Clear();
		foreach (AROR_ShopOffer offer : catalogue)
		{
			if (offer.m_iCategory == m_iTab)
				m_aShown.Insert(offer);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTabs()
	{
		array<ref AROR_ShopOffer> catalogue = AROR_Imports.GetCatalogue();
		float x = MARGIN;
		foreach (int tab, string label : m_aCategories)
		{
			int count;
			foreach (AROR_ShopOffer offer : catalogue)
			{
				if (offer.m_iCategory == tab)
					count++;
			}

			float width = (label.Length() + 4) * 16 * CHAR_WIDTH_FACTOR + 36;
			bool selected = tab == m_iTab;
			int fill = AROR_UIColors.SURFACE_RAISED;
			int text = AROR_UIColors.TEXT_MUTED;
			if (selected)
			{
				fill = AROR_UIColors.REBEL_DIM;
				text = AROR_UIColors.REBEL;
			}

			Button(x, TABS_TOP, width, TABS_HEIGHT - 4, Upper(label) + "  " + count, "tab:" + tab, C(fill), C(text), 16);
			if (selected)
				Panel(x, TABS_TOP + TABS_HEIGHT - 4, width, 3, C(AROR_UIColors.REBEL));

			x += width + 4;
		}

		Panel(MARGIN, TABS_TOP + TABS_HEIGHT - 1, WIDTH - MARGIN * 2, 1, C(AROR_UIColors.LINE_STRONG));
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawList()
	{
		m_iScroll = ClampScroll(m_iScroll, m_aShown.Count(), VISIBLE_ROWS);
		float rowWidth = LIST_WIDTH - SCROLLBAR_WIDTH - 8;
		if (m_aShown.IsEmpty())
			TextCentre(MARGIN + LIST_WIDTH * 0.5, LIST_TOP + 120, 22, "No vehicles in this category.", C(AROR_UIColors.TEXT_MUTED), true);

		for (int row = 0; row < VISIBLE_ROWS; row++)
		{
			int index = m_iScroll + row;
			if (index >= m_aShown.Count())
				break;

			DrawVehicleRow(m_aShown[index], MARGIN, LIST_TOP + row * (ROW_HEIGHT + ROW_GAP), rowWidth);
		}

		DrawScrollbar(MARGIN + LIST_WIDTH - SCROLLBAR_WIDTH, LIST_TOP, LIST_BOTTOM - LIST_TOP, m_aShown.Count(), VISIBLE_ROWS, m_iScroll, "list");
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawVehicleRow(notnull AROR_ShopOffer offer, float x, float y, float width)
	{
		bool unlocked = IsUnlocked(offer);
		Panel(x, y, width, ROW_HEIGHT, C(AROR_UIColors.SURFACE_RAISED));
		Panel(x, y, 260, ROW_HEIGHT, C(AROR_UIColors.PLATE_STRONG));

		ResourceName image = offer.GetImage();
		if (!image.IsEmpty())
			Picture(x + 8, y + 8, 244, ROW_HEIGHT - 16, image);
		else if (!ItemPreview(x + 8, y + 8, 244, ROW_HEIGHT - 16, offer.m_sPrefab))
			TextCentre(x + 130, y + ROW_HEIGHT * 0.5 - 10, 16, Upper(offer.m_sType), C(AROR_UIColors.TEXT_MUTED), true);

		int bought;
		m_mBought.Find(offer.m_sPrefab, bought);
		if (bought > 0)
		{
			Panel(x + 8, y + 8, 124, 26, C(AROR_UIColors.REBEL_DIM));
			Text(x + 16, y + 11, 110, 22, 14, string.Format("x%1 IMPORTED", bought), C(AROR_UIColors.TEXT), true);
		}

		int nameColor = AROR_UIColors.TEXT;
		int priceColor = AROR_UIColors.TEXT;
		if (!unlocked)
		{
			nameColor = AROR_UIColors.TEXT_FAINT;
			priceColor = AROR_UIColors.TEXT_FAINT;
		}

		float textX = x + 276;
		Text(textX, y + 24, 460, 36, 28, Upper(offer.GetName()), C(nameColor), true);
		Text(textX, y + 64, 460, 24, 17, offer.m_sType, C(AROR_UIColors.TEXT_MUTED));

		DrawRequirements(offer, x + 740, y + 30);
		TextRight(x + width - 420, y + 44, 24, AROR_ImportShop.Money(offer.m_iPrice), C(priceColor), true);

		float buttonsX = x + width - 400;
		Panel(buttonsX - 1, y + 30, 192, 66, C(AROR_UIColors.LINE_STRONG));
		Panel(buttonsX + 209, y + 30, 192, 66, C(AROR_UIColors.LINE_STRONG));
		if (!unlocked)
		{
			Button(buttonsX, y + 31, 190, 64, "FACTION  LOCKED", "", C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT_FAINT), 15);
			Button(buttonsX + 210, y + 31, 190, 64, "PERSONAL  LOCKED", "", C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT_FAINT), 15);
			return;
		}

		string price = AROR_ImportShop.Money(offer.m_iPrice);
		if (m_bLeader)
			Button(buttonsX, y + 31, 190, 64, "BUY FACTION  " + price, "buy:faction:" + offer.m_sPrefab, C(AROR_UIColors.REBEL_DIM), C(AROR_UIColors.TEXT), 15);
		else
			Button(buttonsX, y + 31, 190, 64, "FACTION  LEADER ONLY", "", C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT_FAINT), 15);

		Button(buttonsX + 210, y + 31, 190, 64, "BUY PERSONAL  " + price, "buy:personal:" + offer.m_sPrefab, C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT), 15);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnResultShown(AROR_ShopResult result)
	{
		if (!result.m_bOk || result.m_sKey.IsEmpty())
			return;

		// The key is "<prefab>:faction" or "<prefab>:personal"
		int split = result.m_sKey.LastIndexOf(":");
		if (split < 1)
			return;

		string prefab = result.m_sKey.Substring(0, split);
		int bought;
		m_mBought.Find(prefab, bought);
		m_mBought.Set(prefab, bought + 1);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool OnWheel(float x, float y, int wheel)
	{
		if (wheel > 0)
			m_iScroll--;
		else
			m_iScroll++;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected void DoAction(string action)
	{
		if (action == "popup:close")
			ClosePopup();
		else if (action == "list:up")
			m_iScroll--;
		else if (action == "list:down")
			m_iScroll++;
		else if (action.IndexOf("tab:") == 0)
		{
			m_iTab = action.Substring(4, action.Length() - 4).ToInt();
			m_iScroll = 0;
		}
		else if (action.IndexOf("buy:") == 0)
			Buy(action.Substring(4, action.Length() - 4));
	}

	//------------------------------------------------------------------------------------------------
	//! "faction:<prefab>" or "personal:<prefab>". One click, one vehicle, no confirmation.
	protected void Buy(string request)
	{
		int split = request.IndexOf(":");
		if (split < 1)
			return;

		bool fromFaction = request.Substring(0, split) == "faction";
		string prefab = request.Substring(split + 1, request.Length() - split - 1);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.AROR_RequestVehicleImport(prefab, fromFaction && m_bLeader);
	}
}
