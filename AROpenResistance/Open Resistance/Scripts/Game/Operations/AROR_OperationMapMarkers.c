//! Outstanding enemy QRFs and attacks on any open game map (client only, driven by AROR_LocationMapIconsComponent):
//! a pulsing target ring with diagonal ticks around the threatened location, and a bold caption under the
//! location's name such as "UNDER NATO ATTACK  ·  ETA 3:05" that stays for as long as the response is outstanding.
//! Attacks are red, QRFs amber. Reads the operation manager's threat board.
class AROR_OperationMapMarkers
{
	//! Ring radius in reference pixels: just outside the 40 px location icon and inside its name, which starts 25 px
	//! right of centre. The ticks are diagonal so they clear the name and the caption.
	protected static const float RING_RADIUS = 23.5;
	protected static const float TICK_LENGTH = 10;
	protected static const float RING_SIZE = 74;		//!< Canvas square holding the ring and ticks
	protected static const float RING_WIDTH = 2.6;
	protected static const float KEYLINE_WIDTH = 5;
	protected static const float PULSE_SECONDS = 1.2;
	protected static const int RING_STEPS = 40;
	protected static const int CAPTION_FONT_SIZE = 18;
	protected static const float CAPTION_WIDTH = 520;
	protected static const float CAPTION_HEIGHT = 28;
	protected static const float CAPTION_OFFSET_X = 25;	//!< Lined up with the location's name
	protected static const float CAPTION_OFFSET_Y = 32;	//!< Just below the name's plate

	protected ref array<Widget> m_aWidgets = {};
	//! Caption per ring, a sibling rather than a child so the ring's small frame never clips it
	protected ref array<Widget> m_aCaptionWidgets = {};
	//! Dark plate behind each caption (created before it, so drawn beneath), and its draw commands
	protected ref array<Widget> m_aPlateWidgets = {};
	protected ref array<ref array<ref CanvasWidgetCommand>> m_aPlateCommands = {};
	protected ref array<string> m_aCaptions = {};
	protected ref array<int> m_aColors = {};
	//! Draw commands per ring canvas: the canvas does not keep its own copy
	protected ref array<ref array<ref CanvasWidgetCommand>> m_aCommands = {};
	protected float m_fNextCaptionsAt;

