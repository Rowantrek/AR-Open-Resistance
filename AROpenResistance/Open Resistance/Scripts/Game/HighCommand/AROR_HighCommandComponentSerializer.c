//! Saves the resistance name (AROR_HighCommandComponent on the game mode), chosen at campaign setup and changed in
//! HICOMM. Squads are session-only and are not saved (see AROR_HighCommandComponent).
class AROR_HighCommandComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_HighCommandComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.Cast(component);

		context.WriteValue("version", VERSION);
		context.WriteValue("resistanceName", highCommand.GetResistanceName());
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.Cast(component);

		int version;
		context.ReadValue("version", version);

		string name;
		context.ReadValue("resistanceName", name);
		highCommand.SetResistanceName_S(name);
		return true;
	}
}
