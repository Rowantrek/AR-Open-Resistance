void AROR_OnOperationChanged(AROR_Operation operation);
typedef func AROR_OnOperationChanged;
typedef ScriptInvokerBase<AROR_OnOperationChanged> AROR_OnOperationChangedInvoker;

[ComponentEditorProps(category: "OpenResistance/Operations", description: "Server attack scheduler and QRF requests for both enemy roles. Attach once to the game mode.")]
class AROR_OperationManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class AROR_OperationManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("0", desc: "Enable money-backed QRFs: readiness gates equipment, crackdown sets response intensity. Requires Economy. Explicit recipe fixtures remain available when disabled.")]
	protected bool m_bQRFStrategicEconomy;
	[Attribute("0", desc: "After a hostile player capture, allow an unsecured tier-1 ground response if no paid recipe is affordable. Overhead and confirmed losses still debit Economy or become saved debt; capacity and cooldowns remain authoritative.")]
	protected bool m_bMinimumCaptureQRF;
	[Attribute("2", desc: "Maximum fresh planning attempts after a capture QRF fails before activation. Retries retain the original capture age and normal role cooldown.", params: "0 3 1")]
	protected int m_iCapturePlanningRetries;
	[Attribute("0", desc: "Share last-seen QRF vehicle markers after visual observation by living Resistance players")]
	protected bool m_bQRFSpotting;
	[Attribute("0", desc: "Temporary debugging: show all spawned QRF vehicles on Resistance maps without observation, range or line-of-sight checks", category: "QRF spotting")]
	protected bool m_bQRFDebugMarkers;
	[Attribute("800", desc: "Maximum unaided ground vehicle spotting distance, metres", params: "100 2000 50")]
	protected float m_fQRFSpotGroundRange;
	[Attribute("1500", desc: "Maximum aircraft spotting distance, metres", params: "100 3000 50")]
	protected float m_fQRFSpotAirRange;
	[Attribute("0.9", desc: "Minimum head-view dot product for spotting; 0.9 is roughly a 26-degree half cone", params: "0.5 0.99 0.01")]
	protected float m_fQRFSpotMinimumDot;
	[Attribute("8", desc: "Maximum native spotting traces per server second, across all players and vehicles", params: "1 16 1")]
	protected int m_iQRFSpotTraceLimit;
	[Attribute("90", desc: "Seconds before unrefreshed last-seen QRF markers expire", params: "15 300 5")]
	protected int m_iQRFSpotLifetime;
	protected ref AROR_QRFSpotting m_QRFSpotting = new AROR_QRFSpotting();
	[Attribute("2", desc: "Initial equipment-pool readiness for each AI role; saved capture progress drives levels 1-5", params: "1 5 1")]
	protected int m_iStartingReadinessLevel;
	[Attribute("30", desc: "Target percentage of ordinary dispatched QRFs containing airborne forces, separately per army. Counts standalone air and joint responses; funding and launch safety still apply.", params: "0 100 1")]
	protected int m_iAirborneQRFShare;
	protected int m_iOccupierQRFDispatches;
	protected int m_iOccupierAirDispatches;
	protected int m_iInvaderQRFDispatches;
	protected int m_iInvaderAirDispatches;

	int GetQRFDispatchCount(AROR_EFactionRole role, bool airborne = false)
	{
		if (role == AROR_EFactionRole.OCCUPIERS)
		{
			if (airborne)
				return m_iOccupierAirDispatches;
			return m_iOccupierQRFDispatches;
		}
		if (role == AROR_EFactionRole.INVADERS)
		{
			if (airborne)
				return m_iInvaderAirDispatches;
			return m_iInvaderQRFDispatches;
		}
		return 0;
	}

	bool PreferAirQRF(AROR_EFactionRole role)
	{
		return AROR_QRFResponseMix.PreferAir(GetQRFDispatchCount(role), GetQRFDispatchCount(role, true), m_iAirborneQRFShare);
	}

	void RestoreQRFResponseMix_S(int occupierTotal, int occupierAir, int invaderTotal, int invaderAir)
	{
		if (!Replication.IsServer() || !AROR_QRFResponseMix.CountsValid(occupierTotal, occupierAir) || !AROR_QRFResponseMix.CountsValid(invaderTotal, invaderAir))
			return;
		m_iOccupierQRFDispatches = occupierTotal;
		m_iOccupierAirDispatches = occupierAir;
		m_iInvaderQRFDispatches = invaderTotal;
		m_iInvaderAirDispatches = invaderAir;
	}

	protected void RecordQRFDispatchMix_S(AROR_Operation operation)
	{
		if (!Replication.IsServer() || !operation || !operation.m_bResponseMixTracked || operation.m_bResponseMixCounted || !AROR_QRFResponseMix.IsCampaignQRF(operation))
			return;
		if (operation.GetState() != AROR_EOperationState.ACTIVE || !IsEnemyRole(operation.GetRole()))
			return;
		operation.m_bResponseMixCounted = true;
		int total = GetQRFDispatchCount(operation.GetRole());
		int air = GetQRFDispatchCount(operation.GetRole(), true);
		// Preserve the ratio without overflowing bounded save counters in very long campaigns.
		if (total >= 1000000000)
		{
			total = total / 2;
			air = air / 2;
		}
		total++;
		if (operation.m_bResponseMixAirborne)
			air++;
		if (operation.GetRole() == AROR_EFactionRole.OCCUPIERS)
		{
			m_iOccupierQRFDispatches = total;
			m_iOccupierAirDispatches = air;
		}
		else
		{
			m_iInvaderQRFDispatches = total;
			m_iInvaderAirDispatches = air;
		}
		Print(string.Format("[AROR MIX] role=%1 dispatched=%2 airborneOrJoint=%3 targetPercent=%4", operation.GetRole(), total, air, m_iAirborneQRFShare), LogLevel.NORMAL);
	}
	[Attribute("20", desc: "Initial crackdown percentage for each AI role; 20 starts new campaigns at response tier 2 (each tier is 20)", params: "0 100 1")]
	protected int m_iStartingCrackdown;
	[Attribute("50", desc: "Long-term readiness progress per newly recorded capture loss. Combat and quiet time do not change this progress.", params: "0 100 1")]
	protected int m_iCaptureReadinessPressure;
	[Attribute("100", desc: "Crackdown points per newly recorded capture loss, on a 0-1000 scale; 100 gives two capture losses per response tier", params: "0 1000 5")]
	protected int m_iCaptureCrackdown;
	[Attribute("600", desc: "Seconds without validated evidence or confirmed QRF losses before crackdown starts cooling", params: "60 3600 30")]
	protected int m_iCrackdownQuietTime;
	[Attribute("0", desc: "Crackdown points removed each quiet minute, on a 0-1000 scale; 0 turns cooling off. Equipment readiness progress is retained.", params: "0 1000 1")]
	protected int m_iCrackdownDecay;
	[Attribute("50", desc: "Fixed QRF deployment overhead, charged once when ACTIVE", params: "0 100000 1")]
	protected int m_iQRFBaseOverhead;
	[Attribute("10", desc: "Additional overhead per bound response tier", params: "0 10000 1")]
	protected int m_iQRFTierOverhead;
	[Attribute("180", desc: "Unrefreshed defense threat expiry, seconds", params: "30 900 1")]
	protected int m_iQRFThreatLifetime;
	protected ref AROR_QRFFinance m_QRFFinance = new AROR_QRFFinance();
	protected ref array<ref AROR_QRFThreat> m_aQRFThreats = {};
	//! Public campaign summary for RebIntel; operations and threat queues remain server-only.
	[RplProp()]
	protected int m_iOccupierPressure;
	[RplProp()]
	protected int m_iInvaderPressure;
	[RplProp()]
	protected int m_iOccupierCrackdown;
	[RplProp()]
	protected int m_iInvaderCrackdown;
	[RplProp()]
	protected bool m_bThreatStateReady;
	protected int m_iOccupierEvidenceAt;
	protected int m_iInvaderEvidenceAt;
	protected int m_iOccupierLastThreat;
	protected int m_iInvaderLastThreat;
	protected bool m_bStrategyRestored;
	protected int m_iNextStrategyTick;
	protected int m_iNextCrackdownDecay;
	protected int m_iAppliedOccupierLoss;
	protected int m_iAppliedInvaderLoss;

	bool IsQRFStrategyEnabled()
	{
		return m_bQRFStrategicEconomy;
	}
	bool AllowsMinimumCaptureQRF()
	{
		return m_bQRFStrategicEconomy && m_bMinimumCaptureQRF;
	}
	void SetQRFObserverMapOpen_S(int playerId, bool open)
	{
		if (Replication.IsServer())
			m_QRFSpotting.SetMapOpen_S(playerId, open);
	}
	//! Empty report IDs can mean queued strategic work rather than rejection.
	bool HasPendingQRFThreat(AROR_EFactionRole role, string locationId)
	{
		foreach (AROR_QRFThreat threat : m_aQRFThreats)
		{
			if (threat.m_eRole == role && threat.m_sLocationId == locationId)
				return true;
		}
		return false;
	}
	AROR_QRFFinance GetQRFFinance()
	{
		return m_QRFFinance;
	}
	int QRFOverhead(int tier)
	{
		return m_iQRFBaseOverhead + m_iQRFTierOverhead * tier;
	}
	int GetReadinessPressure(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.OCCUPIERS)
			return m_iOccupierPressure;
		if (role == AROR_EFactionRole.INVADERS)
			return m_iInvaderPressure;
		return 0;
	}
	int GetReadinessLevel(AROR_EFactionRole role)
	{
		return AROR_QRFStrategicPolicy.ReadinessLevel(GetReadinessPressure(role));
	}
	//! Every machine: false until the server has initialized or restored the summary.
	bool HasThreatState()
	{
		return m_bThreatStateReady;
	}
	int GetCrackdown(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.OCCUPIERS)
			return m_iOccupierCrackdown;
		if (role == AROR_EFactionRole.INVADERS)
			return m_iInvaderCrackdown;
		return 0;
	}
	int GetCrackdownPercent(AROR_EFactionRole role)
	{
		// Keep displayed percentage thresholds aligned with response-level thresholds.
		return Math.Floor(Math.Clamp(GetCrackdown(role), 0, 1000) / 10.0);
	}
	int GetCrackdownLevel(AROR_EFactionRole role)
	{
		return AROR_QRFStrategicPolicy.ResponseLevel(GetCrackdown(role));
	}
	//! Read-only save snapshot includes confirmed losses not yet applied by the policy tick.
	int GetCrackdownForSave(AROR_EFactionRole role)
	{
		int unapplied;
		if (role == AROR_EFactionRole.OCCUPIERS)
			unapplied = m_QRFFinance.m_iOccupierLossValue - m_iAppliedOccupierLoss;
		else if (role == AROR_EFactionRole.INVADERS)
			unapplied = m_QRFFinance.m_iInvaderLossValue - m_iAppliedInvaderLoss;
		else
			return 0;
		int pending = Math.Max(0, unapplied) + m_QRFFinance.PendingLossValue(role);
		int crackdown = GetCrackdown(role);
		if (m_bQRFStrategicEconomy)
			crackdown += AROR_QRFStrategicPolicy.LossCrackdown(pending);
		return Math.Clamp(crackdown, 0, 1000);
	}
	//! Trusted server integration seam; public UI only reads the replicated summary.
	void SetCrackdown_S(AROR_EFactionRole role, int value)
	{
		if (!Replication.IsServer() || !IsEnemyRole(role))
			return;
		value = Math.Clamp(value, 0, 1000);
		if (GetCrackdown(role) == value)
			return;
		if (role == AROR_EFactionRole.OCCUPIERS)
			m_iOccupierCrackdown = value;
		else
			m_iInvaderCrackdown = value;
		Replication.BumpMe();
	}
	//! Either scale's level 1-5, for UI and tools that treat both alike
	int GetThreatLevel(AROR_EFactionRole role, AROR_EThreatScale scale)
	{
		if (scale == AROR_EThreatScale.READINESS)
			return GetReadinessLevel(role);
		return GetCrackdownLevel(role);
	}
	//! Trusted server/admin seam; no new unprotected client RPC is added.
	void SetReadinessLevel_S(AROR_EFactionRole role, int level)
	{
		if (Replication.IsServer() && IsEnemyRole(role) && level >= 1 && level <= 5)
			SetReadinessPressure_S(role, AROR_QRFStrategicPolicy.PressureForLevel(level));
	}
	protected void SetReadinessPressure_S(AROR_EFactionRole role, int value)
	{
		if (!Replication.IsServer() || !IsEnemyRole(role))
			return;
		value = Math.Clamp(value, 0, 1000);
		if (GetReadinessPressure(role) == value)
			return;
		int before = GetReadinessLevel(role);
		if (role == AROR_EFactionRole.OCCUPIERS)
			m_iOccupierPressure = Math.Clamp(value, 0, 1000);
		else if (role == AROR_EFactionRole.INVADERS)
			m_iInvaderPressure = Math.Clamp(value, 0, 1000);
		Replication.BumpMe();
		if (before != GetReadinessLevel(role))
			Print(string.Format("[AROR STRATEGY] role=%1 readiness %2 -> %3", role, before, GetReadinessLevel(role)), LogLevel.NORMAL);
	}
	void RestoreQRFStrategy_S(int occupierPressure, int invaderPressure, int occupierDebt, int invaderDebt, int occupierCrackdown = -1, int invaderCrackdown = -1)
	{
		if (!Replication.IsServer())
			return;
		SetReadinessPressure_S(AROR_EFactionRole.OCCUPIERS, occupierPressure);
		SetReadinessPressure_S(AROR_EFactionRole.INVADERS, invaderPressure);
		// Version 1 stored a single pressure for both concepts. Preserve its readings once.
		if (occupierCrackdown < 0)
			occupierCrackdown = occupierPressure;
		if (invaderCrackdown < 0)
			invaderCrackdown = invaderPressure;
		SetCrackdown_S(AROR_EFactionRole.OCCUPIERS, occupierCrackdown);
		SetCrackdown_S(AROR_EFactionRole.INVADERS, invaderCrackdown);
		m_iOccupierLastThreat = m_iClock;
		m_iInvaderLastThreat = m_iClock;
		m_QRFFinance.SetDebt_S(AROR_EFactionRole.OCCUPIERS, occupierDebt);
		m_QRFFinance.SetDebt_S(AROR_EFactionRole.INVADERS, invaderDebt);
		m_bStrategyRestored = true;
		m_bThreatStateReady = true;
		Replication.BumpMe();
	}
	protected void RecordStrategicEvidence(AROR_EFactionRole role)
	{
		if (!m_bQRFStrategicEconomy)
			return;
		if (role == AROR_EFactionRole.OCCUPIERS)
		{
			m_iOccupierLastThreat = m_iClock;
			if (m_iClock < m_iOccupierEvidenceAt)
				return;
			m_iOccupierEvidenceAt = m_iClock + 30;
		}
		else
		{
			m_iInvaderLastThreat = m_iClock;
			if (m_iClock < m_iInvaderEvidenceAt)
				return;
			m_iInvaderEvidenceAt = m_iClock + 30;
		}
		SetCrackdown_S(role, GetCrackdown(role) + 5);
	}
	//! Called once per queued capture or existing response; retries do not call this.
	protected void RecordCaptureEscalation_S(AROR_EFactionRole role)
	{
		SetReadinessPressure_S(role, GetReadinessPressure(role) + m_iCaptureReadinessPressure);
		SetCrackdown_S(role, GetCrackdown(role) + m_iCaptureCrackdown);
	}
	protected string QueueQRFThreat_S(AROR_EFactionRole role, string targetId, bool captured = false)
	{
		if (!IsReady() || !IsEnemyRole(role) || !HasLocation(targetId))
			return string.Empty;
		if (role == AROR_EFactionRole.INVADERS && !m_bInvaderOperationsEnabled)
			return string.Empty;
		RecordStrategicEvidence(role);
		foreach (AROR_Operation operation : m_aOperations)
		{
			if (!operation.IsTerminal() && operation.GetRole() == role && operation.GetTargetLocationId() == targetId && operation.GetType() == AROR_EOperationType.QRF)
			{
				if (captured && !operation.m_bCaptureThreatRecorded)
				{
					operation.m_bCaptureThreatRecorded = true;
					RecordCaptureEscalation_S(role);
				}
				return operation.GetId();
			}
		}
		AROR_QRFThreat threat;
		foreach (AROR_QRFThreat pending : m_aQRFThreats)
		{
			if (pending.m_eRole == role && pending.m_sLocationId == targetId)
				threat = pending;
		}
		if (!threat)
		{
			if (m_aQRFThreats.Count() >= 32)
				return string.Empty;
			threat = new AROR_QRFThreat();
			threat.m_eRole = role;
			threat.m_sLocationId = targetId;
			threat.m_iFirstSeen = m_iClock;
			AROR_LocationComponent location = AROR_LocationManagerComponent.GetInstance().GetLocationById(targetId);
			threat.m_iImportance = AROR_QRFStrategicPolicy.Importance(location.GetLocationType());
			m_aQRFThreats.Insert(threat);
			Print(string.Format("[AROR STRATEGY] queued threat role=%1 target=%2 capture=%3", role, targetId, captured), LogLevel.NORMAL);
		}
		threat.m_iLastSeen = m_iClock;
		if (captured && !threat.m_bCaptured)
			RecordCaptureEscalation_S(role);
		threat.m_bCaptured = threat.m_bCaptured || captured;
		if (m_iClock >= threat.m_iNextEvidence)
		{
			threat.m_iReports = Math.Min(6, threat.m_iReports + 1);
			threat.m_iNextEvidence = m_iClock + 30;
		}
		return string.Empty;
	}
	protected void ProcessQRFThreats()
	{
		for (int index = m_aQRFThreats.Count() - 1; index >= 0; index--)
		{
			AROR_QRFThreat expired = m_aQRFThreats[index];
			int lifetime = m_iQRFThreatLifetime;
			if (expired.m_bCaptured)
				lifetime = Math.Max(lifetime, 600);
			if (!HasLocation(expired.m_sLocationId) || !IsCaptureThreatRelevant(expired) || m_iClock - expired.m_iLastSeen > lifetime)
				m_aQRFThreats.Remove(index);
		}
		array<AROR_EFactionRole> roles = {};
		AROR_FactionRoles.GetAIMilitary(roles);
		foreach (AROR_EFactionRole role : roles)
		{
			if (role == AROR_EFactionRole.INVADERS && !m_bInvaderOperationsEnabled)
				continue;
			AROR_QRFThreat best;
			foreach (AROR_QRFThreat candidate : m_aQRFThreats)
			{
				if (candidate.m_eRole != role || candidate.m_iNextAttempt > m_iClock)
					continue;
				if (!best || candidate.Score(m_iClock) > best.Score(m_iClock))
					best = candidate;
			}
			if (!best)
				continue;
			best.m_iNextAttempt = m_iClock + 15;
			// Queued intensity follows current crackdown; admission freezes intensity and equipment.
			best.m_iDesiredTier = GetCrackdownLevel(role);
			AROR_OperationExecutorComponent executor = AROR_OperationExecutorComponent.Cast(GetOwner().FindComponent(AROR_OperationExecutorComponent));
			// Leave unaffordable/capacity-blocked threats queued, without consuming cooldown.
			if (!executor || !executor.CanFundQRFThreat_S(role, best.m_iDesiredTier, best.m_bCaptured, best.m_sLocationId))
			{
				if (m_iClock >= best.m_iNextWaitLog)
				{
					best.m_iNextWaitLog = m_iClock + 30;
					string why = "no operation executor";
					if (executor)
						why = executor.ThreatWaitReason_S(role, best.m_iDesiredTier, best.m_bCaptured, best.m_sLocationId);
					Print(string.Format("[AROR STRATEGY] waiting role=%1 target=%2 desiredTier=%3 available=%4 debt=%5: %6", role, best.m_sLocationId, best.m_iDesiredTier, m_QRFFinance.Available(role), m_QRFFinance.Debt(role), why), LogLevel.NORMAL);
				}
				continue;
			}
			string id = RequestOperation_S(role, AROR_EOperationType.QRF, best.m_sLocationId, m_iQRFReaction);
			AROR_Operation created = GetOperation(id);
			if (!created)
			{
				if (m_iClock >= best.m_iNextWaitLog)
				{
					best.m_iNextWaitLog = m_iClock + 30;
					Print(string.Format("[AROR STRATEGY] waiting role=%1 target=%2; scheduler cooldown, active capacity or conflicting target", role, best.m_sLocationId), LogLevel.NORMAL);
				}
				continue;
			}
			if (created.m_iRequestedTier == 0)
			{
				created.m_iRequestedTier = best.m_iDesiredTier;
				created.m_iPoolReadinessLevel = GetReadinessLevel(role);
				created.m_iThreatPriority = best.Score(m_iClock);
				created.m_bCaptureThreatRecorded = best.m_bCaptured;
				created.m_iCaptureReportedAt = best.m_iFirstSeen;
				created.m_iPlanningRetries = best.m_iPlanningRetries;
				created.m_sRejectedOrigins = best.m_sRejectedOrigins;
				Print(string.Format("[AROR STRATEGY] admitted %1 readiness=%2 desiredTier=%3 priority=%4", id, GetReadinessLevel(role), created.m_iRequestedTier, created.m_iThreatPriority), LogLevel.NORMAL);
			}
			m_aQRFThreats.RemoveItem(best);
		}
	}
	protected bool IsCaptureThreatRelevant(AROR_QRFThreat threat)
	{
		if (!threat.m_bCaptured)
			return true;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return false;
		AROR_LocationComponent location = locations.GetLocationById(threat.m_sLocationId);
		return location && location.GetControllingRole() != threat.m_eRole;
	}
	[Attribute("0", desc: "Allow physical Invader attacks and QRF requests. Shared default is off; opt in per scenario after configuring role assets and land origins.")]
	protected bool m_bInvaderOperationsEnabled;
	[Attribute("0", desc: "Request a QRF for the previous AI owner when a town/city changes to a hostile role during the campaign. Existing Invader gate, cooldowns and capacity still apply.")]
	protected bool m_bTownCaptureQRFs;
	[Attribute("0", desc: "Request a QRF for the previous AI owner after a military site (outpost, base, airport, factory, port, heliport or resources) is captured. Role gates, money, cooldowns and capacity still apply.")]
	protected bool m_bOutpostCaptureQRFs;
	[Attribute("0", desc: "Allow hostile damage/combat reports to request QRFs. Disabled means automatic responses are driven by enabled location captures instead.")]
	protected bool m_bCombatQRFs;
	protected ref AROR_QRFCaptureObserver m_CaptureObserver;

	bool AreInvaderOperationsEnabled()
	{
		return m_bInvaderOperationsEnabled;
	}
	int GetClockSeconds()
	{
		return m_iClock;
	}
	[Attribute("", desc: "Optional Occupier scheduled attack target: stable location ID. Empty disables automatic attacks for this role.")]
	protected string m_sOccupiersAttackTargetId;
	[Attribute("", desc: "Optional Invader scheduled attack target: stable location ID. Empty disables automatic attacks for this role.")]
	protected string m_sInvadersAttackTargetId;
	[Attribute("300", desc: "Minimum seconds between scheduled attacks per role", params: "1 86400 1")]
	protected int m_iAttackInterval;
	[Attribute("60", desc: "Preparation delay in seconds for attacks", params: "0 3600 1")]
	protected int m_iAttackPreparation;
	[Attribute("10", desc: "Reaction delay in seconds for QRFs", params: "0 3600 1")]
	protected int m_iQRFReaction;
	[Attribute("120", desc: "Minimum seconds between accepted QRF requests per role", params: "1 86400 1")]
	protected int m_iQRFCooldown;
	[Attribute("1", desc: "Maximum outstanding attacks per role, including planned requests; also capped at one attack globally", params: "1 16 1")]
	protected int m_iMaxAttacks;
	[Attribute("1", desc: "Separate maximum outstanding QRFs per role", params: "1 16 1")]
	protected int m_iMaxQRFs;
	[Attribute("30", desc: "Seconds allowed for executor spawn acknowledgement", params: "1 3600 1")]
	protected int m_iSpawnTimeout;
	[Attribute("900", desc: "Seconds allowed for an active operation before failure", params: "1 86400 1")]
	protected int m_iActiveTimeout;
	[Attribute("32", desc: "Maximum terminal operation records retained for inspection", params: "0 256 1")]
	protected int m_iHistoryLimit;

	protected static AROR_OperationManagerComponent s_Instance;
	protected AROR_CommanderManagerComponent m_Commanders;
	protected ref array<ref AROR_Operation> m_aOperations = {};
	protected ref AROR_OnOperationChangedInvoker m_OnOperationChanged = new AROR_OnOperationChangedInvoker();
	protected ref array<ref AROR_Operation> m_aPendingNotifications = {};
	protected bool m_bNotifying;
	protected int m_iClock;
	protected int m_iNextId;
	protected int m_iOccupiersNextAttack;
	protected int m_iInvadersNextAttack;
	protected int m_iOccupiersNextQRF;
	protected int m_iInvadersNextQRF;
	protected ref array<ref AROR_CombatContact> m_aCombatContacts = {};
	protected int m_iContactSequence;

	//! Trusted server combat integration; carries snapshots, never live attacker references.
	//! evidence: false for intel snapshots (recon, local reports) that must not escalate Crackdown
	void ReportCombatContact_S(AROR_EFactionRole role, string locationId, vector position, bool evidence = true)
	{
		if (!IsReady() || !IsEnemyRole(role) || !HasLocation(locationId))
			return;
		AROR_CombatContact contact;
		foreach (AROR_CombatContact existing : m_aCombatContacts)
		{
			if (existing.m_eRole == role && existing.m_sLocationId == locationId)
				contact = existing;
		}
		if (contact && m_iClock - contact.m_iTime < 5)
			return;
		if (!contact)
		{
			if (m_aCombatContacts.Count() >= 32)
				m_aCombatContacts.Remove(0);
			contact = new AROR_CombatContact();
			m_aCombatContacts.Insert(contact);
		}
		contact.m_eRole = role;
		contact.m_sLocationId = locationId;
		contact.m_vPosition = position;
		contact.m_iTime = m_iClock;
		m_iContactSequence++;
		contact.m_iSequence = m_iContactSequence;
		if (evidence)
			RecordStrategicEvidence(role);
	}

	AROR_CombatContact GetRecentCombatContact(AROR_EFactionRole role, vector origin, float radius, int maxAge)
	{
		if (!IsReady())
			return null;
		AROR_CombatContact best;
		foreach (AROR_CombatContact contact : m_aCombatContacts)
		{
			if (contact.m_eRole != role || m_iClock - contact.m_iTime > maxAge)
				continue;
			if (vector.DistanceXZ(contact.m_vPosition, origin) > radius)
				continue;
			if (!best || contact.m_iSequence > best.m_iSequence)
				best = contact;
		}
		return best;
	}

	static AROR_OperationManagerComponent GetInstance()
	{
		return s_Instance;
	}
	AROR_OnOperationChangedInvoker GetOnOperationChanged()
	{
		return m_OnOperationChanged;
	}

	AROR_Operation GetOperation(string id)
	{
		foreach (AROR_Operation operation : m_aOperations)
		{
			if (operation.GetId() == id)
				return operation;
		}
		return null;
	}

	int GetOperations(notnull array<AROR_Operation> outOperations)
	{
		outOperations.Clear();
		foreach (AROR_Operation operation : m_aOperations)
			outOperations.Insert(operation);
		return outOperations.Count();
	}

	//! Trusted server call. Target hostility must be checked by the territory/strategy caller.
	//! Returns the existing ID for an identical outstanding request, or empty on rejection.
	string ScheduleAttack_S(AROR_EFactionRole role, string targetId, int delaySeconds = -1)
	{
		if (delaySeconds == -1)
			delaySeconds = m_iAttackPreparation;
		return RequestOperation_S(role, AROR_EOperationType.ATTACK, targetId, delaySeconds);
	}

	//! Trusted combat/territory integration: defendingRole owns the attacked location.
	//! No client RPC: callers must establish ownership and a real hostile attack before reporting.
	string ReportAttack_S(AROR_EFactionRole defendingRole, string locationId)
	{
		if (!m_bCombatQRFs)
			return string.Empty;
		if (m_bQRFStrategicEconomy)
			return QueueQRFThreat_S(defendingRole, locationId);
		return RequestOperation_S(defendingRole, AROR_EOperationType.QRF, locationId, m_iQRFReaction);
	}

	//! Trusted canonical control-change adapter. Town capture gives no enemy-position intel.
	string ReportTownCapture_S(AROR_EFactionRole previousRole, string locationId)
	{
		return ReportLocationCapture_S(previousRole, locationId);
	}

	//! Canonical territory-event adapter; separate settlement/outpost opt-ins, no live attacker lookup.
	string ReportLocationCapture_S(AROR_EFactionRole previousRole, string locationId)
	{
		if (!IsReady() || !IsEnemyRole(previousRole))
			return string.Empty;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return string.Empty;
		AROR_LocationComponent location = locations.GetLocationById(locationId);
		if (!location || location.GetControllingRole() == previousRole)
			return string.Empty;
		AROR_ELocationType type = location.GetLocationType();
		bool enabled;
		if (type == AROR_ELocationType.TOWN || type == AROR_ELocationType.CITY)
			enabled = m_bTownCaptureQRFs;
		else if (AROR_CaptureLocationAction.IsCapturableType(type))
			enabled = m_bOutpostCaptureQRFs;
		if (!enabled)
			return string.Empty;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return string.Empty;
		Faction previousFaction = roles.GetFaction(previousRole);
		Faction currentFaction = roles.GetFaction(location.GetControllingRole());
		if (!previousFaction || !currentFaction || !previousFaction.IsFactionEnemy(currentFaction))
			return string.Empty;
		// Territorial pressure also works when physical Invader operation admission is disabled.
		// Only player-faction gains provoke this path; simulated AI battles do not escalate themselves.
		if (previousRole == AROR_EFactionRole.INVADERS && !m_bInvaderOperationsEnabled)
		{
			AROR_InvaderCampaignComponent campaign = AROR_InvaderCampaignComponent.GetInstance();
			if (campaign && campaign.IsAdvanceActive() && location.GetControllingRole() == AROR_EFactionRole.RESISTANCE)
			{
				RecordStrategicEvidence(previousRole);
				RecordCaptureEscalation_S(previousRole);
			}
			return string.Empty;
		}
		string operationId;
		if (m_bQRFStrategicEconomy)
			operationId = QueueQRFThreat_S(previousRole, locationId, true);
		else
			operationId = RequestOperation_S(previousRole, AROR_EOperationType.QRF, locationId, m_iQRFReaction);
		AROR_Operation captureResponse = GetOperation(operationId);
		if (captureResponse && !captureResponse.m_bCaptureThreatRecorded)
		{
			captureResponse.m_bCaptureThreatRecorded = true;
			captureResponse.m_iCaptureReportedAt = m_iClock;
		}
		Print(string.Format("[AROR] Location capture response: target=%1 previousRole=%2 currentRole=%3 QRF=%4 queued=%5", locationId, previousRole, location.GetControllingRole(), operationId, HasPendingQRFThreat(previousRole, locationId)), LogLevel.NORMAL);
		return operationId;
	}

	//! Trusted damage-report adapter: snapshot the attributed attacker at report time only.
	//! Existing requests keep their force, deadline and cooldown; only pre-search intel changes.
	string ReportAttackAt_S(AROR_EFactionRole defendingRole, string locationId, vector reportedPosition)
	{
		// Local defensive reactions and recent-contact safety do not dispatch QRFs.
		// The opt-in below controls admission only, not trusted combat observation.
		ReportCombatContact_S(defendingRole, locationId, reportedPosition);
		if (!m_bCombatQRFs)
			return string.Empty;
		string id = ReportAttack_S(defendingRole, locationId);
		AROR_Operation operation = GetOperation(id);
		if (operation && !operation.IsTerminal() && operation.GetQRFSearchPlan())
			operation.GetQRFSearchPlan().ReportContact_S(reportedPosition, m_iClock);
		return id;
	}

	//! Freeze the shared plan once, using the scheduler clock for contact expiry.
	AROR_QRFSearchPlan PrepareQRFSearch_S(string operationId, int maxContactAge)
	{
		AROR_Operation operation = GetOperation(operationId);
		if (!IsReady() || !operation || operation.GetState() != AROR_EOperationState.ACTIVE || maxContactAge < 1 || !operation.GetQRFSearchPlan())
			return null;
		operation.GetQRFSearchPlan().BeginSearch_S(m_iClock, maxContactAge);
		return operation.GetQRFSearchPlan();
	}

	//! Executor acknowledges that its force exists and has received its orders.
	bool MarkActive_S(string operationId)
	{
		AROR_Operation operation = GetOperation(operationId);
		if (!operation || operation.GetState() != AROR_EOperationState.SPAWNING || !IsReady())
			return false;
		m_QRFFinance.Activate_S(operation.m_Funding);
		return ChangeState_S(operation, AROR_EOperationState.ACTIVE, string.Empty);
	}

	//! Executor/territory integration reports a terminal outcome; never changes ownership itself.
	bool ResolveOperation_S(string operationId, AROR_EOperationState result, string reason)
	{
		if (result != AROR_EOperationState.SUCCEEDED && result != AROR_EOperationState.FAILED && result != AROR_EOperationState.CANCELLED)
			return false;
		return ChangeState_S(GetOperation(operationId), result, reason);
	}
	//! Only an explicit pre-activation route/launch failure may retry a capture.
	//! Never re-escalate readiness pressure or bypass the normal request cooldown.
	bool ReportQRFPlanningFailure_S(string operationId, string reason)
	{
		if (!IsReady())
			return false;
		AROR_Operation operation = GetOperation(operationId);
		if (!operation || operation.GetType() != AROR_EOperationType.QRF || operation.GetState() != AROR_EOperationState.SPAWNING)
			return false;
		if (!ResolveOperation_S(operationId, AROR_EOperationState.FAILED, reason))
			return false;
		if (!m_bQRFStrategicEconomy || !operation.m_bCaptureThreatRecorded)
			return true;
		if (operation.m_iPlanningRetries >= Math.Clamp(m_iCapturePlanningRetries, 0, 3) || m_iClock - operation.m_iCaptureReportedAt >= 600)
			return true;
		if (m_aQRFThreats.Count() >= 32 || HasPendingQRFThreat(operation.GetRole(), operation.GetTargetLocationId()))
			return true;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent target;
		if (locations)
			target = locations.GetLocationById(operation.GetTargetLocationId());
		if (!target || target.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
			return true;
		AROR_QRFThreat retry = new AROR_QRFThreat();
		retry.m_eRole = operation.GetRole();
		retry.m_sLocationId = operation.GetTargetLocationId();
		retry.m_bCaptured = true;
		retry.m_iFirstSeen = operation.m_iCaptureReportedAt;
		retry.m_iLastSeen = operation.m_iCaptureReportedAt;
		retry.m_iReports = 1;
		retry.m_iImportance = AROR_QRFStrategicPolicy.Importance(target.GetLocationType());
		retry.m_iPlanningRetries = operation.m_iPlanningRetries + 1;
		retry.m_iNextAttempt = Math.Max(m_iClock + 15, GetNextRequestTime(operation.GetRole(), AROR_EOperationType.QRF));
		retry.m_sRejectedOrigins = operation.m_sRejectedOrigins;
		if (operation.m_TacticalCommander && !operation.m_TacticalCommander.m_sOriginLocationId.IsEmpty())
			retry.m_sRejectedOrigins += "|" + operation.m_TacticalCommander.m_sOriginLocationId + "|";
		m_aQRFThreats.Insert(retry);
		Print(string.Format("[AROR STRATEGY] capture planning retry=%1 target=%2 earliest=%3; original age and cooldown retained", retry.m_iPlanningRetries, retry.m_sLocationId, retry.m_iNextAttempt), LogLevel.WARNING);
		return true;
	}

	protected bool IsReady()
	{
		return Replication.IsServer() && s_Instance == this && m_Commanders && m_Commanders.IsCampaignActive();
	}

	protected bool IsEnemyRole(AROR_EFactionRole role)
	{
		return role == AROR_EFactionRole.OCCUPIERS || role == AROR_EFactionRole.INVADERS;
	}

	protected bool HasLocation(string id)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		return !id.IsEmpty() && locations && locations.GetLocationById(id);
	}

	protected int GetNextRequestTime(AROR_EFactionRole role, AROR_EOperationType type)
	{
		if (type == AROR_EOperationType.ATTACK)
		{
			if (role == AROR_EFactionRole.OCCUPIERS)
				return m_iOccupiersNextAttack;
			return m_iInvadersNextAttack;
		}
		if (role == AROR_EFactionRole.OCCUPIERS)
			return m_iOccupiersNextQRF;
		return m_iInvadersNextQRF;
	}

	protected string RequestOperation_S(AROR_EFactionRole role, AROR_EOperationType type, string targetId, int delaySeconds, bool debugRequest = false)
	{
		// Unsupported enum values would never enter either scheduler processing pass.
		if (type != AROR_EOperationType.ATTACK && type != AROR_EOperationType.QRF)
			return string.Empty;
		if (role == AROR_EFactionRole.INVADERS && !m_bInvaderOperationsEnabled)
			return string.Empty;
		if (!IsReady() || !IsEnemyRole(role) || delaySeconds < 0 || delaySeconds > 86400 || !HasLocation(targetId))
			return string.Empty;

		int count;
		bool attackOutstanding;
		foreach (AROR_Operation operation : m_aOperations)
		{
			if (!operation.IsTerminal() && operation.GetType() == AROR_EOperationType.ATTACK)
				attackOutstanding = true;
			if (operation.IsTerminal() || operation.GetRole() != role)
				continue;
			if (operation.GetTargetLocationId() == targetId)
			{
				if (operation.GetType() == type)
					return operation.GetId();
				return string.Empty;
			}
			if (operation.GetType() == type)
				count++;
		}
		int limit = m_iMaxAttacks;
		if (type == AROR_EOperationType.ATTACK && attackOutstanding)
			return string.Empty;
		if (type == AROR_EOperationType.QRF)
			limit = m_iMaxQRFs;
		if (count >= limit || (!debugRequest && m_iClock < GetNextRequestTime(role, type)))
			return string.Empty;

		m_iNextId++;
		string id = string.Format("operation_%1", m_iNextId);
		AROR_Operation created = new AROR_Operation(id, role, type, targetId, m_iClock, delaySeconds);
		created.m_bDebugRequest = debugRequest;
		m_aOperations.Insert(created);
		// Test requests (MARS) never move the real campaign timers
		if (!debugRequest && type == AROR_EOperationType.ATTACK)
		{
			if (role == AROR_EFactionRole.OCCUPIERS)
				m_iOccupiersNextAttack = m_iClock + m_iAttackInterval;
			else
				m_iInvadersNextAttack = m_iClock + m_iAttackInterval;
		}
		else if (!debugRequest)
		{
			if (role == AROR_EFactionRole.OCCUPIERS)
				m_iOccupiersNextQRF = m_iClock + m_iQRFCooldown;
			else
				m_iInvadersNextQRF = m_iClock + m_iQRFCooldown;
		}
		NotifyChanged(created);
		return id;
	}

	protected bool ChangeState_S(AROR_Operation operation, AROR_EOperationState state, string reason)
	{
		if (!Replication.IsServer() || s_Instance != this || !operation || !operation.Transition_S(state, m_iClock, reason))
			return false;
		if (operation.IsTerminal() && operation.m_Funding)
		{
			m_QRFFinance.Observe_S(operation.m_Funding);
			operation.m_Funding.m_bClosed = true;
		}
		if (state == AROR_EOperationState.ACTIVE && operation.m_bResponseMixTracked)
			RecordQRFDispatchMix_S(operation);
		NotifyChanged(operation);
		return true;
	}

	protected void NotifyChanged(AROR_Operation operation)
	{
		Print(string.Format("[AROR] Operation %1 role=%2 type=%3 target=%4 state=%5: %6", operation.GetId(), operation.GetRole(), operation.GetType(), operation.GetTargetLocationId(), operation.GetState(), operation.GetResult()), LogLevel.NORMAL);
		m_aPendingNotifications.Insert(operation);
		// An executor may resolve an operation inside its dispatch callback. ScriptInvoker
		// forbids recursive Invoke; deliver nested changes after the current invocation returns.
		if (m_bNotifying)
			return;
		m_bNotifying = true;
		while (m_aPendingNotifications.Count() > 0)
		{
			AROR_Operation next = m_aPendingNotifications[0];
			m_aPendingNotifications.Remove(0);
			m_OnOperationChanged.Invoke(next);
		}
		m_bNotifying = false;
	}

	protected void OnCommanderEvaluation(AROR_Commander commander)
	{
		if (!IsReady() || !commander || commander.IsPlayerDirected())
			return;
		string targetId;
		if (commander.GetRole() == AROR_EFactionRole.OCCUPIERS)
			targetId = m_sOccupiersAttackTargetId;
		else if (commander.GetRole() == AROR_EFactionRole.INVADERS)
			targetId = m_sInvadersAttackTargetId;
		if (!targetId.IsEmpty())
			ScheduleAttack_S(commander.GetRole(), targetId);
	}

	protected int ActiveTimeoutFor(AROR_Operation operation)
	{
		// The allowance runs from the estimated arrival, so a long drive does not eat the fight
		int travel = Math.Max(0, operation.m_iEtaSeconds);
		if (operation.UsesTacticalResponse() && operation.m_TacticalCommander && operation.m_TacticalCommander.m_iTownHoldSeconds > 0)
			return Math.Max(m_iActiveTimeout, operation.m_TacticalCommander.m_iTownHoldSeconds + 1800) + travel;
		return m_iActiveTimeout + travel;
	}

	protected void ProcessOperations(AROR_EOperationType type)
	{
		// Snapshot allows synchronous executor callbacks to schedule or resolve other requests safely.
		array<AROR_Operation> snapshot = {};
		GetOperations(snapshot);
		foreach (AROR_Operation operation : snapshot)
		{
			if (operation.IsTerminal() || operation.GetType() != type)
				continue;
			if (operation.GetType() == AROR_EOperationType.ATTACK || operation.m_bDebugRequest)
			{
				AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
				AROR_LocationComponent target;
				if (locations)
					target = locations.GetLocationById(operation.GetTargetLocationId());
				if (!target || target.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
				{
					if (target && target.GetControllingRole() == operation.GetRole() && operation.GetType() == AROR_EOperationType.ATTACK && operation.GetState() == AROR_EOperationState.ACTIVE)
						ChangeState_S(operation, AROR_EOperationState.SUCCEEDED, "Target captured through canonical territory rules");
					else
						ChangeState_S(operation, AROR_EOperationState.CANCELLED, "Target is no longer Rebel-owned territory");
					continue;
				}
			}
			if (!HasLocation(operation.GetTargetLocationId()))
				ChangeState_S(operation, AROR_EOperationState.CANCELLED, "Target location no longer registered");
			else if (operation.GetState() == AROR_EOperationState.PLANNED && m_iClock >= operation.GetDueTime())
				ChangeState_S(operation, AROR_EOperationState.SPAWNING, "Executor requested");
			else if (operation.GetState() == AROR_EOperationState.SPAWNING && m_iClock - operation.GetStateSince() >= Math.Max(m_iSpawnTimeout, operation.m_iSpawnPreparationTimeout))
				ChangeState_S(operation, AROR_EOperationState.FAILED, "Spawn acknowledgement timed out");
			else if (operation.GetState() == AROR_EOperationState.ACTIVE && m_iClock - operation.GetStateSince() >= ActiveTimeoutFor(operation))
				ChangeState_S(operation, AROR_EOperationState.FAILED, "Operation timed out");
		}
	}

	//! Evidence/losses change short-term crackdown; equipment progress never decays here.
	protected void UpdateCrackdown_S()
	{
		if (!Replication.IsServer() || !m_bQRFStrategicEconomy)
			return;
		if (m_QRFFinance.m_iOccupierLossValue > m_iAppliedOccupierLoss)
		{
			int occupierLoss = m_QRFFinance.m_iOccupierLossValue - m_iAppliedOccupierLoss;
			SetCrackdown_S(AROR_EFactionRole.OCCUPIERS, GetCrackdown(AROR_EFactionRole.OCCUPIERS) + AROR_QRFStrategicPolicy.LossCrackdown(occupierLoss));
			m_iAppliedOccupierLoss = m_QRFFinance.m_iOccupierLossValue;
			m_iOccupierLastThreat = m_iClock;
		}
		if (m_QRFFinance.m_iInvaderLossValue > m_iAppliedInvaderLoss)
		{
			int invaderLoss = m_QRFFinance.m_iInvaderLossValue - m_iAppliedInvaderLoss;
			SetCrackdown_S(AROR_EFactionRole.INVADERS, GetCrackdown(AROR_EFactionRole.INVADERS) + AROR_QRFStrategicPolicy.LossCrackdown(invaderLoss));
			m_iAppliedInvaderLoss = m_QRFFinance.m_iInvaderLossValue;
			m_iInvaderLastThreat = m_iClock;
		}
		if (m_iClock >= m_iNextCrackdownDecay)
		{
			m_iNextCrackdownDecay = m_iClock + 60;
			if (m_iClock - m_iOccupierLastThreat >= m_iCrackdownQuietTime)
				SetCrackdown_S(AROR_EFactionRole.OCCUPIERS, GetCrackdown(AROR_EFactionRole.OCCUPIERS) - m_iCrackdownDecay);
			if (m_iClock - m_iInvaderLastThreat >= m_iCrackdownQuietTime)
				SetCrackdown_S(AROR_EFactionRole.INVADERS, GetCrackdown(AROR_EFactionRole.INVADERS) - m_iCrackdownDecay);
		}
	}

	protected void Tick()
	{
		if (!Replication.IsServer() || s_Instance != this)
			return;
		if (!m_Commanders)
		{
			m_Commanders = AROR_CommanderManagerComponent.GetInstance();
			if (m_Commanders)
				m_Commanders.GetOnCommanderEvaluation().Insert(OnCommanderEvaluation);
		}
		if (!IsReady())
			return;
		if ((m_bTownCaptureQRFs || m_bOutpostCaptureQRFs) && !m_CaptureObserver)
		{
			// Subscribe after campaign restoration/setup; initial ownership is a baseline.
			AROR_QRFCaptureObserver observer = new AROR_QRFCaptureObserver();
			if (observer.Initialize_S(this))
				m_CaptureObserver = observer;
		}
		m_iClock++;
		if (m_bQRFSpotting || m_bQRFDebugMarkers)
		{
			AROR_OperationExecutorComponent executor = AROR_OperationExecutorComponent.Cast(GetOwner().FindComponent(AROR_OperationExecutorComponent));
			m_QRFSpotting.Tick_S(executor, m_iClock, m_fQRFSpotGroundRange, m_fQRFSpotAirRange, m_fQRFSpotMinimumDot, m_iQRFSpotTraceLimit, m_iQRFSpotLifetime, m_bQRFDebugMarkers);
		}
		if (m_iClock >= m_iNextStrategyTick)
		{
			m_iNextStrategyTick = m_iClock + 5;
			m_QRFFinance.Tick_S();
			UpdateCrackdown_S();
			if (m_bQRFStrategicEconomy)
				ProcessQRFThreats();
		}
		ProcessOperations(AROR_EOperationType.QRF);
		ProcessOperations(AROR_EOperationType.ATTACK);

		int historyCount;
		for (int i = m_aOperations.Count() - 1; i >= 0; i--)
		{
			if (!m_aOperations[i].IsTerminal())
				continue;
			historyCount++;
			if (historyCount > m_iHistoryLimit)
				m_aOperations.Remove(i);
		}
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;
		if (s_Instance || m_iAttackInterval < 1 || m_iAttackPreparation < 0 || m_iQRFReaction < 0 || m_iQRFCooldown < 1 || m_iMaxAttacks < 1 || m_iMaxQRFs < 1 || m_iSpawnTimeout < 1 || m_iActiveTimeout < 1 || m_iHistoryLimit < 0)
		{
			Print("[AROR] Operation manager duplicate or invalid configuration; disabled", LogLevel.ERROR);
			return;
		}
		s_Instance = this;
		bool strategyValid = m_iStartingReadinessLevel >= 1 && m_iStartingReadinessLevel <= 5 && m_iAirborneQRFShare >= 0 && m_iAirborneQRFShare <= 100;
		if (m_iStartingCrackdown < 0 || m_iStartingCrackdown > 100)
			strategyValid = false;
		if (m_iCaptureReadinessPressure < 0 || m_iCaptureReadinessPressure > 100 || m_iCaptureCrackdown < 0 || m_iCaptureCrackdown > 1000)
			strategyValid = false;
		if (m_iCrackdownQuietTime < 60 || m_iCrackdownQuietTime > 3600 || m_iCrackdownDecay < 0 || m_iCrackdownDecay > 1000)
			strategyValid = false;
		if (m_iQRFBaseOverhead < 0 || m_iQRFBaseOverhead > 100000 || m_iQRFTierOverhead < 0 || m_iQRFTierOverhead > 10000)
			strategyValid = false;
		if (m_iQRFThreatLifetime < 30 || m_iQRFThreatLifetime > 900)
			strategyValid = false;
		if (m_iQRFSpotTraceLimit < 1 || m_iQRFSpotTraceLimit > 16 || m_iQRFSpotLifetime < 15 || m_iQRFSpotLifetime > 300)
			strategyValid = false;
		if (m_fQRFSpotGroundRange < 100 || m_fQRFSpotGroundRange > 2000 || m_fQRFSpotAirRange < 100 || m_fQRFSpotAirRange > 3000)
			strategyValid = false;
		if (m_fQRFSpotMinimumDot < 0.5 || m_fQRFSpotMinimumDot > 0.99)
			strategyValid = false;
		if (!strategyValid)
		{
			Print("[AROR] Invalid QRF strategy settings; scheduler disabled", LogLevel.ERROR);
			s_Instance = null;
			return;
		}
		if (!m_bStrategyRestored)
		{
			m_iOccupierPressure = AROR_QRFStrategicPolicy.PressureForLevel(m_iStartingReadinessLevel);
			m_iInvaderPressure = m_iOccupierPressure;
			m_iOccupierCrackdown = m_iStartingCrackdown * 10;
			m_iInvaderCrackdown = m_iOccupierCrackdown;
		}
		m_bThreatStateReady = true;
		Replication.BumpMe();
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);
		Print("[AROR] Operation scheduler initialised for Occupiers and Invaders", LogLevel.NORMAL);
		Print("[AROR] Operation notifications: nested changes queued (recursive Invoke protection enabled)", LogLevel.NORMAL);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Tick);
		if (m_CaptureObserver)
			m_CaptureObserver.Detach_S();
		m_CaptureObserver = null;
		if (m_Commanders)
			m_Commanders.GetOnCommanderEvaluation().Remove(OnCommanderEvaluation);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}
}