	//------------------------------------------------------------------------------------------------
	void ClearWidgets()
	{
		foreach (Widget widget : m_aWidgets)
		{
			if (widget)
				widget.RemoveFromHierarchy();
		}
		foreach (Widget caption : m_aCaptionWidgets)
		{
			if (caption)
				caption.RemoveFromHierarchy();
		}
		foreach (Widget plate : m_aPlateWidgets)
		{
			if (plate)
				plate.RemoveFromHierarchy();
		}

		m_aWidgets.Clear();
		m_aCaptionWidgets.Clear();
		m_aPlateWidgets.Clear();
		m_aPlateCommands.Clear();
		m_aCaptions.Clear();
		m_aColors.Clear();
		m_aCommands.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while a map is open
	void Update(Widget parent, ResourceName layout, SCR_MapEntity mapEntity)
	{
		AROR_OperationManagerComponent manager = Manager();
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!parent || !mapEntity || !manager || !locations)
		{
			ClearWidgets();
			return;
		}

		array<AROR_OperationThreatInfo> threats = {};
		manager.GetThreats(threats);
		array<AROR_OperationThreatInfo> shown = {};
		array<AROR_LocationComponent> targets = {};
		foreach (AROR_OperationThreatInfo threat : threats)
		{
			AROR_LocationComponent target = locations.GetLocationById(threat.m_sTargetId);
			if (!target)
				continue;

			shown.Insert(threat);
			targets.Insert(target);
		}

		while (m_aWidgets.Count() > shown.Count())
		{
			int last = m_aWidgets.Count() - 1;
			if (m_aWidgets[last])
				m_aWidgets[last].RemoveFromHierarchy();
			if (m_aCaptionWidgets[last])
				m_aCaptionWidgets[last].RemoveFromHierarchy();
			if (m_aPlateWidgets[last])
				m_aPlateWidgets[last].RemoveFromHierarchy();

			m_aWidgets.Remove(last);
			m_aCaptionWidgets.Remove(last);
			m_aPlateWidgets.Remove(last);
			m_aPlateCommands.Remove(last);
			m_aCaptions.Remove(last);
			m_aColors.Remove(last);
			m_aCommands.Remove(last);
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		bool refreshCaptions = now >= m_fNextCaptionsAt;
		if (refreshCaptions)
			m_fNextCaptionsAt = now + 1;

		float wave = 0.5 + 0.5 * Math.Sin(now * Math.PI2 / PULSE_SECONDS);

		foreach (int i, AROR_OperationThreatInfo info : shown)
		{
			bool created = i >= m_aWidgets.Count();
			if (created)
			{
				m_aWidgets.Insert(CreateRing(workspace, layout, parent));
				m_aPlateWidgets.Insert(CreatePlate(workspace, layout, parent));
				m_aPlateCommands.Insert(new array<ref CanvasWidgetCommand>());
				m_aCaptionWidgets.Insert(CreateCaption(workspace, layout, parent));
				m_aCaptions.Insert(string.Empty);
				m_aColors.Insert(0);
				m_aCommands.Insert(new array<ref CanvasWidgetCommand>());
			}

			Widget root = m_aWidgets[i];
			Widget captionRoot = m_aCaptionWidgets[i];
			int color = AROR_UIColors.WARN;
			if (info.m_eType == AROR_EOperationType.ATTACK)
				color = AROR_UIColors.DANGER;

			// Widgets are reused when threats change order: redraw the ring when its colour changes
			if (created || m_aColors[i] != color)
			{
				m_aColors[i] = color;
				if (root)
				{
					CanvasWidget canvas = CanvasWidget.Cast(root.FindAnyWidget("Frame"));
					if (canvas)
						DrawRing(canvas, m_aCommands[i], workspace.DPIScale(RING_SIZE), workspace.DPIScale(1), color);
				}
				m_aCaptions[i] = string.Empty;
			}

			RichTextWidget caption;
			if (captionRoot)
				caption = RichTextWidget.Cast(captionRoot.FindAnyWidget("QRFLabel"));
			if (caption && (refreshCaptions || m_aCaptions[i].IsEmpty()))
			{
				string text = Caption(info, manager.GetThreatEtaRemaining(info));
				if (text != m_aCaptions[i])
				{
					caption.SetColor(AROR_UIColors.Get(color));
					caption.SetText(text);
					m_aCaptions[i] = text;
					// The plate follows the caption's length; the countdown keeps it nearly constant
					Widget plate = m_aPlateWidgets[i];
					if (plate)
					{
						float width = AROR_MapLabelPlate.Width(text, CAPTION_FONT_SIZE);
						FrameSlot.SetSize(plate, width, CAPTION_HEIGHT);
						CanvasWidget plateCanvas = CanvasWidget.Cast(plate.FindAnyWidget("Frame"));
						if (plateCanvas)
							AROR_MapLabelPlate.Draw(plateCanvas, m_aPlateCommands[i], width, CAPTION_HEIGHT, color);
					}
				}
			}

			vector center = targets[i].GetCenter();
			int x, y;
			mapEntity.WorldToScreen(center[0], center[2], x, y, true);
			float screenX = workspace.DPIUnscale(x);
			float screenY = workspace.DPIUnscale(y);
			if (root)
			{
				root.SetOpacity(0.55 + 0.45 * wave);
				FrameSlot.SetPos(root, screenX, screenY);
			}
			if (captionRoot)
				FrameSlot.SetPos(captionRoot, screenX + CAPTION_OFFSET_X, screenY + CAPTION_OFFSET_Y);
			if (m_aPlateWidgets[i])
				FrameSlot.SetPos(m_aPlateWidgets[i], screenX + CAPTION_OFFSET_X, screenY + CAPTION_OFFSET_Y);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "UNDER NATO ATTACK  ·  ETA 3:05", "NATO ATTACK PREPARING", "USSR QRF INBOUND  ·  ETA 1:20", "NATO QRF ON SITE"
	static string Caption(notnull AROR_OperationThreatInfo info, int etaRemaining)
	{
		string army = AROR_OperationManagerComponent.ThreatArmyName(info.m_eRole);
		bool attack = info.m_eType == AROR_EOperationType.ATTACK;
		if (info.m_eState != AROR_EOperationState.ACTIVE)
		{
			if (attack)
				return army + " ATTACK PREPARING";
			return army + " QRF PREPARING";
		}

		string text = army + " QRF INBOUND";
		if (attack)
			text = "UNDER " + army + " ATTACK";
		if (etaRemaining > 0)
			return text + "  ·  " + AROR_OperationEta.Countdown(etaRemaining);
		if (attack)
			return text;
		return army + " QRF ON SITE";
	}

	//------------------------------------------------------------------------------------------------
	protected static Widget CreateRing(notnull WorkspaceWidget workspace, ResourceName layout, notnull Widget parent)
	{
		Widget root = workspace.CreateWidgets(layout, parent);
		if (!root)
			return null;
		FrameSlot.SetAlignment(root, 0.5, 0.5);
		FrameSlot.SetSize(root, RING_SIZE, RING_SIZE);
		Widget icon = root.FindAnyWidget("Icon");
		if (icon)
			icon.SetVisible(false);
		Widget label = root.FindAnyWidget("QRFLabel");
		if (label)
			label.SetVisible(false);
		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! The dark plate under a caption: the icon layout with only its canvas showing
	protected static Widget CreatePlate(notnull WorkspaceWidget workspace, ResourceName layout, notnull Widget parent)
	{
		Widget root = workspace.CreateWidgets(layout, parent);
		if (!root)
			return null;
		Widget icon = root.FindAnyWidget("Icon");
		if (icon)
			icon.SetVisible(false);
		Widget label = root.FindAnyWidget("QRFLabel");
		if (label)
			label.SetVisible(false);
		FrameSlot.SetAlignment(root, 0, 0.5);
		FrameSlot.SetSize(root, 1, CAPTION_HEIGHT);
		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! Built like the location names (icon layout, icon and frame hidden): bold threat colour on a dark plate
	protected static Widget CreateCaption(notnull WorkspaceWidget workspace, ResourceName layout, notnull Widget parent)
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
			label.SetExactFontSize(CAPTION_FONT_SIZE);
			label.SetBold(true);
			FrameSlot.SetPos(label, AROR_MapLabelPlate.TextInset(), CAPTION_FONT_SIZE * 0.12);
			FrameSlot.SetSize(label, CAPTION_WIDTH, CAPTION_HEIGHT);
		}
		FrameSlot.SetAlignment(root, 0, 0.5);
		FrameSlot.SetSize(root, CAPTION_WIDTH, CAPTION_HEIGHT);
		return root;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DrawRing(notnull CanvasWidget canvas, notnull array<ref CanvasWidgetCommand> commands, float size, float pixel, int color)
	{
		commands.Clear();
		float centre = size * 0.5;
		float radius = RING_RADIUS * pixel;
		array<float> ring = {};
		for (int step = 0; step < RING_STEPS; step++)
		{
			float angle = step * Math.PI2 / RING_STEPS;
			ring.Insert(centre + Math.Cos(angle) * radius);
			ring.Insert(centre + Math.Sin(angle) * radius);
		}

		int keyline = 0xC0000000;
		commands.Insert(Line(ring, keyline, KEYLINE_WIDTH * pixel, true));
		commands.Insert(Line(ring, color, RING_WIDTH * pixel, true));

		// Diagonal ticks pointing in at the location, like a reticle
		for (int tick = 0; tick < 4; tick++)
		{
			float tickAngle = (tick + 0.5) * Math.PI * 0.5;
			float dx = Math.Cos(tickAngle);
			float dy = Math.Sin(tickAngle);
			float inner = radius + RING_WIDTH * pixel;
			float outer = radius + TICK_LENGTH * pixel;
			commands.Insert(Line(Segment(centre, dx, dy, inner, outer), keyline, KEYLINE_WIDTH * pixel, false));
			commands.Insert(Line(Segment(centre, dx, dy, inner, outer), color, RING_WIDTH * pixel, false));
		}

		canvas.SetDrawCommands(commands);
	}

	//------------------------------------------------------------------------------------------------
	//! Two points along a direction from the centre, between two radii (each command keeps its own vertices)
	protected static array<float> Segment(float centre, float dx, float dy, float inner, float outer)
	{
		array<float> points = {};
		points.Insert(centre + dx * inner);
		points.Insert(centre + dy * inner);
		points.Insert(centre + dx * outer);
		points.Insert(centre + dy * outer);
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

	//------------------------------------------------------------------------------------------------
	protected static AROR_OperationManagerComponent Manager()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;
		return AROR_OperationManagerComponent.Cast(gameMode.FindComponent(AROR_OperationManagerComponent));
	}
}
