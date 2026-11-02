//! The phone quick dock's "save campaign" slot: a floppy disk on a filled square, added to a canvas's draw commands.
//! Purely decorative; client only.
class AROR_PhoneSaveIcon
{
	//------------------------------------------------------------------------------------------------
	//! x, y, size: the slot in screen units; pixels: canvas pixels per screen unit
	static void Add(notnull array<ref CanvasWidgetCommand> commands, float x, float y, float size, float pixels, Color background, Color foreground)
	{
		AddRect(commands, x, y, size, size, pixels, background);

		// Disk body with the top right corner cut off
		float disk = size * 0.5;
		float left = x + (size - disk) * 0.5;
		float top = y + (size - disk) * 0.5;
		array<float> body = {
			left * pixels, top * pixels,
			(left + disk * 0.8) * pixels, top * pixels,
			(left + disk) * pixels, (top + disk * 0.2) * pixels,
			(left + disk) * pixels, (top + disk) * pixels,
			left * pixels, (top + disk) * pixels
		};
		AddPolygon(commands, body, foreground);

		// Metal shutter along the top with its window, and the paper label along the bottom
		AddRect(commands, left + disk * 0.22, top, disk * 0.48, disk * 0.32, pixels, background);
		AddRect(commands, left + disk * 0.50, top + disk * 0.05, disk * 0.12, disk * 0.22, pixels, foreground);
		AddRect(commands, left + disk * 0.15, top + disk * 0.50, disk * 0.70, disk * 0.38, pixels, background);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddRect(notnull array<ref CanvasWidgetCommand> commands, float x, float y, float width, float height, float pixels, Color color)
	{
		array<float> vertices = {
			x * pixels, y * pixels,
			(x + width) * pixels, y * pixels,
			(x + width) * pixels, (y + height) * pixels,
			x * pixels, (y + height) * pixels
		};
		AddPolygon(commands, vertices, color);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddPolygon(notnull array<ref CanvasWidgetCommand> commands, array<float> vertices, Color color)
	{
		PolygonDrawCommand shape = new PolygonDrawCommand();
		shape.m_iColor = color.PackToInt();
		shape.m_Vertices = vertices;
		commands.Insert(shape);
	}
}
