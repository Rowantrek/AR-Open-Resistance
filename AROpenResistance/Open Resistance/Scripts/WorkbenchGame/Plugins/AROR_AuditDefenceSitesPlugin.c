[WorkbenchPluginAttribute(name: "Audit military defence sites", description: "Read-only audit of all 27 military sites. Dense 25m-grid clearing search within each configured radius, eight orientations, with per-site progress. Does not move entities.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_AuditDefenceSitesPlugin : AROR_GroundCampaignFlagsPlugin
{
	//! Capturable Chicken sites: 23 garrisoned installations, three seaports and Fortress Vostok
	protected static const int MILITARY_SITE_COUNT = 27;

	protected void CollectChildren(IEntity root, array<IEntity> entities)
	{
		entities.Insert(root);
		IEntity child = root.GetChildren();
		while (child)
		{
			CollectChildren(child, entities);
			child = child.GetSibling();
		}
	}
	protected vector WorldOffset(vector center, vector local, float yaw)
	{
		float angle = yaw * Math.DEG2RAD;
		return center + Vector(local[0] * Math.Cos(angle) + local[2] * Math.Sin(angle), 0, -local[0] * Math.Sin(angle) + local[2] * Math.Cos(angle));
	}
	protected bool ClearBox(BaseWorld world, vector point, float size, array<IEntity> excludes)
	{
		TraceBox trace = new TraceBox();
		trace.Start = point + Vector(0, 0.15, 0);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-size, 0, -size);
		trace.Maxs = Vector(size, 2.5, size);
		trace.ExcludeArray = excludes;
		trace.Flags = TraceFlags.ENTS | TraceFlags.VISIBILITY;
		return world.TraceMove(trace, null) >= 1;
	}
	protected IEntity AuthoredPosition(IEntity root, int kind, int ordinal)
	{
		IEntity child = root.GetChildren();
		while (child)
		{
			AROR_DefencePositionComponent position = AROR_DefencePositionComponent.Cast(child.FindComponent(AROR_DefencePositionComponent));
			if (position && position.MatchesDefencePosition(kind, ordinal)) return child;
			child = child.GetSibling();
		}
		return null;
	}
	protected bool OverlapsOtherSite(vector center, float half, AROR_LocationComponent site, array<AROR_LocationComponent> locations, float yaw = 0)
	{
		foreach (AROR_LocationComponent other : locations)
		{
			if (!other || other == site || !AROR_CaptureLocationAction.IsCapturableType(other.GetLocationType()))
				continue;
			AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(other.GetOwner().FindComponent(AROR_GarrisonComponent));
			vector otherCenter = other.GetCenter();
			if (garrison)
				otherCenter = garrison.GetDefenceCenter();
			float otherHalf = AROR_DefenceLayout.HalfExtent(other.GetLocationType());
			if (garrison) otherHalf = garrison.GetDefenceHalfExtent();
			vector otherAngles = other.GetOwner().GetYawPitchRoll();
			if (AROR_DefenceLayout.FootprintsOverlap(center, half, yaw, otherCenter, otherHalf, otherAngles[0]))
				return true;
		}
		return false;
	}
	protected int AuditDefenceLevel(AROR_LocationComponent location)
	{
		int level = 3;
		if (location.GetLocationType() == AROR_ELocationType.OUTPOST) level = 1;
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(location.GetOwner().FindComponent(AROR_GarrisonComponent));
		if (garrison && garrison.GetDefenceLevelOverride() > 0)
			level = Math.Clamp(garrison.GetDefenceLevelOverride(), 1, 4);
		if (location.GetLocationType() == AROR_ELocationType.AIRPORT) level = 4;
		return level;
	}
	protected bool CheckSite(BaseWorld world, IEntity root, IEntity flag, AROR_LocationComponent location, vector center, float half, float yaw, out float relief, out int blocked, out int badArcs, bool reportFailures = false)
	{
		blocked = 0;
		badArcs = 0;
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(root.FindComponent(AROR_GarrisonComponent));
		float maxRelief = 2;
		if (garrison) maxRelief = garrison.GetMaxDefenceRelief();
		// Rectangular compounds (narrow sites) keep their own front-to-back depth
		float depth = half;
		if (garrison && garrison.GetDefenceHalfDepth() < garrison.GetDefenceHalfExtent())
			depth = garrison.GetDefenceHalfDepth();
		if (!AROR_DefenceLayout.TerrainFits(world, center, half + 2, maxRelief, relief, yaw, depth + 2))
			return false;
		array<IEntity> ownEntities = {};
		CollectChildren(root, ownEntities);
		array<IEntity> flagOnly = {flag};
		// Check space for the wall run at the same spacing as the authored presets.
		for (int side = 0; side < 4; side++)
		{
			float length = half;
			if (side == 1 || side == 3) length = depth;
			int segments = Math.Ceil(2 * length / 2.5);
			for (int segment = 0; segment < segments; segment++)
			{
				float along = -length + (segment + 0.5) * 2 * length / segments;
				if (side == 2 && Math.AbsFloat(along) < 6.25)
					continue;
				vector local = Vector(along, 0, depth);
				if (side == 1) local = Vector(half, 0, along);
				if (side == 2) local = Vector(along, 0, -depth);
				if (side == 3) local = Vector(-half, 0, along);
				vector point = WorldOffset(center, local, yaw);
				point[1] = world.GetSurfaceY(point[0], point[2]);
				if (!ClearBox(world, point, 1.25, ownEntities))
				{
					blocked++;
					if (reportFailures && blocked <= 8)
						Print(string.Format("[AROR DEFENCE DETAIL] id=%1 wallSide=%2 segment=%3 point=%4 blocked by scene geometry", location.GetId(), side, segment, point), LogLevel.WARNING);
				}
			}
		}
		int level = AuditDefenceLevel(location);
		for (int kind = 0; kind <= 3; kind++)
		{
			int count = 0;
			if (kind == 0) count = AROR_GarrisonPolicy.InfantryTeams(level);
			if (kind == 0 && garrison) count += garrison.GetExtraInfantryTeams();
			if (kind == 1) count = AROR_GarrisonPolicy.StaticCount(level);
			if (kind == 1 && garrison) count = garrison.GetStaticWeaponCount(level);
			if (kind == 2 && level >= 3 && location.GetLocationType() != AROR_ELocationType.OUTPOST) count = 1;
			if (kind == 2 && garrison && !garrison.AllowsParkedArmour()) count = 0;
			if (kind == 2 && garrison && location.GetLocationType() != AROR_ELocationType.OUTPOST) count += garrison.GetExtraParkedCount();
			if (kind == 3 && location.GetLocationType() != AROR_ELocationType.OUTPOST) count = AROR_GarrisonPolicy.MannedCount(level);
			for (int ordinal = 0; ordinal < count; ordinal++)
			{
				vector point = WorldOffset(center, AROR_DefenceLayout.LocalPosition(kind, ordinal, half), yaw);
				float facing = yaw + AROR_DefenceLayout.Facing(kind, ordinal);
				IEntity authored = AuthoredPosition(root, kind, ordinal);
				if (authored)
				{
					vector authoredLocal = root.VectorToLocal(authored.GetOrigin() - flag.GetOrigin());
					point = WorldOffset(center, authoredLocal, yaw);
					vector authoredAngles = authored.GetYawPitchRoll();
					vector rootAngles = root.GetYawPitchRoll();
					facing = yaw + authoredAngles[0] - rootAngles[0];
				}
				point[1] = world.GetSurfaceY(point[0], point[2]);
				float size = 5;
				if (kind == 0) size = 2.5;
				if (kind == 1) size = 1.5;
				if (!ClearBox(world, point, size, flagOnly))
				{
					blocked++;
					if (reportFailures)
						Print(string.Format("[AROR DEFENCE DETAIL] id=%1 kind=%2 ordinal=%3 point=%4 blocked spawn pad", location.GetId(), kind, ordinal, point), LogLevel.WARNING);
				}
				if (kind == 1 && !AROR_DefenceLayout.ClearMGArc(world, point, facing))
				{
					badArcs++;
					if (reportFailures)
						Print(string.Format("[AROR DEFENCE DETAIL] id=%1 kind=1 ordinal=%2 point=%3 facing=%4 blocked gun arc", location.GetId(), ordinal, point, facing), LogLevel.WARNING);
				}
			}
		}
		return blocked == 0 && badArcs == 0;
	}
	override void Run()
	{
		RunSiteAudit(string.Empty);
	}

	//! Both full-island and targeted runs use the dense grid and the same configured
	//! physical footprint checks. Suggestions are evidence, not automatic edits.
	protected void RunSiteAudit(string targetId, bool searchAlternatives = true)
	{
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor) return;
		WorldEditorAPI api = editor.GetApi();
		if (!api || !api.IsEditMode() || api.IsPrefabEditMode()) return;
		string path;
		api.GetWorldPath(path);
		if (!path.Contains("AROR_Everon_Chicken.ent"))
		{
			Print("[AROR DEFENCE AUDIT] Open Chicken in edit mode. This command never edits or moves entities.", LogLevel.WARNING);
			return;
		}
		array<AROR_LocationComponent> locations = {};
		for (int index = 0; index < api.GetEditorEntityCount(); index++)
		{
			IEntitySource source = api.GetEditorEntity(index);
			if (!source || !source.GetName().StartsWith("AROR_Location_everon_")) continue;
			IEntity entity = api.SourceToEntity(source);
			if (!entity) continue;
			AROR_LocationComponent location = AROR_LocationComponent.Cast(entity.FindComponent(AROR_LocationComponent));
			if (location && AROR_CaptureLocationAction.IsCapturableType(location.GetLocationType())) locations.Insert(location);
		}
		if (locations.Count() != MILITARY_SITE_COUNT)
		{
			Print(string.Format("[AROR DEFENCE AUDIT] Expected %1 military sites, found %2; reload Chicken before auditing.", MILITARY_SITE_COUNT, locations.Count()), LogLevel.ERROR);
			return;
		}
		int accepted;
		int suggestions;
		int reviewed;
		array<vector> chosenCenters = {};
		array<float> chosenHalves = {};
		array<float> chosenYaws = {};
		int expected = MILITARY_SITE_COUNT;
		if (!targetId.IsEmpty()) expected = 1;
		if (searchAlternatives)
			Print(string.Format("[AROR DEFENCE AUDIT] begin reviewed=%1 grid=25 radius=per-site orientations=8; read-only, this may take several minutes", expected), LogLevel.NORMAL);
		else
			Print(string.Format("[AROR DEFENCE AUDIT] begin reviewed=%1 mode=current-only; read-only, checking the actual saved placements without relocation search", expected), LogLevel.NORMAL);
		BaseWorld world = api.GetWorld();
		foreach (AROR_LocationComponent site : locations)
		{
			if (!targetId.IsEmpty() && site.GetId() != targetId) continue;
			IEntity root = site.GetOwner();
			IEntity flag = FindFlag(api, root);
			if (!flag)
			{
				Print(string.Format("[AROR DEFENCE AUDIT] id=%1 missing attached capture flag; site not measured", site.GetId()), LogLevel.ERROR);
				continue;
			}
			reviewed++;
			Print(string.Format("[AROR DEFENCE AUDIT] progress=%1/%2 id=%3", reviewed, expected, site.GetId()), LogLevel.NORMAL);
			vector center = flag.GetOrigin();
			vector angles = root.GetYawPitchRoll();
			float half = AROR_DefenceLayout.HalfExtent(site.GetLocationType());
			float maxRelief = 2;
			float searchRadius = 800;
			AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(root.FindComponent(AROR_GarrisonComponent));
			if (garrison)
			{
				half = garrison.GetDefenceHalfExtent();
				maxRelief = garrison.GetMaxDefenceRelief();
				searchRadius = garrison.GetEditorSearchRadius();
			}
			bool found = false;
			int rings = searchRadius / 25;
			if (rings * 25 < searchRadius) rings++;
			int side = rings * 2 + 1;
			int candidateCount = side * side; // Complete square grid, radially filtered below.
			if (!searchAlternatives) candidateCount = 1;
			Print(string.Format("[AROR DEFENCE AUDIT] id=%1 searchRadius=%2", site.GetId(), searchRadius), LogLevel.NORMAL);
			for (int candidate = 0; candidate < candidateCount && !found; candidate++)
			{
				vector offset = AROR_DefenceLayout.DenseSiteSearchOffset(candidate);
				if (vector.DistanceXZ(vector.Zero, offset) > searchRadius) continue;
				vector proposed = center + offset;
				if (garrison && !garrison.IsDefenceCenterAllowed(proposed, half)) continue;
				// These are proposed NEW location centres, so the old capture circle is not a search boundary.
				int orientations = 8;
				if (!searchAlternatives) orientations = 1;
				for (int orientation = 0; orientation < orientations; orientation++)
				{
					float proposedYaw = angles[0] + orientation * 45;
					float relief;
					int blocked, badArcs;
					bool reportFailures = candidate == 0 && orientation == 0;
					bool clear = CheckSite(world, root, flag, site, proposed, half, proposedYaw, relief, blocked, badArcs, reportFailures);
					bool overlap = OverlapsOtherSite(proposed, half, site, locations, proposedYaw);
					if (clear && !overlap)
					{
						for (int chosen = 0; chosen < chosenCenters.Count(); chosen++)
						{
							if (AROR_DefenceLayout.FootprintsOverlap(proposed, half, proposedYaw, chosenCenters[chosen], chosenHalves[chosen], chosenYaws[chosen]))
								overlap = true;
						}
					}
					proposed[1] = world.GetSurfaceY(proposed[0], proposed[2]);
					if (candidate == 0 && orientation == 0)
						Print(string.Format("[AROR DEFENCE AUDIT] id=%1 current=%2 half=%3 relief=%4 blocked=%5 badArcs=%6 overlap=%7 suitable=%8 limit=%9", site.GetId(), proposed, half, relief, blocked, badArcs, overlap, clear && !overlap, maxRelief), LogLevel.NORMAL);
					if (!clear || overlap) continue;
					found = true;
					chosenCenters.Insert(proposed);
					chosenHalves.Insert(half);
					chosenYaws.Insert(proposedYaw);
					if (candidate == 0 && orientation == 0) accepted++;
					else
					{
						suggestions++;
						Print(string.Format("[AROR DEFENCE AUDIT] id=%1 proposed=%2 yaw=%3 half=%4 relief=%5 candidate=%6; inspect access/model fit, then reconcile the CSV before applying", site.GetId(), proposed, proposedYaw, half, relief, candidate), LogLevel.NORMAL);
					}
					break;
				}
			}
			if (!found)
			{
				if (searchAlternatives)
					Print(string.Format("[AROR DEFENCE AUDIT] id=%1 needs an authored clearing or smaller tailored compound; no suitable bounded candidate", site.GetId()), LogLevel.WARNING);
				else
					Print(string.Format("[AROR DEFENCE AUDIT] id=%1 current footprint needs correction; no relocation search requested", site.GetId()), LogLevel.WARNING);
			}
		}
		Print(string.Format("[AROR DEFENCE AUDIT] current suitable=%1 suggested relocations=%2 reviewed=%3. No files/entities changed; driving, navmesh and actual model joints remain Play checks.", accepted, suggestions, reviewed), LogLevel.NORMAL);
	}
}
