//! The local player's personal waypoint: set by shift-clicking the map (see AROR_LocationMapIconsComponent), drawn on
//! the map (AROR_NavigationMapMarkers) and in the world as a HUD marker with the distance to it.
//! Client only and private: nothing is sent to the server or saved.
class AROR_PlayerWaypoint
{
	protected static const float HUD_SIZE = 22;				//!< Diamond size in reference pixels
	protected static const float HUD_EDGE_MARGIN = 48;		//!< Off-screen waypoints stick to the screen edge this far in
	protected static const float HEIGHT_ABOVE_GROUND = 2;	//!< World height of the HUD marker above the terrain
	protected static const int LABEL_SIZE = 17;
	static const string MARKER_NAME = "PERSONAL MARKER";

	protected ResourceName m_sLayout;
	protected bool m_bActive;
	protected vector m_vPosition;
	protected Widget m_wHud;
	protected ref array<ref CanvasWidgetCommand> m_aHudCommands = {};
	protected string m_sHudText;

	//------------------------------------------------------------------------------------------------
	//! layout: the map icon layout (an 'Icon' image, a 'QRFLabel' text and a 'Frame' canvas)
	void AROR_PlayerWaypoint(ResourceName layout)
	{
		m_sLayout = layout;
	}

	//------------------------------------------------------------------------------------------------
	void ~AROR_PlayerWaypoint()
	{
		Clear();
	}

	//------------------------------------------------------------------------------------------------
	bool IsActive()
	{
		return m_bActive;
	}

	//------------------------------------------------------------------------------------------------
	vector GetPosition()
	{
		return m_vPosition;
	}

