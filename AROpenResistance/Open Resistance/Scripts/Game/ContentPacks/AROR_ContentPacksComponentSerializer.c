//! Saves which content packs the admin turned off (or on, for packs off by default) in the campaign
//! (AROR_ContentPacksComponent on the game mode). Packs without an override follow their default, so a pack added
//! after the campaign started is used as its author intended.
class AROR_ContentPacksComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_ContentPacksComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.Cast(component);

		array<string> disabledIds = {};
		array<string> enabledIds = {};
		packs.GetPackOverrides(disabledIds, enabledIds);

		context.WriteValue("version", VERSION);
		context.WriteValue("disabledPackIds", disabledIds);
		context.WriteValue("enabledPackIds", enabledIds);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.Cast(component);

		int version;
		context.ReadValue("version", version);

		array<string> disabledIds = {};
		array<string> enabledIds = {};
		context.ReadValue("disabledPackIds", disabledIds);
		context.ReadValue("enabledPackIds", enabledIds);
		packs.SetPackOverrides_S(disabledIds, enabledIds);
		return true;
	}
}
