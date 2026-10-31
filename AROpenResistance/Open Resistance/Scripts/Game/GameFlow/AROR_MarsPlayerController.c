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
}
