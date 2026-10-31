//! Tactical data for one role-owned QRF. Positions are report snapshots, not enemy references.
//! Native group AI retains movement, perception, weapon use and individual combat.
class AROR_QRFTacticalCommander
{
	int m_iPackages;
	int m_iFirstWaveCount;
	int m_iFirstDispatch = -1;
	int m_iWaveDelay;
	int m_iReserveDeadline;
	int m_iContactSequence;
	int m_iContactVersion;
	int m_iNextContactCheck;
	int m_iNextRetask;
	int m_iMissionSeconds;
	int m_iTownHoldSeconds;
	int m_iTownHoldStarted = -1;
	int m_iRetaskSeconds;
	bool m_bHasContact;
	bool m_bWithdraw;
	bool m_bSafeWithdrawalOrigin = true;
	bool m_bRosterDirty;
	bool m_bKnowsTargetControl;
	AROR_EFactionRole m_eTargetControl;
	AROR_EFactionRole m_eRole;	//!< The army this response belongs to
	bool m_bAssault;	//!< An attack: stage at rally points, then assault on two axes
	bool m_bJointResponse;	//!< One authored response sharing ground and airborne packages
	bool m_bAssaultReleased;
	int m_iStagingDeadline = -1;	//!< Executor clock when the assault goes even if not every package has staged
	vector m_vContact;
	vector m_vLastTaskContact;
	vector m_vTown;
	vector m_vOrigin;
	string m_sOriginLocationId;
	float m_fTownRadius;
	float m_fSecurityRadius;
	float m_fMargin;
	float m_fThreatStandoff;
	float m_fWithdrawFraction;
	int m_iLosses;
	ref AROR_QRFSearchPlan m_SearchPlan;
	protected ref array<int> m_aInitial = {};
	protected ref array<int> m_aLiving = {};
	protected ref array<bool> m_aLost = {};
	protected ref array<int> m_aWave = {};

	void Initialize(int tier, int packages, bool waves, int splitTier, int delay, int reserveWait, int missionSeconds, int retaskSeconds, float withdrawFraction)
	{
		m_aInitial.Clear();
		m_aLiving.Clear();
		m_aLost.Clear();
		m_aWave.Clear();
		m_iFirstDispatch = -1;
		m_iTownHoldStarted = -1;
		m_iLosses = 0;
		m_bWithdraw = false;
		m_bHasContact = false;
		m_bSafeWithdrawalOrigin = true;
		m_bRosterDirty = false;
		m_bKnowsTargetControl = false;
		m_bAssault = false;
		m_bAssaultReleased = false;
		m_bJointResponse = false;
		m_iStagingDeadline = -1;
		m_iContactSequence = 0;
		m_iContactVersion = 0;
		m_iNextContactCheck = 0;
		m_iNextRetask = 0;
		m_SearchPlan = null;
		m_iPackages = packages;
		m_iFirstWaveCount = packages;
		if (waves && tier >= splitTier && packages >= 4)
			m_iFirstWaveCount = (packages + 1) / 2;
		m_iWaveDelay = delay;
		m_iReserveDeadline = reserveWait;
		m_iMissionSeconds = missionSeconds;
		m_iRetaskSeconds = retaskSeconds;
		m_fWithdrawFraction = withdrawFraction;
		for (int index = 0; index < packages; index++)
		{
			m_aInitial.Insert(0);
			m_aLiving.Insert(0);
			m_aLost.Insert(false);
			if (index < m_iFirstWaveCount)
				m_aWave.Insert(0);
			else
				m_aWave.Insert(1);
		}
	}

	//! Scripted responses that must arrive in two waves regardless of tier and scenario wave settings.
	void ForceTwoWaves()
	{
		if (m_iPackages < 2 || m_iFirstWaveCount < m_iPackages)
			return;
		m_iFirstWaveCount = (m_iPackages + 1) / 2;
		for (int index = 0; index < m_iPackages; index++)
		{
			if (index < m_iFirstWaveCount)
				m_aWave[index] = 0;
			else
				m_aWave[index] = 1;
		}
	}

