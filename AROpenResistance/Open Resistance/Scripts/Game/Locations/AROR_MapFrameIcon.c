//! Map badge for a location, drawn on a CanvasWidget (no textures). Kept simple so it reads at a glance on the
//! busy terrain map: a solid shape in the holder's faction colour inside a dark keyline, and one thick symbol in
//! white (or near-black on a light colour) for the location type. Towns and cities carry no symbol: the map prints
//! their names. The shape says friend or foe the way the design system's glyphs do: a square for the Resistance
//! (ours), a diamond for the enemy (Occupiers or Invaders), a circle for anyone else (neutral).
class AROR_MapFrameIcon
{
	protected static const float GRID = 24;		//!< Glyph design grid
	protected static const float LIGHT_FILL = 0.62;	//!< Fills brighter than this (luma) take a dark symbol

	//------------------------------------------------------------------------------------------------
	//! size: canvas pixels (square); pixel: canvas pixels per reference pixel (line widths).
	//! commands: owned by the caller and kept while the icon exists: the canvas does not keep its own copy.
	static void Draw(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float size, float pixel, AROR_ELocationType type, AROR_EFactionRole owner, Color factionColor)
	{
		commands.Clear();
		Color stroke = StrokeColor(factionColor);
		array<float> frame = {};
		FrameOutline(owner, 0, 0, size, frame);

		commands.Insert(Line(frame, KeylineColor().PackToInt(), KEYLINE_WIDTH * pixel, true));
		PolygonDrawCommand body = new PolygonDrawCommand();
		body.m_iColor = FillColor(stroke).PackToInt();
		body.m_Vertices = frame;
		commands.Insert(body);

		int glyph = GlyphColor(stroke).PackToInt();
		array<ref array<float>> strokes = {};
		GlyphStrokes(type, owner, 0, 0, size, strokes);
		foreach (array<float> strokeLine : strokes)
		{
			commands.Insert(Line(strokeLine, glyph, GLYPH_WIDTH * pixel, false));
		}

		canvas.SetDrawCommands(commands);
	}

	//------------------------------------------------------------------------------------------------
	//! A garrison post's flag: pole and pennant in the holding faction's colour on a dark keyline. hollow: the post
	//! is taken (outline only). size: canvas pixels (square).
	static void DrawPennant(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float size, float pixel, Color factionColor, bool hollow)
	{
		commands.Clear();
		Color stroke = StrokeColor(factionColor);
		array<float> pole = {size * 0.3, size * 0.92, size * 0.3, size * 0.08};
		array<float> pennant = {size * 0.3, size * 0.1, size * 0.9, size * 0.3, size * 0.3, size * 0.5};
		commands.Insert(Line(pole, KeylineColor().PackToInt(), (GLYPH_WIDTH + 2) * pixel, false));
		commands.Insert(Line(pennant, KeylineColor().PackToInt(), (PENNANT_EDGE + 2) * pixel, true));
		if (!hollow)
		{
			PolygonDrawCommand cloth = new PolygonDrawCommand();
			cloth.m_iColor = stroke.PackToInt();
			cloth.m_Vertices = pennant;
			commands.Insert(cloth);
		}
		commands.Insert(Line(pennant, stroke.PackToInt(), PENNANT_EDGE * pixel, true));
		commands.Insert(Line(pole, stroke.PackToInt(), GLYPH_WIDTH * pixel, false));
		canvas.SetDrawCommands(commands);
	}

	//! Last-seen asset badge. Kinds are the spotting wire format, independent of location types.
	static void DrawContact(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float size, float pixel, int kind, AROR_EFactionRole owner, Color factionColor)
	{
		commands.Clear();
		array<float> frame = {};
		FrameOutline(owner, 0, 0, size, frame);
		commands.Insert(Line(frame, KeylineColor().PackToInt(), KEYLINE_WIDTH * pixel, true));
		PolygonDrawCommand body = new PolygonDrawCommand();
		body.m_iColor = FillColor(factionColor).PackToInt();
		body.m_Vertices = frame;
		commands.Insert(body);
		array<ref array<float>> strokes = {};
		ContactGlyph(kind, strokes);
		foreach (array<float> stroke : strokes)
		{
			array<float> points = {};
			for (int index = 0; index + 1 < stroke.Count(); index += 2)
			{
				points.Insert(size * (0.2 + stroke[index] / GRID * 0.6));
				points.Insert(size * (0.2 + stroke[index + 1] / GRID * 0.6));
			}
			commands.Insert(Line(points, GlyphColor(factionColor).PackToInt(), 2 * pixel, false));
		}
		canvas.SetDrawCommands(commands);
	}

