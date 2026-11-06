//! The Black Market (see ARCHITECTURE.md -> Black Market): a phone shop for items. It sells every item in the enabled
//! content packs except vehicles (those are Imports, see AROR_Imports). Each entry needs a number
//! of held import facilities and optionally one seaport or one heliport/airport. Players fill a cart and import it;
//! the server checks everything again, takes payment (personal funds, or faction funds for a Resistance Leader who
//! chooses so) and puts the items in Resistance Storage.
//! The catalogue is the same on every machine (it comes from the replicated enabled packs); the client sends prefabs
//! and quantities, never prices. Extender mods change what is sold by adding or changing content packs.
class AROR_BlackMarket
{
	protected static const int MAX_CART_LINES = 40;
	protected static const int MAX_LINE_QUANTITY = 50;

	protected static ref array<ref AROR_ShopOffer> s_aCatalogue;
	protected static int s_iCatalogueRevision;

	// Server: invoked after a delivery, (int playerId, array<string> prefabs, array<int> quantities, int total, bool fromFaction)
	protected static ref ScriptInvoker s_OnImported;

	//------------------------------------------------------------------------------------------------
	//! Tab labels, in tab order. Index = AROR_ShopOffer.m_iCategory.
	static void GetCategoryNames(notnull array<string> outNames)
	{
		outNames.Insert("Weapons");
		outNames.Insert("Accessories");
		outNames.Insert("Ammunition");
		outNames.Insert("Gear");
		outNames.Insert("Equipment");
	}

	//------------------------------------------------------------------------------------------------
	//! Preset category behind each tab, in tab order
	protected static AROR_EFactionPresetCategory GetPresetCategory(int category)
	{
		switch (category)
		{
			case 0: return AROR_EFactionPresetCategory.WEAPONS;
			case 1: return AROR_EFactionPresetCategory.WEAPON_ACCESSORIES;
			case 2: return AROR_EFactionPresetCategory.AMMUNITION;
			case 3: return AROR_EFactionPresetCategory.GEAR;
		}

		return AROR_EFactionPresetCategory.EQUIPMENT;
	}

	//------------------------------------------------------------------------------------------------
	//! Every item on offer, grouped by tab. Rebuilt when the enabled content packs change.
	static array<ref AROR_ShopOffer> GetCatalogue()
	{
		int revision = AROR_ImportShop.GetCatalogueRevision();
		if (!s_aCatalogue || s_iCatalogueRevision != revision)
		{
			s_aCatalogue = {};
			s_iCatalogueRevision = revision;
			Build(s_aCatalogue);
		}

		return s_aCatalogue;
	}

	//------------------------------------------------------------------------------------------------
	//! The enabled content packs' items; each prefab appears once (the packs already resolved duplicates and replacements)
	protected static void Build(notnull array<ref AROR_ShopOffer> outOffers)
	{
		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
		if (!packs)
			return;

		array<string> names = {};
		GetCategoryNames(names);
		foreach (int category, string name : names)
		{
			array<AROR_ContentPackEntry> entries = {};
			packs.GetEnabledEntries(GetPresetCategory(category), entries);
			foreach (AROR_ContentPackEntry entry : entries)
			{
				if (entry.CanResistancePurchase())
					outOffers.Insert(AROR_ImportShop.CreateOffer(entry, category, false));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static AROR_ShopOffer FindOffer(ResourceName prefab)
	{
		foreach (AROR_ShopOffer offer : GetCatalogue())
		{
			if (offer.m_sPrefab == prefab)
				return offer;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: invoked after every delivery (see the fields above)
	static ScriptInvoker GetOnImported_S()
	{
		if (!s_OnImported)
			s_OnImported = new ScriptInvoker();

		return s_OnImported;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: validates the whole cart, takes payment and delivers it to Resistance Storage. All or nothing.
	static void Import_S(int playerId, notnull array<string> prefabs, notnull array<int> quantities, bool fromFaction, notnull AROR_ShopResult result)
	{
		if (!Replication.IsServer())
			return;

		if (!AROR_ImportShop.IsResistancePlayer(playerId))
		{
			result.Fail("Resistance only", "Only the Resistance can trade on the Black Market.");
			return;
		}

		if (AROR_ImportPoints.CountFacilities(AROR_EFactionRole.RESISTANCE) < 1)
		{
			result.Fail("No import facilities", "Hold a seaport, heliport or airport to import.");
			return;
		}

		int lines = Math.Min(prefabs.Count(), quantities.Count());
		if (lines < 1 || lines > MAX_CART_LINES)
		{
			result.Fail("Cart not accepted", "The cart is empty or too large.");
			return;
		}

		// Resolve every line against the preset first: the price always comes from the server's own catalogue
		array<ref AROR_ShopOffer> offers = {};
		array<int> counts = {};
		int total;
		int itemCount;
		for (int i = 0; i < lines; i++)
		{
			AROR_ShopOffer offer = FindOffer(prefabs[i]);
			if (!offer)
			{
				result.Fail("No longer on offer", "An item in your cart is no longer sold.");
				return;
			}

			if (!offer.IsAvailable())
			{
				result.Fail("Requirement not met", string.Format("%1 needs more import facilities.", offer.GetName()));
				return;
			}

			int quantity = Math.Clamp(quantities[i], 0, MAX_LINE_QUANTITY);
			if (quantity < 1)
				continue;

			offers.Insert(offer);
			counts.Insert(quantity);
			total += offer.m_iPrice * quantity;
			itemCount += quantity;
		}

		if (offers.IsEmpty())
		{
			result.Fail("Cart not accepted", "The cart is empty.");
			return;
		}

		AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.GetInstance();
		if (!storage)
		{
			result.Fail("Storage unavailable", "Resistance Storage is not ready.");
			return;
		}

		if (!AROR_ImportShop.Pay_S(playerId, total, fromFaction, result))
			return;

		array<string> delivered = {};
		foreach (int n, AROR_ShopOffer offer : offers)
		{
			storage.Add_S(offer.m_sPrefab, counts[n]);
			delivered.Insert(offer.m_sPrefab);
		}

		string what = string.Format("%1 items", itemCount);
		if (itemCount == 1)
			what = "1 item";

		result.Succeed(what + " delivered", string.Format("Paid %1 from %2. Items are in Storage.",AROR_ImportShop.Money(total), AROR_ImportShop.FundsName(fromFaction)));
		Print(string.Format("[AROR] Black Market: player %1 imported %2 for $%3 (%4)", playerId, what, total, AROR_ImportShop.FundsName(fromFaction)), LogLevel.NORMAL);

		if (fromFaction)
			AROR_ImportShop.AnnounceFactionSpend_S(playerId, what, total);

		GetOnImported_S().Invoke(playerId, delivered, counts, total, fromFaction);
	}
}
