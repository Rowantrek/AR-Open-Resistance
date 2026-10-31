[ComponentEditorProps(category: "OpenResistance/Economy", description: "Faction funds and player money. Attach to the game mode entity.")]
class AROR_EconomyComponentClass : SCR_BaseGameModeComponentClass
{
}

void AROR_OnFactionFundsChanged(AROR_EFactionRole role, int funds);
typedef func AROR_OnFactionFundsChanged;

void AROR_OnLocalPlayerMoneyChanged(int money);
typedef func AROR_OnLocalPlayerMoneyChanged;

void AROR_OnManpowerChanged(int manpower);
typedef func AROR_OnManpowerChanged;

//! Economy (see ARCHITECTURE.md -> Economy): a bank balance per faction role, personal money per player, and the
//! Resistance's manpower. The server owns all balances. Faction funds and manpower replicate to every client; a
//! player's money is sent only to that player. All are saved (AROR_EconomyComponentSerializer); player money is keyed
//! by the player's identity so it survives reconnects and restarts.
class AROR_EconomyComponent : SCR_BaseGameModeComponent
{
	// DEBUG: raised for HICOMM squad testing (squads cost funds and manpower); rebalance later
	[Attribute("40", desc: "Resistance manpower when a new campaign starts", params: "0 inf", category: "Starting values")]
	protected int m_iStartingManpower;

	// DEBUG: raised for HICOMM squad testing; rebalance later
	[Attribute("20000", desc: "Resistance funds when a new campaign starts", params: "0 inf", category: "Starting values")]
	protected int m_iStartingResistanceFunds;

	[Attribute("10000", desc: "Occupiers funds when a new campaign starts", params: "0 inf", category: "Starting values")]
	protected int m_iStartingOccupiersFunds;

	[Attribute("10000", desc: "Invaders funds when a new campaign starts", params: "0 inf", category: "Starting values")]
	protected int m_iStartingInvadersFunds;

	[Attribute("100", desc: "Personal money for a player joining the campaign for the first time", params: "0 inf", category: "Starting values")]
	protected int m_iStartingPlayerMoney;

	protected static AROR_EconomyComponent s_Instance;

	[RplProp(onRplName: "OnFundsReplicated")]
	protected int m_iResistanceFunds;

	[RplProp(onRplName: "OnFundsReplicated")]
	protected int m_iOccupiersFunds;

	[RplProp(onRplName: "OnFundsReplicated")]
	protected int m_iInvadersFunds;

	//! Resistance only
	[RplProp(onRplName: "OnManpowerReplicated")]
	protected int m_iManpower;

	protected ref ScriptInvokerBase<AROR_OnManpowerChanged> m_OnManpowerChanged = new ScriptInvokerBase<AROR_OnManpowerChanged>();

	//! Server: player key (identity ID) -> money
	protected ref map<string, int> m_mPlayerMoney = new map<string, int>();

	//! Client: the local player's money, as last sent by the server
	protected int m_iLocalPlayerMoney;

	protected ref ScriptInvokerBase<AROR_OnFactionFundsChanged> m_OnFactionFundsChanged = new ScriptInvokerBase<AROR_OnFactionFundsChanged>();
	protected ref ScriptInvokerBase<AROR_OnLocalPlayerMoneyChanged> m_OnLocalPlayerMoneyChanged = new ScriptInvokerBase<AROR_OnLocalPlayerMoneyChanged>();

