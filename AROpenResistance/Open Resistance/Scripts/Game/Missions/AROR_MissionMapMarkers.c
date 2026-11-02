//! Active Resistance missions on any open game map (client only, driven by AROR_LocationMapIconsComponent): a
//! pulsing amber diamond with the codename, type, time left and status at the objective, and a green square at a
//! convoy's destination or where freed prisoners are heading. Reads AROR_MissionBoardComponent's snapshot.
class AROR_MissionMapMarkers
{
	protected static const float MARKER_SIZE = 18;
	protected static const float DESTINATION_SIZE = 14;
	protected static const float PULSE_SECONDS = 1.6;

	protected ref array<Widget> m_aWidgets = {};
	protected ref array<string> m_aCaptions = {};
	protected float m_fNextCaptionsAt;

	//------------------------------------------------------------------------------------------------
	void ClearWidgets()
	{
		foreach (Widget widget : m_aWidgets)
		{
			if (widget)
				widget.RemoveFromHierarchy();
		}

		m_aWidgets.Clear();
		m_aCaptions.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while a map is open
	void Update(Widget parent, ResourceName layout, SCR_MapEntity mapEntity)
	{
		AROR_MissionBoardComponent board = AROR_MissionBoardComponent.GetInstance();
		if (!parent || !mapEntity || !board)
		{
			ClearWidgets();
			return;
		}

		array<AROR_MissionInfo> missions = {};
		board.GetMissions(missions);

		// One marker per active mission, plus one per destination
		array<AROR_MissionInfo> owners = {};
		array<bool> destinations = {};
		foreach (AROR_MissionInfo mission : missions)
		{
			if (!mission.m_bActive)
				continue;

			owners.Insert(mission);
			destinations.Insert(false);
			if (mission.m_bHasDestination)
			{
				owners.Insert(mission);
				destinations.Insert(true);
			}
		}

		while (m_aWidgets.Count() > owners.Count())
		{
			int last = m_aWidgets.Count() - 1;
			if (m_aWidgets[last])
				m_aWidgets[last].RemoveFromHierarchy();

			m_aWidgets.Remove(last);
			m_aCaptions.Remove(last);
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		bool refreshCaptions = now >= m_fNextCaptionsAt;
		if (refreshCaptions)
			m_fNextCaptionsAt = now + 1;

		float wave = Math.Sin(now * Math.PI2 / PULSE_SECONDS);
		Color amber = Color.FromRGBA(230, 168, 58, 255);
		Color green = Color.FromRGBA(159, 211, 107, 255);

		foreach (int i, AROR_MissionInfo info : owners)
		{
			bool destination = destinations[i];
			bool created = i >= m_aWidgets.Count();
			if (created)
			{
				m_aWidgets.Insert(workspace.CreateWidgets(layout, parent));
				m_aCaptions.Insert(string.Empty);
			}

			Widget root = m_aWidgets[i];
			if (!root)
				continue;

			ImageWidget image = ImageWidget.Cast(root.FindAnyWidget("Icon"));
			RichTextWidget caption = RichTextWidget.Cast(root.FindAnyWidget("QRFLabel"));
			float size = MARKER_SIZE;
			Color color = amber;
			vector position = info.m_vPosition;
			if (destination)
			{
				size = DESTINATION_SIZE;
				color = green;
				position = info.m_vDestination;
			}

			if (created)
			{
				FrameSlot.SetAlignment(root, 0.5, 0.5);
				if (caption)
				{
					caption.SetVisible(true);
					caption.SetExactFontSize(17);
					caption.SetShadow(2, 0xFF000000, 0.8, 1, 1);
				}
			}

			// Widgets are reused when missions change order: refresh shape, colour and caption every time
			FrameSlot.SetSize(root, size, size);
			if (image)
			{
				image.SetColor(color);
				image.SetPivot(0.5, 0.5);
				if (destination)
					image.SetRotation(0);
				else
					image.SetRotation(45);
			}

			if (caption)
			{
				FrameSlot.SetPos(caption, size + 8, -6);
				FrameSlot.SetSize(caption, 640, 30);
				caption.SetColor(color);
				if (created || refreshCaptions)
				{
					string text = Caption(board, info, destination);
					if (text != m_aCaptions[i])
					{
						caption.SetText(text);
						m_aCaptions[i] = text;
					}
				}
			}

			float opacity = 1;
			if (!destination)
				opacity = 0.65 + 0.35 * (0.5 + 0.5 * wave);

			root.SetOpacity(opacity);

			int x, y;
			mapEntity.WorldToScreen(position[0], position[2], x, y, true);
			FrameSlot.SetPos(root, workspace.DPIUnscale(x), workspace.DPIUnscale(y));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string Caption(AROR_MissionBoardComponent board, AROR_MissionInfo info, bool destination)
	{
		if (destination)
		{
			string place = AROR_MissionTexts.LocationName(info.m_sDestinationId);
			if (info.m_sDestinationId.IsEmpty())
				place = "safe location";

			return string.Format("%1  ·  %2", info.m_sCodename, place);
		}

		string typeName = AROR_MissionTexts.TypeName(info.m_eType);
		typeName.ToUpper();
		string text = string.Format("%1  ·  %2  ·  %3", info.m_sCodename, typeName, AROR_MissionTexts.Countdown(board.GetSecondsLeft(info)));
		if (info.m_fAreaRadius >= 100)
			text += string.Format("  ·  SEARCH AREA %1 M", Math.Round(info.m_fAreaRadius));

		return text;
	}
}
