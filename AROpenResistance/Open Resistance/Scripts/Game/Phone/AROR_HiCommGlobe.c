//! HICOMM login screen's rotating wireframe globe: latitude/longitude lines, relay nodes and pulsing data arcs
//! between them, drawn on a CanvasWidget (screen pixels). Purely decorative; client only.
class AROR_HiCommGlobe
{
	protected static const float SPIN_SPEED = 0.18;	//!< Radians per second
	protected static const float TILT = 0.38;			//!< Axis tilt towards the viewer (radians)

	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};

	// Projection for the current frame
	protected float m_fCentreX;
	protected float m_fCentreY;
	protected float m_fRadius;
	protected float m_fSpin;

	//------------------------------------------------------------------------------------------------
	//! time: seconds since the screen opened; pixel: canvas pixels per app unit
	void Draw(CanvasWidget canvas, float width, float height, float time, float pixel)
	{
		m_aCommands.Clear();
		m_fCentreX = width * 0.5;
		m_fCentreY = height * 0.5;
		m_fRadius = Math.Min(width, height) * 0.42;
		m_fSpin = time * SPIN_SPEED;

		// Atmosphere glow and disc
		AddCircle(m_fRadius * 1.06, AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 0.05), true, 1);
		AddCircle(m_fRadius, AROR_HiCommMap.Srgb(6, 18, 12, 0.95), true, 1);
		AddCircle(m_fRadius, AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 0.18), false, 6 * pixel);
		AddCircle(m_fRadius, AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 0.8), false, 1.5 * pixel);

		// Graticule: latitudes every 30 degrees, longitudes every 20 (front side only)
		Color graticule = AROR_HiCommMap.Rgb(AROR_HiCommMap.CONTOUR, 0.75);
		for (int latitude = -60; latitude <= 60; latitude += 30)
		{
			array<vector> points = {};
			for (int longitude = 0; longitude <= 360; longitude += 8)
			{
				points.Insert(Vector(latitude, longitude, 0));
			}

			AddPath(points, graticule, 1 * pixel);
		}

		for (int longitude = 0; longitude < 360; longitude += 20)
		{
			array<vector> points = {};
			for (int latitude = -90; latitude <= 90; latitude += 6)
			{
				points.Insert(Vector(latitude, longitude, 0));
			}

			AddPath(points, graticule, 1 * pixel);
		}

		// Relay network: arcs pulse outward from the first node (the local uplink)
		array<vector> nodes = {Vector(46, 2, 0), Vector(51, -0.5, 0), Vector(40, -74, 0), Vector(-23, -46, 0), Vector(35, 139, 0), Vector(-33, 151, 0), Vector(1, 103, 0), Vector(55, 37, 0), Vector(30, 31, 0), Vector(-1, 36, 0)};
		Color arc = AROR_HiCommMap.Rgb(0xF6BA40, 0.7);
		for (int i = 1; i < nodes.Count(); i++)
		{
			AddArc(nodes[0], nodes[i], arc, 1.5 * pixel);

			// A packet travelling along each arc, staggered per link
			float progress = time * 0.35 + i * 0.137;
			progress = progress - Math.Floor(progress);
			vector packet = ArcPoint(nodes[0], nodes[i], progress);
			vector screen;
			if (ProjectUnit(packet, screen))
				AddSquare(screen, 2.5 * pixel, AROR_HiCommMap.Rgb(0xF6BA40, 1));
		}

		foreach (int i, vector node : nodes)
		{
			vector screen;
			if (!ProjectLatLon(node[0], node[1], screen))
				continue;

			float pulse = time * 1.2 + i * 0.4;
			pulse = pulse - Math.Floor(pulse);
			AddRing(screen, (4 + 10 * pulse) * pixel, AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 0.6 * (1 - pulse)), 1.2 * pixel);
			AddSquare(screen, 3 * pixel, AROR_HiCommMap.Rgb(AROR_HiCommMap.COAST, 1));
		}

		canvas.SetDrawCommands(m_aCommands);
	}

	//------------------------------------------------------------------------------------------------
	//! Unit-sphere position of a latitude/longitude (degrees), before spin and tilt
	protected static vector ToUnit(float latitude, float longitude)
	{
		float lat = latitude * Math.DEG2RAD;
		float lon = longitude * Math.DEG2RAD;
		return Vector(Math.Cos(lat) * Math.Sin(lon), Math.Sin(lat), Math.Cos(lat) * Math.Cos(lon));
	}

	//------------------------------------------------------------------------------------------------
	//! Spins and tilts a unit-sphere point and projects it; false if it is on the far side
	protected bool ProjectUnit(vector unit, out vector screen)
	{
		float spinCos = Math.Cos(m_fSpin);
		float spinSin = Math.Sin(m_fSpin);
		float x = unit[0] * spinCos + unit[2] * spinSin;
		float z = -unit[0] * spinSin + unit[2] * spinCos;
		float y = unit[1];

		float tiltCos = Math.Cos(TILT);
		float tiltSin = Math.Sin(TILT);
		float tiltedY = y * tiltCos - z * tiltSin;
		float tiltedZ = y * tiltSin + z * tiltCos;

		// Arcs are lifted above the surface, so they stay visible a little past the horizon
		float scale = unit.Length();
		screen = Vector(m_fCentreX + x * m_fRadius, m_fCentreY - tiltedY * m_fRadius, 0);
		return tiltedZ > 0 || (scale > 1.001 && tiltedZ > -0.15);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ProjectLatLon(float latitude, float longitude, out vector screen)
	{
		return ProjectUnit(ToUnit(latitude, longitude), screen);
	}

	//------------------------------------------------------------------------------------------------
	//! Point along the great circle between two nodes, lifted above the surface mid-way (t = 0..1)
	protected static vector ArcPoint(vector from, vector to, float t)
	{
		vector a = ToUnit(from[0], from[1]);
		vector b = ToUnit(to[0], to[1]);
		vector point = a * (1 - t) + b * t;
		float length = point.Length();
		if (length < 0.0001)
			return a;

		float lift = 1 + 0.18 * Math.Sin(Math.PI * t);
		return point * (lift / length);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddArc(vector from, vector to, Color color, float width)
	{
		array<float> run = {};
		for (int step = 0; step <= 24; step++)
		{
			vector screen;
			if (ProjectUnit(ArcPoint(from, to, step / 24.0), screen))
			{
				run.Insert(screen[0]);
				run.Insert(screen[1]);
			}
			else
			{
				FlushLine(run, color, width);
			}
		}

		FlushLine(run, color, width);
	}

	//------------------------------------------------------------------------------------------------
	//! Polyline through latitude/longitude points (vector: lat, lon), split where it passes behind the globe
	protected void AddPath(array<vector> points, Color color, float width)
	{
		array<float> run = {};
		foreach (vector point : points)
		{
			vector screen;
			if (ProjectLatLon(point[0], point[1], screen))
			{
				run.Insert(screen[0]);
				run.Insert(screen[1]);
			}
			else
			{
				FlushLine(run, color, width);
			}
		}

		FlushLine(run, color, width);
	}

	//------------------------------------------------------------------------------------------------
	protected void FlushLine(array<float> run, Color color, float width)
	{
		if (run.Count() >= 4)
		{
			LineDrawCommand line = new LineDrawCommand();
			line.m_iColor = color.PackToInt();
			line.m_fWidth = width;
			array<float> points = {};
			points.Copy(run);
			line.m_Vertices = points;
			line.m_bShouldEnclose = false;
			m_aCommands.Insert(line);
		}

		run.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCircle(float radius, Color color, bool filled, float width)
	{
		array<float> points = {};
		for (int i = 0; i < 48; i++)
		{
			float angle = Math.PI2 * i / 48;
			points.Insert(m_fCentreX + Math.Cos(angle) * radius);
			points.Insert(m_fCentreY + Math.Sin(angle) * radius);
		}

		AddShape(points, color, filled, width);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddRing(vector centre, float radius, Color color, float width)
	{
		array<float> points = {};
		for (int i = 0; i < 16; i++)
		{
			float angle = Math.PI2 * i / 16;
			points.Insert(centre[0] + Math.Cos(angle) * radius);
			points.Insert(centre[1] + Math.Sin(angle) * radius);
		}

		AddShape(points, color, false, width);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddSquare(vector centre, float half, Color color)
	{
		AddShape({centre[0] - half, centre[1] - half, centre[0] + half, centre[1] - half, centre[0] + half, centre[1] + half, centre[0] - half, centre[1] + half}, color, true, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddShape(array<float> points, Color color, bool filled, float width)
	{
		if (filled)
		{
			PolygonDrawCommand polygon = new PolygonDrawCommand();
			polygon.m_iColor = color.PackToInt();
			polygon.m_Vertices = points;
			m_aCommands.Insert(polygon);
			return;
		}

		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color.PackToInt();
		line.m_fWidth = width;
		line.m_Vertices = points;
		line.m_bShouldEnclose = true;
		m_aCommands.Insert(line);
	}
}