	//------------------------------------------------------------------------------------------------
	static AROR_EconomyComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine; fires on clients when the replicated funds arrive and on the server when they change
	ScriptInvokerBase<AROR_OnFactionFundsChanged> GetOnFactionFundsChanged()
	{
		return m_OnFactionFundsChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Client; fires when the server sends the local player's money
	ScriptInvokerBase<AROR_OnLocalPlayerMoneyChanged> GetOnLocalPlayerMoneyChanged()
	{
		return m_OnLocalPlayerMoneyChanged;
	}

	//------------------------------------------------------------------------------------------------
	// Faction funds
	//------------------------------------------------------------------------------------------------
	//! Every machine
	int GetFactionFunds(AROR_EFactionRole role)
	{
		switch (role)
		{
			case AROR_EFactionRole.RESISTANCE:
				return m_iResistanceFunds;
			case AROR_EFactionRole.OCCUPIERS:
				return m_iOccupiersFunds;
			case AROR_EFactionRole.INVADERS:
				return m_iInvadersFunds;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Adds (or with a negative amount, removes) funds; never goes below 0.
	void AddFactionFunds_S(AROR_EFactionRole role, int amount)
	{
		if (!Replication.IsServer())
			return;

		SetFactionFunds(role, Math.Max(GetFactionFunds(role) + amount, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Takes the amount if the faction can afford it; returns false (and takes nothing) otherwise.
	bool SpendFactionFunds_S(AROR_EFactionRole role, int amount)
	{
		if (!Replication.IsServer() || amount < 0 || GetFactionFunds(role) < amount)
			return false;

		SetFactionFunds(role, GetFactionFunds(role) - amount);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: restore saved funds (used by the serializer)
	void RestoreFactionFunds_S(int resistance, int occupiers, int invaders)
	{
		if (!Replication.IsServer())
			return;

		SetFactionFunds(AROR_EFactionRole.RESISTANCE, resistance);
		SetFactionFunds(AROR_EFactionRole.OCCUPIERS, occupiers);
		SetFactionFunds(AROR_EFactionRole.INVADERS, invaders);
	}

	//------------------------------------------------------------------------------------------------
	//! True while every faction still has its starting funds (nothing to save)
	bool HasStartingFactionFunds()
	{
		return m_iResistanceFunds == m_iStartingResistanceFunds
			&& m_iOccupiersFunds == m_iStartingOccupiersFunds
			&& m_iInvadersFunds == m_iStartingInvadersFunds;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetFactionFunds(AROR_EFactionRole role, int funds)
	{
		if (funds == GetFactionFunds(role))
			return;

		switch (role)
		{
			case AROR_EFactionRole.RESISTANCE:
				m_iResistanceFunds = funds;
				break;
			case AROR_EFactionRole.OCCUPIERS:
				m_iOccupiersFunds = funds;
				break;
			case AROR_EFactionRole.INVADERS:
				m_iInvadersFunds = funds;
				break;
			default:
				return;
		}

		Replication.BumpMe();
		m_OnFactionFundsChanged.Invoke(role, funds);
	}

	//------------------------------------------------------------------------------------------------
	//! Clients: which role changed is not known, so report all three
	protected void OnFundsReplicated()
	{
		m_OnFactionFundsChanged.Invoke(AROR_EFactionRole.RESISTANCE, m_iResistanceFunds);
		m_OnFactionFundsChanged.Invoke(AROR_EFactionRole.OCCUPIERS, m_iOccupiersFunds);
		m_OnFactionFundsChanged.Invoke(AROR_EFactionRole.INVADERS, m_iInvadersFunds);
	}

	//------------------------------------------------------------------------------------------------
	// Manpower (Resistance)
	//------------------------------------------------------------------------------------------------
	//! Every machine
	int GetManpower()
	{
		return m_iManpower;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine; fires on clients when the replicated value arrives and on the server when it changes
	ScriptInvokerBase<AROR_OnManpowerChanged> GetOnManpowerChanged()
	{
		return m_OnManpowerChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Adds (or with a negative amount, removes) manpower; never goes below 0.
	void AddManpower_S(int amount)
	{
		if (Replication.IsServer())
			SetManpower(Math.Max(m_iManpower + amount, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Takes the amount if there is enough manpower; returns false (and takes nothing) otherwise.
	bool SpendManpower_S(int amount)
	{
		if (!Replication.IsServer() || amount < 0 || m_iManpower < amount)
			return false;

		SetManpower(m_iManpower - amount);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: restore saved manpower (used by the serializer)
	void RestoreManpower_S(int manpower)
	{
		if (Replication.IsServer())
			SetManpower(manpower);
	}

	//------------------------------------------------------------------------------------------------
	//! True while manpower is still at its starting value (nothing to save)
	bool HasStartingManpower()
	{
		return m_iManpower == m_iStartingManpower;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetManpower(int manpower)
	{
		if (manpower == m_iManpower)
			return;

		m_iManpower = manpower;
		Replication.BumpMe();
		m_OnManpowerChanged.Invoke(manpower);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnManpowerReplicated()
	{
		m_OnManpowerChanged.Invoke(m_iManpower);
	}

	//------------------------------------------------------------------------------------------------
	// Formatting
	//------------------------------------------------------------------------------------------------
	//! 1234567 -> "$ 1,234,567.00"
	static string FormatMoney(int amount)
	{
		string digits = Math.AbsInt(amount).ToString();
		int count = digits.Length();

		string grouped;
		for (int i = 0; i < count; i++)
		{
			if (i > 0 && (count - i) % 3 == 0)
				grouped += ",";

			grouped += digits.Substring(i, 1);
		}

		if (amount < 0)
			return "$ -" + grouped + ".00";

		return "$ " + grouped + ".00";
	}

	//------------------------------------------------------------------------------------------------
	// Player money
	//------------------------------------------------------------------------------------------------
	//! Client: the local player's money
	int GetLocalPlayerMoney()
	{
		return m_iLocalPlayerMoney;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Players new to the campaign start with the starting money.
	int GetPlayerMoney_S(int playerId)
	{
		string key = GetPlayerKey(playerId);
		if (key.IsEmpty())
			return 0;

		int money;
		if (m_mPlayerMoney.Find(key, money))
			return money;

		return m_iStartingPlayerMoney;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Adds (or with a negative amount, removes) money; never goes below 0.
	void AddPlayerMoney_S(int playerId, int amount)
	{
		if (!Replication.IsServer())
			return;

		SetPlayerMoney(playerId, Math.Max(GetPlayerMoney_S(playerId) + amount, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Takes the amount if the player can afford it; returns false (and takes nothing) otherwise.
	bool SpendPlayerMoney_S(int playerId, int amount)
	{
		if (!Replication.IsServer() || amount < 0 || GetPlayerMoney_S(playerId) < amount)
			return false;

		SetPlayerMoney(playerId, GetPlayerMoney_S(playerId) - amount);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Moves personal money from one online player to another. Rejects non-positive amounts,
	//! self-transfers, and offline or unknown targets.
	bool TransferPlayerMoney_S(int fromPlayerId, int toPlayerId, int amount)
	{
		if (!Replication.IsServer() || amount <= 0 || fromPlayerId == toPlayerId)
			return false;

		if (GetPlayerKey(fromPlayerId).IsEmpty() || GetPlayerKey(toPlayerId).IsEmpty())
			return false;

		if (!IsPlayerOnline(toPlayerId))
			return false;

		if (!SpendPlayerMoney_S(fromPlayerId, amount))
			return false;

		AddPlayerMoney_S(toPlayerId, amount);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Moves personal money into Resistance faction funds.
	bool DonatePlayerMoneyToResistance_S(int fromPlayerId, int amount)
	{
		if (!Replication.IsServer() || amount <= 0)
			return false;

		if (GetPlayerKey(fromPlayerId).IsEmpty())
			return false;

		if (!SpendPlayerMoney_S(fromPlayerId, amount))
			return false;

		AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, amount);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true if the session player ID is currently connected
	protected static bool IsPlayerOnline(int playerId)
	{
		if (playerId <= 0)
			return false;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		return playerIds.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: saved player balances, for the serializer
	void GetPlayerMoneyEntries(notnull array<string> keys, notnull array<int> balances)
	{
		foreach (string key, int money : m_mPlayerMoney)
		{
			keys.Insert(key);
			balances.Insert(money);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: restore saved player balances (used by the serializer), then update connected players
	void RestorePlayerMoney_S(notnull array<string> keys, notnull array<int> balances)
	{
		if (!Replication.IsServer())
			return;

		m_mPlayerMoney.Clear();
		int count = Math.Min(keys.Count(), balances.Count());
		for (int i = 0; i < count; i++)
		{
			m_mPlayerMoney.Set(keys[i], balances[i]);
		}

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			SendPlayerMoney(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SetPlayerMoney(int playerId, int money)
	{
		string key = GetPlayerKey(playerId);
		if (key.IsEmpty())
			return;

		m_mPlayerMoney.Set(key, money);
		SendPlayerMoney(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send a player their current money
	protected void SendPlayerMoney(int playerId)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (playerController)
			playerController.AROR_SendPlayerMoney_S(GetPlayerMoney_S(playerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server sent the local player's money (through the player controller)
	void OnLocalPlayerMoneyReceived(int money)
	{
		m_iLocalPlayerMoney = money;
		m_OnLocalPlayerMoneyChanged.Invoke(money);
	}

	//------------------------------------------------------------------------------------------------
	//! Stable key for saving a player's money: the Bohemia identity ID. Workbench and local sessions may have
	//! none, so those fall back to the player ID (not stable across reconnects).
	protected static string GetPlayerKey(int playerId)
	{
		if (playerId <= 0)
			return string.Empty;

		BackendApi backend = GetGame().GetBackendApi();
		if (backend)
		{
			string identity = backend.GetPlayerIdentityId(playerId);
			if (!identity.IsEmpty())
				return identity;
		}

		return "player_" + playerId;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);

		if (Replication.IsServer())
			SendPlayerMoney(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;

		// Fresh campaign values; a loaded save overrides them through the serializer
		if (Replication.IsServer())
		{
			m_iResistanceFunds = m_iStartingResistanceFunds;
			m_iOccupiersFunds = m_iStartingOccupiersFunds;
			m_iInvadersFunds = m_iStartingInvadersFunds;
			m_iManpower = m_iStartingManpower;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
