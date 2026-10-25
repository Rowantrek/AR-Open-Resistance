#ifdef WORKBENCH
//! Deterministic planner regressions: native terrain/trace acceptance is replaced,
//! while the production cursor, tick budget, caching and candidate geometry run unchanged.
class AROR_ReaperLandingSearchSmokeTests
{
	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		bool passed = true;
		AROR_ReaperLandingSearchProbe exhausted = new AROR_ReaperLandingSearchProbe();
		exhausted.Initialize("1000 0 1000", 220, 18);
		exhausted.Advance(0);
		passed = Check(exhausted.m_iCalls == 2, "Two probes in first tick") && passed;
		exhausted.Advance(0);
		passed = Check(exhausted.m_iCalls == 2, "Repeated caller in same tick does no work") && passed;
		for (int tick = 1; tick <= 36; tick++)
		{
			int before = exhausted.m_iCalls;
			exhausted.Advance(tick);
			passed = Check(exhausted.m_iCalls - before <= 2, "Per-tick probe bound") && passed;
		}
		passed = Check(exhausted.IsComplete() && !exhausted.HasResult(), "Exhaustion ends search") && passed;
		passed = Check(exhausted.m_iCalls == 73 && !exhausted.m_bDuplicate, "Exactly 73 unique candidates") && passed;
		exhausted.Advance(100);
		passed = Check(exhausted.m_iCalls == 73, "Exhausted planner does not rescan") && passed;

		AROR_ReaperLandingSearchProbe found = new AROR_ReaperLandingSearchProbe();
		found.m_iSucceedOn = 3;
		found.Initialize("1000 0 1000", 220, 18);
		found.Advance(0);
		passed = Check(!found.IsComplete(), "Pending state retained between ticks") && passed;
		found.Advance(1);
		passed = Check(found.HasResult() && found.m_iCalls == 3, "Stop at first passing candidate") && passed;
		vector result = found.GetResult();
		found.Advance(2);
		passed = Check(found.m_iCalls == 3 && found.GetResult() == result, "Successful result cached") && passed;
		found.Initialize("2000 0 2000", 220, 18);
		found.m_iCalls = 0;
		found.m_aObserved.Clear();
		found.Advance(0);
		passed = Check(found.m_iCalls == 2 && !found.IsComplete(), "Fresh initialization resets tick/cache state") && passed;

		AROR_ReaperLandingSearchProbe fleet = new AROR_ReaperLandingSearchProbe();
		fleet.m_bAcceptAll = true;
		fleet.Initialize("1000 0 1000", 400, 18, 5, 90);
		for (int fleetTick = 0; fleetTick < 37; fleetTick++)
		{
			int checkedBefore = fleet.GetCheckedCount();
			fleet.Advance(fleetTick);
			passed = Check(fleet.GetCheckedCount() - checkedBefore <= 2, "Fleet search retains two-candidate budget") && passed;
		}
		passed = Check(fleet.IsComplete() && fleet.GetResultCount() == 5, "Five separate pads collected") && passed;
		for (int first = 0; first < fleet.GetResultCount(); first++)
		{
			for (int second = first + 1; second < fleet.GetResultCount(); second++)
				passed = Check(vector.DistanceXZ(fleet.GetResult(first), fleet.GetResult(second)) >= 90, "Fleet pad separation") && passed;
		}
		AROR_ReaperLandingSearchProbe partial = new AROR_ReaperLandingSearchProbe();
		partial.m_iSucceedOn = 1;
		partial.Initialize("1000 0 1000", 400, 18, 5, 90);
		for (int partialTick = 0; partialTick < 37; partialTick++)
			partial.Advance(partialTick);
		passed = Check(partial.IsComplete() && partial.HasResult() && partial.GetResultCount() == 1, "Partial result retained for local queue fallback") && passed;
		AROR_ReaperLandingSearchProbe alternate = new AROR_ReaperLandingSearchProbe();
		alternate.m_bAcceptAll = true;
		alternate.Initialize("1000 0 1000", 400, 18, 1, 90);
		array<vector> reserved = {"1000 0 1000"};
		alternate.SetReservedPads(reserved);
		for (int alternateTick = 0; alternateTick < 37; alternateTick++)
			alternate.Advance(alternateTick);
		passed = Check(alternate.HasResult(), "Alternate found outside reserved pad") && passed;
		if (alternate.HasResult())
			passed = Check(vector.DistanceXZ(alternate.GetResult(), reserved[0]) >= 90, "Alternate respects existing reservations") && passed;
		if (passed)
			Print("[AROR TEST] Landing planner regressions passed", LogLevel.NORMAL);
		return passed;
	}

	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print("[AROR TEST] Landing planner FAILED: " + label, LogLevel.ERROR);
		return condition;
	}
}

class AROR_ReaperLandingSearchProbe : AROR_ReaperLandingSearch
{
	int m_iCalls;
	int m_iSucceedOn;
	bool m_bAcceptAll;
	bool m_bDuplicate;
	ref array<vector> m_aObserved = {};

	override protected bool CandidateClear(vector candidate)
	{
		foreach (vector previous : m_aObserved)
		{
			if (vector.DistanceXZ(previous, candidate) < 0.01)
				m_bDuplicate = true;
		}
		m_aObserved.Insert(candidate);
		m_iCalls++;
		if (m_bAcceptAll)
			return true;
		return m_iSucceedOn > 0 && m_iCalls == m_iSucceedOn;
	}

	override protected float CandidateHeight(vector candidate)
	{
		return 0;
	}
}
#endif
