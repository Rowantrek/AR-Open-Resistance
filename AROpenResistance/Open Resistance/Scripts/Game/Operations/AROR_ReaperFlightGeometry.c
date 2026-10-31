//! Bounded candidate search, not a guarantee that the flight controller can land here.
class AROR_ReaperFlightGeometry
{
	static const int LANDING_CLEAR = 0;
	static const int LANDING_WATER = 1;
	static const int LANDING_SLOPE = 2;
	static const int LANDING_OBSTACLE = 3;
	static vector Direction(vector from, vector to)
	{
		vector direction = to - from;
		direction[1] = 0;
		if (direction.Length() < 1)
			return "0 0 1";
		return direction.Normalized();
	}

	static vector FormationPoint(vector leader, vector heading, float spacing)
	{
		vector right = Vector(heading[2], 0, -heading[0]);
		return leader - heading * spacing + right * spacing;
	}

	//! Centre followed by six rings of twelve samples (73 unique candidates).
	static vector LandingCandidate(vector center, float maxRadius, int ordinal, float fallbackRadius = 0)
	{
		if (ordinal <= 0)
			return center;
		// Preserve the original near-target search, then try four staggered outer
		// rings. Ports can have water/buildings across the entire inner search.
		if (ordinal >= 73 && fallbackRadius > maxRadius)
		{
			int outer = ordinal - 73;
			int outerRing = outer / 12 + 1;
			int outerSector = outer - (outerRing - 1) * 12;
			float outerAngle = (outerSector + 0.5) * 6.2831853 / 12;
			float radius = Math.Lerp(maxRadius, fallbackRadius, outerRing / 4.0);
			return center + Vector(Math.Sin(outerAngle), 0, Math.Cos(outerAngle)) * radius;
		}
		int index = ordinal - 1;
		int ring = index / 12 + 1;
		int sector = index - (ring - 1) * 12;
		float angle = sector * 6.2831853 / 12;
		return center + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * (maxRadius * ring / 6);
	}

	//! Air insertion checks the actual aircraft volume, not a flat ground pad.
	static bool IsAirspaceClear(vector point, float height, float clearance)
	{
		BaseWorld world = GetGame().GetWorld();
		point[1] = world.GetSurfaceY(point[0], point[2]) + height;
		TraceBox trace = new TraceBox();
		trace.Start = point;
		trace.End = point + Vector(0, 1, 0);
		trace.Mins = Vector(-clearance, -5, -clearance);
		trace.Maxs = Vector(clearance, 10, clearance);
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		return world.TraceMove(trace, null) >= 1;
	}

	static bool HasPadSpacing(vector point, array<vector> reservedPads, float spacing)
	{
		if (!reservedPads)
			return true;
		foreach (vector pad : reservedPads)
		{
			if (vector.DistanceXZ(point, pad) < spacing)
				return false;
		}
		return true;
	}

	static bool IsLandingClear(vector point, float clearance)
	{
		return LandingRejectionReason(point, clearance) == LANDING_CLEAR;
	}

	//! Same safety checks for planning and final approach, with a reason for diagnostics.
	static int LandingRejectionReason(vector point, float clearance)
	{
		BaseWorld world = GetGame().GetWorld();
		float height = world.GetSurfaceY(point[0], point[2]);
		if (height <= world.GetOceanHeight(point[0], point[2]) + 1)
			return LANDING_WATER;
		// Sample the complete pad grid, rejecting pronounced slopes/terrain variation.
		for (int x = -2; x <= 2; x++)
		{
			for (int z = -2; z <= 2; z++)
			{
				float sampleX = point[0] + x * clearance / 2;
				float sampleZ = point[2] + z * clearance / 2;
				float sample = world.GetSurfaceY(sampleX, sampleZ);
				if (sample <= world.GetOceanHeight(sampleX, sampleZ) + 1)
					return LANDING_WATER;
				if (Math.AbsFloat(sample - height) > 2)
					return LANDING_SLOPE;
			}
		}
		// Sweep a rotor-sized box down the final vertical corridor. ENTS catches
		// structures, trees and vehicles; terrain was checked separately above.
		TraceBox trace = new TraceBox();
		trace.Start = Vector(point[0], height + 65, point[2]);
		trace.End = Vector(point[0], height + 3, point[2]);
		trace.Mins = Vector(-clearance, -1, -clearance);
		trace.Maxs = Vector(clearance, 10, clearance);
		trace.Flags = TraceFlags.ENTS;
		if (world.TraceMove(trace, null) < 1)
			return LANDING_OBSTACLE;
		return LANDING_CLEAR;
	}
}
