//! One thing the Black Market or Imports sells: an enabled content pack entry plus its price.
class AROR_ShopOffer : Managed
{
	ResourceName m_sPrefab;
	int m_iCategory;		//!< Index into the shop's category list
	int m_iPrice;
	int m_iFacilities;		//!< Import facilities the Resistance must hold
	AROR_EImportPointRequirement m_eSpecial;
	string m_sType;			//!< Entry type, e.g. "Rifle" or "Motor Truck"
	bool m_bVehicle;
	protected string m_sName;

	//------------------------------------------------------------------------------------------------
	//! Catalogue name (resolved on first use, cached)
	string GetName()
	{
		if (m_sName.IsEmpty())
			m_sName = AROR_PrefabInfo.GetName(m_sPrefab, m_bVehicle);

		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetImage()
	{
		return AROR_PrefabInfo.GetImage(m_sPrefab, m_bVehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the Resistance holds what this offer needs (every machine: location control is replicated)
	bool IsAvailable()
	{
		return AROR_ImportPoints.IsMet(AROR_EFactionRole.RESISTANCE, m_eSpecial, m_iFacilities);
	}
}

//! The outcome of a purchase, shown by the app as a popup and a sound
class AROR_ShopResult : Managed
{
	bool m_bOk;
	string m_sMain;		//!< e.g. "UAZ-469" or "Not enough personal funds"
	string m_sSub;		//!< e.g. "Paid $1,800 from faction funds. Delivered to the Garage."
	string m_sKey;		//!< Equal keys merge into one popup with a count (same vehicle, same payer)
	bool m_bVehicle;	//!< True for an Imports (vehicle) purchase

	//------------------------------------------------------------------------------------------------
	void Fail(string main, string sub)
	{
		m_bOk = false;
		m_sMain = main;
		m_sSub = sub;
	}

	//------------------------------------------------------------------------------------------------
	void Succeed(string main, string sub, string key = string.Empty)
	{
		m_bOk = true;
		m_sMain = main;
		m_sSub = sub;
		m_sKey = key;
	}
}

//! Rules and plumbing shared by the Black Market (items) and Imports (vehicles): who may buy, who pays, and the
//! client-side event that tells the open app how a purchase went.
class AROR_ImportShop
{
	protected static ref ScriptInvoker s_OnResult;

	//------------------------------------------------------------------------------------------------
	//! Client: invoked with an AROR_ShopResult when the server answers a purchase
	static ScriptInvoker GetOnResult()
	{
		if (!s_OnResult)
			s_OnResult = new ScriptInvoker();

		return s_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsResistancePlayer(int playerId)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		AROR_EFactionRole role;
		return roles && roles.GetRoleOfFaction(SCR_FactionManager.SGetPlayerFaction(playerId), role) && role == AROR_EFactionRole.RESISTANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! Only a Resistance Leader (HICOMM host or delegate) may spend faction funds. Every machine.
	static bool CanSpendFaction(int playerId)
	{
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		return highCommand && highCommand.IsLeader(playerId);
	}

	//------------------------------------------------------------------------------------------------
	static string FundsName(bool fromFaction)
	{
		if (fromFaction)
			return "faction funds";

		return "personal funds";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spends the amount from faction funds (leaders only) or the player's personal money. On failure the
	//! result is filled in and false is returned; nothing is taken.
	static bool Pay_S(int playerId, int amount, bool fromFaction, notnull AROR_ShopResult result)
	{
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy)
		{
			result.Fail("Economy unavailable", "Try again in a moment.");
			return false;
		}

		if (fromFaction)
		{
			if (!CanSpendFaction(playerId))
			{
				result.Fail("Leader only", "Only the Resistance Leader can spend faction funds.");
				return false;
			}

			if (!economy.SpendFactionFunds_S(AROR_EFactionRole.RESISTANCE, amount))
			{
				result.Fail("Not enough faction funds", string.Format("Short %1. Nothing was bought.", Money(amount - economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE))));
				return false;
			}

			return true;
		}

		if (!economy.SpendPlayerMoney_S(playerId, amount))
		{
			result.Fail("Not enough personal funds", string.Format("Short %1. Nothing was bought.", Money(amount - economy.GetPlayerMoney_S(playerId))));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives a payment back (a delivery that could not be completed)
	static void Refund_S(int playerId, int amount, bool fromFaction)
	{
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy)
			return;

		if (fromFaction)
			economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, amount);
		else
			economy.AddPlayerMoney_S(playerId, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells everyone when a Leader spent faction funds
	static void AnnounceFactionSpend_S(int playerId, string what, int amount)
	{
		string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		string text = string.Format("%1 imported %2 for %3", name, what, Money(amount));
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.NOTIFICATION, text, string.Empty, AROR_UIColors.Get(AROR_UIColors.REBEL), "Faction funds"));
	}

	//------------------------------------------------------------------------------------------------
	//! "$12,400"
	static string Money(int amount)
	{
		string digits = Math.AbsInt(amount).ToString();
		string grouped;
		int count = digits.Length();
		for (int i = 0; i < count; i++)
		{
			if (i > 0 && (count - i) % 3 == 0)
				grouped += ",";
			grouped += digits.Get(i);
		}

		if (amount < 0)
			return "-$" + grouped;

		return "$" + grouped;
	}

	//------------------------------------------------------------------------------------------------
	//! Offer for one content pack entry
	static AROR_ShopOffer CreateOffer(notnull AROR_ContentPackEntry entry, int category, bool vehicle)
	{
		AROR_ShopOffer offer = new AROR_ShopOffer();
		offer.m_sPrefab = entry.GetPrefab();
		offer.m_iCategory = category;
		offer.m_iPrice = entry.GetCost();
		offer.m_iFacilities = entry.GetRequiredImportPoints();
		offer.m_eSpecial = entry.GetImportPointRequirement();
		offer.m_sType = AROR_TextUtils.FormatEnumName(entry.GetTypeName());
		offer.m_bVehicle = vehicle;
		return offer;
	}

	//------------------------------------------------------------------------------------------------
	//! Cache key for the shop catalogues: changes whenever the enabled content packs do (-1 before they exist)
	static int GetCatalogueRevision()
	{
		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
		if (!packs)
			return -1;

		return packs.GetRevision();
	}
}
