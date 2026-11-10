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

	// Threat tracks: one full-map canvas, under the rings, redrawn each frame while anything is inbound
	protected static const float TRACK_WIDTH = 3;
	protected static const float TRACK_KEYLINE = 6;
	protected static const float HEAD_LENGTH = 16;
	protected static const float HEAD_WIDTH = 15;
	protected static const float DASH = 14;
	protected static const float GAP = 9;
	protected static const int MAX_DASHES = 160;
	protected static const float LEAD_SIZE = 7;
	protected static const float LZ_RADIUS = 12;
	protected Widget m_wTracks;
	protected ref array<ref CanvasWidgetCommand> m_aTrackCommands = {};
	protected bool m_bTracksDrawn;

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

		if (m_wTracks)
			m_wTracks.RemoveFromHierarchy();
		m_wTracks = null;
		m_aTrackCommands.Clear();
		m_bTracksDrawn = false;

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
		// Every column and helicopter flight gets its own track, even where they share one ring
		UpdateTracks(parent, layout, mapEntity, locations, threats);
		array<AROR_OperationThreatInfo> shown = {};
		array<AROR_LocationComponent> targets = {};
		foreach (AROR_OperationThreatInfo threat : threats)
		{
			AROR_LocationComponent target = locations.GetLocationById(threat.m_sTargetId);
			if (!target)
				continue;

			// A town's waves and columns share one marker: the one already moving (soonest arrival) speaks for all
			int same = -1;
			if (threat.m_bTownResponse)
			{
				foreach (int index, AROR_OperationThreatInfo other : shown)
				{
					if (other.m_bTownResponse && other.m_sTargetId == threat.m_sTargetId)
						same = index;
				}
			}
			if (same >= 0)
			{
				if (ArrivesSooner(manager, threat, shown[same]))
					shown[same] = threat;
				continue;
			}

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
			if (info.m_eType == AROR_EOperationType.ATTACK || info.m_bTownResponse)
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
		if (info.m_bTownResponse)
			return TownCaption(info, army, etaRemaining);
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
	//! A town garrison's response, worded as the answer to a contact report: "CONTACT · NATO REINFORCEMENTS
	//! FORMING", "NATO REINFORCEMENTS INBOUND · ETA 1:20", "NATO REINFORCEMENTS IN TOWN"
	static string TownCaption(notnull AROR_OperationThreatInfo info, string army, int etaRemaining)
	{
		if (info.m_eState != AROR_EOperationState.ACTIVE)
			return "CONTACT  ·  " + army + " REINFORCEMENTS FORMING";
		if (etaRemaining > 0)
			return army + " REINFORCEMENTS INBOUND  ·  " + AROR_OperationEta.Countdown(etaRemaining);
		return army + " REINFORCEMENTS IN TOWN";
	}

	//! Of two responses to the same town, whether the first is the one to show: moving beats forming, then the
	//! soonest arrival
	protected static bool ArrivesSooner(notnull AROR_OperationManagerComponent manager, notnull AROR_OperationThreatInfo first, notnull AROR_OperationThreatInfo second)
	{
		bool firstActive = first.m_eState == AROR_EOperationState.ACTIVE;
		bool secondActive = second.m_eState == AROR_EOperationState.ACTIVE;
		if (firstActive != secondActive)
			return firstActive;
		return manager.GetThreatEtaRemaining(first) < manager.GetThreatEtaRemaining(second);
	}

	//------------------------------------------------------------------------------------------------
	//! Where each inbound force is and where it is headed: an arrow from its head (vehicle or leader) to the target's
	//! ring, solid for ground columns, dashed for helicopters, which point at their landing zone (a circled X) when
	//! one is chosen. Updated from the threat board every few seconds; the arrow vanishes once the force arrives.
	protected void UpdateTracks(Widget parent, ResourceName layout, SCR_MapEntity mapEntity, AROR_LocationManagerComponent locations, array<AROR_OperationThreatInfo> threats)
	{
		bool any;
		foreach (AROR_OperationThreatInfo threat : threats)
		{
			if (threat.m_bHasLead)
				any = true;
		}

		if (!any)
		{
			if (m_bTracksDrawn && m_wTracks)
			{
				m_aTrackCommands.Clear();
				CanvasWidget cleared = CanvasWidget.Cast(m_wTracks.FindAnyWidget("Frame"));
				if (cleared)
					cleared.SetDrawCommands(m_aTrackCommands);
			}
			m_bTracksDrawn = false;
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!m_wTracks)
			m_wTracks = CreateTrackCanvas(workspace, layout, parent);
		if (!m_wTracks)
			return;

		float width, height;
		parent.GetScreenSize(width, height);
		FrameSlot.SetPos(m_wTracks, 0, 0);
		FrameSlot.SetSize(m_wTracks, workspace.DPIUnscale(width), workspace.DPIUnscale(height));
		CanvasWidget canvas = CanvasWidget.Cast(m_wTracks.FindAnyWidget("Frame"));
		if (!canvas)
			return;

		float pixel = workspace.DPIScale(1);
		m_aTrackCommands.Clear();
		foreach (AROR_OperationThreatInfo info : threats)
		{
			if (!info.m_bHasLead)
				continue;
			AROR_LocationComponent target = locations.GetLocationById(info.m_sTargetId);
			if (!target)
				continue;

			int color = AROR_UIColors.WARN;
			if (info.m_eType == AROR_EOperationType.ATTACK || info.m_bTownResponse)
				color = AROR_UIColors.DANGER;
			int argb = AROR_UIColors.Get(color).PackToInt();

			float leadX, leadY, endX, endY;
			ToScreen(mapEntity, info.m_vLead, leadX, leadY);
			vector goal = target.GetCenter();
			float stopShort = (RING_RADIUS + 8) * pixel;
			if (info.m_bAir && info.m_bHasLanding)
			{
				goal = info.m_vLanding;
				stopShort = (LZ_RADIUS + 5) * pixel;
			}
			ToScreen(mapEntity, goal, endX, endY);

			if (info.m_bAir && info.m_bHasLanding)
				AddLandingZone(endX, endY, pixel, argb);

			float dx = endX - leadX;
			float dy = endY - leadY;
			float length = Math.Sqrt(dx * dx + dy * dy);
			// Arrived (or nearly): the ring says the rest
			if (length <= stopShort + HEAD_LENGTH * pixel)
				continue;

			float ux = dx / length;
			float uy = dy / length;
			float tipX = endX - ux * stopShort;
			float tipY = endY - uy * stopShort;
			float baseX = tipX - ux * HEAD_LENGTH * pixel;
			float baseY = tipY - uy * HEAD_LENGTH * pixel;

			if (info.m_bAir)
				AddDashes(leadX, leadY, baseX, baseY, ux, uy, pixel, argb);
			else
			{
				m_aTrackCommands.Insert(Line(TrackPoints(leadX, leadY, baseX, baseY), 0xC0000000, TRACK_KEYLINE * pixel, false));
				m_aTrackCommands.Insert(Line(TrackPoints(leadX, leadY, baseX, baseY), argb, TRACK_WIDTH * pixel, false));
			}

			// Arrow head, outlined
			float halfHead = HEAD_WIDTH * 0.5 * pixel;
			array<float> head = {};
			head.Insert(tipX);
			head.Insert(tipY);
			head.Insert(baseX - uy * halfHead);
			head.Insert(baseY + ux * halfHead);
			head.Insert(baseX + uy * halfHead);
			head.Insert(baseY - ux * halfHead);
			m_aTrackCommands.Insert(Line(head, 0xC0000000, 3 * pixel, true));
			m_aTrackCommands.Insert(TrackFill(head, argb));

			// The head of the column or the helicopter: a diamond
			float size = LEAD_SIZE * pixel;
			array<float> diamond = {};
			diamond.Insert(leadX);
			diamond.Insert(leadY - size);
			diamond.Insert(leadX + size);
			diamond.Insert(leadY);
			diamond.Insert(leadX);
			diamond.Insert(leadY + size);
			diamond.Insert(leadX - size);
			diamond.Insert(leadY);
			m_aTrackCommands.Insert(Line(diamond, 0xC0000000, 3 * pixel, true));
			m_aTrackCommands.Insert(TrackFill(diamond, argb));
		}

		canvas.SetDrawCommands(m_aTrackCommands);
		m_bTracksDrawn = true;
	}

	//! World x, z to the canvas's pixels
	protected static void ToScreen(SCR_MapEntity mapEntity, vector world, out float x, out float y)
	{
		int screenX, screenY;
		mapEntity.WorldToScreen(world[0], world[2], screenX, screenY, true);
		x = screenX;
		y = screenY;
	}

	protected void AddDashes(float fromX, float fromY, float toX, float toY, float ux, float uy, float pixel, int argb)
	{
		float total = Math.Sqrt((toX - fromX) * (toX - fromX) + (toY - fromY) * (toY - fromY));
		float step = (DASH + GAP) * pixel;
		float travelled = 0;
		for (int dash = 0; dash < MAX_DASHES && travelled < total; dash++)
		{
			float dashEnd = Math.Min(travelled + DASH * pixel, total);
			array<float> segment = TrackPoints(fromX + ux * travelled, fromY + uy * travelled, fromX + ux * dashEnd, fromY + uy * dashEnd);
			m_aTrackCommands.Insert(Line(segment, 0xC0000000, TRACK_KEYLINE * pixel, false));
			m_aTrackCommands.Insert(Line(TrackPoints(fromX + ux * travelled, fromY + uy * travelled, fromX + ux * dashEnd, fromY + uy * dashEnd), argb, TRACK_WIDTH * pixel, false));
			travelled += step;
		}
	}

	//! A helicopter landing zone: a circle with an X
	protected void AddLandingZone(float x, float y, float pixel, int argb)
	{
		float radius = LZ_RADIUS * pixel;
		array<float> ring = {};
		for (int step = 0; step < 24; step++)
		{
			float angle = step * Math.PI2 / 24;
			ring.Insert(x + Math.Cos(angle) * radius);
			ring.Insert(y + Math.Sin(angle) * radius);
		}
		m_aTrackCommands.Insert(Line(ring, 0xC0000000, TRACK_KEYLINE * pixel, true));
		m_aTrackCommands.Insert(Line(ring, argb, TRACK_WIDTH * pixel, true));

		float arm = radius * 0.6;
		m_aTrackCommands.Insert(Line(TrackPoints(x - arm, y - arm, x + arm, y + arm), argb, TRACK_WIDTH * pixel, false));
		m_aTrackCommands.Insert(Line(TrackPoints(x - arm, y + arm, x + arm, y - arm), argb, TRACK_WIDTH * pixel, false));
	}

	protected static array<float> TrackPoints(float x1, float y1, float x2, float y2)
	{
		array<float> points = {};
		points.Insert(x1);
		points.Insert(y1);
		points.Insert(x2);
		points.Insert(y2);
		return points;
	}

	protected static PolygonDrawCommand TrackFill(array<float> points, int color)
	{
		PolygonDrawCommand polygon = new PolygonDrawCommand();
		polygon.m_iColor = color;
		polygon.m_Vertices = points;
		return polygon;
	}

	//! The icon layout with only its canvas showing, spread over the whole map and ignoring the cursor
	protected static Widget CreateTrackCanvas(notnull WorkspaceWidget workspace, ResourceName layout, notnull Widget parent)
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
		root.SetFlags(WidgetFlags.IGNORE_CURSOR);
		Widget frame = root.FindAnyWidget("Frame");
		if (frame)
			frame.SetFlags(WidgetFlags.IGNORE_CURSOR);
		FrameSlot.SetAlignment(root, 0, 0);
		return root;
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
