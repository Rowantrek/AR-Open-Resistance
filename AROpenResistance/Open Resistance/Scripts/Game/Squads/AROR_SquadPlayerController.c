//! Requests travel through the owning controller; caller identity is always derived on the authority.
modded class SCR_PlayerController
{
	void AROR_RequestSquadRename(int groupId, string name)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_SquadRename(groupId, name);
		else
			Rpc(AROR_RpcAsk_SquadRename, groupId, name);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_SquadRename(int groupId, string name)
	{
		AROR_SquadManagerComponent manager = AROR_SquadManagerComponent.GetInstance();
		if (manager)
			manager.Rename_S(GetPlayerId(), groupId, name);
	}

	void AROR_RequestSquadDismiss(int groupId, int recruitId)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_SquadDismiss(groupId, recruitId);
		else
			Rpc(AROR_RpcAsk_SquadDismiss, groupId, recruitId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_SquadDismiss(int groupId, int recruitId)
	{
		AROR_SquadManagerComponent manager = AROR_SquadManagerComponent.GetInstance();
		if (manager)
			manager.Dismiss_S(GetPlayerId(), groupId, recruitId);
	}
}
