[ComponentEditorProps(category: "OpenResistance/Operations", description: "Reports hostile damage to an explicitly assigned defended location. Place on a defending AI group, character or vehicle, never on an attacking group.")]
class AROR_QRFDefenseComponentClass : ScriptComponentClass
{
}

//! Assignment is supplied by the scenario/garrison owner; this component does not own territory.
class AROR_QRFDefenseComponent : ScriptComponent
{
	[Attribute("1", desc: "Enable damage reporting for the assigned defense. An empty location ID remains inert.")]
	protected bool m_bEnabled;
	[Attribute("1", UIWidgets.ComboBox, desc: "Role defending this location", enums: ParamEnumArray.FromEnum(AROR_EFactionRole))]
	protected AROR_EFactionRole m_eDefendingRole;
	[Attribute("", desc: "Explicit stable defended location ID. The scenario/garrison owner must verify ownership.")]
	protected string m_sDefendedLocationId;
	[Attribute("5", desc: "Minimum seconds between report attempts from this entire defense, including rejected requests", params: "1 60 1")]
	protected int m_iReportInterval;
	[Attribute("0", desc: "Log damage subscription status and rejected damage reports for troubleshooting. Disable outside tests.")]
	protected bool m_bDebugReports;
	[Attribute("", desc: "Optional initial faction for an unassigned standalone scenario defender, e.g. US. Applied once on the server, only when affiliation is empty. Empty preserves existing setup; not used for groups.")]
	protected string m_sInitialFactionKey;
	[Attribute("0", desc: "Resolve the initial empty standalone defender affiliation from its defending role via AROR_FactionRolesComponent, instead of an explicit faction key.")]
	protected bool m_bInitializeFromDefendingRole;

	protected ref array<ref AROR_QRFDamageObserver> m_aObservers = {};
	protected int m_iClock;
	protected int m_iNextReportAttempt;
	protected bool m_bMonitoring;
	protected string m_sLastOperationId;
	protected string m_sLastDebugReason;
	protected int m_iNextDebugReport;
	protected int m_iLastObserverCount = -1;
	protected bool m_bInitialFactionReady;

	//! Explicit scenario initialization, separate from validating damage. Never changes an assigned faction.
	protected bool InitializeDefenderFaction_S()
	{
		if (SCR_AIGroup.Cast(GetOwner()))
			return true;
		string initialFactionKey = m_sInitialFactionKey;
		if (m_bInitializeFromDefendingRole)
		{
			AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
			if (!roles)
				return false;
			initialFactionKey = roles.GetFactionKey(m_eDefendingRole);
			if (initialFactionKey.IsEmpty())
				return false;
		}
		if (initialFactionKey.IsEmpty())
			return true;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(GetOwner().FindComponent(FactionAffiliationComponent));
		if (!affiliation)
		{
			Print("[AROR] Scenario defender has no faction affiliation component; QRF monitoring stopped", LogLevel.ERROR);
			StopMonitoring();
			return false;
		}
		if (!affiliation.GetAffiliatedFactionKey().IsEmpty())
			return true;
		FactionManager factions = GetGame().GetFactionManager();
		if (!factions)
			return false;
		if (!factions.GetFactionByKey(initialFactionKey))
		{
			Print(string.Format("[AROR] Scenario defender faction '%1' is missing from the scenario; QRF monitoring stopped", initialFactionKey), LogLevel.ERROR);
			StopMonitoring();
			return false;
		}
		affiliation.SetAffiliatedFactionByKey(initialFactionKey);
		if (affiliation.GetAffiliatedFactionKey() != initialFactionKey)
			return false;
		Print(string.Format("[AROR] Scenario defender faction assigned: owner=%1 faction=%2", GetOwner(), initialFactionKey), LogLevel.NORMAL);
		return true;
	}

	protected void DebugReport(string reason)
	{
		if (!m_bDebugReports || (reason == m_sLastDebugReason && m_iClock < m_iNextDebugReport))
			return;
		m_sLastDebugReason = reason;
		m_iNextDebugReport = m_iClock + 5;
		Print(string.Format("[AROR] QRF defense diagnostic: target=%1 owner=%2: %3", m_sDefendedLocationId, GetOwner(), reason), LogLevel.NORMAL);
	}

