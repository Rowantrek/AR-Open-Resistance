//! Every living player on any open game map (client only, driven by AROR_LocationMapIconsComponent): a NATO-style
//! infantry symbol (rectangle crossed by an X) with the player's name beside it. Resistance colour
//! normally, red while the player is unconscious. Reads the player board of AROR_PlayerMarkersComponent.
class AROR_PlayerMapMarkers
{
	protected static const float SYMBOL_WIDTH = 24;		//!< Reference pixels
	protected static const float SYMBOL_HEIGHT = 16;
	protected static const float LINE_WIDTH = 2.2;
	protected static const float KEYLINE_WIDTH = 4.4;
	protected static const int FILL_COLOR = 0xB0080B09;
	protected static const int KEYLINE_COLOR = 0xC0000000;
	protected static const int NAME_FONT_SIZE = 12;
	protected static const float NAME_WIDTH = 400;
	protected static const float NAME_HEIGHT = 20;
	protected static const float NAME_OFFSET_X = 16;	//!< Right of the symbol's centre, clear of its edge
	protected static const float NAME_OFFSET_Y = 0;

	protected ref array<Widget> m_aWidgets = {};
	protected ref array<Widget> m_aNameWidgets = {};
	//! Draw commands per symbol canvas: the canvas does not keep its own copy
	protected ref array<ref array<ref CanvasWidgetCommand>> m_aCommands = {};
	protected ref array<string> m_aNames = {};
	protected ref array<int> m_aColors = {};

