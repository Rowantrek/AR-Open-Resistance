[WorkbenchPluginAttribute(name: "Ground military preset walls", description: "Undoable grounding of the actual sandbag wall children on Chicken's adopted defence presets. Save afterward; reconcile source edits before regenerating layers.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_GroundDefenceWallsPlugin : AROR_GroundCampaignFlagsPlugin
{
	//! Targeted commands can select locations without changing the shared grounding workflow.
	protected bool ShouldGroundLocation(AROR_LocationComponent location)
	{
		return true;
	}

	override void Run()
	{
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor) return;
		WorldEditorAPI api = editor.GetApi();
		if (!api || !api.IsEditMode() || api.IsPrefabEditMode())
		{
			Print("[AROR DEFENCE GROUND] Stop Play and open Chicken in world edit mode, outside prefab editing, before grounding.", LogLevel.WARNING);
			return;
		}
		string path;
		api.GetWorldPath(path);
		if (!path.Contains("AROR_Everon_Chicken.ent"))
		{
			Print("[AROR DEFENCE GROUND] Open Everon_Chicken; this command does not edit other worlds.", LogLevel.WARNING);
			return;
		}
		int changed;
		int warnings;
		int wallsFound;
		array<EntityID> wallIds = {};
		array<string> locationIds = {};
		array<vector> positions = {};
		array<float> heights = {};
		// SetVariableValue rebuilds runtime prefab children. Finish traversal and
		// copy all measurements before the first mutation; retain no live entities.
		BaseWorld world = api.GetWorld();
		for (int index = 0; index < api.GetEditorEntityCount(); index++)
		{
			IEntitySource source = api.GetEditorEntity(index);
			if (!source || !source.GetName().StartsWith("AROR_Location_everon_")) continue;
			IEntity root = api.SourceToEntity(source);
			if (!root) continue;
			AROR_LocationComponent location = AROR_LocationComponent.Cast(root.FindComponent(AROR_LocationComponent));
			if (!location || !AROR_CaptureLocationAction.IsCapturableType(location.GetLocationType())) continue;
			if (!ShouldGroundLocation(location)) continue;
			IEntity child = root.GetChildren();
			while (child)
			{
				IEntitySource wallSource = api.EntityToSource(child);
				if (wallSource && child.FindComponent(AROR_DefenceWallComponent) && child.GetParent() == root)
				{
					wallsFound++;
					vector transform[4];
					child.GetWorldTransform(transform);
					float height;
					if (GroundHeight(world, child, transform[3], height))
					{
						vector mins, maxs;
						child.GetBounds(mins, maxs);
						float bottom = ModelBottom(transform, mins, maxs);
						vector desired = transform[3];
						desired[1] = desired[1] + height + 0.02 - bottom;
						vector local = root.CoordToLocal(desired);
						wallIds.Insert(wallSource.GetID());
						locationIds.Insert(location.GetId());
						positions.Insert(local);
						heights.Insert(height);
					}
					else warnings++;
				}
				child = child.GetSibling();
			}
		}
		if (wallsFound == 0)
		{
			Print("[AROR DEFENCE GROUND] No attached tagged wall children found. Recompile Game/Workbench scripts and reload Chicken AND its defence preset resources from disk. Zero walls is not successful grounding; do not save a stale scene over the updated layers.", LogLevel.WARNING);
			return;
		}
		if (!api.BeginEntityAction("Ground Chicken defence walls"))
		{
			Print("[AROR DEFENCE GROUND] Could not begin editor action; no walls grounded.", LogLevel.WARNING);
			return;
		}
		for (int planned = 0; planned < wallIds.Count(); planned++)
		{
			IEntitySource freshSource = api.FindEntityByID(wallIds[planned]);
			vector localPosition = positions[planned];
			string coords = string.Format("%1 %2 %3", localPosition[0], localPosition[1], localPosition[2]);
			if (freshSource && api.SetVariableValue(freshSource, null, "coords", coords))
			{
				changed++;
				Print(string.Format("[AROR DEFENCE GROUND] id=%1 source=%2 local=%3 terrain=%4", locationIds[planned], wallIds[planned], localPosition, heights[planned]), LogLevel.NORMAL);
			}
			else warnings++;
		}
		api.EndEntityAction("Ground Chicken defence walls");
		Print(string.Format("[AROR DEFENCE GROUND] found=%1 grounded=%2 warnings=%3. Review and Save; flag, markers, location roots and ownership were not edited. Reconcile wall overrides before regeneration.", wallsFound, changed, warnings), LogLevel.NORMAL);
	}
}
