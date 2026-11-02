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
	bool m_bUnsecured;
	ref array<ref AROR_QRFAssetAccount> m_aAssets = {};
	protected ref array<int> m_aReleasedPackages = {};

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
		asset.m_iPackageIndex = -1;
		asset.m_iValue = value;
		asset.m_bVehicle = vehicle;
		asset.m_sFaction = faction;
		asset.m_sNativeFaction = nativeFaction;
		m_aAssets.Insert(asset);
	}
	//! Preserve the existing registration seam while attributing assets to their owning package.
	void RegisterPackageAsset_S(IEntity entity, int value, bool vehicle, string faction, string nativeFaction, int packageIndex)
	{
		if (!Replication.IsServer() || !entity || packageIndex < 0)
			return;
		Register_S(entity, value, vehicle, faction, nativeFaction);
		foreach (AROR_QRFAssetAccount asset : m_aAssets)
		{
			if (asset.m_Entity == entity && asset.m_iPackageIndex < 0)
			{
				asset.m_iPackageIndex = packageIndex;
				return;
			}
		}
	}

	//! Call only after this package's spawn queues are cancelled or its unbuilt reservation is cancelled.
	//! Keep historical registered value: losses/recoveries are already deducted once by the shared account.
	void ReleaseUnusedPackageQuote_S(int packageIndex, int packageQuote)
	{
		if (!Replication.IsServer() || packageIndex < 0 || packageQuote < 0 || m_aReleasedPackages.Contains(packageIndex))
			return;
		int registered;
		foreach (AROR_QRFAssetAccount asset : m_aAssets)
		{
			if (asset.m_iPackageIndex == packageIndex)
			{
				registered += asset.m_iValue;
				// Owned cleanup observes losses before deletion. External deletion/virtualization can erase
				// that evidence: a null reference proves neither survival nor death. Release its collateral
				// as an unobserved removal, not a recovery or a fabricated combat bill.
				if (!asset.m_bSettled && !asset.m_Entity)
				{
					asset.m_bSettled = true;
					m_iQuote = Math.Max(0, m_iQuote - asset.m_iValue);
					Print(string.Format("[AROR FINANCE] %1 package=%2 unobserved asset removal value=%3; collateral released, loss unknown", m_sOperationId, packageIndex, asset.m_iValue), LogLevel.WARNING);
				}
			}
		}
		m_aReleasedPackages.Insert(packageIndex);
		m_iQuote = Math.Max(0, m_iQuote - Math.Max(0, packageQuote - registered));
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
		// Emergency baseline responses create real liabilities, not fictitious collateral.
		if (m_bUnsecured)
			return 0;
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
	bool HasUnsettledAssets()
	{
		foreach (AROR_QRFAssetAccount asset : m_aAssets)
		{
			if (!asset.m_bSettled && asset.m_Entity)
				return true;
		}
		return false;
	}
}
