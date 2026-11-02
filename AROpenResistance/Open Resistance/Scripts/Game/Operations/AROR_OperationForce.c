//! Owns only entities created for one operation. References to world entities are weak.
class AROR_OperationForce
{
	string m_sOperationId;
	ref AROR_QRFFunding m_Funding;
	int m_iPackageQuote;
	bool m_bOwnsPackageQuote;
	bool m_bPackageQueuesPreserved;
	int m_iPersonnelLossValue;
	int m_iVehicleLossValue;
	string m_sFactionKey;
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	int m_iReservedMembers;
	int m_iReservedGroups = 1;
	int m_iReservedVehicles;
	int m_iPackageIndex;
	bool m_bReservation;
	bool m_bReady;
	bool m_bOrdersIssued;
	bool m_bSawMembers;
	bool m_bArrivalLogged;
	string m_sLastProgress;
	bool m_bLogFriendlyDamage;
	string m_sAssetFactionKey;
	bool m_bTacticalControlled;
	bool m_bTacticalWithdrawing;
	bool m_bTacticalRetained;
	int m_iTacticalRetainedAt;
	int m_iTacticalNextOrder;
	int m_iTacticalLegDeadline;
	int m_iTacticalInfantryDeadline;
	int m_iTacticalNextInfantryLog;
	int m_iTacticalNextSupport;
	int m_iTacticalContactVersion = -1;
	int m_iTacticalSearchStep;
	int m_iTacticalInitialStrength;
	protected ref array<ref AROR_OperationDamageObserver> m_aDamageObservers = {};
	int m_iWakeRetryTicks;
	protected ref array<IEntity> m_aMembers = {};

	void OnAgentAdded(AIAgent agent)
	{
		if (!Replication.IsServer() || !agent)
			return;
		IEntity entity = agent.GetControlledEntity();
		if (entity && !m_aMembers.Contains(entity))
		{
			// Initialize only on first registration. Later faction changes must fail validation,
			// rather than being silently reset by the periodic member counter.
			InitializeSpawnFaction_S(entity);
			m_aMembers.Insert(entity);
			TrackDamage_S(entity);
			m_bSawMembers = true;
		}
	}

	//! Pure policy check: permit only this profile's declared asset faction at creation time.
	bool CanInitializeSpawnFaction(string currentFaction)
	{
		return !m_sFactionKey.IsEmpty() && (currentFaction.IsEmpty() || currentFaction == m_sFactionKey || (!m_sAssetFactionKey.IsEmpty() && currentFaction == m_sAssetFactionKey));
	}

	//! Call once for newly owned entities, never as periodic faction repair.
	bool InitializeSpawnGroupFaction_S(SCR_AIGroup group)
	{
		if (!Replication.IsServer() || !group || !GetGame().GetFactionManager())
			return false;
		Faction target = GetGame().GetFactionManager().GetFactionByKey(m_sFactionKey);
		if (!target)
			return false;
		Faction current = group.GetFaction();
		if (!current)
			return group.InitFactionKey(m_sFactionKey);
		if (!CanInitializeSpawnFaction(current.GetFactionKey()))
			return false;
		if (current.GetFactionKey() == m_sFactionKey)
			return true;
		// InitFactionKey deliberately cannot replace a prefab's existing native faction.
		return group.SetFaction(target);
	}

	//! Call once for newly owned characters/vehicles, never as periodic faction repair.
	bool InitializeSpawnFaction_S(IEntity entity)
	{
		if (!Replication.IsServer() || !entity)
			return false;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation || !CanInitializeSpawnFaction(affiliation.GetAffiliatedFactionKey()))
			return false;
		if (affiliation.GetAffiliatedFactionKey() != m_sFactionKey)
			affiliation.SetAffiliatedFactionByKey(m_sFactionKey);
		return affiliation.GetAffiliatedFactionKey() == m_sFactionKey;
	}

	void TrackDamage_S(IEntity entity)
	{
		if (!Replication.IsServer() || !entity)
			return;
		if (m_Funding)
		{
			bool vehicle = ChimeraCharacter.Cast(entity) == null;
			int value = m_iPersonnelLossValue;
			if (vehicle)
				value = m_iVehicleLossValue;
			m_Funding.RegisterPackageAsset_S(entity, value, vehicle, m_sFactionKey, m_sAssetFactionKey, m_iPackageIndex);
		}
		foreach (AROR_OperationDamageObserver observer : m_aDamageObservers)
		{
			if (observer.GetEntity() == entity)
				return;
		}
		AROR_OperationDamageObserver added = new AROR_OperationDamageObserver();
		if (added.Attach_S(entity, m_sOperationId, m_iPackageIndex, m_bLogFriendlyDamage))
			m_aDamageObservers.Insert(added);
	}

	//! Also captures already-created members when recovering an existing dispatch.
	int CountLivingMembers()
	{
		if (!m_Group)
			return 0;
		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		int living;
		foreach (AIAgent agent : agents)
		{
			OnAgentAdded(agent);
			if (!agent)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (character && character.GetCharacterController() && !character.GetCharacterController().IsDead())
				living++;
		}
		if (living > 0)
			m_bSawMembers = true;
		return living;
	}

	bool HasCorrectMemberFactions()
	{
		foreach (IEntity member : m_aMembers)
		{
			if (!member)
				continue;
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(member.FindComponent(FactionAffiliationComponent));
			if (!affiliation || affiliation.GetAffiliatedFactionKey() != m_sFactionKey)
				return false;
		}
		return true;
	}

	//! Snapshot for deferred operation cleanup; callers never take ownership of these references.
	void GetOwnedMembers(array<IEntity> outMembers)
	{
		outMembers.Copy(m_aMembers);
	}

	void Cleanup_S()
	{
		if (!Replication.IsServer())
			return;
		CountLivingMembers();
		// Observe before native deletion. Living actors retired by cleanup are not combat losses.
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
			operations.GetQRFFinance().Observe_S(m_Funding);
		if (m_Funding)
		{
			foreach (IEntity assetMember : m_aMembers)
			{
				if (assetMember && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(assetMember) == 0)
					m_Funding.Retire_S(assetMember);
			}
		}
		foreach (AROR_OperationDamageObserver observer : m_aDamageObservers)
			observer.Detach_S();
		m_aDamageObservers.Clear();
		if (m_Group)
		{
			m_Group.GetOnAgentAdded().Remove(OnAgentAdded);
			if (m_Waypoint)
				m_Group.RemoveWaypoint(m_Waypoint);
			// Deleting the group retires its outstanding engine spawn requests.
			// Preserve the group entity if a player has taken command during testing.
			if (m_Group.GetPlayerCount() == 0)
				SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
			else
				m_bPackageQueuesPreserved = true;
		}
		foreach (IEntity member : m_aMembers)
		{
			if (member && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(member) == 0)
				SCR_EntityHelper.DeleteEntityAndChildren(member);
		}
		if (m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		m_Group = null;
		m_Waypoint = null;
		m_aMembers.Clear();
		if (m_Funding && m_bOwnsPackageQuote && !m_bPackageQueuesPreserved)
		{
			m_bOwnsPackageQuote = false;
			m_Funding.ReleaseUnusedPackageQuote_S(m_iPackageIndex, m_iPackageQuote);
		}
	}
}
