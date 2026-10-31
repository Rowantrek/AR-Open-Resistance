[WorkbenchPluginAttribute(name: "Find covered factory and resource sites", description: "Read-only survey of the three Chicken economic sites. Rank nearby terrain/object screening, then verify the existing footprint, pads and gun arcs. Leaves all entities, including Chotain's castle, unchanged.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_FindCoveredDefenceSitesPlugin : AROR_AuditDefenceSitesPlugin
{
	//! Screening outside the proposed perimeter. Physical traces carry more weight
	//! than visibility-only foliage; neither constitutes a ballistic penetration test.
	protected int CoverScore(BaseWorld world, vector center, float half, float yaw, array<IEntity> excludes, out int hard, out int concealment, out int terrain)
	{
		hard = 0;
		concealment = 0;
		terrain = 0;
		for (int sector = 0; sector < 8; sector++)
		{
			float angle = (yaw + sector * 45) * Math.DEG2RAD;
			vector direction = Vector(Math.Sin(angle), 0, Math.Cos(angle));
			float edge = half / Math.Max(Math.AbsFloat(direction[0]), Math.AbsFloat(direction[2])) + 5;
			vector start = center + direction * edge;
			start[1] = world.GetSurfaceY(start[0], start[2]) + 1.4;
			TraceParam trace = new TraceParam();
			trace.Start = start;
			trace.End = start + direction * 60;
			trace.ExcludeArray = excludes;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			float physical = world.TraceMove(trace, null);
			if (physical >= 0.08 && physical < 0.95)
				hard++;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS | TraceFlags.VISIBILITY;
			float visibility = world.TraceMove(trace, null);
			if (physical >= 0.95 && visibility >= 0.08 && visibility < 0.95)
				concealment++;
			bool rising;
			for (int sample = 1; sample <= 3; sample++)
			{
				vector point = start + direction * (sample * 20);
				if (world.GetSurfaceY(point[0], point[2]) > start[1] + 0.3)
					rising = true;
			}
			if (rising)
				terrain++;
		}
		return hard * 10 + concealment * 2 + terrain * 3;
	}

	override void Run()
	{
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor) return;
		WorldEditorAPI api = editor.GetApi();
		if (!api || !api.IsEditMode() || api.IsPrefabEditMode()) return;
		string path;
		api.GetWorldPath(path);
		if (!path.Contains("AROR_Everon_Chicken.ent"))
		{
			Print("[AROR COVER AUDIT] Open Chicken in edit mode; no scene changes are made.", LogLevel.WARNING);
			return;
		}
		array<AROR_LocationComponent> locations = {};
		array<AROR_LocationComponent> economic = {};
		for (int index = 0; index < api.GetEditorEntityCount(); index++)
		{
			IEntitySource source = api.GetEditorEntity(index);
			if (!source || !source.GetName().StartsWith("AROR_Location_everon_")) continue;
			IEntity entity = api.SourceToEntity(source);
			if (!entity) continue;
			AROR_LocationComponent location = AROR_LocationComponent.Cast(entity.FindComponent(AROR_LocationComponent));
			if (!location || !AROR_CaptureLocationAction.IsCapturableType(location.GetLocationType())) continue;
			locations.Insert(location);
			if (location.GetLocationType() == AROR_ELocationType.FACTORY || location.GetLocationType() == AROR_ELocationType.RESOURCES)
				economic.Insert(location);
		}
		if (economic.Count() != 3)
		{
			Print(string.Format("[AROR COVER AUDIT] Expected three factory/resource sites, found %1; reload Chicken.", economic.Count()), LogLevel.ERROR);
			return;
		}
		Print("[AROR COVER AUDIT] begin sites=3 grid=50 orientations=8 radius=per-site; read-only", LogLevel.NORMAL);
		BaseWorld world = api.GetWorld();
		array<vector> chosen = {};
		array<float> halves = {};
		array<float> yaws = {};
		int reviewed, proposedCount;
		foreach (AROR_LocationComponent site : economic)
		{
			IEntity root = site.GetOwner();
			IEntity flag = FindFlag(api, root);
			AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(root.FindComponent(AROR_GarrisonComponent));
			if (!flag || !garrison) continue;
			vector center = flag.GetOrigin();
			vector angles = root.GetYawPitchRoll();
			float half = garrison.GetDefenceHalfExtent();
			float radius = Math.Min(1200, garrison.GetEditorSearchRadius());
			array<IEntity> excludes = {};
			CollectChildren(root, excludes);
			int hard, concealment, terrain;
			int currentScore = CoverScore(world, center, half, angles[0], excludes, hard, concealment, terrain);
			Print(string.Format("[AROR COVER AUDIT] id=%1 current=%2 half=%3 score=%4 hard=%5 concealment=%6 terrain=%7 radius=%8", site.GetId(), center, half, currentScore, hard, concealment, terrain, radius), LogLevel.NORMAL);
			int bestScore = currentScore + 4; // Require a material improvement over the current surroundings.
			vector best;
			float bestYaw, bestRelief;
			int bestHard, bestConcealment, bestTerrain;
			bool found;
			int rings = Math.Ceil(radius / 50);
			int width = rings * 2 + 1;
			for (int candidate = 0; candidate < width * width; candidate++)
			{
				vector offset = AROR_DefenceLayout.DenseSiteSearchOffset(candidate) * 2;
				if (vector.DistanceXZ(vector.Zero, offset) > radius) continue;
				vector point = center + offset;
				point[1] = world.GetSurfaceY(point[0], point[2]);
				for (int orientation = 0; orientation < 8; orientation++)
				{
					float yaw = angles[0] + orientation * 45;
					float relief;
					if (!AROR_DefenceLayout.TerrainFits(world, point, half + 2, garrison.GetMaxDefenceRelief(), relief, yaw)) continue;
					int score = CoverScore(world, point, half, yaw, excludes, hard, concealment, terrain);
					score -= Math.Floor(vector.DistanceXZ(center, point) / 100); // Prefer equally sheltered sites nearby.
					if (hard < 2 || hard > 6 || score <= bestScore) continue;
					if (OverlapsOtherSite(point, half, site, locations, yaw)) continue;
					bool overlap;
					for (int prior = 0; prior < chosen.Count(); prior++)
					{
						if (AROR_DefenceLayout.FootprintsOverlap(point, half, yaw, chosen[prior], halves[prior], yaws[prior])) overlap = true;
					}
					if (overlap) continue;
					int blocked, badArcs;
					if (!CheckSite(world, root, flag, site, point, half, yaw, relief, blocked, badArcs)) continue;
					best = point;
					bestYaw = yaw;
					bestRelief = relief;
					bestScore = score;
					bestHard = hard;
					bestConcealment = concealment;
					bestTerrain = terrain;
					found = true;
				}
			}
			reviewed++;
			if (found)
			{
				chosen.Insert(best);
				halves.Insert(half);
				yaws.Insert(bestYaw);
				proposedCount++;
				Print(string.Format("[AROR COVER AUDIT] id=%1 proposed=%2 yaw=%3 half=%4 score=%5 hard=%6 concealment=%7 terrain=%8 relief=%9", site.GetId(), best, bestYaw, half, bestScore, bestHard, bestConcealment, bestTerrain, bestRelief), LogLevel.NORMAL);
			}
			else Print(string.Format("[AROR COVER AUDIT] id=%1 no materially better covered footprint found; retain current placement pending manual review", site.GetId()), LogLevel.WARNING);
		}
		Print(string.Format("[AROR COVER AUDIT] complete reviewed=%1 proposed=%2; no entities/files changed; driving and ballistic cover require native inspection", reviewed, proposedCount), LogLevel.NORMAL);
	}
}
