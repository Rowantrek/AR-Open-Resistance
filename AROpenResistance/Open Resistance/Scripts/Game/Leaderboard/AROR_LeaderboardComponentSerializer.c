//! Saves the campaign leaderboard (AROR_LeaderboardComponent on the game mode): per player identity, the last known
//! name and the kill and assist counts per category. Nothing is written while the board is empty.
class AROR_LeaderboardComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_LeaderboardComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_LeaderboardComponent leaderboard = AROR_LeaderboardComponent.Cast(component);
		if (leaderboard.IsEmpty())
			return ESerializeResult.DEFAULT;

		array<string> keys = {};
		array<string> names = {};
		array<int> counts = {};
		leaderboard.GetEntries(keys, names, counts);

		int countsPerEntry = AROR_LeaderboardComponent.GetCategoryCount() * 2;
		context.WriteValue("version", VERSION);
		context.WriteValue("countsPerEntry", countsPerEntry);
		context.WriteValue("keys", keys);
		context.WriteValue("names", names);
		context.WriteValue("counts", counts);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_LeaderboardComponent leaderboard = AROR_LeaderboardComponent.Cast(component);

		int version;
		context.ReadValue("version", version);

		int countsPerEntry;
		array<string> keys = {};
		array<string> names = {};
		array<int> counts = {};
		if (context.ReadValue("countsPerEntry", countsPerEntry)
			&& context.ReadValue("keys", keys)
			&& context.ReadValue("counts", counts))
		{
			context.ReadValue("names", names);
			leaderboard.RestoreEntries_S(keys, names, counts, countsPerEntry);
		}

		return true;
	}
}
