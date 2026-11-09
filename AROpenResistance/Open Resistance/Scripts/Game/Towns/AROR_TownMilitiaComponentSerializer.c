//! Saves the Resistance town garrisons bought in HICOMM (AROR_TownMilitiaComponent on the game mode, which has a
//! Persistence component): per town, its surviving infantry, APCs and armour. Nothing is written while no town has one.
class AROR_TownMilitiaComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_TownMilitiaComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		AROR_TownMilitiaComponent militia = AROR_TownMilitiaComponent.Cast(component);
		array<string> townIds = {};
		array<int> infantry = {};
		array<int> apcs = {};
		array<int> armour = {};
		militia.GetBook_S(townIds, infantry, apcs, armour);
		if (townIds.IsEmpty())
			return ESerializeResult.DEFAULT;

		context.WriteValue("version", VERSION);
		context.WriteValue("towns", townIds);
		context.WriteValue("infantry", infantry);
		context.WriteValue("apcs", apcs);
		context.WriteValue("armour", armour);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_TownMilitiaComponent militia = AROR_TownMilitiaComponent.Cast(component);
		int version;
		context.ReadValue("version", version);
		array<string> townIds = {};
		array<int> infantry = {};
		array<int> apcs = {};
		array<int> armour = {};
		if (context.ReadValue("towns", townIds) && context.ReadValue("infantry", infantry) && context.ReadValue("apcs", apcs) && context.ReadValue("armour", armour))
			militia.RestoreBook_S(townIds, infantry, apcs, armour);
		return true;
	}
}
