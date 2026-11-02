//! Shared drawing and behaviour of the two shop apps, the Black Market (items, with a cart) and Imports (vehicles, one
//! click each). Both are landscape pooled apps: a top bar with the player's balances, the import facility strip, a list
//! that scrolls by whole rows (mouse wheel or the arrows beside it), and a purchase popup with a sound. Each app owns
//! its own copy of AROR_BlackMarket.layout (the widget names are looked up inside each copy).
class AROR_ShopAppBase : AROR_PhonePooledApp
{
	protected static const ResourceName LAYOUT = "{6AB5C0D7B1A4C3E2}UI/Layouts/Phone/AROR_BlackMarket.layout";

	protected static const int PANEL_POOL = 96;
	protected static const int TEXT_POOL = 120;
	protected static const int BUTTON_POOL = 64;
	protected static const int PICTURE_POOL = 16;

	// Layout (app units, 1700 x 900)
	protected static const float MARGIN = 40;
	protected static const float STRIP_TOP = 100;
	protected static const float STRIP_HEIGHT = 72;
	protected static const float TABS_TOP = 188;
	protected static const float TABS_HEIGHT = 48;
	protected static const float LIST_TOP = 252;
	protected static const float LIST_BOTTOM = 846;
	protected static const float ROW_GAP = 6;
	protected static const float SCROLLBAR_WIDTH = 20;

	// Popup: sits under the top bar, above the list, and replaces the strip and tabs while it shows
	protected static const float POPUP_LEFT = 500;
	protected static const float POPUP_TOP = 96;
	protected static const float POPUP_WIDTH = 700;
	protected static const float POPUP_HEIGHT = 150;
	protected static const float POPUP_SECONDS = 3.8;

	protected Widget m_wResource;
	protected EditBoxWidget m_wSearch;
	protected ref AROR_ShopResult m_Popup;
	protected int m_iPopupCount;
	protected int m_iPorts;
	protected int m_iAir;
	protected int m_iFacilities;
	protected bool m_bLeader;
	protected bool m_bShown;
	protected int m_iSearchShown = -1;

