//! The OPS crest (a sight ring with an amber pointer), drawn on a CanvasWidget for the home screen tile and the
//! login screen. The dashed inner ring turns slowly. Decorative; client only.
class AROR_OpsCrest
{
	protected static const float SPIN_SPEED = 0.25;	//!< Radians per second for the dashed ring
	protected static const int DASHES = 18;

	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};

	//------------------------------------------------------------------------------------------------
	//! width/height: canvas pixels; time: seconds; pixel: canvas pixels per line-width unit
	void Draw(CanvasWidget canvas, float width, float height, float time, float pixel)
	{
		m_aCommands.Clear();
		float centreX = width * 0.5;
		float centreY = height * 0.5;
		float radius = Math.Min(width, height) * 0.45;

		Color phosphor = AROR_HiCommMap.Srgb(159, 211, 107, 1);
		Color dim = AROR_HiCommMap.Srgb(95, 127, 70, 1);
		Color amber = AROR_HiCommMap.Srgb(230, 168, 58, 1);

		// Glow, outer ring
		AddRing(centreX, centreY, radius, AROR_HiCommMap.Srgb(159, 211, 107, 0.12), 5 * pixel);
		AddRing(centreX, centreY, radius, phosphor, 1.6 * pixel);

		// Dashed inner ring, turning
		float inner = radius * 0.69;
		float spin = time * SPIN_SPEED;
		for (int i = 0; i < DASHES; i++)
		{
			float start = spin + i * Math.PI2 / DASHES;
			float end = start + Math.PI2 / DASHES * 0.5;
			array<float> dash = {};
			for (int s = 0; s <= 3; s++)
			{
				float angle = start + (end - start) * s / 3.0;
				dash.Insert(centreX + Math.Cos(angle) * inner);
				dash.Insert(centreY + Math.Sin(angle) * inner);
			}

			AddLine(dash, dim, 1.2 * pixel);
		}

		// Cross ticks from the ring inwards
		float tick = radius * 0.48;
		AddSegment(centreX, centreY - radius, centreX, centreY - radius + tick, phosphor, 1.6 * pixel);
		AddSegment(centreX, centreY + radius, centreX, centreY + radius - tick, phosphor, 1.6 * pixel);
		AddSegment(centreX - radius, centreY, centreX - radius + tick, centreY, phosphor, 1.6 * pixel);
		AddSegment(centreX + radius, centreY, centreX + radius - tick, centreY, phosphor, 1.6 * pixel);

		// Amber pointer
		float half = radius * 0.27;
		array<float> triangle = {centreX - half, centreY + half * 0.75, centreX, centreY - half * 1.1, centreX + half, centreY + half * 0.75};
		PolygonDrawCommand polygon = new PolygonDrawCommand();
		polygon.m_iColor = amber.PackToInt();
		polygon.m_Vertices = triangle;
		m_aCommands.Insert(polygon);

		canvas.SetDrawCommands(m_aCommands);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddRing(float x, float y, float radius, Color color, float width)
	{
		array<float> points = {};
		int segments = 48;
		for (int i = 0; i < segments; i++)
		{
			float angle = i * Math.PI2 / segments;
			points.Insert(x + Math.Cos(angle) * radius);
			points.Insert(y + Math.Sin(angle) * radius);
		}

		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color.PackToInt();
		line.m_fWidth = width;
		line.m_Vertices = points;
		line.m_bShouldEnclose = true;
		m_aCommands.Insert(line);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddSegment(float x0, float y0, float x1, float y1, Color color, float width)
	{
		array<float> points = {x0, y0, x1, y1};
		AddLine(points, color, width);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddLine(array<float> points, Color color, float width)
	{
		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color.PackToInt();
		line.m_fWidth = width;
		line.m_Vertices = points;
		line.m_bShouldEnclose = false;
		m_aCommands.Insert(line);
	}
}
