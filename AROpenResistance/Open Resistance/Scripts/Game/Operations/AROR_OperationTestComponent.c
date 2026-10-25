[ComponentEditorProps(category: "OpenResistance/Operations", description: "Workbench-only one-shot simulated QRF reports or scheduler smoke tests. Use on the Everon_Chicken game mode.")]
class AROR_OperationTestComponentClass : SCR_BaseGameModeComponentClass
{
}

//! The component type exists in packaged builds, but all test execution is Workbench-only.
class AROR_OperationTestComponent : SCR_BaseGameModeComponent
{
	[Attribute("", desc: "Workbench: simulate one attack report against an Occupier-held location ID. Empty skips this report.")]
	protected string m_sOccupiersQRFTestTargetId;
	[Attribute("", desc: "Workbench: simulate one attack report against an Invader-held location ID. Empty skips this report.")]
	protected string m_sInvadersQRFTestTargetId;
	[Attribute("15", desc: "Workbench: seconds after campaign readiness before running the one-shot test")]
	protected int m_iReportDelay;
	[Attribute("0", desc: "Workbench: run scheduler smoke tests instead of QRF reports. Requires a fresh scheduler, default limits/cooldowns, no automatic targets and executor disabled.")]
	protected bool m_bRunSchedulerSmokeTests;
	[Attribute("everon_provins", desc: "Workbench scheduler smoke test attack location ID")]
	protected string m_sSmokeAttackTargetId;
	[Attribute("resistance_hq", desc: "Workbench scheduler smoke test second location ID")]
	protected string m_sSmokeDefendedTargetId;

#ifdef WORKBENCH
	protected int m_iReadySeconds;

	protected void Tick()
	{
		if (!Replication.IsServer())
			return;
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!commanders || !commanders.IsCampaignActive() || !operations)
			return;
		m_iReadySeconds++;
		if (m_iReadySeconds < m_iReportDelay)
			return;
		// Never repeat a simulated attack every tick or accidentally spend additional QRF slots.
		GetGame().GetCallqueue().Remove(Tick);
		if (m_bRunSchedulerSmokeTests)
		{
			AROR_ForcePoolSmokeTests.Run();
			AROR_ResponseScaleSmokeTests.Run();
			AROR_QRFSearchPlanSmokeTests.Run_S();
			AROR_ReaperLandingSearchSmokeTests.Run_S();
			AROR_AirTrafficPolicySmokeTests.Run_S();
			AROR_GroundQRFRecipeSmokeTests.Run_S();
			AROR_QRFSelectionSmokeTests.Run_S();
			AROR_QRFTacticalSmokeTests.Run_S();
			AROR_QRFStrategySmokeTests.Run_S();
			AROR_OperationSmokeTests.Run_S(m_sSmokeAttackTargetId, m_sSmokeDefendedTargetId);
			return;
		}
		if (!m_sOccupiersQRFTestTargetId.IsEmpty())
			ReportTestAttack(operations, AROR_EFactionRole.OCCUPIERS, m_sOccupiersQRFTestTargetId);
		if (!m_sInvadersQRFTestTargetId.IsEmpty())
			ReportTestAttack(operations, AROR_EFactionRole.INVADERS, m_sInvadersQRFTestTargetId);
	}

	protected void ReportTestAttack(AROR_OperationManagerComponent operations, AROR_EFactionRole role, string targetId)
	{
		string id = operations.ReportAttack_S(role, targetId);
		if (id.IsEmpty() && operations.HasPendingQRFThreat(role, targetId))
			Print(string.Format("[AROR TEST] Simulated attack queued: role=%1 target=%2; waiting for strategic admission", role, targetId), LogLevel.NORMAL);
		else if (id.IsEmpty())
			Print(string.Format("[AROR TEST] QRF report rejected: role=%1 target=%2; check target, cooldown, capacity and conflicting attack", role, targetId), LogLevel.ERROR);
		else
			Print(string.Format("[AROR TEST] Simulated attack: role=%1 target=%2 QRF=%3", role, targetId, id), LogLevel.NORMAL);
	}
#endif

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
#ifdef WORKBENCH
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;
		if (!m_bRunSchedulerSmokeTests && m_sOccupiersQRFTestTargetId.IsEmpty() && m_sInvadersQRFTestTargetId.IsEmpty())
			return;
		if (m_iReportDelay < 1)
		{
			Print("[AROR TEST] Report delay must be at least one second; test disabled", LogLevel.ERROR);
			return;
		}
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);
#endif
	}

	override void OnDelete(IEntity owner)
	{
#ifdef WORKBENCH
		GetGame().GetCallqueue().Remove(Tick);
#endif
		super.OnDelete(owner);
	}
}
