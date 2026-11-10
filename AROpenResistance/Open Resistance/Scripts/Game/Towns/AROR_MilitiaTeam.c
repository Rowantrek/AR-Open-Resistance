//! Owns a purchased team's spawn transaction. Missing agents are pending/virtual, never presumed dead.
class AROR_MilitiaTeam
{
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	int m_iSize;
	int m_iRaisedAt;
	int m_iLosses;
	int m_iReportedLosses;
	ref array<ref AROR_MilitiaMember> m_aMembers = {};

	void Attach(SCR_AIGroup group, int size, int now)
	{
		m_Group = group;
		m_iSize = size;
		m_iRaisedAt = now;
		group.GetOnAgentAdded().Insert(OnAgentAdded);
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
			OnAgentAdded(agent);
	}

	protected void OnAgentAdded(AIAgent agent)
	{
		if (!agent || !agent.GetControlledEntity())
			return;
		IEntity character = agent.GetControlledEntity();
		foreach (AROR_MilitiaMember previous : m_aMembers)
		{
			if (previous.m_Character == character)
				return;
		}
		AROR_MilitiaMember member = new AROR_MilitiaMember();
		member.Attach(character);
		m_aMembers.Insert(member);
	}

	int TakeLosses()
	{
		for (int index = m_aMembers.Count() - 1; index >= 0; index--)
		{
			AROR_MilitiaMember member = m_aMembers[index];
			member.CheckDeath();
			if (member.m_bDead)
				m_iLosses++;
			// A native dormant character may disappear alive. It is not a loss and its replacement is observed anew.
			if (member.m_bDead || !member.m_Character)
			{
				member.Detach();
				m_aMembers.Remove(index);
			}
		}
		m_iLosses = Math.Min(m_iLosses, m_iSize);
		int newlyLost = m_iLosses - m_iReportedLosses;
		m_iReportedLosses = m_iLosses;
		return newlyLost;
	}

	int GetStrength()
	{
		return Math.Max(0, m_iSize - m_iLosses);
	}

	void Detach()
	{
		if (m_Group)
			m_Group.GetOnAgentAdded().Remove(OnAgentAdded);
		foreach (AROR_MilitiaMember member : m_aMembers)
			member.Detach();
		m_aMembers.Clear();
	}

	bool Remove_S()
	{
		if (!AROR_ForceSpawner.RemoveGroup_S(m_Group))
			return false;
		Detach();
		if (m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		m_Waypoint = null;
		m_Group = null;
		return true;
	}
}

//! Damage callbacks are checked after the native hit completes, before normal body retirement can erase evidence.
class AROR_MilitiaMember
{
	IEntity m_Character;
	SCR_DamageManagerComponent m_Damage;
	bool m_bDead;

	void Attach(IEntity character)
	{
		m_Character = character;
		m_Damage = SCR_DamageManagerComponent.Cast(character.FindComponent(SCR_DamageManagerComponent));
		if (m_Damage)
			m_Damage.GetOnDamage().Insert(OnDamage);
	}

	protected void OnDamage(BaseDamageContext context)
	{
		GetGame().GetCallqueue().CallLater(CheckDeath, 0, false);
	}

	void CheckDeath()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		CharacterControllerComponent controller;
		if (character)
			controller = character.GetCharacterController();
		if ((m_Damage && m_Damage.IsDestroyed()) || (controller && controller.IsDead()))
			m_bDead = true;
	}

	void Detach()
	{
		if (m_Damage)
			m_Damage.GetOnDamage().Remove(OnDamage);
		GetGame().GetCallqueue().Remove(CheckDeath);
	}
}