	bool IsReserve(int index)
	{
		if (index < 0 || index >= m_aWave.Count())
			return false;
		return m_aWave[index] == 1;
	}
	//! Include mobility, protected support and armour in the lead wave where available.
	void BalanceGroundWaves(array<AROR_ForceProfile> elements)
	{
		if (m_iFirstWaveCount == m_iPackages || elements.Count() != m_iPackages)
			return;
		for (int index = 0; index < m_iPackages; index++)
			m_aWave[index] = 1;
		int selected = 0;
		array<AROR_EForceType> priorities = {AROR_EForceType.MOTORIZED, AROR_EForceType.MECHANIZED, AROR_EForceType.ARMOUR, AROR_EForceType.INFANTRY};
		foreach (AROR_EForceType type : priorities)
		{
			if (selected >= m_iFirstWaveCount)
				break;
			for (int candidate = 0; candidate < m_iPackages; candidate++)
			{
				if (elements[candidate].m_eType == type && m_aWave[candidate] == 1)
				{
					m_aWave[candidate] = 0;
					selected++;
					break;
				}
			}
		}
		for (int fill = 0; fill < m_iPackages && selected < m_iFirstWaveCount; fill++)
		{
			if (m_aWave[fill] == 1)
			{
				m_aWave[fill] = 0;
				selected++;
			}
		}
	}
	void MarkDispatched(int now)
	{
		if (m_iFirstDispatch < 0)
			m_iFirstDispatch = now;
	}
	bool ReserveExpired(int now)
	{
		return m_bWithdraw || (m_iFirstDispatch >= 0 && now - m_iFirstDispatch >= m_iReserveDeadline);
	}
	bool CanConstruct(int index, int now, int liveCost, int limit, int cost)
	{
		if (index < 0 || index >= m_iPackages || now < 0)
			return false;
		if (liveCost < 0 || limit < 1 || cost < 1)
			return false;
		if (m_bWithdraw || liveCost + cost > limit)
			return false;
		if (!IsReserve(index))
			return m_iFirstDispatch < 0;
		if (m_iFirstDispatch < 0 || ReserveExpired(now))
			return false;
		int delay = m_iWaveDelay;
		if (m_iLosses > 0)
			delay = Math.Min(delay, 15);
		return now - m_iFirstDispatch >= delay;
	}
	void RecordStrength(int index, int living)
	{
		if (index < 0 || index >= m_iPackages || m_aLost[index])
			return;
		if (m_aInitial[index] == 0 && living > 0)
		{
			m_aInitial[index] = living;
			m_bRosterDirty = true;
		}
		m_aLiving[index] = living;
	}
	void MarkLost(int index)
	{
		if (index < 0 || index >= m_iPackages || m_aLost[index])
			return;
		m_aLost[index] = true;
		m_aLiving[index] = 0;
		m_iLosses++;
		m_bRosterDirty = true;
	}
	//! A returned element leaves the tactical roster without fabricating a combat casualty.
	void MarkRecovered(int index)
	{
		if (index < 0 || index >= m_iPackages || m_aLost[index])
			return;
		m_aLost[index] = true;
		m_aInitial[index] = 0;
		m_aLiving[index] = 0;
		m_bRosterDirty = true;
	}
	//! A captured town stays the objective; territory/support rules still own actual control.
	bool RecordTargetControl(AROR_EFactionRole role)
	{
		bool changed = m_bKnowsTargetControl && m_eTargetControl != role;
		m_bKnowsTargetControl = true;
		m_eTargetControl = role;
		if (changed)
			m_bRosterDirty = true;
		return changed;
	}
	void RefreshRoster(int now)
	{
		if (!m_bRosterDirty || now < m_iNextRetask)
			return;
		m_bRosterDirty = false;
		m_iContactVersion++;
		m_iNextRetask = now + m_iRetaskSeconds;
	}
	int ActiveSectorCount()
	{
		int count = 0;
		for (int index = 0; index < m_iPackages; index++)
		{
			if (m_aInitial[index] > 0 && !m_aLost[index])
				count++;
		}
		return Math.Max(1, count);
	}
	int ActiveSectorIndex(int packageIndex)
	{
		int rank = 0;
		for (int index = 0; index < packageIndex && index < m_iPackages; index++)
		{
			if (m_aInitial[index] > 0 && !m_aLost[index])
				rank++;
		}
		return rank;
	}
	bool BeginTownHold(int now)
	{
		if (m_iTownHoldSeconds <= 0 || m_iTownHoldStarted >= 0)
			return false;
		m_iTownHoldStarted = now;
		return true;
	}

	bool ShouldWithdraw(int now, bool reserveAvailable)
	{
		if (m_bWithdraw)
			return true;
		if (m_iFirstDispatch < 0)
			return false;
		// Temporary fixed security task: travel does not consume town occupation time, but heavy losses still end it
		if (m_iTownHoldSeconds > 0)
		{
			if (m_iTownHoldStarted >= 0 && now - m_iTownHoldStarted >= m_iTownHoldSeconds)
				m_bWithdraw = true;
			if (IsBroken(reserveAvailable))
				m_bWithdraw = true;
			return m_bWithdraw;
		}
		if (now - m_iFirstDispatch >= m_iMissionSeconds)
			m_bWithdraw = true;
		if (IsBroken(reserveAvailable))
			m_bWithdraw = true;
		return m_bWithdraw;
	}

	//! Too few survivors to fight on, with no reserve left to come
	protected bool IsBroken(bool reserveAvailable)
	{
		int initial = 0;
		int living = 0;
		for (int index = 0; index < m_iPackages; index++)
		{
			initial += m_aInitial[index];
			living += m_aLiving[index];
		}
		return !reserveAvailable && initial > 0 && living <= initial * m_fWithdrawFraction;
	}

	//! The objective is held by someone else: this response must retake it, not just secure it
	bool IsTargetLost()
	{
		return m_bKnowsTargetControl && m_eTargetControl != m_eRole;
	}

