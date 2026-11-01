//! Client map layer: the local player's own position (an arrow along their facing) and their personal waypoint
//! (AROR_PlayerWaypoint). Widgets live only while a map is open; positioned every frame by AROR_LocationMapIconsComponent.
class AROR_NavigationMapMarkers
{
	protected static const float ARROW_SIZE = 28;		//!< Reference pixels
	protected static const float WAYPOINT_SIZE = 22;
	protected static const int LABEL_SIZE = 17;

	protected Widget m_wSelf;
	protected Widget m_wWaypoint;
	protected ref array<ref CanvasWidgetCommand> m_aSelfCommands = {};
	protected ref array<ref CanvasWidgetCommand> m_aWaypointCommands = {};
	protected string m_sWaypointText;
	protected float m_fDrawnHeading = -1000;

	//------------------------------------------------------------------------------------------------
	void ClearWidgets()
	{
		if (m_wSelf)
			m_wSelf.RemoveFromHierarchy();
		if (m_wWaypoint)
			m_wWaypoint.RemoveFromHierarchy();
		m_wSelf = null;
		m_wWaypoint = null;
		m_aSelfCommands.Clear();
		m_aWaypointCommands.Clear();
		m_sWaypointText = string.Empty;
		m_fDrawnHeading = -1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Waypoint hit test for shift-click removal: true when the screen point is on the waypoint marker
	static bool IsOnWaypoint(notnull SCR_MapEntity mapEntity, notnull AROR_PlayerWaypoint waypoint, int screenX, int screenY)
	{
		if (!waypoint.IsActive())
			return false;

		vector position = waypoint.GetPosition();
		int x, y;
		mapEntity.WorldToScreen(position[0], position[2], x, y, true);
		float radius = GetGame().GetWorkspace().DPIScale(WAYPOINT_SIZE * 0.5 + 6);
		return (x - screenX) * (x - screenX) + (y - screenY) * (y - screenY) <= radius * radius;
	}

	//------------------------------------------------------------------------------------------------
	void Update(Widget parent, ResourceName layout, SCR_MapEntity mapEntity, AROR_PlayerWaypoint waypoint, bool showSelf)
	{
		if (!parent || !mapEntity)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		IEntity player = AROR_PlayerWaypoint.GetLocalEntity();
		UpdateSelf(parent, layout, mapEntity, workspace, player, showSelf);
		UpdateWaypoint(parent, layout, mapEntity, workspace, player, waypoint);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateSelf(Widget parent, ResourceName layout, SCR_MapEntity mapEntity, WorkspaceWidget workspace, IEntity player, bool showSelf)
	{
		if (!showSelf || !player)
		{
			if (m_wSelf)
				m_wSelf.SetVisible(false);
			return;
		}

		if (!m_wSelf)
		{
			m_wSelf = CreateMarker(parent, layout, ARROW_SIZE);
			if (!m_wSelf)
				return;
		}

		// Heading on the map: north up, so world +Z is screen up
		vector forward = player.GetWorldTransformAxis(2);
		float heading = Math.Atan2(forward[0], forward[2]) * Math.RAD2DEG;
		if (Math.AbsFloat(heading - m_fDrawnHeading) > 1)
		{
			CanvasWidget canvas = CanvasWidget.Cast(m_wSelf.FindAnyWidget("Frame"));
			if (canvas)
				DrawArrow(canvas, m_aSelfCommands, workspace.DPIScale(ARROW_SIZE), heading);
			m_fDrawnHeading = heading;
		}

		vector origin = player.GetOrigin();
		int x, y;
		mapEntity.WorldToScreen(origin[0], origin[2], x, y, true);
		m_wSelf.SetVisible(true);
		FrameSlot.SetPos(m_wSelf, workspace.DPIUnscale(x), workspace.DPIUnscale(y));
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateWaypoint(Widget parent, ResourceName layout, SCR_MapEntity mapEntity, WorkspaceWidget workspace, IEntity player, AROR_PlayerWaypoint waypoint)
	{
		if (!waypoint || !waypoint.IsActive())
		{
			if (m_wWaypoint)
				m_wWaypoint.SetVisible(false);
			return;
		}

		if (!m_wWaypoint)
		{
			m_wWaypoint = CreateMarker(parent, layout, WAYPOINT_SIZE);
			if (!m_wWaypoint)
				return;

			CanvasWidget canvas = CanvasWidget.Cast(m_wWaypoint.FindAnyWidget("Frame"));
			if (canvas)
				AROR_PlayerWaypoint.DrawDiamond(canvas, m_aWaypointCommands, workspace.DPIScale(WAYPOINT_SIZE), AROR_UIColors.WARN);

			RichTextWidget label = RichTextWidget.Cast(m_wWaypoint.FindAnyWidget("QRFLabel"));
			if (label)
			{
				label.SetVisible(true);
				label.SetExactFontSize(LABEL_SIZE);
				label.SetColor(AROR_UIColors.Get(AROR_UIColors.WARN));
				label.SetBold(true);
				FrameSlot.SetSizeToContent(label, true);
				FrameSlot.SetPos(label, WAYPOINT_SIZE + 4, 0);
			}
			m_sWaypointText = string.Empty;
		}

		string text = AROR_PlayerWaypoint.MARKER_NAME;
		if (player)
			text += "  " + waypoint.DistanceText(player.GetOrigin());
		if (text != m_sWaypointText)
		{
			m_sWaypointText = text;
			RichTextWidget caption = RichTextWidget.Cast(m_wWaypoint.FindAnyWidget("QRFLabel"));
			if (caption)
				caption.SetText(text);
		}

		vector position = waypoint.GetPosition();
		int x, y;
		mapEntity.WorldToScreen(position[0], position[2], x, y, true);
		m_wWaypoint.SetVisible(true);
		FrameSlot.SetPos(m_wWaypoint, workspace.DPIUnscale(x), workspace.DPIUnscale(y));
	}

	//------------------------------------------------------------------------------------------------
	protected Widget CreateMarker(Widget parent, ResourceName layout, float size)
	{
		Widget root = GetGame().GetWorkspace().CreateWidgets(layout, parent);
		if (!root)
			return null;

		FrameSlot.SetAlignment(root, 0.5, 0.5);
		FrameSlot.SetSize(root, size, size);

		Widget icon = root.FindAnyWidget("Icon");
		if (icon)
			icon.SetVisible(false);
		Widget label = root.FindAnyWidget("QRFLabel");
		if (label)
			label.SetVisible(false);
		Widget canvas = root.FindAnyWidget("Frame");
		if (canvas)
			canvas.SetVisible(true);

		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! Arrowhead centred in a size x size canvas (pixels), pointing along a compass heading in degrees
	protected static void DrawArrow(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float size, float heading)
	{
		float half = size * 0.5;
		float radians = heading * Math.DEG2RAD;
		// Screen direction of the heading (screen y grows downwards) and its perpendicular
		float forwardX = Math.Sin(radians);
		float forwardY = -Math.Cos(radians);
		float sideX = -forwardY;
		float sideY = forwardX;

		array<float> points = {};
		AddPoint(points, half, forwardX, forwardY, sideX, sideY, half * 0.95, 0);		// Tip
		AddPoint(points, half, forwardX, forwardY, sideX, sideY, -half * 0.7, half * 0.65);	// Back right
		AddPoint(points, half, forwardX, forwardY, sideX, sideY, -half * 0.3, 0);			// Notch
		AddPoint(points, half, forwardX, forwardY, sideX, sideY, -half * 0.7, -half * 0.65);	// Back left

		PolygonDrawCommand fill = new PolygonDrawCommand();
		fill.m_iColor = AROR_UIColors.REBEL;
		fill.m_Vertices = points;

		LineDrawCommand outline = new LineDrawCommand();
		outline.m_iColor = AROR_UIColors.REBEL_INK;
		outline.m_fWidth = Math.Max(1, size * 0.07);
		outline.m_Vertices = points;
		outline.m_bShouldEnclose = true;

		commands.Clear();
		commands.Insert(fill);
		commands.Insert(outline);
		canvas.SetDrawCommands(commands);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the canvas point `along` the heading and `across` it from the centre
	protected static void AddPoint(notnull array<float> points, float centre, float forwardX, float forwardY, float sideX, float sideY, float along, float across)
	{
		points.Insert(centre + forwardX * along + sideX * across);
		points.Insert(centre + forwardY * along + sideY * across);
	}
}