	//! Trusted server integration. Call again when a garrison receives a new defense assignment.
	bool BindDefense_S(AROR_EFactionRole role, string locationId)
	{
		if (!GetGame().InPlayMode() || !Replication.IsServer() || !IsEnemyRole(role) || locationId.IsEmpty() || m_iReportInterval < 1 || m_iReportInterval > 60 || GetOwner().FindComponent(AROR_QRFDefenseComponent) != this)
			return false;
		ClearDefense_S();
		m_eDefendingRole = role;
		m_sDefendedLocationId = locationId;
		m_bEnabled = true;
		StartMonitoring_S();
		return true;
	}

	//! Ownership/garrison code must clear the assignment on capture or reassignment.
	//! Existing QRF cancellation is the territory caller's responsibility, not one soldier's.
	void ClearDefense_S()
	{
		if (!Replication.IsServer())
			return;
		m_bEnabled = false;
		m_sDefendedLocationId = string.Empty;
		m_sLastOperationId = string.Empty;
		m_iNextReportAttempt = 0;
		StopMonitoring();
	}

	protected bool IsEnemyRole(AROR_EFactionRole role)
	{
		return role == AROR_EFactionRole.OCCUPIERS || role == AROR_EFactionRole.INVADERS;
	}

	//! Internal subscription callback. Not a client RPC or a substitute for engine damage handling.
	void ReportDamage_S(IEntity victim, BaseDamageContext context)
	{
		if (!Replication.IsServer() || !m_bMonitoring || !m_bEnabled || !victim || !context)
			return;
		if (context.damageValue <= 0)
		{
			DebugReport(string.Format("Damage event ignored: non-positive damage=%1 type=%2", context.damageValue, context.damageType));
			return;
		}
		if (m_iClock < m_iNextReportAttempt)
		{
			DebugReport("Damage event received during local report throttle");
			return;
		}
		if (context.damageType == EDamageType.HEALING || context.damageType == EDamageType.REGENERATION || !context.instigator)
		{
			DebugReport("Damage ignored: healing/regeneration or no instigator");
			return;
		}
		if (context.instigator.GetInstigatorType() == InstigatorType.INSTIGATOR_GM)
		{
			DebugReport("Damage ignored: Game Master instigator");
			return;
		}
		bool observed;
		foreach (AROR_QRFDamageObserver observer : m_aObservers)
		{
			if (observer.GetEntity() == victim)
				observed = true;
		}
		if (!observed)
		{
			DebugReport("Damage ignored: victim is no longer monitored");
			return;
		}

		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!commanders || !commanders.IsCampaignActive() || !operations || !locations || !IsEnemyRole(m_eDefendingRole))
		{
			DebugReport("Damage ignored: campaign/managers unavailable or defending role invalid");
			return;
		}
		AROR_Commander commander = commanders.GetCommander(m_eDefendingRole);
		AROR_LocationComponent location = locations.GetLocationById(m_sDefendedLocationId);
		if (!commander || !location || !location.IsPositionInside(victim.GetOrigin()))
		{
			DebugReport("Damage ignored: commander/location missing or victim outside assigned location");
			return;
		}
		IEntity attacker = context.instigator.GetInstigatorEntity();
		if (!attacker || attacker == victim)
		{
			DebugReport(string.Format("Damage ignored: instigator has no entity or is victim (instigator type=%1)", context.instigator.GetInstigatorType()));
			return;
		}
		FactionAffiliationComponent defenseAffiliation = FactionAffiliationComponent.Cast(victim.FindComponent(FactionAffiliationComponent));
		FactionAffiliationComponent attackerAffiliation = FactionAffiliationComponent.Cast(attacker.FindComponent(FactionAffiliationComponent));
		if (!defenseAffiliation)
		{
			DebugReport("Damage ignored: defender has no faction affiliation component");
			return;
		}
		if (defenseAffiliation.GetAffiliatedFactionKey() != commander.GetFactionKey())
		{
			DebugReport(string.Format("Damage ignored: defender faction='%1', expected='%2'", defenseAffiliation.GetAffiliatedFactionKey(), commander.GetFactionKey()));
			return;
		}
		if (!attackerAffiliation)
		{
			DebugReport(string.Format("Damage ignored: instigator entity has no faction affiliation; instigator type=%1 player ID=%2 entity=%3", context.instigator.GetInstigatorType(), context.instigator.GetInstigatorPlayerID(), attacker));
			return;
		}
		Faction defenderFaction = defenseAffiliation.GetAffiliatedFaction();
		Faction attackerFaction = attackerAffiliation.GetAffiliatedFaction();
		if (!defenderFaction || !attackerFaction || !defenderFaction.IsFactionEnemy(attackerFaction))
		{
			DebugReport(string.Format("Damage ignored: defender=%1 attacker=%2; faction unavailable or diplomacy does not mark attacker hostile", defenseAffiliation.GetAffiliatedFactionKey(), attackerAffiliation.GetAffiliatedFactionKey()));
			return;
		}

