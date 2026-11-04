#ifdef WORKBENCH
class AROR_ResponseScaleSmokeTests
{
	static bool Run()
	{
		AROR_ForceProfile profile = AROR_ForceProfile.CreateForScript();
		profile.m_sId = "scale_test";
		profile.m_bEnabled = true;
		profile.m_eRole = AROR_EFactionRole.OCCUPIERS;
		profile.m_sFactionKey = "US";
		profile.m_eType = AROR_EForceType.MOTORIZED;
		profile.m_sVehicle = "test_vehicle.et";
		profile.m_iCrewCount = 1;
		profile.m_iPersonnelBudget = 8;
		profile.m_fDismountDistance = 150;
		profile.m_iAttackWeight = 40;
		profile.m_iQRFWeight = 40;
		array<ref AROR_ForceProfile> profiles = {profile};
		array<ref AROR_ResponseLevel> levels = {};
		for (int i = 1; i <= 5; i++)
		{
			AROR_ResponseLevel level = new AROR_ResponseLevel();
			level.m_iLevel = i;
			level.m_iAttackWeight = 20;
			level.m_iQRFWeight = 20;
			levels.Insert(level);
		}
		bool passed = true;
		// Verify every possible ticket, so every level is reachable with exactly its configured weight.
		array<int> counts = {0, 0, 0, 0, 0};
		for (int ticket = 0; ticket < 100; ticket++)
		{
			int picked = AROR_ResponseScale.PickLevel(levels, profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 20, 128, 10, ticket);
			if (picked < 1 || picked > 5)
				passed = Check(false, "Ticket must select a valid level") && passed;
			else
				counts[picked - 1] = counts[picked - 1] + 1;
		}
		foreach (int count : counts)
			passed = Check(count == 20, "Each level receives exactly 20 tickets") && passed;
		passed = Check(AROR_ResponseScale.PickLevel(levels, profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 20, 128, 10, 100) == 0, "Upper ticket boundary") && passed;
		profiles.Insert(profile);
		passed = Check(AROR_ResponseScale.PickLevel(levels, profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 20, 128, 10, 20) == 2, "Additional profile variants do not bias level weights") && passed;
		profiles.Remove(1);
		passed = Check(AROR_ResponseScale.PickLevel(levels, profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 2, 8, 1, 19) == 1, "Only feasible level remains selectable") && passed;
		passed = Check(AROR_ResponseScale.PickLevel(levels, profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 2, 8, 1, 20) == 0, "Infeasible higher levels receive no tickets") && passed;
		passed = Check(AROR_ForcePool.GetEligibleWeight(profile, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 10, 40, 5, 5) == 40, "Level five reserves full package cost") && passed;
		passed = Check(AROR_ForcePool.GetEligibleWeight(profile, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 10, 39, 5, 5) == 0, "Partial level-five member budget rejected") && passed;
		passed = Check(AROR_ForcePool.GetEligibleWeight(profile, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 10, 40, 4, 5) == 0, "Partial level-five vehicle budget rejected") && passed;
		levels[0].m_iQRFWeight = 0;
		passed = Check(AROR_ResponseScale.PickLevel(levels, profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.QRF, 20, 128, 10, 0) == 2, "Zero-weight QRF level excluded") && passed;
		passed = Check(AROR_ResponseScale.PickLevel(levels, profiles, AROR_EFactionRole.OCCUPIERS, "US", AROR_EOperationType.ATTACK, 20, 128, 10, 0) == 1, "Attack level weights independent") && passed;
		Print(string.Format("[AROR TEST] Response-scale smoke tests passed=%1", passed), LogLevel.NORMAL);
		return passed;
	}

	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print(string.Format("[AROR TEST] FAILED response-scale check: %1", label), LogLevel.ERROR);
		return condition;
	}
}
#endif
