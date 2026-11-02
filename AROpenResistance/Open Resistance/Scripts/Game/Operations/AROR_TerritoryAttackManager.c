//! Territory policy only. Existing operations/executor/tactical commander own the physical attack.
//! The Occupiers attack Resistance-held military territory as soon as the Resistance holds any: the front site
//! nearest an Occupier base first, after a short per-site capture grace, with a rest between their attacks. The
//! Invaders' southward push (AROR_InvaderCampaignComponent) launches its physical attacks through LaunchTerritoryAttack_S.
modded class AROR_OperationManagerComponent
{
	[Attribute("0", desc: "Schedule Occupier attacks on Resistance-held military territory. Settlements remain support-only and HQ is excluded.")]
	protected bool m_bTerritoryAttacksEnabled;
	[Attribute("300", desc: "Minimum seconds from capture (or campaign load) before a site can be attacked; the 5-minute flip lock blocks a recapture before then anyway", params: "0 7200 30")]
	protected int m_iTerritoryAttackGrace;
	[Attribute("300", desc: "Wait after a capture QRF on the site ends before attacking it. A site whose capture QRF is still deployed is not attacked; a site without one does not wait.", params: "0 3600 30")]
	protected int m_iTerritoryAttackAfterQRF;
	[Attribute("900", desc: "Occupier recovery time after one of their attacks ends, before the next", params: "60 7200 30")]
	protected int m_iTerritoryAttackRest;
	protected ref array<ref AROR_TerritoryAttackState> m_aAttackTerritories = {};
	protected AROR_LocationManagerComponent m_AttackLocations;
	protected int m_iAttackRestUntil;
	protected int m_iNextTerritoryAttackTick;
	protected string m_sAttackHoldKey;	//!< Last logged reason no attack started; logged again only when it changes
	protected string m_sAttackFailure;	//!< Why the last LaunchTerritoryAttack_S refused, for its caller

	override protected void Tick()
	{
		super.Tick();
		if (!Replication.IsServer() || !m_bTerritoryAttacksEnabled || !IsReady())
			return;
		if (!m_AttackLocations)
		{
			m_AttackLocations = AROR_LocationManagerComponent.GetInstance();
			if (!m_AttackLocations)
				return;
			array<AROR_LocationComponent> all = {};
			m_AttackLocations.GetLocations(all);
			foreach (AROR_LocationComponent location : all)
				OnAttackTerritoryChanged(location);
			m_AttackLocations.GetOnLocationControlChanged().Insert(OnAttackTerritoryChanged);
			m_AttackLocations.GetOnLocationRegistered().Insert(OnAttackTerritoryChanged);
			m_AttackLocations.GetOnLocationUnregistered().Insert(OnAttackTerritoryRemoved);
		}
		if (m_iClock < m_iNextTerritoryAttackTick)
			return;
		m_iNextTerritoryAttackTick = m_iClock + 15;
		if (m_iClock < m_iAttackRestUntil)
		{
			HoldAttacks("rest", string.Format("resting after the last attack for another %1s", m_iAttackRestUntil - m_iClock));
			return;
		}
		// One attack at a time across the map, whichever army runs it
		AROR_Operation busy = OutstandingAttack();
		if (busy)
		{
			HoldAttacks("busy:" + busy.GetId(), string.Format("waiting for attack %1 (role=%2 target=%3 state=%4) to end", busy.GetId(), busy.GetRole(), busy.GetTargetLocationId(), busy.GetState()));
			return;
		}
		// The Occupiers' own pending capture responses take precedence over their attacks
		int threats = QueuedThreats(AROR_EFactionRole.OCCUPIERS);
		if (threats > 0)
		{
			HoldAttacks("threats", string.Format("%1 queued Occupier capture response(s) take priority", threats));
			return;
		}
		AROR_OperationExecutorComponent executor = AROR_OperationExecutorComponent.Cast(GetOwner().FindComponent(AROR_OperationExecutorComponent));
		if (!executor)
		{
			HoldAttacks("executor", "no operation executor on the game mode");
			return;
		}
		AROR_TerritoryAttackState selected;
		float selectedFront;
		string waitKey;
		string waiting;
		int reported;
		foreach (AROR_TerritoryAttackState state : m_aAttackTerritories)
		{
			AROR_LocationComponent target = m_AttackLocations.GetLocationById(state.m_sLocationId);
			if (!IsAttackTerritory(target, AROR_EFactionRole.OCCUPIERS))
				continue;
			string reason = AttackWaitReason(state);
			string category = reason;
			if (reason.IsEmpty() && !executor.HasMarsOrigin_S(AROR_EFactionRole.OCCUPIERS, state.m_sLocationId))
			{
				reason = "no safe Occupier launch site";
				category = reason;
			}
			if (!reason.IsEmpty())
			{
				// Key on the kind of wait only, so a countdown alone does not log every 15 seconds
				int colon = category.IndexOf(":");
				if (colon >= 0)
					category = category.Substring(0, colon);
				waitKey += state.m_sLocationId + "=" + category + ";";
				if (reported < 4)
					waiting += state.m_sLocationId + " " + reason + "; ";
				reported++;
				continue;
			}
			// The front first: the held site closest to an Occupier base
			float front = FrontDistance(target, AROR_EFactionRole.OCCUPIERS);
			if (!selected || front < selectedFront || (front == selectedFront && state.m_sLocationId.Compare(selected.m_sLocationId) < 0))
			{
				selected = state;
				selectedFront = front;
			}
		}
		if (!selected)
		{
			if (reported == 0)
				HoldAttacks("none", "no Resistance-held military territory to attack");
			else
				HoldAttacks("sites:" + waitKey, string.Format("no site ready (%1 held): %2", reported, waiting));
			return;
		}
		selected.m_iNextAttempt = m_iClock + 30;
		string id = LaunchTerritoryAttack_S(AROR_EFactionRole.OCCUPIERS, selected.m_sLocationId);
		if (id.IsEmpty())
		{
			HoldAttacks(m_sAttackFailure, m_sAttackFailure);
			return;
		}
		m_sAttackHoldKey = string.Empty;
		selected.m_iNextAttempt = m_iClock + Math.Max(60, m_iTerritoryAttackRest);
		Print(string.Format("[AROR ATTACK] %1 target=%2 captureAge=%3 front=%4m", id, selected.m_sLocationId, m_iClock - selected.m_iCapturedAt, Math.Round(selectedFront)), LogLevel.NORMAL);
	}

	//! Trusted server call shared by both armies' attack policies: check the launch site and funding, schedule the
	//! attack and freeze its metrics. Returns the operation id (an existing one when the same attack is already
	//! outstanding), or empty with the reason in GetAttackFailure().
	string LaunchTerritoryAttack_S(AROR_EFactionRole role, string locationId)
	{
		m_sAttackFailure = string.Empty;
		if (!IsReady() || !IsEnemyRole(role))
		{
			m_sAttackFailure = "operations are not ready";
			return string.Empty;
		}
		if (role == AROR_EFactionRole.INVADERS && !m_bInvaderOperationsEnabled)
		{
			m_sAttackFailure = "physical Invader operations are disabled in this scenario (m_bInvaderOperationsEnabled)";
			return string.Empty;
		}
		foreach (AROR_Operation existing : m_aOperations)
		{
			if (!existing.IsTerminal() && existing.GetType() == AROR_EOperationType.ATTACK && existing.GetRole() == role && existing.GetTargetLocationId() == locationId)
				return existing.GetId();
		}
		AROR_OperationExecutorComponent executor = AROR_OperationExecutorComponent.Cast(GetOwner().FindComponent(AROR_OperationExecutorComponent));
		if (!executor)
		{
			m_sAttackFailure = "no operation executor on the game mode";
			return string.Empty;
		}
		if (!executor.HasMarsOrigin_S(role, locationId))
		{
			m_sAttackFailure = string.Format("no safe launch site for role %1 against %2", role, locationId);
			return string.Empty;
		}
		int tier = GetCrackdownLevel(role);
		if (m_bQRFStrategicEconomy && !executor.CanFundQRFThreat_S(role, tier, false, locationId))
		{
			m_sAttackFailure = string.Format("role %1 cannot fund a tier %2 attack on %3", role, tier, locationId);
			return string.Empty;
		}
		string id = ScheduleAttack_S(role, locationId);
		AROR_Operation attack = GetOperation(id);
		if (!attack)
		{
			m_sAttackFailure = string.Format("the scheduler refused an attack on %1 (another attack, interval or capacity)", locationId);
			return string.Empty;
		}
		attack.m_iRequestedTier = tier;
		attack.m_iPoolReadinessLevel = GetReadinessLevel(role);
		Print(string.Format("[AROR ATTACK] %1 scheduled role=%2 target=%3 readiness=%4 crackdownTier=%5", id, role, locationId, attack.m_iPoolReadinessLevel, tier), LogLevel.NORMAL);
		return id;
	}

	string GetAttackFailure()
	{
		return m_sAttackFailure;
	}

	//! A Resistance-held military site the role could take. A stronghold returns only to its own army: an Occupier
	//! attack on a Resistance-held Vostok could never succeed.
	bool IsAttackTerritory(AROR_LocationComponent location, AROR_EFactionRole role)
	{
		if (!location || location.GetControllingRole() != AROR_EFactionRole.RESISTANCE || location.IsControlFixed() || !AROR_CaptureLocationAction.IsCapturableType(location.GetLocationType()))
			return false;
		AROR_StrongholdComponent stronghold = AROR_StrongholdComponent.Cast(location.GetOwner().FindComponent(AROR_StrongholdComponent));
		return !stronghold || stronghold.AllowsCaptureBy(role);
	}

	//! Distance from a site to the nearest base the role can launch from
	protected float FrontDistance(AROR_LocationComponent site, AROR_EFactionRole role)
	{
		float nearest = float.MAX;
		array<AROR_LocationComponent> all = {};
		m_AttackLocations.GetLocations(all);
		foreach (AROR_LocationComponent origin : all)
		{
			if (origin && origin.GetControllingRole() == role && AROR_QRFOriginPolicy.IsOriginType(origin.GetLocationType()))
				nearest = Math.Min(nearest, vector.DistanceXZ(site.GetCenter(), origin.GetCenter()));
		}
		return nearest;
	}

	//! Why a held site is not yet due, or empty when it is. Text before any colon names the kind of wait.
	protected string AttackWaitReason(AROR_TerritoryAttackState state)
	{
		if (!state.m_sActiveQRF.IsEmpty())
			return "capture QRF " + state.m_sActiveQRF + " still deployed";
		// The scheduler allows one request per army and target: wait out their own QRF rather than be refused
		foreach (AROR_Operation operation : m_aOperations)
		{
			if (!operation.IsTerminal() && operation.GetRole() == AROR_EFactionRole.OCCUPIERS && operation.GetType() == AROR_EOperationType.QRF && operation.GetTargetLocationId() == state.m_sLocationId)
				return "Occupier QRF " + operation.GetId() + " still responding";
		}
		int captureGrace = state.m_iCapturedAt + Math.Max(0, m_iTerritoryAttackGrace) - m_iClock;
		if (captureGrace > 0)
			return string.Format("capture grace: %1s left", captureGrace);
		if (state.m_iQRFCompletedAt >= 0)
		{
			int afterQRF = state.m_iQRFCompletedAt + Math.Max(0, m_iTerritoryAttackAfterQRF) - m_iClock;
			if (afterQRF > 0)
				return string.Format("recovery after capture QRF: %1s left", afterQRF);
		}
		if (m_iClock < state.m_iNextAttempt)
			return string.Format("retry: %1s left", state.m_iNextAttempt - m_iClock);
		return string.Empty;
	}

	protected void HoldAttacks(string key, string message)
	{
		if (key == m_sAttackHoldKey)
			return;
		m_sAttackHoldKey = key;
		Print("[AROR ATTACK] no attack: " + message, LogLevel.NORMAL);
	}

	protected void OnAttackTerritoryChanged(AROR_LocationComponent location)
	{
		if (!Replication.IsServer() || !location)
			return;
		OnAttackTerritoryRemoved(location);
		if (!IsAttackTerritory(location, AROR_EFactionRole.OCCUPIERS))
			return;
		AROR_TerritoryAttackState state = new AROR_TerritoryAttackState();
		state.m_sLocationId = location.GetId();
		state.m_iCapturedAt = m_iClock;
		state.m_iNextAttempt = m_iClock;
		m_aAttackTerritories.Insert(state);
	}

	protected void OnAttackTerritoryRemoved(AROR_LocationComponent location)
	{
		if (!location)
			return;
		for (int i = m_aAttackTerritories.Count() - 1; i >= 0; i--)
		{
			if (m_aAttackTerritories[i].m_sLocationId == location.GetId())
				m_aAttackTerritories.Remove(i);
		}
	}

	//! The attack still in progress, if any (either army, including test attacks)
	protected AROR_Operation OutstandingAttack()
	{
		foreach (AROR_Operation operation : m_aOperations)
		{
			if (!operation.IsTerminal() && operation.GetType() == AROR_EOperationType.ATTACK)
				return operation;
		}
		return null;
	}

	protected int QueuedThreats(AROR_EFactionRole role)
	{
		int count;
		foreach (AROR_QRFThreat threat : m_aQRFThreats)
		{
			if (threat.m_eRole == role)
				count++;
		}
		return count;
	}

	override protected void NotifyChanged(AROR_Operation operation)
	{
		// Subscribers may synchronously resolve ACTIVE; record this transition first.
		RecordTerritoryOperationChange(operation);
		super.NotifyChanged(operation);
	}

	protected void RecordTerritoryOperationChange(AROR_Operation operation)
	{
		if (!Replication.IsServer() || !m_bTerritoryAttacksEnabled || !operation || operation.m_bDebugRequest)
			return;
		if (operation.GetType() == AROR_EOperationType.ATTACK)
		{
			// Only the Occupiers' own attacks start their rest; the Invaders' push keeps its own cadence
			if (operation.IsTerminal() && operation.GetRole() == AROR_EFactionRole.OCCUPIERS)
				m_iAttackRestUntil = m_iClock + Math.Max(60, m_iTerritoryAttackRest);
			return;
		}
		if (!operation.m_bCaptureThreatRecorded)
			return;
		foreach (AROR_TerritoryAttackState state : m_aAttackTerritories)
		{
			if (state.m_sLocationId != operation.GetTargetLocationId())
				continue;
			if (operation.GetState() == AROR_EOperationState.ACTIVE)
			{
				state.m_sActiveQRF = operation.GetId();
				state.m_iQRFCompletedAt = -1;
			}
			else if (operation.IsTerminal() && state.m_sActiveQRF == operation.GetId())
			{
				state.m_iQRFCompletedAt = m_iClock;
				state.m_sActiveQRF = string.Empty;
			}
		}
	}

	override void OnDelete(IEntity owner)
	{
		if (m_AttackLocations)
		{
			m_AttackLocations.GetOnLocationControlChanged().Remove(OnAttackTerritoryChanged);
			m_AttackLocations.GetOnLocationRegistered().Remove(OnAttackTerritoryChanged);
			m_AttackLocations.GetOnLocationUnregistered().Remove(OnAttackTerritoryRemoved);
		}
		m_AttackLocations = null;
		m_aAttackTerritories.Clear();
		super.OnDelete(owner);
	}
}