		// Throttle fresh observations before scheduler callbacks; strategic admission may queue the threat.
		m_iNextReportAttempt = m_iClock + m_iReportInterval;
		string id = operations.ReportAttackAt_S(m_eDefendingRole, m_sDefendedLocationId, attacker.GetOrigin());
		if (id.IsEmpty())
		{
			if (operations.HasPendingQRFThreat(m_eDefendingRole, m_sDefendedLocationId))
				DebugReport("Hostile damage verified; threat queued for strategic priority, funds and capacity evaluation");
			else
				DebugReport("Hostile damage verified, but scheduler rejected QRF: check cooldown, capacity and conflicting operation");
		}
		if (!id.IsEmpty() && id != m_sLastOperationId)
		{
			m_sLastOperationId = id;
			Print(string.Format("[AROR] Defense under attack: role=%1 target=%2 QRF=%3", m_eDefendingRole, m_sDefendedLocationId, id), LogLevel.NORMAL);
		}
	}

	protected void RefreshObservers_S()
	{
		array<IEntity> entities = {};
		SCR_AIGroup group = SCR_AIGroup.Cast(GetOwner());
		if (group)
		{
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (agent && agent.GetControlledEntity())
					entities.Insert(agent.GetControlledEntity());
			}
		}
		else if (GetOwner())
			entities.Insert(GetOwner());

		for (int i = m_aObservers.Count() - 1; i >= 0; i--)
		{
			AROR_QRFDamageObserver observer = m_aObservers[i];
			if (!observer.GetEntity() || !entities.Contains(observer.GetEntity()))
			{
				observer.Detach_S();
				m_aObservers.Remove(i);
			}
			else
				entities.RemoveItem(observer.GetEntity());
		}
		foreach (IEntity entity : entities)
		{
			AROR_QRFDamageObserver added = new AROR_QRFDamageObserver();
			if (added.Attach_S(entity, this))
			{
				m_aObservers.Insert(added);
				FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
				if (affiliation)
					DebugReport(string.Format("Subscribed defender faction='%1' entity=%2", affiliation.GetAffiliatedFactionKey(), entity));
				else
					DebugReport("Subscribed defender has no faction affiliation component");
			}
		}
		if (m_iLastObserverCount != m_aObservers.Count())
		{
			m_iLastObserverCount = m_aObservers.Count();
			DebugReport(string.Format("Damage subscriptions=%1 (zero means no controlled member/damage manager was found)", m_iLastObserverCount));
		}
	}

	protected void Tick()
	{
		if (!Replication.IsServer() || !m_bMonitoring)
			return;
		m_iClock++;
		if (!m_bInitialFactionReady)
		{
			m_bInitialFactionReady = InitializeDefenderFaction_S();
			if (!m_bInitialFactionReady)
				return;
		}
		RefreshObservers_S();
	}

	protected void StartMonitoring_S()
	{
		if (m_bMonitoring || !GetGame().InPlayMode() || !Replication.IsServer() || !m_bEnabled || m_sDefendedLocationId.IsEmpty())
			return;
		if (!IsEnemyRole(m_eDefendingRole) || m_iReportInterval < 1 || m_iReportInterval > 60 || GetOwner().FindComponent(AROR_QRFDefenseComponent) != this)
		{
			Print("[AROR] Invalid QRF defense assignment or report interval; monitoring disabled", LogLevel.ERROR);
			return;
		}
		m_bMonitoring = true;
		m_iLastObserverCount = -1;
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);
		Print(string.Format("[AROR] QRF defense monitoring: role=%1 target=%2", m_eDefendingRole, m_sDefendedLocationId), LogLevel.NORMAL);
	}

	protected void StopMonitoring()
	{
		GetGame().GetCallqueue().Remove(Tick);
		m_bMonitoring = false;
		foreach (AROR_QRFDamageObserver observer : m_aObservers)
			observer.Detach_S();
		m_aObservers.Clear();
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	override void EOnInit(IEntity owner)
	{
		StartMonitoring_S();
	}

	override void OnDelete(IEntity owner)
	{
		StopMonitoring();
		super.OnDelete(owner);
	}
}