	//! Only fresh evidence inside the defended vicinity can change ground orders.
	void UpdateContact_S(AROR_CombatContact report, int now)
	{
		if (!Replication.IsServer() || now < m_iNextRetask)
			return;
		bool changed = false;
		if (!report)
		{
			changed = m_bHasContact;
			m_bHasContact = false;
			m_vContact = m_vTown;
		}
		else
		{
			changed = !m_bHasContact;
			if (vector.DistanceXZ(m_vLastTaskContact, report.m_vPosition) > 75)
				changed = true;
			m_bHasContact = true;
			m_vContact = report.m_vPosition;
			m_iContactSequence = report.m_iSequence;
		}
		if (!changed && m_SearchPlan)
			return;
		m_iNextRetask = now + m_iRetaskSeconds;
		m_iContactVersion++;
		m_bRosterDirty = false;
		m_vLastTaskContact = m_vContact;
		m_SearchPlan = new AROR_QRFSearchPlan();
		if (m_bHasContact)
			m_SearchPlan.ReportContact_S(m_vContact, now);
		m_SearchPlan.BeginSearch_S(now, 1);
	}

	//! Separate approach wedges on the origin side, avoiding identical ingress coordinates.
	vector ApproachPoint(int index, vector origin, float distance)
	{
		vector heading = origin - m_vTown;
		heading[1] = 0;
		if (heading.Length() < 1)
			heading = "0 0 1";
		else
			heading = heading.Normalized();
		float angle = Math.Atan2(heading[0], heading[2]);
		int side = index - (index / 2) * 2;
		float spread = 0.35 + (index / 2) * 0.18;
		// Attacks come in on two axes, well apart, instead of one lane from the origin
		if (m_bAssault)
			spread = 0.9 + (index / 2) * 0.2;
		if (side == 0)
			spread = -spread;
		angle += spread;
		return m_vTown + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * distance;
	}
	//! Fixed-size alternatives: preferred wedge, adjacent wedges and the direct origin-side lane.
	vector ApproachCandidate(int index, vector origin, float distance, int attempt)
	{
		vector preferred = ApproachPoint(index, origin, distance);
		if (attempt == 0)
			return preferred;
		// After the preferred ingress wedges, search the whole town road ring.
		// Narrow lanes from a new origin must not restrict every package to one hillside.
		if (attempt >= 4)
		{
			float fullAngle = (attempt - 4) * 0.78539816 + index * 0.2;
			float fullDistance = Math.Max(30, distance);
			if (attempt >= 8)
				fullDistance += 20;
			return m_vTown + Vector(Math.Sin(fullAngle), 0, Math.Cos(fullAngle)) * fullDistance;
		}
		vector heading = preferred - m_vTown;
		int ring = attempt / 4;
		int slot = attempt - ring * 4;
		if (slot == 3)
			heading = origin - m_vTown;
		float angle = Math.Atan2(heading[0], heading[2]);
		if (slot == 1)
			angle += 0.45;
		if (slot == 2)
			angle -= 0.45;
		if (ring == 1)
			distance += 50;
		if (ring == 2)
			distance -= 50;
		if (ring >= 3)
			distance += 100;
		distance = Math.Max(60, Math.Min(distance, m_fTownRadius + m_fMargin - 75));
		return m_vTown + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * distance;
	}
	vector InfantryPoint(int index, int step)
	{
		// Retaking: every fireteam converges on the objective (the flag area), spread a little per package
		if (IsTargetLost())
		{
			float angle = index * 1.2566;
			return m_vTown + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * 12;
		}
		vector center = m_vTown;
		if (step == 0 && m_bHasContact)
			center = m_vContact;
		if (!m_SearchPlan)
			return center;
		float radius = m_fTownRadius * 0.65;
		if (m_fSecurityRadius > 0)
			radius = Math.Min(radius, m_fSecurityRadius * 0.75);
		if (step == 0)
			radius = 25;
		vector point = m_SearchPlan.GetSectorPoint(center, radius, ActiveSectorIndex(index), ActiveSectorCount(), step);
		float distance = vector.DistanceXZ(point, m_vTown);
		float limit = m_fTownRadius + m_fMargin;
		if (m_fSecurityRadius > 0)
			limit = Math.Min(limit, m_fSecurityRadius);
		if (distance > limit && distance > 0)
			point = m_vTown + (point - m_vTown) * (limit / distance);
		return point;
	}
	vector SupportPoint(int index)
	{
		vector center = m_vTown;
		if (m_bHasContact)
			center = m_vContact;
		vector heading = m_vOrigin - center;
		heading[1] = 0;
		if (heading.Length() < 1)
			heading = "0 0 1";
		else
			heading = heading.Normalized();
		vector right = Vector(heading[2], 0, -heading[0]);
		vector point = center + heading * Math.Min(60, Math.Max(30, m_fThreatStandoff * 0.6));
		point += right * ((ActiveSectorIndex(index) - (ActiveSectorCount() - 1) * 0.5) * 30);
		float distance = vector.DistanceXZ(point, m_vTown);
		float limit = m_fTownRadius + m_fMargin;
		if (m_fSecurityRadius > 0)
			limit = Math.Min(limit, m_fSecurityRadius);
		if (distance > limit && distance > 0)
			point = m_vTown + (point - m_vTown) * (limit / distance);
		return point;
	}
}
