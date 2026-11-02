//! One cached outward search. No complete rescans in the executor's retry loop.
class AROR_ReaperLandingSearch
{
	protected vector m_vCenter;
	protected float m_fRadius;
	protected float m_fClearance;
	protected float m_fFallbackRadius;
	protected int m_iCandidateLimit = 73;
	protected int m_iRejectedSpacing;
	protected int m_iRejectedWater;
	protected int m_iRejectedSlope;
	protected int m_iRejectedObstacle;
	protected int m_iLastRejection;
	protected int m_iAdjustedPads;
	protected int m_iAdjustmentAttempts;
	protected float m_fAdjustmentRadius;
	protected int m_iCursor;
	protected int m_iLastTick = -1;
	protected bool m_bComplete;
	protected bool m_bFound;
	protected int m_iDesired = 1;
	protected float m_fSpacing;
	protected ref array<vector> m_aResults = {};
	protected ref array<vector> m_aReserved = {};

	void Initialize(vector center, float radius, float clearance, int desired = 1, float spacing = 0, float fallbackRadius = 0, float adjustmentRadius = 60)
	{
		m_vCenter = center;
		m_fRadius = radius;
		m_fClearance = clearance;
		m_fFallbackRadius = Math.Max(radius, fallbackRadius);
		m_iCandidateLimit = 73;
		if (m_fFallbackRadius > radius)
			m_iCandidateLimit = 121;
		m_iRejectedSpacing = 0;
		m_iRejectedWater = 0;
		m_iRejectedSlope = 0;
		m_iRejectedObstacle = 0;
		m_iLastRejection = 0;
		m_iAdjustedPads = 0;
		m_iAdjustmentAttempts = 0;
		m_fAdjustmentRadius = Math.Clamp(adjustmentRadius, 0, 150);
		m_iCursor = 0;
		m_iLastTick = -1;
		m_bComplete = false;
		m_bFound = false;
		m_iDesired = desired;
		m_fSpacing = spacing;
		m_aResults.Clear();
		m_aReserved.Clear();
	}

	void SetReservedPads(array<vector> pads)
	{
		m_aReserved.Clear();
		foreach (vector pad : pads)
			m_aReserved.Insert(pad);
	}

	//! At most two radial seeds per tick. Only flat/dry obstructed seeds get a
	//! native local search, bounded to three attempts and the configured radius.
	void Advance(int tick)
	{
		if (m_bComplete || tick == m_iLastTick)
			return;
		m_iLastTick = tick;
		for (int checks = 0; checks < 2; checks++)
		{
			if (m_iCursor >= m_iCandidateLimit)
				break;
			vector candidate = AROR_ReaperFlightGeometry.LandingCandidate(m_vCenter, m_fRadius, m_iCursor, m_fFallbackRadius);
			m_iCursor++;
			if (!AROR_ReaperFlightGeometry.HasPadSpacing(candidate, m_aResults, m_fSpacing))
			{
				m_iRejectedSpacing++;
				continue;
			}
			if (!AROR_ReaperFlightGeometry.HasPadSpacing(candidate, m_aReserved, m_fSpacing))
			{
				m_iRejectedSpacing++;
				continue;
			}
			bool clear = CandidateClear(candidate);
			if (!clear && m_iLastRejection == AROR_ReaperFlightGeometry.LANDING_OBSTACLE)
			{
				vector adjusted;
				if (AdjustCandidate(candidate, adjusted))
				{
					candidate = adjusted;
					clear = true;
				}
			}
			if (clear)
			{
				candidate[1] = CandidateHeight(candidate);
				m_bFound = true;
				m_aResults.Insert(candidate);
				if (m_aResults.Count() >= m_iDesired)
				{
					m_bComplete = true;
					return;
				}
			}
		}
		if (m_iCursor >= m_iCandidateLimit)
			m_bComplete = true;
	}

	protected bool CandidateClear(vector candidate)
	{
		int reason = AROR_ReaperFlightGeometry.LandingRejectionReason(candidate, m_fClearance);
		m_iLastRejection = reason;
		if (reason == AROR_ReaperFlightGeometry.LANDING_WATER)
			m_iRejectedWater++;
		else if (reason == AROR_ReaperFlightGeometry.LANDING_SLOPE)
			m_iRejectedSlope++;
		else if (reason == AROR_ReaperFlightGeometry.LANDING_OBSTACLE)
			m_iRejectedObstacle++;
		return reason == AROR_ReaperFlightGeometry.LANDING_CLEAR;
	}

	//! Reuse the native empty-terrain query already used for other spawn planners,
	//! then re-run the full helicopter-specific checks. Never accept the query alone.
	protected bool AdjustCandidate(vector seed, out vector adjusted)
	{
		if (m_fAdjustmentRadius <= 0)
			return false;
		m_iAdjustmentAttempts++;
		if (!SCR_WorldTools.FindEmptyTerrainPosition(adjusted, seed, m_fAdjustmentRadius, m_fClearance, 3))
			return false;
		if (vector.DistanceXZ(adjusted, seed) > m_fAdjustmentRadius || vector.DistanceXZ(adjusted, m_vCenter) > m_fFallbackRadius)
			return false;
		if (!AROR_ReaperFlightGeometry.HasPadSpacing(adjusted, m_aResults, m_fSpacing))
			return false;
		if (!AROR_ReaperFlightGeometry.HasPadSpacing(adjusted, m_aReserved, m_fSpacing))
			return false;
		if (!CandidateClear(adjusted))
			return false;
		m_iAdjustedPads++;
		return true;
	}

	protected float CandidateHeight(vector candidate)
	{
		return GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
	}

	bool IsComplete() { return m_bComplete; }
	bool HasResult() { return m_bFound; }
	vector GetResult(int index = 0) { return m_aResults[index]; }
	int GetResultCount() { return m_aResults.Count(); }
	int GetCheckedCount() { return m_iCursor; }
	int GetCandidateLimit() { return m_iCandidateLimit; }

	string GetDiagnostics()
	{
		string diagnostics = string.Format("checked=%1/%2 pads=%3 spacing=%4 water=%5 slope=%6 obstacles=%7 innerRadius=%8 outerRadius=%9", m_iCursor, m_iCandidateLimit, m_aResults.Count(), m_iRejectedSpacing, m_iRejectedWater, m_iRejectedSlope, m_iRejectedObstacle, m_fRadius, m_fFallbackRadius);
		return diagnostics + string.Format(" adjusted=%1/%2", m_iAdjustedPads, m_iAdjustmentAttempts);
	}
}
