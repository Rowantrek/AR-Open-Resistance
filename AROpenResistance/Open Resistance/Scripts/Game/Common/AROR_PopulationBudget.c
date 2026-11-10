//! Server-wide admission for mod-owned AI. Queued slots count immediately; native/Game Master actors outside these
//! systems are not included. No actors are removed to meet a limit. Engine AI-budget enforcement remains intact.
class AROR_PopulationBudget
{
	protected static SCR_BaseGameMode s_Mode;
	protected static ref array<ref AROR_PopulationGroup> s_aGroups = {};

	protected static SCR_BaseGameMode Mode()
	{
		SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (mode != s_Mode)
		{
			s_aGroups.Clear();
			s_Mode = mode;
		}
		return mode;
	}

	static void RegisterGroup_S(SCR_AIGroup group, int size)
	{
		if (!Replication.IsServer() || !group || size <= 0)
			return;
		Mode();
		foreach (AROR_PopulationGroup previous : s_aGroups)
		{
			if (previous.m_Group == group)
			{
				previous.m_iSize = Math.Max(previous.m_iSize, size);
				return;
			}
		}
		AROR_PopulationGroup entry = new AROR_PopulationGroup();
		entry.m_Group = group;
		entry.m_iSize = size;
		s_aGroups.Insert(entry);
	}

	static int GetAvailable_S(bool ambient = false)
	{
		if (!Replication.IsServer())
			return 0;
		SCR_BaseGameMode mode = Mode();
		if (!mode)
			return 0;
		int committed;
		for (int index = s_aGroups.Count() - 1; index >= 0; index--)
		{
			AROR_PopulationGroup entry = s_aGroups[index];
			if (!entry.m_Group)
			{
				s_aGroups.Remove(index);
				continue;
			}
			// Keep the full queued reservation until its owner removes the group; partial materialization cannot
			// release slots that are still waiting in the native queue.
			committed += entry.m_iSize;
		}
		int limit = 320;
		AROR_OperationExecutorComponent executor = AROR_OperationExecutorComponent.Cast(mode.FindComponent(AROR_OperationExecutorComponent));
		if (executor)
		{
			committed += executor.GetCommittedPersonnel_S();
			limit = executor.GetPopulationLimit(ambient);
		}
		AROR_GarrisonManagerComponent garrisons = AROR_GarrisonManagerComponent.GetInstance();
		if (garrisons)
			committed += garrisons.GetCommittedPersonnel_S();
		AROR_SquadManagerComponent recruits = AROR_SquadManagerComponent.GetInstance();
		if (recruits)
			committed += recruits.GetCommittedPersonnel_S();
		return Math.Max(0, limit - committed);
	}
}

class AROR_PopulationGroup
{
	SCR_AIGroup m_Group;
	int m_iSize;
}
