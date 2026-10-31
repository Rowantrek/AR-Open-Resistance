//! Grant/cadence only. Economy and Locations keep ownership of funds and territory persistence.
class AROR_InvaderCampaignComponentSerializer : ScriptedComponentSerializer
{
	override static typename GetTargetType()
	{
		return AROR_InvaderCampaignComponent;
	}
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		AROR_InvaderCampaignComponent campaign = AROR_InvaderCampaignComponent.Cast(component);
		context.WriteValue("version", 1);
		context.WriteValue("granted", campaign.HasOpeningGrant());
		context.WriteValue("quarantined", campaign.IsQuarantined());
		context.WriteValue("delay", campaign.RemainingDelay());
		return ESerializeResult.OK;
	}
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_InvaderCampaignComponent campaign = AROR_InvaderCampaignComponent.Cast(component);
		int version, delay;
		bool granted, quarantined;
		if (!context.ReadValue("version", version) || version != 1 || !context.ReadValue("granted", granted) || !context.ReadValue("quarantined", quarantined) || !context.ReadValue("delay", delay) || delay < 0 || delay > 3600)
		{
			campaign.Quarantine_S();
			return true;
		}
		campaign.RestoreCampaign_S(granted, quarantined, delay);
		return true;
	}
}