	//! A distinct silhouette for each asset, including a rotor on both helicopter classes.
	static void ContactGlyph(int kind, notnull array<ref array<float>> strokes)
	{
		if (kind == 3 || kind == 4)
		{
			strokes.Insert(Points("2 4 22 4"));
			strokes.Insert(Points("12 4 12 8"));
			strokes.Insert(Points("7 9 15 9 19 15 7 15 7 9"));
			strokes.Insert(Points("7 12 2 12 2 7"));
			strokes.Insert(Points("9 15 9 19 19 19"));
			if (kind == 3)
				strokes.Insert(Points("11 9 11 15"));
			else
			{
				strokes.Insert(Points("15 15 22 18"));
				strokes.Insert(Points("3 18 7 22 11 18"));
			}
			return;
		}
		switch (kind)
		{
			case 1: // IFV: tracked hull with infantry cross
				strokes.Insert(Points("3 16 21 16 21 21 3 21 3 16"));
				strokes.Insert(Points("5 13 5 5 19 13"));
				strokes.Insert(Points("19 5 5 13"));
				return;
			case 2: // Armour: track oval and long gun
				strokes.Insert(Arc(12, 15, 9, 0, 360, 12));
				strokes.Insert(Points("8 12 16 12 16 18 8 18 8 12"));
				strokes.Insert(Points("12 12 12 1"));
				return;
			case 5: // Light armed vehicle: wheels and weapon
				strokes.Insert(Points("3 16 3 10 17 10 21 16 3 16"));
				strokes.Insert(Points("9 10 9 5 20 5"));
				strokes.Insert(Points("6 17 6 21"));
				strokes.Insert(Points("18 17 18 21"));
				return;
			case 6: // Dismounted squad: infantry cross
				strokes.Insert(Points("4 4 20 20"));
				strokes.Insert(Points("20 4 4 20"));
				return;
		}
		// Transport truck: cargo body, cab and wheels
		strokes.Insert(Points("2 6 14 6 14 17 2 17 2 6"));
		strokes.Insert(Points("14 11 19 11 22 17 14 17"));
		strokes.Insert(Points("6 17 6 21"));
		strokes.Insert(Points("18 17 18 21"));
	}

	//------------------------------------------------------------------------------------------------
	// Shared geometry: HICOMM's map draws the same symbols with its own canvas helpers
	//------------------------------------------------------------------------------------------------
	static const float KEYLINE_WIDTH = 4;	//!< Reference pixels; drawn under the fill, so half shows outside
	static const float BORDER_WIDTH = 0;	//!< No separate border: the fill is the faction colour
	static const float GLYPH_WIDTH = 3.2;
	protected static const float PENNANT_EDGE = 2;

