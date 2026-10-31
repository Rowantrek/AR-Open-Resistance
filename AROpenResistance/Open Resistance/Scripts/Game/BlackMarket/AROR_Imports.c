//! Imports ("Used Imports", see ARCHITECTURE.md -> Black Market): a phone shop for vehicles. It sells every land
//! vehicle and helicopter in the enabled content packs, one click at a time (no cart). Each entry needs a number of held import
//! facilities and optionally one seaport or one heliport/airport. A bought vehicle goes straight into the Resistance
//! Garage in factory condition (full fuel, default cargo); the Garage's capacity limits how many can be held.
//! A Resistance Leader pays from faction funds or personal funds per click; everyone else pays personally.
class AROR_Imports
{
	protected static ref array<ref AROR_ShopOffer> s_aCatalogue;
	protected static int s_iCatalogueRevision;

	// Server: invoked after a delivery, (int playerId, string prefab, int price, bool fromFaction)
	protected static ref ScriptInvoker s_OnImported;

	//------------------------------------------------------------------------------------------------
	//! Tab labels, in tab order. Index = AROR_ShopOffer.m_iCategory.
	static void GetCategoryNames(notnull array<string> outNames)
	{
		outNames.Insert("Light vehicles");
		outNames.Insert("Trucks");
		outNames.Insert("Armoured");
		outNames.Insert("Helicopters");
	}

	//------------------------------------------------------------------------------------------------
	//! Tab for a preset vehicle type, or -1 for types Imports does not sell (static weapons, boats)
	protected static int GetCategoryOf(int vehicleType)
	{
		switch (vehicleType)
		{
			case AROR_EVehicleType.MOTOR_CAR:
			case AROR_EVehicleType.ARMED_CAR:
			case AROR_EVehicleType.AMBULANCE:
				return 0;
			case AROR_EVehicleType.MOTOR_TRUCK:
			case AROR_EVehicleType.ARMED_TRUCK:
			case AROR_EVehicleType.SUPPLY_TRUCK:
			case AROR_EVehicleType.FUEL_TRUCK:
				return 1;
			case AROR_EVehicleType.APC:
			case AROR_EVehicleType.IFV:
			case AROR_EVehicleType.TANK:
			case AROR_EVehicleType.AA_VEHICLE:
			case AROR_EVehicleType.RADAR_AA_VEHICLE:
				return 2;
			case AROR_EVehicleType.HELI_TRANSPORT:
			case AROR_EVehicleType.HELI_ATTACK:
				return 3;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
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
	//! The enabled content packs' vehicles; each prefab appears once (the packs already resolved duplicates and replacements)
	protected static void Build(notnull array<ref AROR_ShopOffer> outOffers)
	{
		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
		if (!packs)
			return;

		array<AROR_ContentPackEntry> entries = {};
		packs.GetEnabledEntries(AROR_EFactionPresetCategory.VEHICLES, entries);
		foreach (AROR_ContentPackEntry entry : entries)
		{
			int category = GetCategoryOf(entry.GetTypeValue());
			if (category >= 0)
				outOffers.Insert(AROR_ImportShop.CreateOffer(entry, category, true));
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
	//! Every machine: garage slots still free
	static int GetFreeSlots()
	{
		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (!garage)
			return 0;

		return Math.Max(garage.GetCapacity() - garage.GetCount(), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: buys one vehicle into the Garage. Checks requirements and capacity before taking payment.
	static void Buy_S(int playerId, string prefab, bool fromFaction, notnull AROR_ShopResult result)
	{
		result.m_bVehicle = true;
		if (!Replication.IsServer())
			return;

		if (!AROR_ImportShop.IsResistancePlayer(playerId))
		{
			result.Fail("Resistance only", "Only the Resistance can import vehicles.");
			return;
		}

		AROR_ShopOffer offer = FindOffer(prefab);
		if (!offer)
		{
			result.Fail("No longer on offer", "That vehicle is no longer sold.");
			return;
		}

		if (!offer.IsAvailable())
		{
			result.Fail("Requirement not met", string.Format("%1 needs more import facilities.", offer.GetName()));
			return;
		}

		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (!garage)
		{
			result.Fail("Garage unavailable", "The Resistance Garage is not ready.");
			return;
		}

		if (garage.IsFull())
		{
			result.Fail("Garage full", "Free a garage slot before importing another vehicle.");
			return;
		}

		if (!AROR_ImportShop.Pay_S(playerId, offer.m_iPrice, fromFaction, result))
			return;

		if (garage.AddVehicleByPrefab_S(offer.m_sPrefab) < 0)
		{
			AROR_ImportShop.Refund_S(playerId, offer.m_iPrice, fromFaction);
			result.Fail("Import failed", "The vehicle could not be added to the Garage. You were not charged.");
			return;
		}

		string key = offer.m_sPrefab;
		if (fromFaction)
			key += ":faction";
		else
			key += ":personal";

		result.Succeed(offer.GetName(), string.Format("Paid %1 from %2. In the Garage.",AROR_ImportShop.Money(offer.m_iPrice), AROR_ImportShop.FundsName(fromFaction)), key);
		Print(string.Format("[AROR] Imports: player %1 imported %2 for $%3 (%4)", playerId, offer.GetName(), offer.m_iPrice, AROR_ImportShop.FundsName(fromFaction)), LogLevel.NORMAL);

		if (fromFaction)
			AROR_ImportShop.AnnounceFactionSpend_S(playerId, offer.GetName(), offer.m_iPrice);

		GetOnImported_S().Invoke(playerId, offer.m_sPrefab, offer.m_iPrice, fromFaction);
	}
}
