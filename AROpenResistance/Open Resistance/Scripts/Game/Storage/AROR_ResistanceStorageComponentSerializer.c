//! Saves Resistance item stock (AROR_ResistanceStorageComponent on the game mode, which has a Persistence component).
//! Nothing is written while the stock is empty.
class AROR_ResistanceStorageComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_ResistanceStorageComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.Cast(component);
		if (storage.IsEmpty())
			return ESerializeResult.DEFAULT;

		array<ResourceName> prefabs = {};
		array<int> quantities = {};
		storage.GetEntries(prefabs, quantities);

		context.WriteValue("version", VERSION);
		context.WriteValue("prefabs", prefabs);
		context.WriteValue("quantities", quantities);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.Cast(component);

		int version;
		context.ReadValue("version", version);

		array<ResourceName> prefabs = {};
		array<int> quantities = {};
		if (context.ReadValue("prefabs", prefabs) && context.ReadValue("quantities", quantities))
			storage.RestoreEntries_S(prefabs, quantities);

		return true;
	}
}
