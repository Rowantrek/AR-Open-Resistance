//! One complete profile package, including its infantry, crew, fuel and ammunition readiness.
class AROR_QRFReadyStock
{
	string m_sProfileId;
	int m_iReadyPackages;
}

//! Immutable-by-contract server planning input supplied by the future military-resource owner.
//! Counts are availability, not reservations. Never interpret missing data as unlimited stock.
class AROR_QRFReadinessSnapshot
{
	AROR_EFactionRole m_eRole;
	string m_sFactionKey;
	string m_sOriginLocationId;
	int m_iCapturedAt;
	int m_iValidForSeconds = 30;
	bool m_bGroundRouteReady;
	int m_iArrivalSeconds = -1;
	int m_iFreeGroups;
	int m_iFreePersonnel;
	int m_iFreeVehicles;
	ref array<ref AROR_QRFReadyStock> m_aStock = {};

	int GetReadyPackages(string profileId)
	{
		foreach (AROR_QRFReadyStock stock : m_aStock)
		{
			if (stock && stock.m_sProfileId == profileId)
				return stock.m_iReadyPackages;
		}
		return 0;
	}

	bool IsValidAt(int now)
	{
		if (m_eRole != AROR_EFactionRole.OCCUPIERS && m_eRole != AROR_EFactionRole.INVADERS)
			return false;
		if (m_sFactionKey.IsEmpty() || m_sOriginLocationId.IsEmpty())
			return false;
		if (m_iValidForSeconds < 1 || m_iValidForSeconds > 300)
			return false;
		if (now < 0 || m_iCapturedAt < 0)
			return false;
		if (m_iCapturedAt > now || now - m_iCapturedAt > m_iValidForSeconds)
			return false;
		if (m_iFreeGroups < 0 || m_iFreePersonnel < 0 || m_iFreeVehicles < 0)
			return false;
		if (!m_aStock || m_aStock.Count() > 64)
			return false;
		array<string> seen = {};
		foreach (AROR_QRFReadyStock stock : m_aStock)
		{
			if (!stock || stock.m_sProfileId.IsEmpty() || stock.m_iReadyPackages < 0)
				return false;
			if (seen.Contains(stock.m_sProfileId))
				return false;
			seen.Insert(stock.m_sProfileId);
		}
		return true;
	}

	AROR_QRFReadinessSnapshot WithRuntimeCapacity(int groups, int personnel, int vehicles)
	{
		AROR_QRFReadinessSnapshot copy = new AROR_QRFReadinessSnapshot();
		copy.m_eRole = m_eRole;
		copy.m_sFactionKey = m_sFactionKey;
		copy.m_sOriginLocationId = m_sOriginLocationId;
		copy.m_iCapturedAt = m_iCapturedAt;
		copy.m_iValidForSeconds = m_iValidForSeconds;
		copy.m_bGroundRouteReady = m_bGroundRouteReady;
		copy.m_iArrivalSeconds = m_iArrivalSeconds;
		copy.m_iFreeGroups = Math.Min(m_iFreeGroups, Math.Max(0, groups));
		copy.m_iFreePersonnel = Math.Min(m_iFreePersonnel, Math.Max(0, personnel));
		copy.m_iFreeVehicles = Math.Min(m_iFreeVehicles, Math.Max(0, vehicles));
		if (m_aStock)
		{
			// Enforce cannot Copy an owning ref array through a non-owning array signature.
			// Clone entries as well so the preview cannot mutate provider-owned stock.
			foreach (AROR_QRFReadyStock stock : m_aStock)
			{
				if (!stock)
				{
					copy.m_aStock.Insert(null);
					continue;
				}
				AROR_QRFReadyStock clonedStock = new AROR_QRFReadyStock();
				clonedStock.m_sProfileId = stock.m_sProfileId;
				clonedStock.m_iReadyPackages = stock.m_iReadyPackages;
				copy.m_aStock.Insert(clonedStock);
			}
		}
		return copy;
	}
}