	//------------------------------------------------------------------------------------------------
	//! The frame for an owner, as x, y pairs in the square at (left, top) of the given size
	static void FrameOutline(AROR_EFactionRole owner, float left, float top, float size, notnull array<float> outPoints)
	{
		array<float> fractions;
		if (owner == AROR_EFactionRole.RESISTANCE)
			fractions = Points("0.12 0.12 0.88 0.12 0.88 0.88 0.12 0.88");
		else if (owner == AROR_EFactionRole.OCCUPIERS || owner == AROR_EFactionRole.INVADERS)
			fractions = Points("0.5 0.04 0.96 0.5 0.5 0.96 0.04 0.5");
		else
			fractions = Circle(0.5, 0.5, 0.4, 20);

		for (int i = 0; i + 1 < fractions.Count(); i += 2)
		{
			outPoints.Insert(left + fractions[i] * size);
			outPoints.Insert(top + fractions[i + 1] * size);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The type glyph's polylines, centred in the frame for that owner, in the square at (left, top)
	static void GlyphStrokes(AROR_ELocationType type, AROR_EFactionRole owner, float left, float top, float size, notnull array<ref array<float>> outStrokes)
	{
		float box = size * 0.5;
		if (owner == AROR_EFactionRole.RESISTANCE)
			box = size * 0.56;
		else if (owner == AROR_EFactionRole.OCCUPIERS || owner == AROR_EFactionRole.INVADERS)
			box = size * 0.48;

		float scale = box / GRID;
		float offsetX = left + (size - box) * 0.5;
		float offsetY = top + (size - box) * 0.5;
		array<ref array<float>> strokes = {};
		Glyph(type, strokes);
		foreach (array<float> strokeLine : strokes)
		{
			array<float> points = {};
			for (int i = 0; i + 1 < strokeLine.Count(); i += 2)
			{
				points.Insert(offsetX + strokeLine[i] * scale);
				points.Insert(offsetY + strokeLine[i + 1] * scale);
			}
			outStrokes.Insert(points);
		}
	}

	//------------------------------------------------------------------------------------------------
	static Color StrokeColor(Color factionColor)
	{
		if (factionColor)
			return factionColor;
		return Color.FromRGBA(200, 205, 200, 255);
	}

	//------------------------------------------------------------------------------------------------
	//! Solid faction colour
	static Color FillColor(notnull Color stroke)
	{
		return new Color(stroke.R(), stroke.G(), stroke.B(), 1);
	}

	//------------------------------------------------------------------------------------------------
	//! The symbol: white on a dark or saturated fill, near-black on a light one (rebel green, neutral grey)
	static Color GlyphColor(notnull Color stroke)
	{
		float luma = 0.299 * stroke.R() + 0.587 * stroke.G() + 0.114 * stroke.B();
		if (luma > LIGHT_FILL)
			return Color.FromRGBA(6, 17, 10, 255);
		return Color.FromRGBA(255, 255, 255, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color KeylineColor()
	{
		return Color.FromRGBA(2, 3, 2, 230);
	}

	//------------------------------------------------------------------------------------------------
	//! Stroke glyphs on a 24-unit grid, one polyline per array
	protected static void Glyph(AROR_ELocationType type, notnull array<ref array<float>> strokes)
	{
		switch (type)
		{
			case AROR_ELocationType.CITY:
			case AROR_ELocationType.TOWN:
				// Plain shape: the map prints settlement names, and many small symbols only add noise
				return;
			case AROR_ELocationType.HQ:
				strokes.Insert(Points("6 22 6 2"));
				strokes.Insert(Points("6 3 19 3 19 11 6 11"));
				return;
			case AROR_ELocationType.MILITARY_BASE:
				strokes.Insert(Star(12, 12.5, 10.5, 4.4));
				return;
			case AROR_ELocationType.OUTPOST:
				strokes.Insert(Points("7 22 7 2"));
				strokes.Insert(Points("7 3 19 7 7 11"));
				return;
			case AROR_ELocationType.PORT:
				strokes.Insert(Arc(12, 4, 2.2, 0, 360, 10));
				strokes.Insert(Points("12 6.2 12 21"));
				strokes.Insert(Points("7 10 17 10"));
				strokes.Insert(Arc(12, 13, 8, 0, 180, 10));
				return;
			case AROR_ELocationType.AIRPORT:
				strokes.Insert(Points("12 2 12 22"));
				strokes.Insert(Points("2 14 12 9 22 14"));
				strokes.Insert(Points("8 21 12 19 16 21"));
				return;
			case AROR_ELocationType.HELIPORT:
				strokes.Insert(Arc(12, 12, 10, 0, 360, 16));
				strokes.Insert(Points("8 7 8 17"));
				strokes.Insert(Points("16 7 16 17"));
				strokes.Insert(Points("8 12 16 12"));
				return;
			case AROR_ELocationType.FACTORY:
				strokes.Insert(Points("2 22 2 10 8 14 8 10 14 14 14 3 20 3 20 22 2 22"));
				return;
			case AROR_ELocationType.RESOURCES:
				strokes.Insert(Points("4 6 20 6 20 20 4 20 4 6"));
				strokes.Insert(Points("4 6 20 20"));
				strokes.Insert(Points("20 6 4 20"));
				return;
		}
		strokes.Insert(Arc(12, 12, 3, 0, 360, 10));
	}

	//------------------------------------------------------------------------------------------------
	//! Closed five-pointed star outline
	protected static array<float> Star(float centreX, float centreY, float outer, float inner)
	{
		array<float> points = {};
		for (int i = 0; i <= 10; i++)
		{
			float radius = outer;
			if (Math.Mod(i, 2) == 1)
				radius = inner;
			float angle = (-90 + i * 36) * Math.DEG2RAD;
			points.Insert(centreX + Math.Cos(angle) * radius);
			points.Insert(centreY + Math.Sin(angle) * radius);
		}
		return points;
	}

	//------------------------------------------------------------------------------------------------
	//! Arc from one angle to another (degrees, 0 = right, 90 = down), as a polyline
	protected static array<float> Arc(float centreX, float centreY, float radius, float from, float to, int steps)
	{
		array<float> points = {};
		for (int i = 0; i <= steps; i++)
		{
			float angle = (from + (to - from) * i / steps) * Math.DEG2RAD;
			points.Insert(centreX + Math.Cos(angle) * radius);
			points.Insert(centreY + Math.Sin(angle) * radius);
		}
		return points;
	}

	//------------------------------------------------------------------------------------------------
	//! Closed circle as x, y pairs (same units as the centre and radius)
	protected static array<float> Circle(float centreX, float centreY, float radius, int steps)
	{
		array<float> points = {};
		for (int i = 0; i < steps; i++)
		{
			float angle = i * Math.PI2 / steps;
			points.Insert(centreX + Math.Cos(angle) * radius);
			points.Insert(centreY + Math.Sin(angle) * radius);
		}
		return points;
	}

	//------------------------------------------------------------------------------------------------
	//! "x y x y ..." to numbers
	protected static array<float> Points(string list)
	{
		array<string> parts = {};
		list.Split(" ", parts, true);
		array<float> points = {};
		foreach (string part : parts)
		{
			points.Insert(part.ToFloat());
		}
		return points;
	}

	//------------------------------------------------------------------------------------------------
	protected static LineDrawCommand Line(array<float> points, int color, float width, bool closed)
	{
		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color;
		line.m_fWidth = width;
		line.m_Vertices = points;
		line.m_bShouldEnclose = closed;
		return line;
	}
}
