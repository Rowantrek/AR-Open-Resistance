#ifdef WORKBENCH
class AROR_QRFSearchPlanSmokeTests
{
	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		bool passed = true;
		vector contact;
		AROR_QRFSearchPlan plan = new AROR_QRFSearchPlan();
		passed = Check(!plan.GetContactCenter("0 0 0", 250, 150, contact), "No invented contact") && passed;
		plan.ReportContact_S("1000 0 0", 10);
		plan.ReportContact_S("0 0 1000", 20);
		plan.BeginSearch_S(30, 120);
		passed = Check(plan.GetContactCenter("0 0 0", 250, 150, contact) && vector.DistanceXZ(contact, "0 0 400") < 0.1, "Latest pre-search report clamped to vicinity") && passed;
		passed = Check(!plan.ReportContact_S("100 0 100", 31), "Searching cannot track a moving attacker") && passed;
		vector first = plan.GetSectorPoint("0 0 0", 100, 0, 3, 2);
		vector second = plan.GetSectorPoint("0 0 0", 100, 1, 3, 2);
		vector third = plan.GetSectorPoint("0 0 0", 100, 2, 3, 2);
		passed = Check(vector.DistanceXZ(first, second) > 170 && vector.DistanceXZ(second, third) > 170 && vector.DistanceXZ(first, third) > 170, "Three packages get different sectors") && passed;
		passed = Check(plan.ClaimStop_S("0 0 0", 50) && !plan.ClaimStop_S("49 0 0", 50) && plan.ClaimStop_S("50 0 0", 50), "Shared stop spacing") && passed;
		AROR_QRFSearchPlan stale = new AROR_QRFSearchPlan();
		stale.ReportContact_S("0 0 0", 0);
		stale.BeginSearch_S(121, 120);
		passed = Check(!stale.GetContactCenter("0 0 0", 250, 150, contact), "Stale contact falls back to town patrol") && passed;
		AROR_QRFSearchPlan isolated = new AROR_QRFSearchPlan();
		passed = Check(isolated.CanClaimStop("0 0 0", 50), "Another operation has independent assignments") && passed;
		for (int stop = 0; stop < 64; stop++)
		{
			vector boundedPoint = "0 0 0";
			boundedPoint[0] = stop * 100;
			passed = Check(isolated.ClaimStop_S(boundedPoint, 50), "Bounded history accepts distinct stop") && passed;
		}
		passed = Check(!isolated.ClaimStop_S("10000 0 0", 50), "Stop history cannot exceed its cap") && passed;
		Print(string.Format("[AROR TEST] QRF search-plan smoke tests passed=%1", passed), LogLevel.NORMAL);
		return passed;
	}

	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print(string.Format("[AROR TEST] FAILED QRF search plan: %1", label), LogLevel.ERROR);
		return condition;
	}
}
#endif
