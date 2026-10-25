#ifdef WORKBENCH
//! Explicit, disposable-world server smoke test. Never runs automatically or in packaged builds.
class AROR_OperationSmokeTests
{
	static bool Run_S(string attackTargetId, string defendedLocationId)
	{
		AROR_OperationManagerComponent manager = AROR_OperationManagerComponent.GetInstance();
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!Replication.IsServer() || !manager || !commanders || !commanders.IsCampaignActive() || attackTargetId == defendedLocationId)
		{
			Print("[AROR TEST] Requires active server campaign, two different registered locations and a fresh scheduler with default settings", LogLevel.ERROR);
			return false;
		}
		array<AROR_Operation> existing = {};
		manager.GetOperations(existing);
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations || !locations.GetLocationById(attackTargetId) || !locations.GetLocationById(defendedLocationId) || existing.Count() > 0)
		{
			Print("[AROR TEST] Both test locations must exist and the scheduler must contain no operations", LogLevel.ERROR);
			return false;
		}

		if (!manager.AreInvaderOperationsEnabled())
		{
			bool gated = Check(manager.ReportAttack_S(AROR_EFactionRole.INVADERS, defendedLocationId).IsEmpty(), "Invader QRF gate");
			gated = Check(manager.ScheduleAttack_S(AROR_EFactionRole.INVADERS, attackTargetId).IsEmpty(), "Invader attack gate") && gated;
			gated = Check(manager.ReportAttackAt_S(AROR_EFactionRole.INVADERS, defendedLocationId, "0 0 0").IsEmpty(), "Invader damage-report gate") && gated;
			Print("[AROR TEST] Invader dispatch is disabled; dual-role scheduling tests skipped. Enable only in a disposable test world when explicitly testing Invaders.", LogLevel.NORMAL);
			manager.GetOperations(existing);
			gated = Check(existing.Count() == 0, "Disabled requests created no operations") && gated;
			return gated;
		}

		bool passed = true;
		string occupierAttack = manager.ScheduleAttack_S(AROR_EFactionRole.OCCUPIERS, attackTargetId, 86400);
		string invaderAttack = manager.ScheduleAttack_S(AROR_EFactionRole.INVADERS, attackTargetId, 86400);
		passed = Check(!occupierAttack.IsEmpty() && !invaderAttack.IsEmpty() && occupierAttack != invaderAttack, "Independent role attacks") && passed;
		passed = Check(manager.ScheduleAttack_S(AROR_EFactionRole.OCCUPIERS, attackTargetId, 86400) == occupierAttack, "Attack deduplication") && passed;

		string occupierQRF = manager.ReportAttack_S(AROR_EFactionRole.OCCUPIERS, defendedLocationId);
		string invaderQRF = manager.ReportAttack_S(AROR_EFactionRole.INVADERS, defendedLocationId);
		passed = Check(!occupierQRF.IsEmpty() && !invaderQRF.IsEmpty() && occupierQRF != invaderQRF, "Reserved QRF capacity while attacks are planned") && passed;
		passed = Check(manager.ReportAttack_S(AROR_EFactionRole.OCCUPIERS, defendedLocationId) == occupierQRF, "Repeated combat report deduplication") && passed;
		passed = Check(manager.ReportAttackAt_S(AROR_EFactionRole.OCCUPIERS, defendedLocationId, "0 0 0") == occupierQRF, "Contact report reuses existing QRF") && passed;
		passed = Check(manager.GetOperation(occupierQRF).GetQRFSearchPlan() && !manager.GetOperation(occupierAttack).GetQRFSearchPlan(), "Only QRFs own a search plan") && passed;
		passed = Check(manager.ScheduleAttack_S(AROR_EFactionRole.RESISTANCE, attackTargetId).IsEmpty(), "Resistance cannot schedule autonomous attacks") && passed;
		passed = Check(manager.ReportAttack_S(AROR_EFactionRole.RESISTANCE, defendedLocationId).IsEmpty(), "Resistance cannot request enemy QRFs") && passed;
		passed = Check(manager.ScheduleAttack_S(AROR_EFactionRole.OCCUPIERS, "").IsEmpty(), "Empty target rejected") && passed;
		passed = Check(manager.ScheduleAttack_S(AROR_EFactionRole.OCCUPIERS, attackTargetId, -2).IsEmpty(), "Invalid delay rejected") && passed;
		passed = Check(!manager.MarkActive_S(occupierAttack), "Cannot activate before spawn dispatch") && passed;
		passed = Check(!manager.ResolveOperation_S(occupierAttack, AROR_EOperationState.SUCCEEDED, "Invalid early success"), "Cannot succeed before activation") && passed;

		// Cancel test records before returning; none can reach dispatch in this synchronous test.
		AROR_OperationReentryProbe probe = new AROR_OperationReentryProbe();
		probe.m_Manager = manager;
		probe.m_sTriggerId = occupierAttack;
		probe.m_sFollowupId = invaderAttack;
		manager.GetOnOperationChanged().Insert(probe.OnChanged);
		array<string> ids = {occupierAttack, invaderAttack, occupierQRF, invaderQRF};
		foreach (string id : ids)
		{
			if (!id.IsEmpty() && manager.GetOperation(id) && !manager.GetOperation(id).IsTerminal())
				passed = Check(manager.ResolveOperation_S(id, AROR_EOperationState.CANCELLED, "Workbench smoke test cleanup"), "Test operation cancelled") && passed;
		}
		manager.GetOnOperationChanged().Remove(probe.OnChanged);
		passed = Check(!probe.m_bReentered && probe.m_iFollowupNotifications == 1 && manager.GetOperation(invaderAttack) && manager.GetOperation(invaderAttack).IsTerminal(), "Nested resolution notifies once without recursive Invoke") && passed;
		passed = Check(!manager.MarkActive_S(occupierAttack), "Late acknowledgement rejected") && passed;
		passed = Check(manager.ReportAttack_S(AROR_EFactionRole.OCCUPIERS, defendedLocationId).IsEmpty(), "QRF cooldown survives cancellation") && passed;
		passed = Check(manager.ScheduleAttack_S(AROR_EFactionRole.INVADERS, attackTargetId).IsEmpty(), "Attack cooldown survives cancellation") && passed;

		// Exercise successful execution and late-result rejection without requiring an AI executor.
		AROR_Operation operation = new AROR_Operation("test_lifecycle", AROR_EFactionRole.INVADERS, AROR_EOperationType.QRF, defendedLocationId, 0, 0);
		passed = Check(!operation.BindForceProfile_S("test_profile"), "Cannot bind force before dispatch") && passed;
		passed = Check(operation.Transition_S(AROR_EOperationState.SPAWNING, 1, ""), "Dispatch") && passed;
		passed = Check(!operation.BindForceProfile_S("test_profile", 0) && !operation.BindForceProfile_S("test_profile", 6), "Invalid response levels rejected") && passed;
		passed = Check(operation.BindForceProfile_S("test_profile", 5) && operation.GetResponseLevel() == 5, "Bind level-five force at dispatch") && passed;
		passed = Check(!operation.BindForceProfile_S("another_profile", 1) && operation.GetForceProfileId() == "test_profile" && operation.GetResponseLevel() == 5, "Force and level cannot reroll") && passed;
		passed = Check(operation.Transition_S(AROR_EOperationState.ACTIVE, 2, ""), "Acknowledge") && passed;
		passed = Check(operation.Transition_S(AROR_EOperationState.SUCCEEDED, 3, "Defended"), "Resolve success") && passed;
		passed = Check(!operation.Transition_S(AROR_EOperationState.FAILED, 4, "Late callback"), "Terminal outcome cannot be overwritten") && passed;
		Print(string.Format("[AROR TEST] Operation smoke tests passed=%1", passed), LogLevel.NORMAL);
		return passed;
	}

	protected static bool Check(bool condition, string label)
	{
		if (!condition)
			Print(string.Format("[AROR TEST] FAILED: %1", label), LogLevel.ERROR);
		return condition;
	}
}
#endif
