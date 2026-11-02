//! F-key action on a Resistance arsenal: save the user's worn gear into one of three loadout slots.
class AROR_SaveLoadoutSlotAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "Loadout slot (0 = Loadout 1)", "", ParamEnumArray.FromEnum(AROR_ELoadoutSlotIndex))]
	protected AROR_ELoadoutSlotIndex m_eSlotIndex;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!AROR_ResistanceArsenalComponent.IsResistanceArsenal(GetOwner()))
			return false;

		if (!user || !ChimeraCharacter.Cast(user))
			return false;

		return IsResistanceUser(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!CanBeShownScript(user))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
			return;

		AROR_PlayerLoadoutsComponent loadouts = AROR_PlayerLoadoutsComponent.GetInstance();
		if (!loadouts)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Loadouts unavailable");
			return;
		}

		loadouts.SaveSlot_S(playerId, pUserEntity, m_eSlotIndex);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = string.Format("Save Loadout %1", m_eSlotIndex + 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsResistanceUser(IEntity user)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles || !user)
			return false;

		Faction faction = SCR_Faction.GetEntityFaction(user);
		AROR_EFactionRole role;
		if (!roles.GetRoleOfFaction(faction, role))
			return false;

		return role == AROR_EFactionRole.RESISTANCE;
	}
}
