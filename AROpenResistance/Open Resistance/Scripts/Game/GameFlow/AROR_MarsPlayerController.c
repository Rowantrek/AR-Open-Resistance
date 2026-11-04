//! Keep MARS RPCs separate from the shared phone/admin controller integration.
modded class SCR_PlayerController
{
	void AROR_RequestMars(string credential, AROR_EFactionRole role, string selection, int tier, int readiness = 0)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_Mars(credential, role, selection, tier, readiness);
		else
			Rpc(AROR_RpcAsk_Mars, credential, role, selection, tier, readiness);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_Mars(string credential, int role, string selection, int tier, int readiness)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
			operations.RequestMars_S(GetPlayerId(), credential, role, selection, tier, readiness);
	}

	//! Server answer to a MARS request, shown in the requester's MARS app (the hint alone is easy to miss)
	void AROR_SendMarsReply_S(string message)
	{
		if (!Replication.IsServer())
			return;
		// Inline local check: AROR_IsLocal is added by a modded file compiled after this one
		if (GetGame().GetPlayerController() == this)
			AROR_RpcDo_MarsReply(message);
		else
			Rpc(AROR_RpcDo_MarsReply, message);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_MarsReply(string message)
	{
		AROR_MarsApp.ReceiveServerReply(message);
	}
}
