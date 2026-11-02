//! Saves the campaign settings chosen at setup (AROR_CampaignSettingsComponent on the game mode) as setting IDs and
//! values. Settings added after the campaign started load with their default; values for settings that are no longer
//! defined are kept.
class AROR_CampaignSettingsComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_CampaignSettingsComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_CampaignSettingsComponent settings = AROR_CampaignSettingsComponent.Cast(component);

		array<string> settingIds = {};
		array<string> values = {};
		settings.GetValues(settingIds, values);

		context.WriteValue("version", VERSION);
		context.WriteValue("settingIds", settingIds);
		context.WriteValue("settingValues", values);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_CampaignSettingsComponent settings = AROR_CampaignSettingsComponent.Cast(component);

		int version;
		context.ReadValue("version", version);

		array<string> settingIds = {};
		array<string> values = {};
		context.ReadValue("settingIds", settingIds);
		context.ReadValue("settingValues", values);
		settings.RestoreValues_S(settingIds, values);
		return true;
	}
}
