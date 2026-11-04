//! Server: dresses the members of one AI group as the engine spawns them (see AROR_FactionPresetsComponent.FillGroup_S).
//! Holds the group's unit types in slot order and the character dressed for each slot. Every undressed newcomer takes
//! the first slot whose character is gone (never spawned yet, or despawned, e.g. by group dormancy, and spawned again
//! when the group wakes), so respawned members are dressed too. Members already wearing clothing (e.g. survivors
//! re-added to a group) are left alone. Every slot of a role is the same base character, so a slot that fails to spawn
//! can at worst shift the types of later members. Lives as long as its group.
class AROR_GroupOutfitter
{
	protected SCR_AIGroup m_Group;
	protected AROR_EFactionRole m_eRole;
	protected ref array<AROR_EUnitType> m_aTypes = {};
	protected ref array<IEntity> m_aDressed = {};

	//------------------------------------------------------------------------------------------------
	void AROR_GroupOutfitter(notnull SCR_AIGroup group, AROR_EFactionRole role, notnull array<AROR_EUnitType> types)
	{
		m_Group = group;
		m_eRole = role;
		m_aTypes.Copy(types);
		m_aDressed.Resize(types.Count());
		group.GetOnAgentAdded().Insert(OnAgentAdded);
	}

	//------------------------------------------------------------------------------------------------
	SCR_AIGroup GetGroup()
	{
		return m_Group;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return !m_Group || m_aTypes.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	void Detach()
	{
		if (m_Group)
			m_Group.GetOnAgentAdded().Remove(OnAgentAdded);

		m_aTypes.Clear();
		m_aDressed.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		IEntity character = agent.GetControlledEntity();
		if (!character || m_aDressed.Contains(character))
			return;

		if (!AROR_LoadoutOutfitter.IsUndressed(character))
		{
			Print(string.Format("[AROR] Group outfitter: %1 joined already wearing clothing, so it keeps its own kit", SCR_ResourceNameUtils.GetPrefabName(character)), LogLevel.WARNING);
			return;
		}

		foreach (int slot, IEntity dressed : m_aDressed)
		{
			if (dressed)
				continue;

			m_aDressed[slot] = character;
			AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
			if (presets)
				presets.OutfitLater_S(character, m_eRole, m_aTypes[slot]);

			return;
		}

		Print(string.Format("[AROR] Group outfitter: %1 joined a group with every slot taken (%2), so it is not dressed", SCR_ResourceNameUtils.GetPrefabName(character), m_aTypes.Count()), LogLevel.WARNING);
	}
}
