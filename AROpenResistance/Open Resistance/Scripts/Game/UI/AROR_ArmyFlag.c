//! A small national flag for an army (faction preset), drawn on a CanvasWidget so no flag textures are needed.
//! US Army: 13 stripes and a blue canton with star dots. Soviet Army: red field with a yellow star. Any other army:
//! a block in its faction colour. Extender mods add flags with a modded class overriding DrawFlag().
class AROR_ArmyFlag
{
	//------------------------------------------------------------------------------------------------
	//! width/height: canvas pixels. commands: owned by the caller and kept while the flag shows (the canvas
	//! does not keep its own copy)
	static void Draw(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float width, float height, string presetId, Color fallback)
	{
		DrawAt(canvas, commands, 0, 0, width, height, presetId, fallback, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! As Draw, with the flag's top-left corner at left/top on the canvas and a chosen keyline width
	static void DrawAt(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float left, float top, float width, float height, string presetId, Color fallback, float keyline)
	{
		Build(commands, left, top, width, height, presetId, fallback, keyline);
		canvas.SetDrawCommands(commands);
	}

	//------------------------------------------------------------------------------------------------
	//! The flag's polygons and keyline without a canvas, for maps that draw with their own helpers (HICOMM)
	static void Build(notnull array<ref CanvasWidgetCommand> commands, float left, float top, float width, float height, string presetId, Color fallback, float keyline)
	{
		commands.Clear();
		DrawFlag(commands, width, height, presetId, fallback);

		// Thin dark keyline so light flags still read on the plate
		array<float> outline = {0, 0, width, 0, width, height, 0, height};
		LineDrawCommand edge = new LineDrawCommand();
		edge.m_iColor = Color.FromRGBA(0, 0, 0, 200).PackToInt();
		edge.m_fWidth = keyline;
		edge.m_Vertices = outline;
		edge.m_bShouldEnclose = true;
		commands.Insert(edge);

		if (left != 0 || top != 0)
		{
			foreach (CanvasWidgetCommand command : commands)
			{
				PolygonDrawCommand polygon = PolygonDrawCommand.Cast(command);
				if (polygon)
					Offset(polygon.m_Vertices, left, top);
				LineDrawCommand line = LineDrawCommand.Cast(command);
				if (line)
					Offset(line.m_Vertices, left, top);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Offset(array<float> vertices, float left, float top)
	{
		if (!vertices)
			return;
		for (int i = 0; i + 1 < vertices.Count(); i += 2)
		{
			vertices[i] = vertices[i] + left;
			vertices[i + 1] = vertices[i + 1] + top;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void DrawFlag(notnull array<ref CanvasWidgetCommand> commands, float width, float height, string presetId, Color fallback)
	{
		if (presetId == "US_ARMY")
		{
			// 13 stripes, red first; canton over the top 7, two fifths of the width
			float stripe = height / 13;
			for (int i = 0; i < 13; i++)
			{
				int stripeColor = 0xFFFFFFFF;
				if (i % 2 == 0)
					stripeColor = 0xFFB22234;
				Rect(commands, 0, i * stripe, width, stripe + 0.5, stripeColor);
			}
			float cantonWidth = width * 0.4;
			float cantonHeight = stripe * 7;
			Rect(commands, 0, 0, cantonWidth, cantonHeight, 0xFF3C3B6E);
			float dot = Math.Max(1, height * 0.08);
			for (int row = 0; row < 3; row++)
			{
				for (int column = 0; column < 3; column++)
				{
					float x = cantonWidth * (0.2 + column * 0.3) - dot * 0.5;
					float y = cantonHeight * (0.2 + row * 0.3) - dot * 0.5;
					Rect(commands, x, y, dot, dot, 0xFFFFFFFF);
				}
			}
			return;
		}

		if (presetId == "USSR_ARMY")
		{
			Rect(commands, 0, 0, width, height, 0xFFCC0000);
			Star(commands, width * 0.2, height * 0.27, height * 0.2, 0xFFFFD700);
			return;
		}

		Color color = fallback;
		if (!color)
			color = AROR_UIColors.Get(AROR_UIColors.FACTION_CIV);
		PolygonDrawCommand block = new PolygonDrawCommand();
		block.m_iColor = color.PackToInt();
		array<float> corners = {0, 0, width, 0, width, height, 0, height};
		block.m_Vertices = corners;
		commands.Insert(block);
	}

	//------------------------------------------------------------------------------------------------
	//! argb: packed sRGB token-style colour (converted for the canvas)
	protected static void Rect(notnull array<ref CanvasWidgetCommand> commands, float x, float y, float width, float height, int argb)
	{
		PolygonDrawCommand rect = new PolygonDrawCommand();
		rect.m_iColor = AROR_UIColors.Get(argb).PackToInt();
		array<float> corners = {x, y, x + width, y, x + width, y + height, x, y + height};
		rect.m_Vertices = corners;
		commands.Insert(rect);
	}

	//------------------------------------------------------------------------------------------------
	//! Five-pointed star as a centre pentagon plus five point triangles (canvas polygons must be convex)
	protected static void Star(notnull array<ref CanvasWidgetCommand> commands, float centreX, float centreY, float radius, int argb)
	{
		int color = AROR_UIColors.Get(argb).PackToInt();
		float inner = radius * 0.4;
		array<float> pentagon = {};
		for (int i = 0; i < 5; i++)
		{
			float innerAngle = (-90 + 36 + i * 72) * Math.DEG2RAD;
			pentagon.Insert(centreX + Math.Cos(innerAngle) * inner);
			pentagon.Insert(centreY + Math.Sin(innerAngle) * inner);
		}
		PolygonDrawCommand middle = new PolygonDrawCommand();
		middle.m_iColor = color;
		middle.m_Vertices = pentagon;
		commands.Insert(middle);

		for (int p = 0; p < 5; p++)
		{
			float tipAngle = (-90 + p * 72) * Math.DEG2RAD;
			int left = (p + 4) % 5;
			array<float> point = {centreX + Math.Cos(tipAngle) * radius, centreY + Math.Sin(tipAngle) * radius, pentagon[p * 2], pentagon[p * 2 + 1], pentagon[left * 2], pentagon[left * 2 + 1]};
			PolygonDrawCommand triangle = new PolygonDrawCommand();
			triangle.m_iColor = color;
			triangle.m_Vertices = point;
			commands.Insert(triangle);
		}
	}
}
