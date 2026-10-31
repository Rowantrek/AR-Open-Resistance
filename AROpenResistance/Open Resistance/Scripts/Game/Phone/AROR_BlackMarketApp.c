//! Black Market phone app (landscape; see AROR_BlackMarket and docs/UI_DESIGN.md -> Black Market). Items only:
//! weapons, accessories, ammunition, gear, equipment and vehicle ammunition from the Resistance preset. The player
//! adds items to a cart (a slim scrolling column on the right) and imports it in one click; there is no confirmation
//! step. A Resistance Leader chooses between faction funds and personal funds; everyone else pays personally.
//! A popup and a sound report every purchase. Vehicles are sold by the separate Imports app.
class AROR_BlackMarketApp : AROR_ShopAppBase
{
	protected static const float LIST_WIDTH = 1196;
	protected static const float ROW_HEIGHT = 84;
	protected static const int VISIBLE_ROWS = 6;
	protected static const float CART_LEFT = 1260;
	protected static const float CART_WIDTH = 400;
	protected static const float CART_TOP = 100;
	protected static const float CART_BOTTOM = 846;
	protected static const float LINES_TOP = 152;
	protected static const float LINE_HEIGHT = 80;
	protected static const int VISIBLE_LINES = 5;
	protected static const int MAX_LINE_QUANTITY = 50;

	protected ref array<string> m_aCategories = {};
	protected ref array<AROR_ShopOffer> m_aShown = {};
	protected string m_sShownKey;
	protected array<ref AROR_ShopOffer> m_ShownFrom;	//!< The catalogue m_aShown was built from (rebuilt when the preset changes)
	protected string m_sNote;
	protected int m_iTab;
	protected string m_sQuery;
	protected int m_iScroll;
	protected int m_iCartScroll;
	protected ref array<string> m_aCartPrefabs = {};
	protected ref array<int> m_aCartQuantities = {};
	protected bool m_bPayFaction = true;
	protected bool m_bAwaiting;

