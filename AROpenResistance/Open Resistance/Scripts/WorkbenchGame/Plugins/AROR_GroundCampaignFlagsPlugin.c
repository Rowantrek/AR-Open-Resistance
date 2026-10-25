[WorkbenchPluginAttribute(name: "Ground all campaign flags", description: "Ground and audit all 41 Chicken campaign poles using editor terrain; one undo action. Save the world afterward.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_GroundCampaignFlagsPlugin : WorldEditorPlugin
{
	override void Run()
	{
		RunAudit(true);
	}

	protected IEntity FindFlag(WorldEditorAPI api, IEntity root)
	{
		IEntity child = root.GetChildren();
		while (child)
		{
			IEntitySource source = api.EntityToSource(child);
			if (source)
			{
				for (int i = 0; i < source.GetComponentCount(); i++)
				{
					IEntityComponentSource component = source.GetComponent(i);
					if (component && component.GetClassName() == "AROR_CaptureFlagPlacementComponent")
						return child;
				}
			}
			IEntity nested = FindFlag(api, child);
			if (nested)
				return nested;
			child = child.GetSibling();
		}
		return null;
	}

	//! Physical WORLD trace excludes building roofs and the flag itself.
	protected bool GroundHeight(BaseWorld world, IEntity flag, vector position, out float height)
	{
		height = world.GetSurfaceY(position[0], position[2]);
		TraceParam trace = new TraceParam();
		trace.Start = Vector(position[0], height + 1000, position[2]);
		trace.End = Vector(position[0], height - 1000, position[2]);
		trace.Flags = TraceFlags.WORLD;
		trace.Exclude = flag;
		float fraction = world.TraceMove(trace, null);
		if (fraction < 0 || fraction >= 1)
			return false;
		float distance = trace.End[1] - trace.Start[1];
		height = trace.Start[1] + distance * fraction;
		return height > world.GetOceanHeight(position[0], position[2]) + 0.25;
	}

	protected bool IsClear(BaseWorld world, IEntity flag, vector position)
	{
		TraceBox trace = new TraceBox();
		trace.Start = position + Vector(0, 0.15, 0);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-1.5, 0, -1.5);
		trace.Maxs = Vector(1.5, 7, 1.5);
		trace.Exclude = flag;
		trace.Flags = TraceFlags.ENTS;
		return world.TraceMove(trace, null) >= 1;
	}

	//! The mesh pivot need not be its base. Project the local bounds through the
	//! actual basis so Ground aligns the visible model, including rotated parents.
	protected float ModelBottom(vector transform[4], vector mins, vector maxs)
	{
		float bottom = transform[3][1];
		for (int axis = 0; axis < 3; axis++)
		{
			float first = transform[axis][1] * mins[axis];
			float second = transform[axis][1] * maxs[axis];
			bottom += Math.Min(first, second);
		}
		return bottom;
	}

	void RunAudit(bool apply)
	{
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor)
			return;
		WorldEditorAPI api = editor.GetApi();
		if (!api || !api.IsEditMode() || api.IsPrefabEditMode())
		{
			Print("[AROR EDITOR FLAG] Stop Play and open the Chicken world in edit mode first.", LogLevel.WARNING);
			return;
		}
		string path;
		api.GetWorldPath(path);
		if (!path.Contains("AROR_Everon_Chicken.ent"))
		{
			Print("[AROR EDITOR FLAG] Refusing to edit a world other than Everon_Chicken.", LogLevel.WARNING);
			return;
		}
		array<IEntitySource> roots = {};
		for (int index = 0; index < api.GetEditorEntityCount(); index++)
		{
			IEntitySource source = api.GetEditorEntity(index);
			if (source && source.GetName().StartsWith("AROR_Location_everon_"))
				roots.Insert(source);
		}
		if (roots.Count() != 41)
		{
			Print(string.Format("[AROR EDITOR FLAG] Expected 41 Chicken locations, found %1. Reload the world; no edits made.", roots.Count()), LogLevel.ERROR);
			return;
		}
		if (apply && !api.BeginEntityAction("Ground Chicken campaign flags"))
			return;
		int changed;
		int inspected;
		int warnings;
		BaseWorld world = api.GetWorld();
		foreach (IEntitySource rootSource : roots)
		{
			IEntity root = api.SourceToEntity(rootSource);
			IEntity flag;
			if (root)
				flag = FindFlag(api, root);
			if (!flag || !flag.GetParent())
			{
				Print(string.Format("[AROR EDITOR FLAG] Missing flag child: %1", rootSource.GetName()), LogLevel.ERROR);
				warnings++;
				continue;
			}
			vector transform[4];
			flag.GetWorldTransform(transform);
			vector before = transform[3];
			float height;
			if (!GroundHeight(world, flag, before, height))
			{
				Print(string.Format("[AROR EDITOR FLAG] No valid land terrain: %1 position=%2", rootSource.GetName(), before), LogLevel.WARNING);
				warnings++;
				continue;
			}
			vector mins, maxs;
			flag.GetBounds(mins, maxs);
			float bottom = ModelBottom(transform, mins, maxs);
			vector desired = before;
			desired[1] = before[1] + height + 0.1 - bottom;
			vector ground = desired;
			ground[1] = height;
			bool clear = IsClear(world, flag, ground);
			if (!clear)
				warnings++;
			bool written;
			if (apply)
			{
				IEntitySource flagSource = api.EntityToSource(flag);
				// Editor serializes child coords locally. The API dirties the real source
				// and participates in Undo, unlike runtime SetWorldTransform/Update.
				vector local = flag.GetParent().CoordToLocal(desired);
				string coords = string.Format("%1 %2 %3", local[0], local[1], local[2]);
				written = flagSource && api.SetVariableValue(flagSource, null, "coords", coords);
				if (written)
					changed++;
				else
					warnings++;
			}
			inspected++;
			Print(string.Format("[AROR EDITOR FLAG] name=%1 before=%2 ground=%3 desired=%4 clear=%5 written=%6 boundsMin=%7 boundsMax=%8", rootSource.GetName(), before, height, desired, clear, written, mins, maxs), LogLevel.NORMAL);
			Print(string.Format("[AROR EDITOR FLAG] model base: name=%1 bottom=%2 terrain=%3 gap=%4", rootSource.GetName(), bottom, height, bottom - height), LogLevel.NORMAL);
		}
		if (apply)
			api.EndEntityAction("Ground Chicken campaign flags");
		Print(string.Format("[AROR EDITOR FLAG] inspected=%1 written=%2 warnings=%3. Save the world after reviewing; blocked sites need visual inspection. No location centres or ownership changed.", inspected, changed, warnings), LogLevel.NORMAL);
	}
}
