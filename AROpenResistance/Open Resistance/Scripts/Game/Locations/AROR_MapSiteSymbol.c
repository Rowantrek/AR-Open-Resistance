//! A location's map marker: a bold silhouette that says what the site is at a glance (fort, bunker, aircraft,
//! anchor, helipad, factory, crates, house, skyline), filled with the holder's faction colour inside a dark halo so it
//! reads on light terrain, with dark cut-outs for detail (gate, firing slit, windows). Built as canvas commands from
//! convex pieces only, so the game map (CanvasWidget) and HICOMM (which replays the commands) draw the same symbol.
//! Strongholds draw their army's flag instead (AROR_ArmyFlag).
class AROR_MapSiteSymbol
{
	protected static const float GRID = 24;		//!< Design grid of the shapes below
	protected static const float HALO = 0.11;	//!< Halo line width as a share of the symbol size (half shows outside)

	//------------------------------------------------------------------------------------------------
	//! commands: cleared, then filled; the caller keeps them while drawn (a canvas does not keep its own copy).
	//! left, top, size: the symbol's square in the caller's pixels.
	static void Build(notnull array<ref CanvasWidgetCommand> commands, float left, float top, float size, AROR_ELocationType type, Color factionColor)
	{
		commands.Clear();
		array<ref array<float>> body = {};
		array<ref array<float>> cuts = {};
		Shapes(type, body, cuts);

		float scale = size / GRID;
		int dark = AROR_MapFrameIcon.KeylineColor().PackToInt();
		int fill = AROR_MapFrameIcon.FillColor(AROR_MapFrameIcon.StrokeColor(factionColor)).PackToInt();
		array<ref array<float>> placed = {};
		foreach (array<float> shape : body)
		{
			placed.Insert(Place(shape, left, top, scale));
		}

		// Every outline first, then every fill: the fills cover the halo wherever pieces overlap, leaving one
		// clean outline around the whole silhouette
		foreach (array<float> outline : placed)
		{
			commands.Insert(Line(outline, dark, size * HALO));
		}

		foreach (array<float> piece : placed)
		{
			commands.Insert(Polygon(piece, fill));
		}

		foreach (array<float> cut : cuts)
		{
			commands.Insert(Polygon(Place(cut, left, top, scale), dark));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Convex pieces on the 24-unit grid: the silhouette (body) and the dark details on top (cuts)
	protected static void Shapes(AROR_ELocationType type, notnull array<ref array<float>> body, notnull array<ref array<float>> cuts)
	{
		switch (type)
		{
			case AROR_ELocationType.MILITARY_BASE:	// Fort: wall, three merlons, arched gate
				body.Insert(Shape("R 3 10 21 21"));
				body.Insert(Shape("R 3 5 7.5 10.5"));
				body.Insert(Shape("R 9.75 5 14.25 10.5"));
				body.Insert(Shape("R 16.5 5 21 10.5"));
				cuts.Insert(Shape("R 9.8 15.5 14.2 21"));
				cuts.Insert(Shape("C 12 15.5 2.2"));
				return;

			case AROR_ELocationType.OUTPOST:		// Bunker: dome on a footing, firing slit
				body.Insert(ArcPolygon(12, 15, 9.5, 180, 360, 16));
				body.Insert(Shape("R 1.5 14.5 22.5 18.5"));
				cuts.Insert(Shape("R 7 10 17 12.2"));
				return;

			case AROR_ELocationType.AIRPORT:		// Aircraft from above
				body.Insert(Shape("R 10.6 3.5 13.4 20.5"));
				body.Insert(Shape("C 12 3.6 1.4"));
				body.Insert(Shape("P 2 13 10.6 8 10.6 12.5 2 15.5"));
				body.Insert(Shape("P 13.4 8 22 13 22 15.5 13.4 12.5"));
				body.Insert(Shape("P 6.5 21 10.6 18 10.6 20.5 6.5 22.5"));
				body.Insert(Shape("P 13.4 18 17.5 21 17.5 22.5 13.4 20.5"));
				return;

			case AROR_ELocationType.HELIPORT:		// Helipad: disc with an H
				body.Insert(Shape("C 12 12 10"));
				cuts.Insert(Shape("R 7.5 6.5 10 17.5"));
				cuts.Insert(Shape("R 14 6.5 16.5 17.5"));
				cuts.Insert(Shape("R 10 10.8 14 13.2"));
				return;

			case AROR_ELocationType.PORT:			// Anchor: ring, stock, shank, curved arms with flukes
				body.Insert(Shape("C 12 4.5 3.2"));
				body.Insert(Shape("R 10.8 6.5 13.2 20.5"));
				body.Insert(Shape("R 6.5 9 17.5 11.2"));
				ArcBand(body, 12, 12, 9.5, 7.2, 20, 160, 8);
				body.Insert(Shape("P 0.8 15 4.2 10.5 6.2 15.5"));
				body.Insert(Shape("P 23.2 15 17.8 15.5 19.8 10.5"));
				cuts.Insert(Shape("C 12 4.5 1.4"));
				return;

			case AROR_ELocationType.FACTORY:		// Sawtooth roof, chimney, windows
				body.Insert(Shape("R 2 13 22 21"));
				body.Insert(Shape("P 2 13 2 7.5 8 13"));
				body.Insert(Shape("P 8 13 8 7.5 14 13"));
				body.Insert(Shape("P 14 13 14 7.5 20 13"));
				body.Insert(Shape("R 18.5 3 21.5 13"));
				cuts.Insert(Shape("R 4.5 15.5 7.5 18.5"));
				cuts.Insert(Shape("R 10.5 15.5 13.5 18.5"));
				cuts.Insert(Shape("R 16.5 15.5 19.5 18.5"));
				return;

			case AROR_ELocationType.RESOURCES:		// Stacked supply crates
				body.Insert(Shape("R 2 12.5 11.25 21"));
				body.Insert(Shape("R 12.75 12.5 22 21"));
				body.Insert(Shape("R 7.4 3.5 16.6 11"));
				cuts.Insert(Shape("R 2 16.25 11.25 17.25"));
				cuts.Insert(Shape("R 12.75 16.25 22 17.25"));
				cuts.Insert(Shape("R 7.4 6.75 16.6 7.75"));
				return;

			case AROR_ELocationType.HQ:				// Flag on a pole
				body.Insert(Shape("R 4.5 2 7 22"));
				body.Insert(Shape("R 7 3 20 12"));
				body.Insert(Shape("R 2.5 20.5 9 22.5"));
				return;

			case AROR_ELocationType.TOWN:			// House
				body.Insert(Shape("P 2 11.5 12 3 22 11.5"));
				body.Insert(Shape("R 4.5 11 19.5 21"));
				cuts.Insert(Shape("R 10.5 15 13.5 21"));
				return;

			case AROR_ELocationType.CITY:			// Skyline of three blocks with windows
				body.Insert(Shape("R 2 10 8.5 21"));
				body.Insert(Shape("R 8.5 4 15.5 21"));
				body.Insert(Shape("R 15.5 8 22 21"));
				cuts.Insert(Shape("R 10.75 7 13.25 9.5"));
				cuts.Insert(Shape("R 10.75 12 13.25 14.5"));
				cuts.Insert(Shape("R 4 13 6.5 15.5"));
				cuts.Insert(Shape("R 17.5 11 20 13.5"));
				return;
		}

		body.Insert(Shape("C 12 12 6"));
	}

	//------------------------------------------------------------------------------------------------
	//! "R left top right bottom", "C x y radius" or "P x y x y ..." (convex) to a polygon's x, y pairs
	protected static array<float> Shape(string spec)
	{
		array<string> parts = {};
		spec.Split(" ", parts, true);
		array<float> numbers = {};
		for (int i = 1; i < parts.Count(); i++)
		{
			numbers.Insert(parts[i].ToFloat());
		}

		string kind = parts[0];
		if (kind == "C")
			return ArcPolygon(numbers[0], numbers[1], numbers[2], 0, 360, 20);

		if (kind != "R")
			return numbers;

		array<float> rectangle = {};
		rectangle.Insert(numbers[0]);
		rectangle.Insert(numbers[1]);
		rectangle.Insert(numbers[2]);
		rectangle.Insert(numbers[1]);
		rectangle.Insert(numbers[2]);
		rectangle.Insert(numbers[3]);
		rectangle.Insert(numbers[0]);
		rectangle.Insert(numbers[3]);
		return rectangle;
	}

	//------------------------------------------------------------------------------------------------
	//! Points along an arc (degrees, 0 = right, 90 = down); as a polygon its ends close with a straight chord
	protected static array<float> ArcPolygon(float centreX, float centreY, float radius, float from, float to, int steps)
	{
		array<float> points = {};
		int last = steps;
		if (to - from >= 360)
			last = steps - 1;	// A full circle: no repeated first point

		for (int i = 0; i <= last; i++)
		{
			float angle = (from + (to - from) * i / steps) * Math.DEG2RAD;
			points.Insert(centreX + Math.Cos(angle) * radius);
			points.Insert(centreY + Math.Sin(angle) * radius);
		}

		return points;
	}

	//------------------------------------------------------------------------------------------------
	//! A curved band between two radii, as one convex quad per step
	protected static void ArcBand(notnull array<ref array<float>> outPieces, float centreX, float centreY, float outer, float inner, float from, float to, int steps)
	{
		for (int i = 0; i < steps; i++)
		{
			float a = (from + (to - from) * i / steps) * Math.DEG2RAD;
			float b = (from + (to - from) * (i + 1) / steps) * Math.DEG2RAD;
			array<float> quad = {};
			quad.Insert(centreX + Math.Cos(a) * outer);
			quad.Insert(centreY + Math.Sin(a) * outer);
			quad.Insert(centreX + Math.Cos(b) * outer);
			quad.Insert(centreY + Math.Sin(b) * outer);
			quad.Insert(centreX + Math.Cos(b) * inner);
			quad.Insert(centreY + Math.Sin(b) * inner);
			quad.Insert(centreX + Math.Cos(a) * inner);
			quad.Insert(centreY + Math.Sin(a) * inner);
			outPieces.Insert(quad);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static array<float> Place(notnull array<float> shape, float left, float top, float scale)
	{
		array<float> points = {};
		for (int i = 0; i + 1 < shape.Count(); i += 2)
		{
			points.Insert(left + shape[i] * scale);
			points.Insert(top + shape[i + 1] * scale);
		}

		return points;
	}

	//------------------------------------------------------------------------------------------------
	protected static PolygonDrawCommand Polygon(array<float> points, int color)
	{
		PolygonDrawCommand polygon = new PolygonDrawCommand();
		polygon.m_iColor = color;
		polygon.m_Vertices = points;
		return polygon;
	}

	//------------------------------------------------------------------------------------------------
	protected static LineDrawCommand Line(array<float> points, int color, float width)
	{
		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color;
		line.m_fWidth = width;
		line.m_Vertices = points;
		line.m_bShouldEnclose = true;
		return line;
	}
}
