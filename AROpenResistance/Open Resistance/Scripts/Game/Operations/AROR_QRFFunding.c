//! Session allocation, not another bank. Only overhead and observed losses debit Economy.
class AROR_QRFFunding
{
	string m_sOperationId;
	AROR_EFactionRole m_eRole;
	int m_iQuote;
	int m_iOverhead;
	int m_iLosses;
	int m_iRecovered;
	bool m_bActivated;
	bool m_bClosed;
	ref array<ref AROR_QRFAssetAccount> m_aAssets = {};

	void Register_S(IEntity entity, int value, bool vehicle, string faction, string nativeFaction)
	{
		if (!Replication.IsServer() || !entity || value <= 0)
			return;
		foreach (AROR_QRFAssetAccount existing : m_aAssets)
		{
			if (existing.m_Entity == entity)
				return;
		}
		AROR_QRFAssetAccount asset = new AROR_QRFAssetAccount();
		asset.m_Entity = entity;
		asset.m_iValue = value;
		asset.m_bVehicle = vehicle;
		asset.m_sFaction = faction;
		asset.m_sNativeFaction = nativeFaction;
		m_aAssets.Insert(asset);
	}
	int ObserveLosses_S()
	{
		if (!Replication.IsServer())
			return 0;
		int losses;
		foreach (AROR_QRFAssetAccount asset : m_aAssets)
		{
			if (!asset.IsLost())
				continue;
			asset.m_bSettled = true;
			losses += asset.m_iValue;
			Print(string.Format("[AROR FINANCE] %1 confirmed asset loss value=%2 vehicle=%3", m_sOperationId, asset.m_iValue, asset.m_bVehicle), LogLevel.NORMAL);
		}
		m_iLosses += losses;
		return losses;
	}
	//! Call immediately before owned actors are removed normally, after ObserveLosses_S.
	void Retire_S(IEntity entity)
	{
		if (!Replication.IsServer() || !entity)
			return;
		foreach (AROR_QRFAssetAccount asset : m_aAssets)
		{
			if (asset.m_Entity != entity || asset.m_bSettled || asset.IsLost())
				continue;
			asset.m_bSettled = true;
			m_iRecovered += asset.m_iValue;
		}
	}
	int Reserved()
	{
		int physical;
		foreach (AROR_QRFAssetAccount asset : m_aAssets)
		{
			if (!asset.m_bSettled && asset.m_Entity)
				physical += asset.m_iValue;
		}
		int reserved = physical;
		if (!m_bClosed)
			reserved = Math.Max(physical, m_iQuote - m_iLosses - m_iRecovered);
		if (!m_bActivated && !m_bClosed)
			reserved += m_iOverhead;
		return Math.Max(0, reserved);
	}
}
