//! One cached outward search. No complete rescans in the executor's retry loop.
class AROR_ReaperLandingSearch
{
	protected vector m_vCenter;
	protected float m_fRadius;
	protected float m_fClearance;
	protected int m_iCursor;
	protected int m_iLastTick = -1;
	protected bool m_bComplete;
	protected bool m_bFound;
	protected int m_iDesired = 1;
	protected float m_fSpacing;
	protected ref array<vector> m_aResults = {};
	protected ref array<vector> m_aReserved = {};

	void Initialize(vector center, float radius, float clearance, int desired = 1, float spacing = 0)
	{
		m_vCenter = center;
		m_fRadius = radius;
		m_fClearance = clearance;
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

	//! At most two terrain/obstacle checks in one executor tick, including repeated callers.
	void Advance(int tick)
	{
		if (m_bComplete || tick == m_iLastTick)
			return;
		m_iLastTick = tick;
		for (int checks = 0; checks < 2; checks++)
		{
			if (m_iCursor >= 73)
				break;
			vector candidate = AROR_ReaperFlightGeometry.LandingCandidate(m_vCenter, m_fRadius, m_iCursor);
			m_iCursor++;
			if (!AROR_ReaperFlightGeometry.HasPadSpacing(candidate, m_aResults, m_fSpacing))
				continue;
			if (!AROR_ReaperFlightGeometry.HasPadSpacing(candidate, m_aReserved, m_fSpacing))
				continue;
			if (CandidateClear(candidate))
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
		if (m_iCursor >= 73)
			m_bComplete = true;
	}

	protected bool CandidateClear(vector candidate)
	{
		return AROR_ReaperFlightGeometry.IsLandingClear(candidate, m_fClearance);
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
}