	//------------------------------------------------------------------------------------------------
	//! True for Imports (vehicles), false for the Black Market (items)
	protected bool IsVehicleShop()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True while the player has chosen to pay from faction funds (Leaders only)
	protected bool PaysFromFaction()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Creates this app's copy of the layout under the phone and finds its pools
	protected bool InitShop(Widget phoneRoot, string prefix)
	{
		m_wResource = GetGame().GetWorkspace().CreateWidgets(LAYOUT, phoneRoot);
		if (!m_wResource || !InitPools(m_wResource, prefix, PANEL_POOL, TEXT_POOL, BUTTON_POOL, PICTURE_POOL, PICTURE_POOL))
			return false;

		m_wSearch = EditBoxWidget.Cast(m_wResource.FindAnyWidget(prefix + "Search"));
		if (m_wSearch)
		{
			m_wSearch.SetColor(AROR_UIColors.Get(AROR_UIColors.TEXT));
			m_wSearch.SetPlaceholderText("Search all items");
			m_wSearch.SetVisible(false);
		}

		m_wBackground.SetColor(AROR_UIColors.Get(AROR_UIColors.SURFACE));
		SetVisible(false, 1);
		AROR_ImportShop.GetOnResult().Insert(OnShopResult);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The app's layout copy also holds its home tile (bound by the phone)
	Widget GetResource()
	{
		return m_wResource;
	}

	//------------------------------------------------------------------------------------------------
	void Shutdown()
	{
		AROR_ImportShop.GetOnResult().Remove(OnShopResult);
		GetGame().GetCallqueue().Remove(ClosePopup);
		if (m_wResource)
			m_wResource.RemoveFromHierarchy();
		m_wResource = null;
	}

	//------------------------------------------------------------------------------------------------
	override void SetVisible(bool visible, float opacity)
	{
		super.SetVisible(visible, opacity);
		if (visible && !m_bShown)
			OnOpened();
		else if (!visible && m_bShown)
			ClosePopup();

		m_bShown = visible;
	}

	//------------------------------------------------------------------------------------------------
	//! The app has just been opened
	protected void OnOpened()
	{
	}

	//------------------------------------------------------------------------------------------------
	// Facilities and rights (once per frame)
	//------------------------------------------------------------------------------------------------
	protected void RefreshState()
	{
		m_iPorts = AROR_ImportPoints.CountPorts(AROR_EFactionRole.RESISTANCE);
		m_iAir = AROR_ImportPoints.CountAir(AROR_EFactionRole.RESISTANCE);
		m_iFacilities = m_iPorts + m_iAir;
		m_bLeader = AROR_ImportShop.CanSpendFaction(GetLocalPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Does the Resistance hold what the offer needs? (same rule as the server, from this frame's counts)
	protected bool IsUnlocked(notnull AROR_ShopOffer offer)
	{
		if (m_iFacilities < offer.m_iFacilities)
			return false;

		if (offer.m_eSpecial == AROR_EImportPointRequirement.PORT && m_iPorts < 1)
			return false;

		if (offer.m_eSpecial == AROR_EImportPointRequirement.AIR && m_iAir < 1)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Shared screens
	//------------------------------------------------------------------------------------------------
	protected void DrawTopBar(string kicker, string title)
	{
		Text(MARGIN, 12, 700, 22, 15, kicker, C(AROR_UIColors.TEXT_FAINT), true);
		Text(MARGIN, 32, 700, 50, 38, title, C(AROR_UIColors.TEXT), true);

		int personal;
		int faction;
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (economy)
		{
			personal = economy.GetLocalPlayerMoney();
			faction = economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE);
		}

		bool fromFaction = m_bLeader && PaysFromFaction();
		int personalColor = AROR_UIColors.TEXT_MUTED;
		int factionColor = AROR_UIColors.TEXT_MUTED;
		if (fromFaction)
			factionColor = AROR_UIColors.REBEL;
		else
			personalColor = AROR_UIColors.REBEL;

		float right = WIDTH - MARGIN;
		TextRight(right, 14, 14, "PERSONAL", C(AROR_UIColors.TEXT_MUTED), true);
		TextRight(right, 34, 28, AROR_ImportShop.Money(personal), C(personalColor), true);
		if (m_bLeader)
		{
			float factionRight = right - 270;
			TextRight(factionRight, 14, 14, "FACTION FUNDS", C(AROR_UIColors.TEXT_MUTED), true);
			TextRight(factionRight, 34, 28, AROR_ImportShop.Money(faction), C(factionColor), true);
		}

		Panel(0, 84, WIDTH, 1, C(AROR_UIColors.LINE));
	}

	//------------------------------------------------------------------------------------------------
	//! Import facilities held, with the seaport / heliport / airport split; Imports also shows the garage slots
	protected void DrawFacilityStrip(float width, bool showGarage)
	{
		Panel(MARGIN, STRIP_TOP, width, STRIP_HEIGHT, C(AROR_UIColors.LINE));
		Panel(MARGIN + 1, STRIP_TOP + 1, width - 2, STRIP_HEIGHT - 2, C(AROR_UIColors.SURFACE_RAISED));

		float x = MARGIN + 24;
		Text(x, STRIP_TOP + 28, 240, 22, 15, "IMPORT FACILITIES", C(AROR_UIColors.TEXT_MUTED), true);
		Text(x + 200, STRIP_TOP + 12, 80, 50, 40, m_iFacilities.ToString(), C(AROR_UIColors.TEXT), true);

		int heliports = AROR_ImportPoints.CountHeld(AROR_EFactionRole.RESISTANCE, AROR_ELocationType.HELIPORT);
		int airports = m_iAir - heliports;
		float chipX = x + 290;
		DrawFacilityChip(chipX, "SEAPORTS", m_iPorts);
		DrawFacilityChip(chipX + 170, "HELIPORTS", heliports);
		DrawFacilityChip(chipX + 340, "AIRPORTS", airports);

		if (showGarage)
		{
			AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
			int capacity;
			int slotsFree = AROR_Imports.GetFreeSlots();
			if (garage)
				capacity = garage.GetCapacity();

			int slotColor = AROR_UIColors.TEXT;
			if (slotsFree <= 1)
				slotColor = AROR_UIColors.WARN;

			float right = MARGIN + width - 24;
			TextRight(right, STRIP_TOP + 20, 24, string.Format("/ %1", capacity), C(AROR_UIColors.TEXT_MUTED), true);
			TextRight(right - TextWidth(string.Format("/ %1", capacity), 24) - 12, STRIP_TOP + 12, 40, slotsFree.ToString(), C(slotColor), true);
			TextRight(right - 150, STRIP_TOP + 28, 15, "GARAGE SLOTS FREE", C(AROR_UIColors.TEXT_MUTED), true);
			return;
		}

		string hint = "Hold more seaports, heliports and airports to unlock heavier imports.";
		if (m_iFacilities < 1)
			hint = "Hold at least one seaport, heliport or airport to import.";
		Text(MARGIN + width - 24 - 330, STRIP_TOP + 16, 330, 44, 15, hint, C(AROR_UIColors.TEXT_MUTED));
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawFacilityChip(float x, string label, int count)
	{
		int color = AROR_UIColors.TEXT_FAINT;
		if (count > 0)
			color = AROR_UIColors.TEXT;

		Text(x, STRIP_TOP + 26, 170, 24, 17, string.Format("%1  %2", label, count), C(color), count > 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Zero facilities held: the shop is closed and the facilities to take are listed
	protected void DrawNoAccess(string verb)
	{
		float top = STRIP_TOP;
		float height = 846 - STRIP_TOP;
		Panel(MARGIN, top, WIDTH - MARGIN * 2, height, C(AROR_UIColors.LINE));
		Panel(MARGIN + 1, top + 1, WIDTH - MARGIN * 2 - 2, height - 2, C(AROR_UIColors.SURFACE_RAISED));
		Panel(WIDTH * 0.5, top + 1, 1, height - 2, C(AROR_UIColors.LINE));

		Text(MARGIN + 48, top + 150, 700, 28, 20, "NO ACCESS", C(AROR_UIColors.DANGER), true);
		Text(MARGIN + 48, top + 190, 700, 110, 44, "NO IMPORT FACILITIES HELD", C(AROR_UIColors.TEXT), true);
		Text(MARGIN + 48, top + 320, 700, 120, 21, "Nothing can be " + verb + " until the Resistance holds a seaport, heliport or airport. Capture one at its flag, then come back.", C(AROR_UIColors.TEXT_MUTED));

		Text(WIDTH * 0.5 + 40, top + 36, 700, 24, 16, "IMPORT FACILITIES ON THIS MAP", C(AROR_UIColors.TEXT_MUTED), true);
		array<AROR_LocationComponent> facilities = {};
		AROR_ImportPoints.GetFacilities(facilities);
		float y = top + 80;
		foreach (AROR_LocationComponent facility : facilities)
		{
			if (!facility || y > 820)
				continue;

			Panel(WIDTH * 0.5 + 40, y - 8, WIDTH * 0.5 - MARGIN - 80, 1, C(AROR_UIColors.LINE));
			Text(WIDTH * 0.5 + 40, y, 480, 28, 22, facility.GetName(), C(AROR_UIColors.TEXT), true);
			TextRight(WIDTH - MARGIN - 40, y + 4, 15, "HELD BY " + AROR_RebIntelHud.ArmyName(facility.GetControllingRole()), C(AROR_UIColors.TEXT_MUTED), true);
			Text(WIDTH * 0.5 + 40, y + 28, 480, 22, 16, FacilityKind(facility.GetLocationType()), C(AROR_UIColors.TEXT_FAINT));
			y += 70;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string FacilityKind(AROR_ELocationType type)
	{
		if (type == AROR_ELocationType.PORT)
			return "Seaport";
		if (type == AROR_ELocationType.HELIPORT)
			return "Heliport";
		return "Airport";
	}

	//------------------------------------------------------------------------------------------------
	//! What the offer needs, one line each: facility count, then the special location. Missing ones show NEEDS in amber.
	protected void DrawRequirements(notnull AROR_ShopOffer offer, float x, float y)
	{
		float lineY = y;
		if (offer.m_iFacilities > 0)
		{
			string facilities = string.Format("%1 FACILITIES", offer.m_iFacilities);
			if (offer.m_iFacilities == 1)
				facilities = "1 FACILITY";

			DrawRequirementLine(x, lineY, facilities, m_iFacilities >= offer.m_iFacilities);
			lineY += 26;
		}

		string special = AROR_ImportPoints.SpecialName(offer.m_eSpecial);
		if (!special.IsEmpty())
		{
			bool held = m_iPorts >= 1;
			if (offer.m_eSpecial == AROR_EImportPointRequirement.AIR)
				held = m_iAir >= 1;

			DrawRequirementLine(x, lineY, Upper(special), held);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawRequirementLine(float x, float y, string label, bool met)
	{
		if (met)
			Text(x, y, 280, 22, 15, label, C(AROR_UIColors.TEXT_MUTED), true);
		else
			Text(x, y, 280, 22, 15, "NEEDS " + label, C(AROR_UIColors.WARN), true);
	}

	//------------------------------------------------------------------------------------------------
	// Scrolling
	//------------------------------------------------------------------------------------------------
	//! Arrows and a thumb beside a list that shows `visible` of `total` rows starting at `first`.
	//! Buttons carry "<actionPrefix>:up" and "<actionPrefix>:down".
	protected void DrawScrollbar(float x, float top, float height, int total, int visible, int first, string actionPrefix)
	{
		if (total <= visible)
			return;

		Button(x, top, SCROLLBAR_WIDTH, 24, "^", actionPrefix + ":up", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.REBEL), 14);
		Button(x, top + height - 24, SCROLLBAR_WIDTH, 24, "v", actionPrefix + ":down", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.REBEL), 14);

		float trackTop = top + 30;
		float trackHeight = height - 60;
		Panel(x + 8, trackTop, 4, trackHeight, C(AROR_UIColors.LINE));
		float thumbHeight = Math.Max(trackHeight * visible / total, 24);
		float thumbTop = trackTop + (trackHeight - thumbHeight) * first / (total - visible);
		Panel(x + 6, thumbTop, 8, thumbHeight, C(AROR_UIColors.LINE_STRONG));
	}

	//------------------------------------------------------------------------------------------------
	protected static int ClampScroll(int first, int total, int visible)
	{
		return Math.ClampInt(first, 0, Math.Max(total - visible, 0));
	}

	//------------------------------------------------------------------------------------------------
	// Purchase popup and sound
	//------------------------------------------------------------------------------------------------
	protected void OnShopResult(AROR_ShopResult result)
	{
		if (!result || result.m_bVehicle != IsVehicleShop())
			return;

		// The same purchase again while its popup is still open raises the count instead of stacking
		if (result.m_bOk && !result.m_sKey.IsEmpty() && m_Popup && m_Popup.m_bOk && m_Popup.m_sKey == result.m_sKey)
			m_iPopupCount++;
		else
			m_iPopupCount = 1;

		m_Popup = result;
		GetGame().GetCallqueue().Remove(ClosePopup);
		GetGame().GetCallqueue().CallLater(ClosePopup, POPUP_SECONDS * 1000);

		if (result.m_bOk)
			SCR_UISoundEntity.SoundEvent(SoundSuccess(), true);
		else
			SCR_UISoundEntity.SoundEvent(SoundError(), true);

		OnResultShown(result);
	}

	//------------------------------------------------------------------------------------------------
	//! A result for this shop has arrived (the Black Market empties its cart on success)
	protected void OnResultShown(AROR_ShopResult result)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! UI sound events (see SCR_SoundEvent); change here to use other sounds
	protected static string SoundSuccess()
	{
		return SCR_SoundEvent.TASK_SUCCEED;
	}

	//------------------------------------------------------------------------------------------------
	protected static string SoundError()
	{
		return SCR_SoundEvent.SOUND_CP_NEGATIVEFEEDBACK_NEGATIVE;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClosePopup()
	{
		GetGame().GetCallqueue().Remove(ClosePopup);
		m_Popup = null;
		m_iPopupCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawPopup()
	{
		if (!m_Popup)
			return;

		float x = POPUP_LEFT;
		float y = POPUP_TOP;
		int accent = AROR_UIColors.DANGER;
		int ink = AROR_UIColors.DANGER_INK;
		string heading = "PURCHASE FAILED";
		if (m_Popup.m_bOk)
		{
			accent = AROR_UIColors.REBEL;
			ink = AROR_UIColors.REBEL_INK;
			heading = "PURCHASE SUCCESSFUL";
		}

		Panel(x, y, POPUP_WIDTH, POPUP_HEIGHT, C(accent));
		Panel(x + 1, y + 1, POPUP_WIDTH - 2, POPUP_HEIGHT - 2, C(AROR_UIColors.PLATE_STRONG));
		Panel(x, y, POPUP_WIDTH, 40, C(accent));
		Text(x + 20, y + 9, POPUP_WIDTH - 140, 26, 18, heading, C(ink), true);
		if (m_iPopupCount > 1)
			TextRight(x + POPUP_WIDTH - 20, y + 9, 18, string.Format("x%1", m_iPopupCount), C(ink), true);

		Text(x + 20, y + 48, POPUP_WIDTH - 40, 40, 28, Upper(m_Popup.m_sMain), C(AROR_UIColors.TEXT), true);
		Text(x + 20, y + 90, POPUP_WIDTH - 150, 24, 16, m_Popup.m_sSub, C(AROR_UIColors.TEXT_MUTED));
		Button(x + POPUP_WIDTH - 120, y + 104, 100, 34, "OK", "popup:close", C(AROR_UIColors.SURFACE_RAISED), C(AROR_UIColors.TEXT), 15);
	}

	//------------------------------------------------------------------------------------------------
	// Search box (Black Market)
	//------------------------------------------------------------------------------------------------
	protected void ShowSearch(bool show, float x, float y, float width, float height)
	{
		if (!m_wSearch)
			return;

		int value = 0;
		if (show)
			value = 1;

		if (m_iSearchShown != value)
		{
			m_iSearchShown = value;
			m_wSearch.SetVisible(show);
		}

		if (show)
			Place(m_wSearch, x * m_fScale, y * m_fScale, width * m_fScale, height * m_fScale);
	}

	//------------------------------------------------------------------------------------------------
	// Helpers
	//------------------------------------------------------------------------------------------------
	protected static Color C(int token)
	{
		return AROR_UIColors.Get(token);
	}
}
