//! Saves per-player arsenal loadout slots (AROR_PlayerLoadoutsComponent on the game mode).
//! Nothing is written while every slot is empty.
class AROR_PlayerLoadoutsComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_PlayerLoadoutsComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_PlayerLoadoutsComponent loadouts = AROR_PlayerLoadoutsComponent.Cast(component);
		if (loadouts.IsEmpty())
			return ESerializeResult.DEFAULT;

		array<string> playerKeys = {};
		array<int> slotIndices = {};
		array<string> loadoutJsons = {};
		array<string> itemPlayerKeys = {};
		array<int> itemSlotIndices = {};
		array<ResourceName> itemPrefabs = {};
		array<int> itemQuantities = {};
		loadouts.GetEntries(playerKeys, slotIndices, loadoutJsons, itemPlayerKeys, itemSlotIndices, itemPrefabs, itemQuantities);

		context.WriteValue("version", VERSION);
		context.WriteValue("playerKeys", playerKeys);
		context.WriteValue("slotIndices", slotIndices);
		context.WriteValue("loadoutJsons", loadoutJsons);
		context.WriteValue("itemPlayerKeys", itemPlayerKeys);
		context.WriteValue("itemSlotIndices", itemSlotIndices);
		context.WriteValue("itemPrefabs", itemPrefabs);
		context.WriteValue("itemQuantities", itemQuantities);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_PlayerLoadoutsComponent loadouts = AROR_PlayerLoadoutsComponent.Cast(component);

		int version;
		context.ReadValue("version", version);

		array<string> playerKeys = {};
		array<int> slotIndices = {};
		array<string> loadoutJsons = {};
		array<string> itemPlayerKeys = {};
		array<int> itemSlotIndices = {};
		array<ResourceName> itemPrefabs = {};
		array<int> itemQuantities = {};

		if (context.ReadValue("playerKeys", playerKeys)
			&& context.ReadValue("slotIndices", slotIndices)
			&& context.ReadValue("loadoutJsons", loadoutJsons))
		{
			context.ReadValue("itemPlayerKeys", itemPlayerKeys);
			context.ReadValue("itemSlotIndices", itemSlotIndices);
			context.ReadValue("itemPrefabs", itemPrefabs);
			context.ReadValue("itemQuantities", itemQuantities);
			loadouts.RestoreEntries_S(playerKeys, slotIndices, loadoutJsons, itemPlayerKeys, itemSlotIndices, itemPrefabs, itemQuantities);
		}

		return true;
	}
}
