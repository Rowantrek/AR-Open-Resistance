//! Owner-only snapshots keep private markers off other clients. IDs always come from this controller.
modded class SCR_PlayerController
{
	protected string m_sARORFieldLastMarkers;
	protected string m_sARORFieldLastRecruits;
	protected bool m_bARORFieldSent;
	protected int m_iARORFieldNextRequest;

	void AROR_RequestFieldMarker(int id, vector position, string caption, int scope, int kind, int lifetime)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_FieldMarker(id, position, caption, scope, kind, lifetime);
		else
			Rpc(AROR_RpcAsk_FieldMarker, id, position, caption, scope, kind, lifetime);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_FieldMarker(int id, vector position, string caption, int scope, int kind, int lifetime)
	{
		if (!AROR_FieldRequestAllowed())
			return;
		AROR_PlayerMarkersComponent markers = AROR_PlayerMarkersComponent.GetInstance();
		if (markers)
			markers.Request_S(GetPlayerId(), id, position, caption, scope, kind, lifetime);
	}

	void AROR_RequestRecruit(int action, int recruitId, int targetPlayer, RplId vehicleId)
	{
		if (Replication.IsServer())
			AROR_RpcAsk_Recruit(action, recruitId, targetPlayer, vehicleId);
		else
			Rpc(AROR_RpcAsk_Recruit, action, recruitId, targetPlayer, vehicleId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void AROR_RpcAsk_Recruit(int action, int recruitId, int targetPlayer, RplId vehicleId)
	{
		if (!AROR_FieldRequestAllowed())
			return;
		AROR_RecruitmentComponent recruitment = AROR_RecruitmentComponent.GetInstance();
		if (recruitment)
			recruitment.Request_S(GetPlayerId(), action, recruitId, targetPlayer, vehicleId);
	}

	protected bool AROR_FieldRequestAllowed()
	{
		if (!Replication.IsServer() || !AROR_FieldAccess.Player(GetPlayerId()))
			return false;
		int now = GetGame().GetWorld().GetWorldTime();
		if (now < m_iARORFieldNextRequest)
			return false;
		m_iARORFieldNextRequest = now + 250;
		return true;
	}

	void AROR_SendFieldState_S(string markers, string recruits)
	{
		if (!Replication.IsServer())
			return;
		if (m_bARORFieldSent && markers == m_sARORFieldLastMarkers && recruits == m_sARORFieldLastRecruits)
			return;
		m_bARORFieldSent = true;
		m_sARORFieldLastMarkers = markers;
		m_sARORFieldLastRecruits = recruits;
		if (GetGame().GetPlayerController() == this)	// AROR_IsLocal() is declared in GameFlow, which compiles after this file
			AROR_RpcDo_FieldState(markers, recruits);
		else
			Rpc(AROR_RpcDo_FieldState, markers, recruits);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void AROR_RpcDo_FieldState(string markers, string recruits)
	{
		AROR_FieldClientState.Receive(markers, recruits);
	}
}
