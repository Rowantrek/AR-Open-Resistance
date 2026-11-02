//! Dark plate drawn behind map text (location names, QRF/attack captions), so light text reads over any terrain:
//! the only font the mod ships is a thin condensed face that fades to grey on Everon's light map at small sizes.
//! A coloured edge on the left carries the owner or threat colour. Drawn on the "Frame" canvas of a map icon layout.
class AROR_MapLabelPlate
{
	static const float PADDING = 6;			//!< Reference pixels between the edge bar and the text, and after the text
	static const float EDGE_WIDTH = 4;		//!< Coloured bar on the left
	static const float CHAR_WIDTH = 0.6;	//!< Average bold capital width as a fraction of the font size
	static const int PLATE_COLOR = 0xE6080B09;	//!< Design-system plate, 90%

	//------------------------------------------------------------------------------------------------
	//! Plate width in reference pixels for a line of capitals; generous, since the font cannot be measured here
	static float Width(string text, int fontSize)
	{
		return EDGE_WIDTH + PADDING * 2 + text.Length() * fontSize * CHAR_WIDTH;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the text starts inside the plate, in reference pixels
	static float TextInset()
	{
		return EDGE_WIDTH + PADDING;
	}

	//------------------------------------------------------------------------------------------------
	//! width and height in reference pixels; edgeColor ARGB
	static void Draw(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float width, float height, int edgeColor)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float w = workspace.DPIScale(width);
		float h = workspace.DPIScale(height);
		float edge = workspace.DPIScale(EDGE_WIDTH);
		commands.Clear();
		commands.Insert(Rectangle(0, 0, w, h, PLATE_COLOR));
		commands.Insert(Rectangle(0, 0, edge, h, edgeColor));
		canvas.SetDrawCommands(commands);
	}

	//------------------------------------------------------------------------------------------------
	protected static PolygonDrawCommand Rectangle(float left, float top, float right, float bottom, int color)
	{
		array<float> points = {};
		points.Insert(left);
		points.Insert(top);
		points.Insert(right);
		points.Insert(top);
		points.Insert(right);
		points.Insert(bottom);
		points.Insert(left);
		points.Insert(bottom);
		PolygonDrawCommand polygon = new PolygonDrawCommand();
		polygon.m_iColor = color;
		polygon.m_Vertices = points;
		return polygon;
	}
}
