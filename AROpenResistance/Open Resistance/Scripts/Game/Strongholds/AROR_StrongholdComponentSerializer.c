//! Early-game window progress and the player kills counted towards the next retaliation. Live responses are session-only.
class AROR_StrongholdComponentSerializer : ScriptedComponentSerializer
{
	override static typename GetTargetType()
	{
		return AROR_StrongholdComponent;
	}
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		AROR_StrongholdComponent stronghold = AROR_StrongholdComponent.Cast(component);
		context.WriteValue("version", 1);
		context.WriteValue("elapsed", stronghold.GetProtectedElapsed());
		context.WriteValue("kills", stronghold.GetPlayerKills());
		return ESerializeResult.OK;
	}
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_StrongholdComponent stronghold = AROR_StrongholdComponent.Cast(component);
		int version, elapsed, kills;
		// An unreadable record keeps the fresh state: the early-game window restarts rather than being lost.
		if (!context.ReadValue("version", version) || version != 1 || !context.ReadValue("elapsed", elapsed) || !context.ReadValue("kills", kills))
			return true;
		stronghold.Restore_S(elapsed, kills);
		return true;
	}
}
