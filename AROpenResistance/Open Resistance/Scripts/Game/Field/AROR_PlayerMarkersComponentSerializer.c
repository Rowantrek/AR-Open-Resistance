class AROR_PlayerMarkersComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;
	override static typename GetTargetType() { return AROR_PlayerMarkersComponent; }

	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		AROR_PlayerMarkersComponent markers = AROR_PlayerMarkersComponent.Cast(component);
		array<string> rows = {};
		markers.GetSavedRows(rows);
		if (rows.IsEmpty())
			return ESerializeResult.DEFAULT;
		context.WriteValue("version", VERSION);
		context.WriteValue("rows", rows);
		return ESerializeResult.OK;
	}

	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		int version;
		array<string> rows = {};
		if (!context.ReadValue("version", version) || version != VERSION)
			return false;
		if (!context.ReadValue("rows", rows))
			return false;
		AROR_PlayerMarkersComponent markers = AROR_PlayerMarkersComponent.Cast(component);
		markers.RestoreRows_S(rows);
		return true;
	}
}
