//! Compiles before AROR_RadioVehicleAction, which calls it
modded class SCR_PlayerController
{
	protected int m_iARORRadioNextRequest;

	//------------------------------------------------------------------------------------------------
	//! Client: set the radio of the vehicle this player sits in (see AROR_RadioManagerComponent.SetRadio_S)
	void AROR_RequestRadio(RplId vehicleId, int station, int volume)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_Radio(vehicleId, station, volume);
		else
			Rpc(AROR_RpcAsk_Radio, vehicleId, station, volume);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_Radio(RplId vehicleId, int station, int volume)
	{
		int now = GetGame().GetWorld().GetWorldTime();
		if (now < m_iARORRadioNextRequest)
			return;

		m_iARORRadioNextRequest = now + 250;
		AROR_RadioManagerComponent manager = AROR_RadioManagerComponent.GetInstance();
		if (manager)
			manager.SetRadio_S(GetPlayerId(), vehicleId, station, volume);
	}
}
