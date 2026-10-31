//! User action on AROR_Prop_Garage: opens the retrieve dialog (client). Anchor is this prop.
class AROR_GarageRetrieveAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user || !ChimeraCharacter.Cast(user))
			return false;

		return IsResistanceUser(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Dialog + ghost placement are client-only
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		if (!pOwnerEntity)
			return;

		AROR_GarageRetrieveUI.Open(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Retrieve vehicle";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
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
