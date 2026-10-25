#ifdef WORKBENCH
class AROR_AirTrafficPolicySmokeTests
{
	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		bool passed = true;
		passed = Check(AROR_AirTrafficPolicy.PredictedSeparation("0 0 0", "20 0 0", "200 0 0", "-20 0 0", 5) < 1, "Head-on conflict") && passed;
		passed = Check(AROR_AirTrafficPolicy.PredictedSeparation("0 0 0", "-20 0 0", "200 0 0", "20 0 0", 5) >= 200, "Diverging traffic") && passed;
		passed = Check(AROR_AirTrafficPolicy.PredictedSeparation("0 0 0", "20 0 0", "200 150 0", "-20 0 0", 5) >= 150, "Vertical separation") && passed;
		passed = Check(AROR_AirTrafficPolicy.PredictedSeparation("0 0 0", "0 0 0", "50 0 0", "0 0 0", 5) == 50, "Stationary traffic") && passed;
		passed = Check(AROR_AirTrafficPolicy.PredictedSeparation("0 0 0", "20 0 0", "1000 0 0", "-20 0 0", 5) >= 800, "Bounded lookahead") && passed;
		if (passed)
			Print("[AROR TEST] Air traffic prediction regressions passed", LogLevel.NORMAL);
		return passed;
	}

	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print("[AROR TEST] Air traffic FAILED: " + label, LogLevel.ERROR);
		return condition;
	}
}
#endif
