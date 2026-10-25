#ifdef WORKBENCH
//! Pure policy/account checks. Never spends campaign money, spawns actors or enables Invaders.
class AROR_QRFStrategySmokeTests
{
	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		bool passed = true;
		for (int level = 1; level <= 5; level++)
		{
			int pressure = AROR_QRFStrategicPolicy.PressureForLevel(level);
			passed = Check(AROR_QRFStrategicPolicy.WarLevel(pressure) == level, "War threshold") && passed;
			passed = Check(AROR_QRFStrategicPolicy.DesiredTier(level, 0, false, 50) == level, "Normal response ceiling") && passed;
			passed = Check(AROR_QRFStrategicPolicy.DesiredTier(level, 0, false, 0) == Math.Max(1, level - 1), "Lighter response variation") && passed;
		}
		passed = Check(AROR_QRFStrategicPolicy.DesiredTier(2, 3, false, 50) == 3, "Sustained threat escalation") && passed;
		passed = Check(AROR_QRFStrategicPolicy.Priority(100, 1000, false, 10000) == 250, "Queue evidence and aging bounded") && passed;
		AROR_QRFFunding account = new AROR_QRFFunding();
		account.m_iQuote = 1450;
		account.m_iOverhead = 60;
		passed = Check(account.Reserved() == 1510, "Entire quote and fee held before dispatch") && passed;
		account.m_bActivated = true;
		passed = Check(account.Reserved() == 1450, "Fee removed from collateral once charged") && passed;
		account.m_bClosed = true;
		passed = Check(account.Reserved() == 0, "Unspawned allocation releases on close") && passed;
		AROR_ForceProfile profile = new AROR_ForceProfile();
		profile.m_eType = AROR_EForceType.AIRBORNE;
		profile.m_iPersonnelBudget = 20;
		profile.m_iEscortCount = 1;
		passed = Check(profile.ReplacementQuote() == 4100, "Air quote includes both aircraft and personnel capacity") && passed;
		passed = Check(AROR_QRFSpotting.InView(500, 0.95, 800, 0.9), "Visible-cone geometry") && passed;
		passed = Check(!AROR_QRFSpotting.InView(900, 1, 800, 0.9), "Beyond visual range") && passed;
		Print(string.Format("[AROR TEST] QRF strategic policy: %1 (pure tests, no bank mutations)", passed), LogLevel.NORMAL);
		return passed;
	}
	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print("[AROR TEST] QRF strategy FAILED: " + label, LogLevel.ERROR);
		return condition;
	}
}
#endif