	//------------------------------------------------------------------------------------------------
	bool Init(Widget phoneRoot)
	{
		if (!InitShop(phoneRoot, "BM"))
			return false;

		AROR_BlackMarket.GetCategoryNames(m_aCategories);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool PaysFromFaction()
	{
		return m_bPayFaction;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnOpened()
	{
		m_iScroll = 0;
		m_iCartScroll = 0;
		m_bAwaiting = false;
		if (m_wSearch)
			m_wSearch.SetText(m_sQuery);
	}

	//------------------------------------------------------------------------------------------------
	void Layout(float left, float top, float scale)
	{
		BeginFrame(left, top, scale, 1);
		RefreshState();
		if (!m_bLeader)
			m_bPayFaction = false;

		DropUnavailableLines();
		DrawTopBar("SMUGGLERS' NET", "BLACK MARKET");

		if (m_iFacilities < 1)
		{
			ShowSearch(false, 0, 0, 0, 0);
			DrawNoAccess("shipped in");
		}
		else
		{
			DrawMarket();
		}

		DrawPopup();
		EndFrame();
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawMarket()
	{
		ReadSearch();
		BuildShown();

		if (m_Popup)
			ShowSearch(false, 0, 0, 0, 0);
		else
		{
			DrawFacilityStrip(LIST_WIDTH, false);
			DrawTabs();
		}

		DrawList();
		DrawCart();
	}

	//------------------------------------------------------------------------------------------------
	//! The search box is a real edit box; the app reads its text each frame
	protected void ReadSearch()
	{
		if (!m_wSearch)
			return;

		string text = m_wSearch.GetText();
		if (text == m_sQuery)
			return;

		m_sQuery = text;
		m_iScroll = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Offers shown now: the selected tab, or every category while a search is typed
	protected void BuildShown()
	{
		array<ref AROR_ShopOffer> catalogue = AROR_BlackMarket.GetCatalogue();
		string query = m_sQuery;
		query.ToLower();
		string key = string.Format("%1|%2", m_iTab, query);
		if (key == m_sShownKey && catalogue == m_ShownFrom)
			return;

		m_sShownKey = key;
		m_ShownFrom = catalogue;
		m_aShown.Clear();
		foreach (AROR_ShopOffer offer : catalogue)
		{
			if (query.IsEmpty())
			{
				if (offer.m_iCategory == m_iTab)
					m_aShown.Insert(offer);

				continue;
			}

			string haystack = offer.GetName() + " " + offer.m_sType + " " + m_aCategories[offer.m_iCategory];
			haystack.ToLower();
			if (haystack.Contains(query))
				m_aShown.Insert(offer);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsSearching()
	{
		return !m_sQuery.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTabs()
	{
		array<ref AROR_ShopOffer> catalogue = AROR_BlackMarket.GetCatalogue();
		float x = MARGIN;
		bool searching = IsSearching();
		string query = m_sQuery;
		query.ToLower();
		foreach (int tab, string label : m_aCategories)
		{
			int count;
			foreach (AROR_ShopOffer offer : catalogue)
			{
				if (offer.m_iCategory != tab)
					continue;

				if (!searching)
				{
					count++;
					continue;
				}

				string haystack = offer.GetName() + " " + offer.m_sType + " " + label;
				haystack.ToLower();
				if (haystack.Contains(query))
					count++;
			}

			float width = (label.Length() + 4) * 16 * CHAR_WIDTH_FACTOR + 36;
			bool selected = !searching && tab == m_iTab;
			int fill = AROR_UIColors.SURFACE_RAISED;
			int text = AROR_UIColors.TEXT_MUTED;
			if (selected)
			{
				fill = AROR_UIColors.REBEL_DIM;
				text = AROR_UIColors.REBEL;
			}
			else if (searching && count < 1)
			{
				text = AROR_UIColors.TEXT_FAINT;
			}

			Button(x, TABS_TOP, width, TABS_HEIGHT - 4, Upper(label) + "  " + count, "tab:" + tab, C(fill), C(text), 16);
			if (selected)
				Panel(x, TABS_TOP + TABS_HEIGHT - 4, width, 3, C(AROR_UIColors.REBEL));

			x += width + 4;
		}

		Panel(MARGIN, TABS_TOP + TABS_HEIGHT - 1, LIST_WIDTH, 1, C(AROR_UIColors.LINE_STRONG));

		// Search: a plate behind the edit box, a clear button while there is text
		float searchLeft = MARGIN + LIST_WIDTH - 270;
		Panel(searchLeft, TABS_TOP + 2, 270, 38, C(AROR_UIColors.LINE_STRONG));
		Panel(searchLeft + 1, TABS_TOP + 3, 268, 36, C(AROR_UIColors.PLATE_STRONG));
		ShowSearch(true, searchLeft + 10, TABS_TOP + 7, 214, 30);
		if (!m_sQuery.IsEmpty())
			Button(searchLeft + 270 - 36, TABS_TOP + 3, 36, 36, "x", "search:clear", C(AROR_UIColors.PLATE_STRONG), C(AROR_UIColors.TEXT_MUTED), 18);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawList()
	{
		m_iScroll = ClampScroll(m_iScroll, m_aShown.Count(), VISIBLE_ROWS);
		float rowWidth = LIST_WIDTH - SCROLLBAR_WIDTH - 8;
		if (m_aShown.IsEmpty())
		{
			string empty = "Nothing in this category.";
			if (IsSearching())
				empty = "Nothing matches \"" + m_sQuery + "\".";
			TextCentre(MARGIN + LIST_WIDTH * 0.5, LIST_TOP + 120, 22, empty, C(AROR_UIColors.TEXT_MUTED), true);
		}

		for (int row = 0; row < VISIBLE_ROWS; row++)
		{
			int index = m_iScroll + row;
			if (index >= m_aShown.Count())
				break;

			DrawItemRow(m_aShown[index], MARGIN, LIST_TOP + row * (ROW_HEIGHT + ROW_GAP), rowWidth);
		}

		DrawScrollbar(MARGIN + LIST_WIDTH - SCROLLBAR_WIDTH, LIST_TOP, LIST_BOTTOM - LIST_TOP, m_aShown.Count(), VISIBLE_ROWS, m_iScroll, "list");
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawItemRow(notnull AROR_ShopOffer offer, float x, float y, float width)
	{
		bool unlocked = IsUnlocked(offer);
		int quantity = GetCartQuantity(offer.m_sPrefab);
		if (quantity > 0)
			Panel(x - 1, y - 1, width + 2, ROW_HEIGHT + 2, C(AROR_UIColors.REBEL_DIM));

		Panel(x, y, width, ROW_HEIGHT, C(AROR_UIColors.SURFACE_RAISED));
		Panel(x, y, 120, ROW_HEIGHT, C(AROR_UIColors.PLATE_STRONG));
		DrawThumb(x, y, 120, ROW_HEIGHT, offer, unlocked);

		int nameColor = AROR_UIColors.TEXT;
		int priceColor = AROR_UIColors.TEXT;
		if (!unlocked)
		{
			nameColor = AROR_UIColors.TEXT_FAINT;
			priceColor = AROR_UIColors.TEXT_FAINT;
		}

		float textX = x + 136;
		Text(textX, y + 14, 420, 30, 22, offer.GetName(), C(nameColor), true);
		string subtitle = offer.m_sType;
		if (IsSearching())
			subtitle = m_aCategories[offer.m_iCategory] + "  -  " + offer.m_sType;
		Text(textX, y + 46, 420, 24, 16, subtitle, C(AROR_UIColors.TEXT_MUTED));

		DrawRequirements(offer, x + 580, y + 16);
		TextRight(x + width - 190, y + 28, 24, AROR_ImportShop.Money(offer.m_iPrice), C(priceColor), true);

		float controlX = x + width - 170;
		if (!unlocked)
		{
			Button(controlX, y + 18, 170, 48, "LOCKED", "", C(AROR_UIColors.SURFACE), C(AROR_UIColors.TEXT_FAINT), 16);
			return;
		}

		Panel(controlX - 1, y + 17, 172, 50, C(AROR_UIColors.LINE_STRONG));
		if (quantity < 1)
		{
			Button(controlX, y + 18, 170, 48, "ADD", "add:" + offer.m_sPrefab, C(AROR_UIColors.SURFACE), C(AROR_UIColors.REBEL), 16);
			return;
		}

		Button(controlX, y + 18, 52, 48, "-", "dec:" + offer.m_sPrefab, C(AROR_UIColors.SURFACE), C(AROR_UIColors.REBEL), 26);
		TextCentre(controlX + 85, y + 28, 22, quantity.ToString(), C(AROR_UIColors.TEXT), true);
		Button(controlX + 118, y + 18, 52, 48, "+", "inc:" + offer.m_sPrefab, C(AROR_UIColors.SURFACE), C(AROR_UIColors.REBEL), 26);
	}

	//------------------------------------------------------------------------------------------------
	//! The offer's picture centred in a box, or its type name when no picture is known
	protected void DrawThumb(float x, float y, float width, float height, notnull AROR_ShopOffer offer, bool unlocked)
	{
		ResourceName image = offer.GetImage();
		if (!image.IsEmpty())
		{
			Picture(x + 6, y + 6, width - 12, height - 12, image);
			return;
		}

		// Items have no catalogue picture: render the prefab like the inventory does
		if (ItemPreview(x + 4, y + 4, width - 8, height - 8, offer.m_sPrefab))
			return;

		int color = AROR_UIColors.TEXT_MUTED;
		if (!unlocked)
			color = AROR_UIColors.TEXT_FAINT;
		TextCentre(x + width * 0.5, y + height * 0.5 - 10, 14, Upper(offer.m_sType), C(color), true);
	}

	//------------------------------------------------------------------------------------------------
	// Cart
	//------------------------------------------------------------------------------------------------
	protected void DrawCart()
	{
		float height = CART_BOTTOM - CART_TOP;
		Panel(CART_LEFT, CART_TOP, CART_WIDTH, height, C(AROR_UIColors.LINE));
		Panel(CART_LEFT + 1, CART_TOP + 1, CART_WIDTH - 2, height - 2, C(AROR_UIColors.SURFACE_RAISED));

		int itemCount;
		int total;
		for (int i = 0; i < m_aCartPrefabs.Count(); i++)
		{
			itemCount += m_aCartQuantities[i];
			AROR_ShopOffer offer = AROR_BlackMarket.FindOffer(m_aCartPrefabs[i]);
			if (offer)
				total += offer.m_iPrice * m_aCartQuantities[i];
		}

		Text(CART_LEFT + 18, CART_TOP + 14, 200, 30, 22, "CART", C(AROR_UIColors.TEXT), true);
		string countLabel = string.Format("%1 ITEMS", itemCount);
		if (itemCount == 1)
			countLabel = "1 ITEM";
		TextRight(CART_LEFT + CART_WIDTH - 18, CART_TOP + 20, 15, countLabel, C(AROR_UIColors.TEXT_MUTED), true);
		Panel(CART_LEFT, CART_TOP + 50, CART_WIDTH, 1, C(AROR_UIColors.LINE));

		if (m_aCartPrefabs.IsEmpty())
		{
			Text(CART_LEFT + 30, LINES_TOP + 130, CART_WIDTH - 60, 24, 20, "CART EMPTY", C(AROR_UIColors.TEXT), true);
			Text(CART_LEFT + 30, LINES_TOP + 162, CART_WIDTH - 60, 70, 17, "Add items from the list. They arrive in Resistance Storage.", C(AROR_UIColors.TEXT_MUTED));
		}

		m_iCartScroll = ClampScroll(m_iCartScroll, m_aCartPrefabs.Count(), VISIBLE_LINES);
		for (int line = 0; line < VISIBLE_LINES; line++)
		{
			int index = m_iCartScroll + line;
			if (index >= m_aCartPrefabs.Count())
				break;

			DrawCartLine(index, LINES_TOP + line * LINE_HEIGHT);
		}

		DrawScrollbar(CART_LEFT + CART_WIDTH - SCROLLBAR_WIDTH - 8, LINES_TOP, VISIBLE_LINES * LINE_HEIGHT, m_aCartPrefabs.Count(), VISIBLE_LINES, m_iCartScroll, "cart");

		// Footer
		float y = LINES_TOP + VISIBLE_LINES * LINE_HEIGHT + 12;
		Panel(CART_LEFT, y - 6, CART_WIDTH, 1, C(AROR_UIColors.LINE));
		float footerLeft = CART_LEFT + 18;
		float footerWidth = CART_WIDTH - 36;
		if (m_bLeader)
		{
			AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
			int faction;
			int personal;
			if (economy)
			{
				faction = economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE);
				personal = economy.GetLocalPlayerMoney();
			}

			Text(footerLeft, y, 200, 20, 14, "PAY FROM", C(AROR_UIColors.TEXT_MUTED), true);
			float half = footerWidth * 0.5;
			DrawPayButton(footerLeft, y + 22, half, "FACTION  " + AROR_ImportShop.Money(faction), "pay:faction", m_bPayFaction);
			DrawPayButton(footerLeft + half, y + 22, half, "PERSONAL  " + AROR_ImportShop.Money(personal), "pay:personal", !m_bPayFaction);
		}
		else
		{
			Text(footerLeft, y + 4, footerWidth, 44, 15, "Paying from personal funds. Only the Resistance Leader can spend faction funds.", C(AROR_UIColors.TEXT_MUTED));
		}

		Text(footerLeft, y + 94, 100, 24, 15, "TOTAL", C(AROR_UIColors.TEXT_MUTED), true);
		TextRight(CART_LEFT + CART_WIDTH - 18, y + 84, 32, AROR_ImportShop.Money(total), C(AROR_UIColors.TEXT), true);

		if (!m_sNote.IsEmpty())
			Text(footerLeft, y + 134, footerWidth, 20, 14, m_sNote, C(AROR_UIColors.WARN));

		if (m_bAwaiting)
			Button(footerLeft, CART_BOTTOM - 78, footerWidth, 60, "IMPORTING...", "", C(AROR_UIColors.LINE), C(AROR_UIColors.TEXT_FAINT), 21);
		else if (m_aCartPrefabs.IsEmpty())
			Button(footerLeft, CART_BOTTOM - 78, footerWidth, 60, "IMPORT", "", C(AROR_UIColors.LINE), C(AROR_UIColors.TEXT_FAINT), 21);
		else
			Button(footerLeft, CART_BOTTOM - 78, footerWidth, 60, "IMPORT  " + AROR_ImportShop.Money(total), "import", C(AROR_UIColors.REBEL), C(AROR_UIColors.REBEL_INK), 21);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawPayButton(float x, float y, float width, string label, string action, bool selected)
	{
		int fill = AROR_UIColors.SURFACE;
		int text = AROR_UIColors.TEXT_MUTED;
		if (selected)
		{
			fill = AROR_UIColors.REBEL_DIM;
			text = AROR_UIColors.TEXT;
		}

		Panel(x, y, width + 1, 54, C(AROR_UIColors.LINE_STRONG));
		Button(x + 1, y + 1, width - 1, 52, label, action, C(fill), C(text), 15);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawCartLine(int index, float y)
	{
		AROR_ShopOffer offer = AROR_BlackMarket.FindOffer(m_aCartPrefabs[index]);
		if (!offer)
			return;

		int quantity = m_aCartQuantities[index];
		float x = CART_LEFT + 18;
		Panel(x, y + LINE_HEIGHT - 1, CART_WIDTH - 36 - SCROLLBAR_WIDTH - 8, 1, C(AROR_UIColors.LINE));
		Panel(x, y + 12, 60, 46, C(AROR_UIColors.PLATE_STRONG));
		ResourceName image = offer.GetImage();
		if (!image.IsEmpty())
			Picture(x + 3, y + 14, 54, 42, image);
		else
			ItemPreview(x + 2, y + 13, 56, 44, offer.m_sPrefab);

		Text(x + 72, y + 8, 150, 44, 17, offer.GetName(), C(AROR_UIColors.TEXT), true);
		Text(x + 72, y + 52, 150, 22, 16, AROR_ImportShop.Money(offer.m_iPrice * quantity), C(AROR_UIColors.TEXT_MUTED));

		float stepX = CART_LEFT + CART_WIDTH - 18 - SCROLLBAR_WIDTH - 8 - 112;
		Panel(stepX, y + 22, 112, 36, C(AROR_UIColors.LINE_STRONG));
		Button(stepX + 1, y + 23, 34, 34, "-", "dec:" + offer.m_sPrefab, C(AROR_UIColors.SURFACE), C(AROR_UIColors.REBEL), 20);
		TextCentre(stepX + 56, y + 28, 17, quantity.ToString(), C(AROR_UIColors.TEXT), true);
		Button(stepX + 77, y + 23, 34, 34, "+", "inc:" + offer.m_sPrefab, C(AROR_UIColors.SURFACE), C(AROR_UIColors.REBEL), 20);
	}

	//------------------------------------------------------------------------------------------------
	// Cart state
	//------------------------------------------------------------------------------------------------
	protected int GetCartQuantity(string prefab)
	{
		int index = m_aCartPrefabs.Find(prefab);
		if (index < 0)
			return 0;

		return m_aCartQuantities[index];
	}

	//------------------------------------------------------------------------------------------------
	protected void ChangeCart(string prefab, int delta)
	{
		m_sNote = string.Empty;
		int index = m_aCartPrefabs.Find(prefab);
		if (index < 0)
		{
			if (delta > 0)
			{
				m_aCartPrefabs.Insert(prefab);
				m_aCartQuantities.Insert(delta);
			}

			return;
		}

		int quantity = Math.ClampInt(m_aCartQuantities[index] + delta, 0, MAX_LINE_QUANTITY);
		if (quantity < 1)
		{
			// Taking a line down to 0 removes it
			m_aCartPrefabs.Remove(index);
			m_aCartQuantities.Remove(index);
		}
		else
		{
			m_aCartQuantities[index] = quantity;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lines that stop being available (a facility was lost, the preset changed) leave the cart
	protected void DropUnavailableLines()
	{
		int dropped;
		for (int i = m_aCartPrefabs.Count() - 1; i >= 0; i--)
		{
			AROR_ShopOffer offer = AROR_BlackMarket.FindOffer(m_aCartPrefabs[i]);
			if (offer && IsUnlocked(offer))
				continue;

			m_aCartPrefabs.Remove(i);
			m_aCartQuantities.Remove(i);
			dropped++;
		}

		if (dropped > 0)
			m_sNote = string.Format("%1 line(s) removed: no longer available.", dropped);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnResultShown(AROR_ShopResult result)
	{
		m_bAwaiting = false;
		if (result.m_bOk)
		{
			m_aCartPrefabs.Clear();
			m_aCartQuantities.Clear();
			m_iCartScroll = 0;
			m_sNote = string.Empty;
		}
	}

	//------------------------------------------------------------------------------------------------
	// Input
	//------------------------------------------------------------------------------------------------
	override protected bool OnWheel(float x, float y, int wheel)
	{
		int step = 1;
		if (wheel > 0)
			step = -1;

		if (x >= CART_LEFT)
			m_iCartScroll += step;
		else
			m_iScroll += step;

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
		else if (action == "cart:up")
			m_iCartScroll--;
		else if (action == "cart:down")
			m_iCartScroll++;
		else if (action == "search:clear")
		{
			m_sQuery = string.Empty;
			if (m_wSearch)
				m_wSearch.SetText(string.Empty);
			m_iScroll = 0;
		}
		else if (action.IndexOf("tab:") == 0)
		{
			m_iTab = action.Substring(4, action.Length() - 4).ToInt();
			m_sQuery = string.Empty;
			if (m_wSearch)
				m_wSearch.SetText(string.Empty);
			m_iScroll = 0;
		}
		else if (action.IndexOf("add:") == 0)
			ChangeCart(action.Substring(4, action.Length() - 4), 1);
		else if (action.IndexOf("inc:") == 0)
			ChangeCart(action.Substring(4, action.Length() - 4), 1);
		else if (action.IndexOf("dec:") == 0)
			ChangeCart(action.Substring(4, action.Length() - 4), -1);
		else if (action == "pay:faction")
			m_bPayFaction = true;
		else if (action == "pay:personal")
			m_bPayFaction = false;
		else if (action == "import")
			SendImport();
	}

	//------------------------------------------------------------------------------------------------
	//! One click, no confirmation: the server answers with a result that opens the popup
	protected void SendImport()
	{
		if (m_bAwaiting || m_aCartPrefabs.IsEmpty())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		m_bAwaiting = true;
		// If the answer never arrives the button must not stay stuck
		GetGame().GetCallqueue().CallLater(StopAwaiting, 6000);
		controller.AROR_RequestBlackMarketImport(m_aCartPrefabs, m_aCartQuantities, m_bLeader && m_bPayFaction);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopAwaiting()
	{
		m_bAwaiting = false;
	}
}