	//------------------------------------------------------------------------------------------------
	//! Places (or moves) the waypoint at a map position; the height is taken from the terrain
	void Set(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (world)
			position[1] = world.GetSurfaceY(position[0], position[2]);

		m_vPosition = position;
		if (m_bActive)
			return;

		m_bActive = true;
		GetGame().GetCallqueue().CallLater(UpdateHud, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		m_bActive = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(UpdateHud);
		if (m_wHud)
			m_wHud.RemoveFromHierarchy();
		m_wHud = null;
		m_sHudText = string.Empty;
		m_aHudCommands.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! "350 m" or "1.4 km" from a position to the waypoint
	string DistanceText(vector from)
	{
		float distance = vector.DistanceXZ(from, m_vPosition);
		if (distance < 1000)
		{
			int metres = Math.Round(distance);
			return string.Format("%1 m", metres);
		}

		int tenths = Math.Round(distance / 100);
		return string.Format("%1.%2 km", tenths / 10, tenths % 10);
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetLocalEntity()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return controller.GetControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Diamond centred in a size x size canvas (pixels), filled with a dark outline so it reads on any background
	static void DrawDiamond(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float size, int color)
	{
		float half = size * 0.5;
		float inner = size * 0.36;
		array<float> points = {half, half - inner, half + inner, half, half, half + inner, half - inner, half};

		PolygonDrawCommand fill = new PolygonDrawCommand();
		fill.m_iColor = color;
		fill.m_Vertices = points;

		LineDrawCommand outline = new LineDrawCommand();
		outline.m_iColor = AROR_UIColors.REBEL_INK;
		outline.m_fWidth = Math.Max(1, size * 0.08);
		outline.m_Vertices = points;
		outline.m_bShouldEnclose = true;

		commands.Clear();
		commands.Insert(fill);
		commands.Insert(outline);
		canvas.SetDrawCommands(commands);
	}

	//------------------------------------------------------------------------------------------------
	//! "NE 045°": compass point and bearing from a position to the waypoint
	string BearingText(vector from)
	{
		float bearing = Math.Atan2(m_vPosition[0] - from[0], m_vPosition[2] - from[2]) * Math.RAD2DEG;
		if (bearing < 0)
			bearing += 360;

		int degrees = Math.Round(bearing);
		if (degrees >= 360)
			degrees -= 360;

		array<string> points = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
		int point = Math.Round(bearing / 45);
		if (point >= 8)
			point = 0;

		string padded = degrees.ToString();
		if (degrees < 10)
			padded = "00" + padded;
		else if (degrees < 100)
			padded = "0" + padded;

		return string.Format("%1 %2°", points[point], padded);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsHudHidden(IEntity player)
	{
		if (!player || GetGame().GetMenuManager().IsAnyMenuOpen())
			return true;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		return mapEntity && mapEntity.IsOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while the waypoint is set: projects it onto the screen, pinned to the screen edge when it is
	//! outside the view. Hidden while a menu (map, phone) is open or the player controls nothing.
	protected void UpdateHud()
	{
		IEntity player = GetLocalEntity();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		if (!workspace || !world || IsHudHidden(player))
		{
			if (m_wHud)
				m_wHud.SetVisible(false);
			return;
		}

		if (!m_wHud && !CreateHud(workspace))
			return;

		vector target = m_vPosition;
		target[1] = target[1] + HEIGHT_ABOVE_GROUND;
		vector screen = workspace.ProjWorldToScreen(target, world);

		float width = workspace.DPIUnscale(workspace.GetWidth());
		float height = workspace.DPIUnscale(workspace.GetHeight());
		float x = screen[0];
		float y = screen[1];
		if (screen[2] < 0)
		{
			// Behind the camera: the projection is mirrored, so flip it and pin it to the bottom edge
			x = width - x;
			y = height;
		}
		x = Math.Clamp(x, HUD_EDGE_MARGIN, Math.Max(HUD_EDGE_MARGIN, width - HUD_EDGE_MARGIN));
		y = Math.Clamp(y, HUD_EDGE_MARGIN, Math.Max(HUD_EDGE_MARGIN, height - HUD_EDGE_MARGIN));

		m_wHud.SetVisible(true);
		FrameSlot.SetPos(m_wHud, x, y);

		vector origin = player.GetOrigin();
		string text = string.Format("%1\n%2  %3", MARKER_NAME, DistanceText(origin), BearingText(origin));
		if (text != m_sHudText)
		{
			m_sHudText = text;
			RichTextWidget label = RichTextWidget.Cast(m_wHud.FindAnyWidget("QRFLabel"));
			if (label)
				label.SetText(text);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Inside the notification overlay: a widget created at the workspace root is not a frame slot child, so it could
	//! not be positioned or sized
	protected bool CreateHud(notnull WorkspaceWidget workspace)
	{
		if (m_sLayout.IsEmpty())
			return false;

		AROR_NotificationManagerComponent notifications = AROR_NotificationManagerComponent.GetInstance();
		if (!notifications)
			return false;

		Widget overlay = notifications.GetHudRoot();
		if (!overlay)
			return false;

		m_wHud = workspace.CreateWidgets(m_sLayout, overlay);
		if (!m_wHud)
		{
			Print("[AROR] Personal marker: HUD layout could not be created", LogLevel.ERROR);
			return false;
		}

		FrameSlot.SetAlignment(m_wHud, 0.5, 0.5);
		FrameSlot.SetSize(m_wHud, HUD_SIZE, HUD_SIZE);

		Widget icon = m_wHud.FindAnyWidget("Icon");
		if (icon)
			icon.SetVisible(false);

		CanvasWidget canvas = CanvasWidget.Cast(m_wHud.FindAnyWidget("Frame"));
		if (canvas)
		{
			canvas.SetVisible(true);
			DrawDiamond(canvas, m_aHudCommands, workspace.DPIScale(HUD_SIZE), AROR_UIColors.WARN);
		}

		// Distance centred under the diamond
		RichTextWidget label = RichTextWidget.Cast(m_wHud.FindAnyWidget("QRFLabel"));
		if (label)
		{
			label.SetVisible(true);
			label.SetExactFontSize(LABEL_SIZE);
			label.SetColor(AROR_UIColors.Get(AROR_UIColors.WARN));
			label.SetBold(true);
			FrameSlot.SetSizeToContent(label, true);
			FrameSlot.SetAlignment(label, 0.5, 0);
			FrameSlot.SetPos(label, HUD_SIZE * 0.5, HUD_SIZE + 2);
		}

		m_sHudText = string.Empty;
		return true;
	}
}
