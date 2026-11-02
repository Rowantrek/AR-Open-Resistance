[WorkbenchPluginAttribute(name: "Find stronghold helipad clearings", description: "Read-only. Checks every helipad in Chicken's compounds against buildings, trees, poles and walls (rotor disc at rotor height, fuselage along the pad heading) and lists clear alternative spots inside each compound. Does not move entities.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_FindHelipadClearingsPlugin : AROR_AuditDefenceSitesPlugin
{
	protected static const float GRID = 2;
	protected static const int MAX_SUGGESTIONS = 8;

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor) return;
		WorldEditorAPI api = editor.GetApi();
		if (!api || !api.IsEditMode() || api.IsPrefabEditMode())
		{
			Print("[AROR HELIPAD] Stop Play and open Chicken in world edit mode first.", LogLevel.WARNING);
			return;
		}
		BaseWorld world = api.GetWorld();
		int sites;
		for (int index = 0; index < api.GetEditorEntityCount(); index++)
		{
			IEntitySource source = api.GetEditorEntity(index);
			if (!source || !source.GetName().StartsWith("AROR_Location_everon_")) continue;
			IEntity root = api.SourceToEntity(source);
			if (!root) continue;
			AROR_LocationComponent location = AROR_LocationComponent.Cast(root.FindComponent(AROR_LocationComponent));
			AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(root.FindComponent(AROR_GarrisonComponent));
			if (!location || !garrison) continue;
			array<IEntity> pads = {};
			array<IEntity> markers = {};
			IEntity child = root.GetChildren();
			while (child)
			{
				if (child.FindComponent(AROR_LandingPadComponent))
					pads.Insert(child);
				else if (child.FindComponent(AROR_DefencePositionComponent))
					markers.Insert(child);
				child = child.GetSibling();
			}
			if (pads.IsEmpty()) continue;
			ReviewSite(world, root, location, garrison, pads, markers);
			sites++;
		}
		Print(string.Format("[AROR HELIPAD] reviewed %1 compounds with helipads. Read-only; nothing moved.", sites), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReviewSite(BaseWorld world, IEntity root, AROR_LocationComponent location, AROR_GarrisonComponent garrison, array<IEntity> pads, array<IEntity> markers)
	{
		array<IEntity> padEntities = {};
		foreach (IEntity pad : pads)
			CollectChildren(pad, padEntities);
		AROR_LandingPadComponent first = AROR_LandingPadComponent.Cast(pads[0].FindComponent(AROR_LandingPadComponent));
		float radius = first.GetRotorRadius();
		float length = first.GetLength();
		vector rootAngles = root.GetYawPitchRoll();
		float yaw = rootAngles[0];
		vector center = garrison.GetDefenceCenter();

		foreach (int padIndex, IEntity pad : pads)
		{
			vector padAngles = pad.GetYawPitchRoll();
			string blocked = Obstruction(world, pad.GetOrigin(), padAngles[0], radius, length, padEntities);
			string state = "clear";
			if (!blocked.IsEmpty())
				state = "BLOCKED by " + blocked;
			Print(string.Format("[AROR HELIPAD] id=%1 pad=%2 local=%3 heading=%4 rotor=%5 length=%6 %7", location.GetId(), padIndex, root.CoordToLocal(pad.GetOrigin()), padAngles[0] - yaw, radius, length, state), LogLevel.NORMAL);
		}

		// Clear spots inside the compound, nearest the centre first, apart from each other and from the other
		// defence positions (the pads' own parked-vehicle markers excepted)
		float halfWidth = garrison.GetDefenceHalfExtent() - 2;
		float halfDepth = garrison.GetDefenceHalfDepth() - 2;
		int rings = Math.Ceil(Math.Max(halfWidth, halfDepth) / GRID);
		array<vector> chosen = {};
		array<int> headings = {0, 90, 45, 135};
		for (int ring = 0; ring <= rings && chosen.Count() < MAX_SUGGESTIONS; ring++)
		{
			for (int ix = -ring; ix <= ring && chosen.Count() < MAX_SUGGESTIONS; ix++)
			{
				for (int iz = -ring; iz <= ring && chosen.Count() < MAX_SUGGESTIONS; iz++)
				{
					if (Math.AbsInt(ix) != ring && Math.AbsInt(iz) != ring)
						continue;
					vector local = Vector(ix * GRID, 0, iz * GRID);
					if (Math.AbsFloat(local[0]) > halfWidth || Math.AbsFloat(local[2]) > halfDepth)
						continue;
					vector point = WorldOffset(center, local, yaw);
					point[1] = world.GetSurfaceY(point[0], point[2]);
					if (!AwayFromMarkers(point, markers, pads, radius))
						continue;
					bool spaced = true;
					foreach (vector previous : chosen)
					{
						if (vector.DistanceXZ(previous, local) < 2 * radius + 2)
							spaced = false;
					}
					if (!spaced)
						continue;
					foreach (int heading : headings)
					{
						if (!Obstruction(world, point, yaw + heading, radius, length, padEntities).IsEmpty())
							continue;
						chosen.Insert(local);
						Print(string.Format("[AROR HELIPAD] id=%1 clear local=%2 %3 heading=%4", location.GetId(), local[0], local[2], heading), LogLevel.NORMAL);
						break;
					}
				}
			}
		}
		if (chosen.IsEmpty())
			Print(string.Format("[AROR HELIPAD] id=%1 no clear helipad spot inside the compound for rotor=%2 length=%3", location.GetId(), radius, length), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Rotor disc clear of guns, hulls, teams and crews (a pad's own parked-vehicle marker sits on it)
	protected bool AwayFromMarkers(vector point, array<IEntity> markers, array<IEntity> pads, float radius)
	{
		foreach (IEntity marker : markers)
		{
			bool onPad;
			foreach (IEntity pad : pads)
			{
				if (vector.DistanceXZ(marker.GetOrigin(), pad.GetOrigin()) < 1)
					onPad = true;
			}
			if (!onPad && vector.DistanceXZ(marker.GetOrigin(), point) < radius + 2)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Empty when a helicopter fits; otherwise what is in the way
	protected string Obstruction(BaseWorld world, vector point, float heading, float radius, float length, array<IEntity> excludes)
	{
		float ground = world.GetSurfaceY(point[0], point[2]);
		vector base = Vector(point[0], ground, point[2]);
		if (!ClearSpan(world, base, radius, AROR_GarrisonComponent.ROTOR_CLEARANCE_BOTTOM, AROR_GarrisonComponent.ROTOR_CLEARANCE_TOP, excludes))
			return "something at rotor height";
		float angle = heading * Math.DEG2RAD;
		vector direction = Vector(Math.Sin(angle), 0, Math.Cos(angle));
		for (float along = -length * 0.5; along <= length * 0.5; along += 3)
		{
			vector body = base + direction * along;
			body[1] = world.GetSurfaceY(body[0], body[2]);
			if (Math.AbsFloat(body[1] - ground) > 1.5)
				return "slope";
			if (!ClearSpan(world, body, 2, 0.3, 3, excludes))
				return "something at fuselage height";
		}
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ClearSpan(BaseWorld world, vector base, float half, float bottom, float top, array<IEntity> excludes)
	{
		TraceBox trace = new TraceBox();
		trace.Start = base + Vector(0, bottom, 0);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-half, 0, -half);
		trace.Maxs = Vector(half, top - bottom, half);
		trace.ExcludeArray = excludes;
		trace.Flags = TraceFlags.ENTS | TraceFlags.VISIBILITY;
		return world.TraceMove(trace, null) >= 1;
	}
}
