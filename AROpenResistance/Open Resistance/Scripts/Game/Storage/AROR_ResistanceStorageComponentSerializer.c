//! Saves Resistance item stock (AROR_ResistanceStorageComponent on the game mode, which has a Persistence component).
//! Nothing is written while the stock is empty.
//! Version 2 adds the loose magazine rounds; version 1 saves load with none.
class AROR_ResistanceStorageComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 2;

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

		array<ResourceName> loosePrefabs = {};
		array<int> looseRounds = {};
		storage.GetLooseRounds(loosePrefabs, looseRounds);

		context.WriteValue("version", VERSION);
		context.WriteValue("prefabs", prefabs);
		context.WriteValue("quantities", quantities);
		context.WriteValue("loosePrefabs", loosePrefabs);
		context.WriteValue("looseRounds", looseRounds);
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

		if (version >= 2)
		{
			array<ResourceName> loosePrefabs = {};
			array<int> looseRounds = {};
			if (context.ReadValue("loosePrefabs", loosePrefabs) && context.ReadValue("looseRounds", looseRounds))
				storage.RestoreLooseRounds_S(loosePrefabs, looseRounds);
		}

		return true;
	}
}
