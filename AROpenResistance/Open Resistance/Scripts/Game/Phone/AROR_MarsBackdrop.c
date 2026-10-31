//! Cached vector rings; no texture assets, native gameplay work or frame-by-frame allocations.
class AROR_MarsBackdrop
{
	protected float m_fWidth = -1;
	protected float m_fHeight = -1;
	protected bool m_bUnlocked;
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};

	void Draw(CanvasWidget canvas, float width, float height, float pixel, bool unlocked)
	{
		if (width == m_fWidth && height == m_fHeight && unlocked == m_bUnlocked)
			return;
		m_fWidth = width;
		m_fHeight = height;
		m_bUnlocked = unlocked;
		m_aCommands.Clear();
		float centreX = width * 0.23;
		float centreY = height * 0.52;
		float radius = height * 0.35;
		if (unlocked)
		{
			centreX = width * 0.78;
			radius = height * 0.40;
		}
		AddRing(centreX, centreY, radius, pixel * 2);
		AddRing(centreX, centreY, radius * 0.78, pixel);
		AddRing(centreX, centreY, radius * 0.52, pixel);
		for (int tick = 0; tick < 24; tick++)
		{
			float angle = tick * Math.PI2 / 24;
			array<float> points = {centreX + Math.Cos(angle) * radius * 0.94, centreY + Math.Sin(angle) * radius * 0.94, centreX + Math.Cos(angle) * radius * 1.035, centreY + Math.Sin(angle) * radius * 1.035};
			LineDrawCommand line = new LineDrawCommand();
			line.m_iColor = AROR_UIColors.Get(AROR_UIColors.MARS_MOTIF).PackToInt();
			line.m_fWidth = pixel;
			line.m_Vertices = points;
			m_aCommands.Insert(line);
		}
		canvas.SetDrawCommands(m_aCommands);
	}

	protected void AddRing(float x, float y, float radius, float width)
	{
		array<float> points = {};
		for (int segment = 0; segment < 72; segment++)
		{
			float angle = segment * Math.PI2 / 72;
			points.Insert(x + Math.Cos(angle) * radius);
			points.Insert(y + Math.Sin(angle) * radius);
		}
		LineDrawCommand ring = new LineDrawCommand();
		ring.m_iColor = AROR_UIColors.Get(AROR_UIColors.MARS_MOTIF).PackToInt();
		ring.m_fWidth = width;
		ring.m_Vertices = points;
		ring.m_bShouldEnclose = true;
		m_aCommands.Insert(ring);
	}
}
