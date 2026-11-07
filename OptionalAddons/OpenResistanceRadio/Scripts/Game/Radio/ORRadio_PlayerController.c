//! Compiles before ORRadio_VehicleAction, which calls it
modded class SCR_PlayerController
{
	protected int m_iORRadioNextRequest;

	//------------------------------------------------------------------------------------------------
	//! Client: set the radio of the vehicle this player sits in (see ORRadio_ManagerComponent.SetRadio_S)
	void ORRadio_Request(RplId vehicleId, int station, int volume)
	{
		if (Replication.IsServer())
			ORRadio_RpcAsk(vehicleId, station, volume);
		else
			Rpc(ORRadio_RpcAsk, vehicleId, station, volume);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void ORRadio_RpcAsk(RplId vehicleId, int station, int volume)
	{
		int now = GetGame().GetWorld().GetWorldTime();
		if (now < m_iORRadioNextRequest)
			return;

		m_iORRadioNextRequest = now + 250;
		ORRadio_ManagerComponent manager = ORRadio_ManagerComponent.GetInstance();
		if (manager)
			manager.SetRadio_S(GetPlayerId(), vehicleId, station, volume);
	}
}
