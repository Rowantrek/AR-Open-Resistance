//! Network bridge for the game flow. Clients can only send RPCs through entities they own,
//! so setup requests and replies travel through the player's controller and are handled by AROR_GameFlowComponent.
modded class SCR_PlayerController
{
	//! Only an observation-suppression bit, never a spotting target or coordinates.
	void AROR_SetQRFMapOpen(bool open)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_QRFMapOpen(open);
		else
			Rpc(AROR_RpcAsk_QRFMapOpen, open);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_QRFMapOpen(bool open)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
			operations.SetQRFObserverMapOpen_S(GetPlayerId(), open);
	}
	//! Only the server originates this owner-targeted snapshot; clients cannot submit sightings.
	void AROR_SendQRFIntel_S(array<string> ids, array<vector> positions, array<int> kinds, array<int> ages, int lifetime, array<int> sides)
	{
		if (!Replication.IsServer())
			return;
		if (AROR_IsLocal())
			AROR_RpcDo_QRFIntel(ids, positions, kinds, ages, lifetime, sides);
		else
			Rpc(AROR_RpcDo_QRFIntel, ids, positions, kinds, ages, lifetime, sides);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_QRFIntel(array<string> ids, array<vector> positions, array<int> kinds, array<int> ages, int lifetime, array<int> sides)
	{
		AROR_QRFMapIntel.Receive(ids, positions, kinds, ages, lifetime, sides);
	}

	//! Separator for the save list, sent as a single string
	protected static const string AROR_SAVE_LIST_SEPARATOR = "\n";

	//------------------------------------------------------------------------------------------------
	// Client -> server
	//------------------------------------------------------------------------------------------------
	void AROR_RequestNewPlaythrough(vector hqPosition, string playthroughName)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_NewPlaythrough(hqPosition, playthroughName);
		else
			Rpc(AROR_RpcAsk_NewPlaythrough, hqPosition, playthroughName);
	}

	//------------------------------------------------------------------------------------------------
	void AROR_RequestSaveList()
	{
		if (Replication.IsServer())
			AROR_RpcAsk_SaveList();
		else
			Rpc(AROR_RpcAsk_SaveList);
	}

	//------------------------------------------------------------------------------------------------
	void AROR_RequestLoad(int saveIndex)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_Load(saveIndex);
		else
			Rpc(AROR_RpcAsk_Load, saveIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Admin menu: unlock Game Master for this player
	void AROR_RequestAdminGameMaster()
	{
		if (Replication.IsServer())
			AROR_RpcAsk_AdminGameMaster();
		else
			Rpc(AROR_RpcAsk_AdminGameMaster);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_AdminGameMaster()
	{
		AROR_AdminMenuComponent adminMenu = AROR_AdminMenuComponent.GetInstance();
		if (adminMenu)
			adminMenu.GrantGameMaster_S(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Admin menu: change support of the town or city nearest to this player
	void AROR_RequestAdminSupport(AROR_EFactionRole role, float delta)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_AdminSupport(role, delta);
		else
			Rpc(AROR_RpcAsk_AdminSupport, role, delta);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_AdminSupport(int role, float delta)
	{
		AROR_AdminMenuComponent adminMenu = AROR_AdminMenuComponent.GetInstance();
		if (adminMenu)
			adminMenu.AdjustNearestSupport_S(GetPlayerId(), role, delta);
	}

	//------------------------------------------------------------------------------------------------
	//! Admin menu: send test notifications and a test hint
	void AROR_RequestAdminTestNotifications()
	{
		if (Replication.IsServer())
			AROR_RpcAsk_AdminTestNotifications();
		else
			Rpc(AROR_RpcAsk_AdminTestNotifications);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_AdminTestNotifications()
	{
		AROR_AdminMenuComponent adminMenu = AROR_AdminMenuComponent.GetInstance();
		if (adminMenu)
			adminMenu.SendTestNotifications_S(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Admin menu: add money to a faction's funds, or (selfMoney) to this player's own money
	void AROR_RequestAdminMoney(AROR_EFactionRole role, bool selfMoney, int amount)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_AdminMoney(role, selfMoney, amount);
		else
			Rpc(AROR_RpcAsk_AdminMoney, role, selfMoney, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Admin menu: run an income payment now
	void AROR_RequestAdminPayIncome()
	{
		if (Replication.IsServer())
			AROR_RpcAsk_AdminPayIncome();
		else
			Rpc(AROR_RpcAsk_AdminPayIncome);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_AdminPayIncome()
	{
		AROR_AdminMenuComponent adminMenu = AROR_AdminMenuComponent.GetInstance();
		if (adminMenu)
			adminMenu.PayIncomeNow_S(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_AdminMoney(int role, bool selfMoney, int amount)
	{
		AROR_AdminMenuComponent adminMenu = AROR_AdminMenuComponent.GetInstance();
		if (adminMenu)
			adminMenu.GiveMoney_S(GetPlayerId(), role, selfMoney, amount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_NewPlaythrough(vector hqPosition, string playthroughName)
	{
		AROR_GameFlowComponent gameFlow = AROR_GameFlowComponent.GetInstance();
		if (gameFlow)
			gameFlow.RequestNewPlaythrough_S(GetPlayerId(), hqPosition, playthroughName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_SaveList()
	{
		AROR_GameFlowComponent gameFlow = AROR_GameFlowComponent.GetInstance();
		if (gameFlow)
			gameFlow.RequestSaveList_S(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_Load(int saveIndex)
	{
		AROR_GameFlowComponent gameFlow = AROR_GameFlowComponent.GetInstance();
		if (gameFlow)
			gameFlow.RequestLoad_S(GetPlayerId(), saveIndex);
	}

	//------------------------------------------------------------------------------------------------
	// Server -> owning client
	//------------------------------------------------------------------------------------------------
	void AROR_OpenSetup_S()
	{
		if (AROR_IsLocal())
			AROR_RpcDo_OpenSetup();
		else
			Rpc(AROR_RpcDo_OpenSetup);
	}

	//------------------------------------------------------------------------------------------------
	void AROR_SendSaveList_S(notnull array<string> saveLabels)
	{
		string joined;
		foreach (int i, string label : saveLabels)
		{
			if (i > 0)
				joined += AROR_SAVE_LIST_SEPARATOR;

			joined += label;
		}

		if (AROR_IsLocal())
			AROR_RpcDo_SaveList(joined);
		else
			Rpc(AROR_RpcDo_SaveList, joined);
	}

	//------------------------------------------------------------------------------------------------
	void AROR_SendSetupResult_S(bool success, string message)
	{
		if (AROR_IsLocal())
			AROR_RpcDo_SetupResult(success, message);
		else
			Rpc(AROR_RpcDo_SetupResult, success, message);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_OpenSetup()
	{
		AROR_GameFlowComponent gameFlow = AROR_GameFlowComponent.GetInstance();
		if (gameFlow)
			gameFlow.OnSetupRequestedReceived();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_SaveList(string joinedLabels)
	{
		array<string> saveLabels = {};
		if (!joinedLabels.IsEmpty())
			joinedLabels.Split(AROR_SAVE_LIST_SEPARATOR, saveLabels, false);

		AROR_GameFlowComponent gameFlow = AROR_GameFlowComponent.GetInstance();
		if (gameFlow)
			gameFlow.OnSaveListReceived(saveLabels);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_SetupResult(bool success, string message)
	{
		AROR_GameFlowComponent gameFlow = AROR_GameFlowComponent.GetInstance();
		if (gameFlow)
			gameFlow.OnSetupResultReceived(success, message);
	}

	//------------------------------------------------------------------------------------------------
	// Notifications: server -> owning client (see AROR_NotificationManagerComponent)
	//------------------------------------------------------------------------------------------------
	void AROR_ShowNotification_S(int type, string text, string icon, int packedColor, string kicker, string sound)
	{
		if (AROR_IsLocal())
			AROR_RpcDo_Notification(type, text, icon, packedColor, kicker, sound);
		else
			Rpc(AROR_RpcDo_Notification, type, text, icon, packedColor, kicker, sound);
	}

	//------------------------------------------------------------------------------------------------
	void AROR_ShowHint_S(string text, bool hasColor, int packedColor, string sound)
	{
		if (AROR_IsLocal())
			AROR_RpcDo_Hint(text, hasColor, packedColor, sound);
		else
			Rpc(AROR_RpcDo_Hint, text, hasColor, packedColor, sound);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_Notification(int type, string text, string icon, int packedColor, string kicker, string sound)
	{
		AROR_NotificationManagerComponent notifications = AROR_NotificationManagerComponent.GetInstance();
		if (notifications)
			notifications.ReceiveNotification(type, text, icon, packedColor, kicker, sound);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_Hint(string text, bool hasColor, int packedColor, string sound)
	{
		Color color;
		if (hasColor)
			color = Color.FromInt(packedColor);

		AROR_NotificationManagerComponent.ShowHint(text, color, sound);
	}

	//------------------------------------------------------------------------------------------------
	// Economy: server -> owning client (see AROR_EconomyComponent)
	//------------------------------------------------------------------------------------------------
	void AROR_SendPlayerMoney_S(int money)
	{
		if (AROR_IsLocal())
			AROR_RpcDo_PlayerMoney(money);
		else
			Rpc(AROR_RpcDo_PlayerMoney, money);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_PlayerMoney(int money)
	{
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (economy)
			economy.OnLocalPlayerMoneyReceived(money);
	}

	//------------------------------------------------------------------------------------------------
	//! True if this controller belongs to the local player (e.g. the host or Workbench player)
	protected bool AROR_IsLocal()
	{
		return GetGame().GetPlayerController() == this;
	}
}
