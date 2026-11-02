//! Shared compact layouts for legacy sites and placeable military composition presets.
class AROR_DefenceLayout
{
	//! Separating-axis test for two rotated square perimeters with an access gap.
	//! A bounding circle incorrectly rejects the adjacent rotated resource layouts.
	static bool FootprintsOverlap(vector center, float halfExtent, float yaw, vector otherCenter, float otherHalfExtent, float otherYaw, float gap = 8)
	{
		float dx = otherCenter[0] - center[0];
		float dz = otherCenter[2] - center[2];
		for (int axis = 0; axis < 4; axis++)
		{
			float angle = yaw + axis * 90;
			if (axis >= 2) angle = otherYaw + (axis - 2) * 90;
			float radians = angle * Math.DEG2RAD;
			float distance = Math.AbsFloat(dx * Math.Cos(radians) - dz * Math.Sin(radians));
			float first = (yaw - angle) * Math.DEG2RAD;
			float second = (otherYaw - angle) * Math.DEG2RAD;
			float extent = halfExtent * (Math.AbsFloat(Math.Cos(first)) + Math.AbsFloat(Math.Sin(first)));
			extent += otherHalfExtent * (Math.AbsFloat(Math.Cos(second)) + Math.AbsFloat(Math.Sin(second)));
			if (distance > extent + gap)
				return false;
		}
		return true;
	}
	static float HalfExtent(AROR_ELocationType type)
	{
		switch (type)
		{
			case AROR_ELocationType.OUTPOST: return 18;
			case AROR_ELocationType.MILITARY_BASE: return 32;
			case AROR_ELocationType.AIRPORT: return 44;
			case AROR_ELocationType.RESOURCES: return 26;
		}
		return 28;
	}
	static vector LocalPosition(int kind, int ordinal, float halfExtent)
	{
		float ring = halfExtent - 8;
		if (kind == 0)
		{
			if (ordinal >= 4) ring = halfExtent - 12; // Cardinal teams need >=7m from perimeter MGs.
			// Opposed first teams; later teams fill intermediate sectors.
			array<float> bearings = {45, 225, 135, 315, 0, 180, 90, 270};
			int sector = Math.Mod(ordinal, 8);
			float bearing = bearings[sector] * Math.DEG2RAD;
			return Vector(Math.Sin(bearing) * ring, 0, Math.Cos(bearing) * ring);
		}
		if (kind == 1)
		{
			array<vector> directions = {"0 0 1", "-1 0 0", "1 0 0", "0 0 -1"};
			int side = Math.Mod(ordinal, 4);
			return directions[side] * (halfExtent - 4);
		}
		if (kind == 2)
			return Vector(-10, 0, -5);
		if (kind == 3)
		{
			array<vector> pads = {"10 0 -5", "-10 0 8", "10 0 8"};
			int pad = Math.Mod(ordinal, 3);
			return pads[pad];
		}
		// Staging points remain inside the perimeter, away from gun and hull footprints.
		float angle = Math.Mod(ordinal, 8) * 45 * Math.DEG2RAD;
		return Vector(Math.Sin(angle) * 4, 0, Math.Cos(angle) * 4);
	}
	static float Facing(int kind, int ordinal)
	{
		if (kind == 1)
		{
			array<float> bearings = {0, 270, 90, 180};
			int side = Math.Mod(ordinal, 4);
			return bearings[side];
		}
		if (kind == 2 || kind == 3)
			return 180; // Face the vehicle-width south entrance.
		return ordinal * 45;
	}
	//! A point check is insufficient: sample the complete prospective wall/defence footprint.
	//! A positive halfDepth makes the footprint rectangular (local front-to-back), otherwise it is square.
	static bool TerrainFits(BaseWorld world, vector center, float halfExtent, float maxRelief, out float relief, float yaw = 0, float halfDepth = 0)
	{
		float low = float.MAX;
		float high = -float.MAX;
		float angle = yaw * Math.DEG2RAD;
		if (halfDepth <= 0)
			halfDepth = halfExtent;
		for (int x = -3; x <= 3; x++)
		{
			for (int z = -3; z <= 3; z++)
			{
				float dx = x * halfExtent / 3;
				float dz = z * halfDepth / 3;
				float px = center[0] + dx * Math.Cos(angle) + dz * Math.Sin(angle);
				float pz = center[2] - dx * Math.Sin(angle) + dz * Math.Cos(angle);
				float height = world.GetSurfaceY(px, pz);
				if (height <= world.GetOceanHeight(px, pz) + 0.3)
				{
					relief = float.MAX;
					return false;
				}
				low = Math.Min(low, height);
				high = Math.Max(high, height);
			}
		}
		relief = high - low;
		return relief <= maxRelief;
	}
	//! Small local repair offsets only; a blocked slot must not escape across the location radius.
	static vector RepairOffset(int attempt)
	{
		if (attempt == 0)
			return vector.Zero;
		float angle = Math.Mod(attempt - 1, 8) * 45 * Math.DEG2RAD;
		int ring = Math.Min(3, (attempt - 1) / 8 + 1);
		return Vector(Math.Sin(angle) * ring * 1.5, 0, Math.Cos(angle) * ring * 1.5);
	}
	static bool ClearMGArc(BaseWorld world, vector point, float yaw)
	{
		for (int ray = 0; ray < 3; ray++)
		{
			float angle = (yaw + (ray - 1) * 15) * Math.DEG2RAD;
			vector direction = Vector(Math.Sin(angle), 0, Math.Cos(angle));
			TraceParam trace = new TraceParam();
			trace.Start = point + Vector(0, 1.4, 0);
			trace.End = trace.Start + direction * 20;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS | TraceFlags.VISIBILITY;
			if (world.TraceMove(trace, null) < 1)
				return false;
			// Tripod guns fire from about half a metre: even low sandbags right in front block them. Props only,
			// so rising terrain further out does not reject an otherwise usable pit.
			TraceParam low = new TraceParam();
			low.Start = point + Vector(0, TRIPOD_HEIGHT, 0);
			low.End = low.Start + direction * TRIPOD_CHECK_DISTANCE;
			low.Flags = TraceFlags.ENTS | TraceFlags.VISIBILITY;
			if (world.TraceMove(low, null) < 1)
				return false;
		}
		return true;
	}
	static const float TRIPOD_HEIGHT = 0.6;			//!< Approximate muzzle height of a tripod static gun
	static const float TRIPOD_CHECK_DISTANCE = 8;	//!< Covers the gun pit and its perimeter wall
	//! Read-only editor relocation search: sixteen 25m rings, eight bearings per ring.
	static vector SiteSearchOffset(int candidate)
	{
		if (candidate <= 0)
			return vector.Zero;
		int ring = (candidate - 1) / 8 + 1;
		float angle = Math.Mod(candidate - 1, 8) * 45 * Math.DEG2RAD;
		return Vector(Math.Sin(angle) * ring * 25, 0, Math.Cos(angle) * ring * 25);
	}
	//! Dense single-site audit: complete square rings on a 25m grid. The caller
	//! restricts the radial distance; this function never moves runtime actors.
	static vector DenseSiteSearchOffset(int candidate)
	{
		if (candidate <= 0)
			return vector.Zero;
		int remaining = candidate - 1;
		int ring = 1;
		while (remaining >= ring * 8)
		{
			remaining -= ring * 8;
			ring++;
		}
		int side = remaining / (ring * 2);
		float along = Math.Mod(remaining, ring * 2) - ring;
		float edge = ring * 25;
		if (side == 0) return Vector(along * 25, 0, edge);
		if (side == 1) return Vector(edge, 0, -along * 25);
		if (side == 2) return Vector(-along * 25, 0, -edge);
		return Vector(-edge, 0, along * 25);
	}
}
