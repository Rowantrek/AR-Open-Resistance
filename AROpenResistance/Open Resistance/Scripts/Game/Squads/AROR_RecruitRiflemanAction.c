//! Inherited by HQ flags and every territory that uses AROR_Prop_CaptureFlag.
class AROR_RecruitRiflemanAction : ScriptedUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		AROR_LocationComponent location = AROR_SquadManagerComponent.GetFlagLocation(GetOwner());
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		return location && roles && AROR_SquadManagerComponent.GetInstance() && location.GetControllingRole() == AROR_EFactionRole.RESISTANCE && SCR_Faction.GetEntityFaction(user) == roles.GetFaction(AROR_EFactionRole.RESISTANCE);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		AROR_SquadManagerComponent manager = AROR_SquadManagerComponent.GetInstance();
		if (!manager || !user)
			return false;
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
		string reason;
		if (manager.CanRecruit(playerId, GetOwner(), reason))
			return true;
		SetCannotPerformReason(reason);
		return false;
	}

	override void PerformAction(IEntity owner, IEntity user)
	{
		if (!Replication.IsServer() || !user)
			return;
		AROR_SquadManagerComponent manager = AROR_SquadManagerComponent.GetInstance();
		if (manager)
			manager.Recruit_S(GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user), GetOwner());
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Recruit rifleman";
		return true;
	}

	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}

	override bool CanBroadcastScript()
	{
		return false;
	}
}
