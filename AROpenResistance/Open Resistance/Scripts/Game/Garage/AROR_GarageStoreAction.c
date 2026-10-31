//! User action on AROR_Prop_Garage: store the vehicle under the player's view.
class AROR_GarageStoreAction : ScriptedUserAction
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
		// Look ray + store request start on the client
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		AROR_GarageStoreAPI.TryStoreLookedAtVehicle();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Store vehicle";
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
