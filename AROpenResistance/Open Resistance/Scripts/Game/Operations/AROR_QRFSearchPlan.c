//! Shared tactical plan for one QRF. Owns positions only, never an attacker entity reference.
class AROR_QRFSearchPlan
{
	protected bool m_bHasContact;
	protected bool m_bStarted;
	protected vector m_vContact;
	protected int m_iContactTime;
	protected float m_fBaseAngle;
	protected ref array<vector> m_aStops = {};

	//! Fresh validated reports may update the snapshot until the first package starts searching.
	bool ReportContact_S(vector position, int now)
	{
		if (!Replication.IsServer() || m_bStarted)
			return false;
		m_vContact = position;
		m_iContactTime = now;
		m_bHasContact = true;
		return true;
	}

	void BeginSearch_S(int now, int maxContactAge)
	{
		if (!Replication.IsServer() || m_bStarted)
			return;
		m_bStarted = true;
		if (now - m_iContactTime > maxContactAge)
			m_bHasContact = false;
		m_fBaseAngle = Math.RandomFloat(0, 6.2831853);
	}

	//! Clamp distant reports to the configured vicinity; never send vehicles chasing across the map.
	bool GetContactCenter(vector townCenter, float townRadius, float margin, out vector position)
	{
		position = townCenter;
		if (!m_bHasContact)
			return false;
		position = m_vContact;
		float distance = vector.DistanceXZ(townCenter, position);
		float limit = townRadius + margin;
		if (distance > limit && distance > 0)
		{
			position[0] = townCenter[0] + (position[0] - townCenter[0]) * limit / distance;
			position[2] = townCenter[2] + (position[2] - townCenter[2]) * limit / distance;
		}
		return true;
	}

	//! Stable package sectors with bounded variation inside each sector.
	vector GetSectorPoint(vector center, float radius, int packageIndex, int packages, int ordinal)
	{
		if (packages < 1)
			packages = 1;
		float width = 6.2831853 / packages;
		// Enforce does not support the % operator. Ordinals are nonnegative;
		// the integer quotient gives the same five-step cycle without another API dependency.
		int cycle = ordinal / 5;
		int step = ordinal - cycle * 5;
		float angle = m_fBaseAngle + packageIndex * width + (step - 2) * width * 0.16;
		vector point = center;
		point[0] = point[0] + Math.Cos(angle) * radius;
		point[2] = point[2] + Math.Sin(angle) * radius;
		return point;
	}

	bool CanClaimStop(vector position, float spacing)
	{
		if (m_aStops.Count() >= 64)
			return false;
		foreach (vector assigned : m_aStops)
		{
			if (vector.DistanceXZ(position, assigned) < spacing)
				return false;
		}
		return true;
	}

	bool ClaimStop_S(vector position, float spacing)
	{
		if (!Replication.IsServer() || !CanClaimStop(position, spacing))
			return false;
		m_aStops.Insert(position);
		return true;
	}
}