	//------------------------------------------------------------------------------------------------
	void ClearWidgets()
	{
		foreach (int i, Widget widget : m_aWidgets)
		{
			RemoveWidgets(i);
		}

		m_aWidgets.Clear();
		m_aNameWidgets.Clear();
		m_aCommands.Clear();
		m_aNames.Clear();
		m_aColors.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveWidgets(int index)
	{
		if (m_aWidgets[index])
			m_aWidgets[index].RemoveFromHierarchy();
		if (m_aNameWidgets[index])
			m_aNameWidgets[index].RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while a map is open
	void Update(Widget parent, ResourceName layout, SCR_MapEntity mapEntity)
	{
		AROR_PlayerMarkersComponent board = AROR_PlayerMarkersComponent.GetInstance();
		if (!parent || !mapEntity || !board)
		{
			ClearWidgets();
			return;
		}

		array<AROR_PlayerMarkerInfo> players = {};
		board.GetPlayers(players);

		while (m_aWidgets.Count() > players.Count())
		{
			int last = m_aWidgets.Count() - 1;
			RemoveWidgets(last);
			m_aWidgets.Remove(last);
			m_aNameWidgets.Remove(last);
			m_aCommands.Remove(last);
			m_aNames.Remove(last);
			m_aColors.Remove(last);
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		int resistance = ResistanceColor();

		foreach (int i, AROR_PlayerMarkerInfo info : players)
		{
			bool created = i >= m_aWidgets.Count();
			if (created)
			{
				m_aWidgets.Insert(CreateSymbol(workspace, layout, parent));
				m_aNameWidgets.Insert(CreateName(workspace, layout, parent));
				m_aCommands.Insert(new array<ref CanvasWidgetCommand>());
				m_aNames.Insert(string.Empty);
				m_aColors.Insert(0);
			}

			int color = resistance;
			if (info.m_bUnconscious)
				color = AROR_UIColors.DANGER;

			// Widgets are reused when players join or leave: redraw whatever the colour or name changed
			string name = GetGame().GetPlayerManager().GetPlayerName(info.m_iPlayerId);
			bool recoloured = created || m_aColors[i] != color;
			if (recoloured)
			{
				m_aColors[i] = color;
				if (m_aWidgets[i])
				{
					CanvasWidget canvas = CanvasWidget.Cast(m_aWidgets[i].FindAnyWidget("Frame"));
					if (canvas)
						DrawSymbol(canvas, m_aCommands[i], workspace.DPIScale(SYMBOL_WIDTH), workspace.DPIScale(SYMBOL_HEIGHT), workspace.DPIScale(1), color);
				}
			}
			if (recoloured || name != m_aNames[i])
			{
				m_aNames[i] = name;
				ApplyName(i, name, color);
			}

			int screenX, screenY;
			mapEntity.WorldToScreen(info.m_fX, info.m_fZ, screenX, screenY, true);
			float x = workspace.DPIUnscale(screenX);
			float y = workspace.DPIUnscale(screenY);
			if (m_aWidgets[i])
				FrameSlot.SetPos(m_aWidgets[i], x, y);
			if (m_aNameWidgets[i])
				FrameSlot.SetPos(m_aNameWidgets[i], x + NAME_OFFSET_X, y + NAME_OFFSET_Y);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyName(int index, string name, int color)
	{
		string text = name;
		text.ToUpper();

		RichTextWidget label;
		if (m_aNameWidgets[index])
			label = RichTextWidget.Cast(m_aNameWidgets[index].FindAnyWidget("QRFLabel"));
		if (label)
		{
			label.SetColor(AROR_UIColors.Get(color));
			label.SetText(text);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Resistance faction colour as ARGB, falling back to the design-system rebel colour
	protected static int ResistanceColor()
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (roles)
		{
			Faction faction = roles.GetFaction(AROR_EFactionRole.RESISTANCE);
			if (faction)
				return faction.GetFactionColor().PackToInt();
		}
		return AROR_UIColors.REBEL;
	}

	//------------------------------------------------------------------------------------------------
	protected static Widget CreateSymbol(notnull WorkspaceWidget workspace, ResourceName layout, notnull Widget parent)
	{
		Widget root = workspace.CreateWidgets(layout, parent);
		if (!root)
			return null;
		FrameSlot.SetAlignment(root, 0.5, 0.5);
		FrameSlot.SetSize(root, SYMBOL_WIDTH, SYMBOL_HEIGHT);
		Widget icon = root.FindAnyWidget("Icon");
		if (icon)
			icon.SetVisible(false);
		Widget label = root.FindAnyWidget("QRFLabel");
		if (label)
			label.SetVisible(false);
		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! The icon layout with icon and frame hidden, so only the name shows
	protected static Widget CreateName(notnull WorkspaceWidget workspace, ResourceName layout, notnull Widget parent)
	{
		Widget root = workspace.CreateWidgets(layout, parent);
		if (!root)
			return null;
		Widget icon = root.FindAnyWidget("Icon");
		if (icon)
			icon.SetVisible(false);
		Widget frame = root.FindAnyWidget("Frame");
		if (frame)
			frame.SetVisible(false);
		RichTextWidget label = RichTextWidget.Cast(root.FindAnyWidget("QRFLabel"));
		if (label)
		{
			label.SetVisible(true);
			label.SetExactFontSize(NAME_FONT_SIZE);
			label.SetBold(true);
			FrameSlot.SetPos(label, 0, NAME_FONT_SIZE * 0.2);
			FrameSlot.SetSize(label, NAME_WIDTH, NAME_HEIGHT);
		}
		FrameSlot.SetAlignment(root, 0, 0.5);
		FrameSlot.SetSize(root, NAME_WIDTH, NAME_HEIGHT);
		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! Infantry symbol: dark-filled rectangle, crossed by an X, in the given colour over a dark keyline
	protected static void DrawSymbol(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float width, float height, float pixel, int color)
	{
		commands.Clear();
		float inset = KEYLINE_WIDTH * pixel * 0.5;
		float left = inset;
		float top = inset;
		float right = width - inset;
		float bottom = height - inset;

		array<float> box = {left, top, right, top, right, bottom, left, bottom};
		PolygonDrawCommand fill = new PolygonDrawCommand();
		fill.m_iColor = FILL_COLOR;
		fill.m_Vertices = box;
		commands.Insert(fill);

		commands.Insert(Line({left, top, right, bottom}, KEYLINE_COLOR, KEYLINE_WIDTH * pixel, false));
		commands.Insert(Line({left, bottom, right, top}, KEYLINE_COLOR, KEYLINE_WIDTH * pixel, false));
		commands.Insert(Line(box, KEYLINE_COLOR, KEYLINE_WIDTH * pixel, true));

		commands.Insert(Line({left, top, right, bottom}, color, LINE_WIDTH * pixel, false));
		commands.Insert(Line({left, bottom, right, top}, color, LINE_WIDTH * pixel, false));
		commands.Insert(Line(box, color, LINE_WIDTH * pixel, true));

		canvas.SetDrawCommands(commands);
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
