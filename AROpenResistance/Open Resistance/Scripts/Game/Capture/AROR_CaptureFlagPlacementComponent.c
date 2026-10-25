[ComponentEditorProps(category: "OpenResistance/Locations", description: "Server places the capture pole on clear nearby walkable terrain, then replicates its world position.")]
class AROR_CaptureFlagPlacementComponentClass : ScriptComponentClass {}

class AROR_CaptureFlagPlacementComponent : ScriptComponent
{
	[Attribute("1", desc: "Find a clear grounded position for this capture pole during Play")]
	protected bool m_bEnabled;
	[Attribute("60", desc: "Maximum search distance from the location centre; also limited to half its radius", params: "5 100 5")]
	protected float m_fSearchRadius;
	[RplProp()]
	protected vector m_vPlacedPosition;
	[RplProp(onRplName: "ApplyPlacement")]
	protected bool m_bPlaced;
	protected int m_iAttempt;
	protected bool m_bHasClearCandidate;
	protected vector m_vClearCandidate;
	static vector Candidate(vector center, float radius, int ordinal)
	{
		if (ordinal <= 0)
			return center;
		int index = ordinal - 1;
		int ring = index / 8 + 1;
		int sector = index - (ring - 1) * 8;
		float angle = sector * 0.78539816;
		return center + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * (radius * ring / 4);
	}

	protected AROR_LocationComponent FindLocation()
	{
		IEntity parent = GetOwner().GetParent();
		while (parent)
		{
			AROR_LocationComponent location = AROR_LocationComponent.Cast(parent.FindComponent(AROR_LocationComponent));
			if (location)
				return location;
			parent = parent.GetParent();
		}
		return null;
	}

	protected bool IsClear(vector point)
	{
		BaseWorld world = GetGame().GetWorld();
		float height = world.GetSurfaceY(point[0], point[2]);
		if (height <= world.GetOceanHeight(point[0], point[2]) + 0.25)
			return false;
		for (int side = 0; side < 4; side++)
		{
			float angle = side * 1.57079633;
			float sample = world.GetSurfaceY(point[0] + Math.Sin(angle) * 1.5, point[2] + Math.Cos(angle) * 1.5);
			if (Math.AbsFloat(sample - height) > 0.6)
				return false;
		}
		TraceBox trace = new TraceBox();
		trace.Start = Vector(point[0], height + 0.15, point[2]);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-1.5, 0, -1.5);
		trace.Maxs = Vector(1.5, 7, 1.5);
		trace.Exclude = GetOwner();
		trace.Flags = TraceFlags.ENTS;
		return world.TraceMove(trace, null) >= 1;
	}

	protected void ApplyPlacement()
	{
		if (!m_bPlaced || !GetOwner())
			return;
		vector transform[4];
		GetOwner().GetWorldTransform(transform);
		transform[3] = m_vPlacedPosition;
		GetOwner().SetWorldTransform(transform);
		// Flags have static collision: commit the child transform, not just its generic parent.
		GetOwner().Update();
	}

	protected void TryPlace()
	{
		if (!Replication.IsServer() || m_bPlaced)
			return;
		AROR_LocationComponent location = FindLocation();
		if (!location)
			return;
		BaseWorld world = GetGame().GetWorld();
		vector center = location.GetCenter();
		float radius = Math.Min(m_fSearchRadius, location.GetRadius() * 0.5);
		vector point = Candidate(center, radius, m_iAttempt);
		point[1] = world.GetSurfaceY(point[0], point[2]) + 0.1;
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		NavmeshWorldComponent soldiers;
		if (aiWorld)
			soldiers = aiWorld.GetNavmeshWorldComponent("Soldiers");
		bool walkable;
		vector nearby;
		if (soldiers)
		{
			if (!soldiers.IsTileLoaded(point))
				soldiers.LoadTileIn(point);
			else if (soldiers.GetReachablePoint(point, 2, nearby))
				walkable = vector.DistanceXZ(point, nearby) <= 3 && Math.AbsFloat(point[1] - nearby[1]) <= 1;
		}
		bool clear = IsClear(point);
		if (clear && !m_bHasClearCandidate)
		{
			m_bHasClearCandidate = true;
			m_vClearCandidate = point;
		}
		if (walkable && clear)
		{
			m_vPlacedPosition = point;
			m_bPlaced = true;
			ApplyPlacement();
			Replication.BumpMe();
			Print(string.Format("[AROR] Capture flag placed: location=%1 position=%2 offset=%3 attempts=%4", location.GetId(), point, vector.DistanceXZ(point, center), m_iAttempt + 1), LogLevel.NORMAL);
			return;
		}
		m_iAttempt++;
		if (m_iAttempt < 33)
			GetGame().GetCallqueue().CallLater(TryPlace, 250, false);
		else
		{
			// Make a failed search visible instead of leaving the static pole below ground.
			center[1] = world.GetSurfaceY(center[0], center[2]) + 0.1;
			if (m_bHasClearCandidate)
				center = m_vClearCandidate;
			m_vPlacedPosition = center;
			m_bPlaced = true;
			ApplyPlacement();
			Replication.BumpMe();
			Print(string.Format("[AROR] Capture flag needs placement verification: location=%1 position=%2 clear=%3; no navmesh-verified candidate", location.GetId(), center, m_bHasClearCandidate), LogLevel.WARNING);
		}
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode() || !m_bEnabled)
			return;
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(TryPlace, 250, false);
		else
			ApplyPlacement();
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(TryPlace);
		super.OnDelete(owner);
	}
}
