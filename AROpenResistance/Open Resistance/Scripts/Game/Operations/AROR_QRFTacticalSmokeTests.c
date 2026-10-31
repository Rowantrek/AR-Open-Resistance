#ifdef WORKBENCH
//! Tactical policy only. No actor spawning, operation requests or enabling Invaders.
class AROR_QRFTacticalSmokeTests
{
	static bool Run_S()
	{
		if (!Replication.IsServer())
			return false;
		bool passed = true;
		for (int tier = 1; tier <= 5; tier++)
		{
			for (int count = 1; count <= 8; count++)
			{
				AROR_QRFTacticalCommander commander = Build(tier, count);
				int expected = count;
				if (tier >= 3 && count >= 4)
					expected = (count + 1) / 2;
				passed = Check(commander.m_iFirstWaveCount == expected, "Tier/size wave split") && passed;
				for (int index = 0; index < count; index++)
					passed = Check(commander.CanConstruct(index, 0, 0, 8, 1) == (index < expected), "Only lead wave constructs before dispatch") && passed;
			}
		}
		AROR_QRFTacticalCommander reserve = Build(5, 8);
		reserve.MarkDispatched(100);
		passed = Check(!reserve.CanConstruct(0, 160, 0, 6, 1), "Dispatched lead wave cannot respawn") && passed;
		passed = Check(!reserve.CanConstruct(4, 159, 4, 6, 1), "Reserve delay enforced") && passed;
		passed = Check(reserve.CanConstruct(4, 160, 4, 6, 1), "Reserve released after delay") && passed;
		passed = Check(!reserve.CanConstruct(4, 160, 6, 6, 1), "Live-package limit enforced") && passed;
		reserve.MarkLost(0);
		reserve.MarkLost(0);
		passed = Check(reserve.m_iLosses == 1, "Loss notification idempotent") && passed;
		passed = Check(reserve.CanConstruct(4, 115, 3, 6, 1), "Loss releases finite reserve early") && passed;
		passed = Check(!reserve.CanConstruct(4, 114, 3, 6, 1), "Minimum reserve reaction delay") && passed;
		passed = Check(!reserve.CanConstruct(4, 400, 0, 6, 1), "Expired reserve never spawns") && passed;
		AROR_QRFTacticalCommander air = Build(5, 5);
		passed = Check(air.m_iFirstWaveCount == 3, "Five air packages split three/two") && passed;
		air.MarkDispatched(100);
		passed = Check(!air.CanConstruct(3, 160, 5, 6, 2), "Air pairs require two free slots") && passed;
		passed = Check(air.CanConstruct(3, 160, 4, 6, 2), "Air pair fits remaining slots") && passed;

		AROR_QRFTacticalCommander retreat = Build(1, 2);
		retreat.MarkDispatched(100);
		retreat.RecordStrength(0, 10);
		retreat.RecordStrength(1, 10);
		retreat.RecordStrength(0, 2);
		retreat.RecordStrength(1, 2);
		passed = Check(!retreat.ShouldWithdraw(110, true), "Available reserve delays casualty withdrawal") && passed;
		passed = Check(retreat.ShouldWithdraw(110, false), "Severe losses without reserve withdraw") && passed;
		passed = Check(!retreat.CanConstruct(0, 110, 0, 6, 1), "Withdrawal prevents spawning") && passed;
		retreat = Build(1, 2);
		retreat.MarkDispatched(100);
		retreat.RecordStrength(0, 10);
		passed = Check(retreat.ShouldWithdraw(700, true), "Mission duration remains bounded") && passed;

		AROR_QRFTacticalCommander balanced = Build(5, 7);
		array<AROR_ForceProfile> elements = {};
		for (int package = 0; package < 7; package++)
		{
			AROR_ForceProfile profile = AROR_ForceProfile.CreateForScript();
			profile.m_eType = AROR_EForceType.MOTORIZED;
			if (package >= 3)
				profile.m_eType = AROR_EForceType.MECHANIZED;
			if (package >= 5)
				profile.m_eType = AROR_EForceType.ARMOUR;
			elements.Insert(profile);
		}
		balanced.BalanceGroundWaves(elements);
		passed = Check(!balanced.IsReserve(0) && !balanced.IsReserve(3) && !balanced.IsReserve(5), "Lead wave contains mobility/protection/armour") && passed;
		int lead = 0;
		for (int candidate = 0; candidate < 7; candidate++)
		{
			if (!balanced.IsReserve(candidate))
				lead++;
		}
		passed = Check(lead == 4, "Balancing preserves wave size and total response") && passed;
		balanced.RecordStrength(0, 6);
		balanced.RecordStrength(3, 6);
		balanced.RecordStrength(5, 3);
		passed = Check(balanced.ActiveSectorCount() == 3 && balanced.ActiveSectorIndex(5) == 2, "Only readied packages occupy search sectors") && passed;
		balanced.RefreshRoster(10);
		int rosterVersion = balanced.m_iContactVersion;
		balanced.MarkLost(3);
		balanced.RefreshRoster(11);
		passed = Check(balanced.m_iContactVersion == rosterVersion, "Loss retasking is rate limited") && passed;
		balanced.RefreshRoster(25);
		passed = Check(balanced.m_iContactVersion == rosterVersion + 1 && balanced.ActiveSectorCount() == 2, "Survivors redistribute after losses") && passed;
		passed = Check(balanced.ActiveSectorIndex(5) == 1, "Survivor ranks close lost sectors") && passed;
		balanced.RecordStrength(6, 3);
		balanced.RefreshRoster(40);
		passed = Check(balanced.ActiveSectorCount() == 3 && balanced.ActiveSectorIndex(6) == 2, "New wave joins sector allocation") && passed;

		AROR_QRFTacticalCommander contact = Build(1, 2);
		contact.m_vTown = "0 0 0";
		AROR_CombatContact report = new AROR_CombatContact();
		report.m_vPosition = "10 0 10";
		contact.UpdateContact_S(report, 10);
		int contactVersion = contact.m_iContactVersion;
		passed = Check(contact.m_bHasContact, "Fresh snapshot starts investigation") && passed;
		report.m_vPosition = "40 0 10";
		contact.UpdateContact_S(report, 25);
		passed = Check(contact.m_iContactVersion == contactVersion, "Small snapshot drift preserves orders") && passed;
		report.m_vPosition = "70 0 10";
		contact.UpdateContact_S(report, 40);
		report.m_vPosition = "100 0 10";
		contact.UpdateContact_S(report, 55);
		passed = Check(contact.m_iContactVersion == contactVersion + 1, "Cumulative snapshot movement retasks") && passed;
		contact.UpdateContact_S(null, 56);
		passed = Check(contact.m_bHasContact, "Expiry retask respects interval") && passed;
		contact.UpdateContact_S(null, 70);
		passed = Check(!contact.m_bHasContact && contact.m_vContact == contact.m_vTown, "Expired snapshot returns search to town") && passed;
		AROR_QRFTacticalCommander capture = Build(1, 2);
		capture.MarkDispatched(100);
		capture.RecordStrength(0, 5);
		passed = Check(!capture.RecordTargetControl(AROR_EFactionRole.OCCUPIERS), "Initial ownership is a baseline") && passed;
		passed = Check(capture.RecordTargetControl(AROR_EFactionRole.RESISTANCE), "Town capture updates the tactical objective") && passed;
		passed = Check(!capture.ShouldWithdraw(110, false), "Town capture alone does not withdraw the QRF") && passed;
		passed = Check(!capture.RecordTargetControl(AROR_EFactionRole.RESISTANCE), "Repeated ownership observations do not reset search") && passed;
		AROR_QRFTacticalCommander approach = Build(1, 2);
		approach.m_vTown = "5500 96 6080";
		approach.m_fTownRadius = 250;
		approach.m_fMargin = 150;
		vector origin = "5100 0 6080";
		passed = Check(approach.ApproachCandidate(1, origin, 200, 0) == approach.ApproachPoint(1, origin, 200), "First road candidate preserves the preferred wedge") && passed;
		for (int attempt = 1; attempt < 16; attempt++)
		{
			vector roadCandidate = approach.ApproachCandidate(1, origin, 200, attempt);
			float townDistance = vector.DistanceXZ(roadCandidate, approach.m_vTown);
			passed = Check(townDistance >= 60 && townDistance <= 325.1, "Alternate road candidates remain in bounded rings") && passed;
		}
		if (passed)
			Print("[AROR TEST] QRF tactical/wave policy regressions passed", LogLevel.NORMAL);
		return passed;
	}
	protected static AROR_QRFTacticalCommander Build(int tier, int count)
	{
		AROR_QRFTacticalCommander commander = new AROR_QRFTacticalCommander();
		commander.Initialize(tier, count, true, 3, 60, 300, 600, 15, 0.4);
		return commander;
	}
	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print("[AROR TEST] Tactical QRF FAILED: " + label, LogLevel.ERROR);
		return condition;
	}
}
#endif
