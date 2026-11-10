//! Client-only last-seen map widgets. Receives only server-filtered Resistance intelligence.
class AROR_QRFMapIntel
{
	protected static ref array<string> s_aIds = {};
	protected static ref array<vector> s_aPositions = {};
	protected static ref array<int> s_aKinds = {};
	protected static ref array<int> s_aSides = {};
	protected static ref array<int> s_aAges = {};
	protected static float s_fReceivedAt;
	protected static int s_iLifetime;
	protected static int s_iRevision;
	protected ref array<Widget> m_aWidgets = {};
	protected ref map<Widget, ref array<ref CanvasWidgetCommand>> m_mContactCommands = new map<Widget, ref array<ref CanvasWidgetCommand>>();
	protected int m_iDrawnRevision = -1;
	protected float m_fDrawnPixelScale;
	protected float m_fNextLabelsAt;

	static void Clear()
	{
		s_aIds.Clear();
		s_aPositions.Clear();
		s_aKinds.Clear();
		s_aSides.Clear();
		s_aAges.Clear();
		s_iRevision++;
	}
	static void Receive(array<string> ids, array<vector> positions, array<int> kinds, array<int> ages, int lifetime, array<int> sides)
	{
		if (RplSession.Mode() == RplMode.Dedicated || !ids || !positions || !kinds || !ages || !sides)
			return;
		if (ids.Count() > 32 || positions.Count() != ids.Count() || kinds.Count() != ids.Count() || ages.Count() != ids.Count() || sides.Count() != ids.Count())
			return;
		if (lifetime < 15 || lifetime > 300)
			return;
		Clear();
		for (int index = 0; index < ids.Count(); index++)
		{
			if (ids[index].IsEmpty() || s_aIds.Contains(ids[index]) || kinds[index] < 0 || kinds[index] > 6 || ages[index] < 0 || ages[index] >= lifetime)
				continue;
			if (sides[index] != AROR_EFactionRole.OCCUPIERS && sides[index] != AROR_EFactionRole.INVADERS)
				continue;
			s_aIds.Insert(ids[index]);
			s_aPositions.Insert(positions[index]);
			s_aKinds.Insert(kinds[index]);
			s_aSides.Insert(sides[index]);
			s_aAges.Insert(ages[index]);
		}
		s_iLifetime = Math.Clamp(lifetime, 15, 300);
		s_fReceivedAt = GetGame().GetWorld().GetWorldTime() * 0.001;
	}
	static string Label(int kind)
	{
		switch (kind)
		{
			case 1: return "IFV";
			case 2: return "ARMOURED VEHICLE";
			case 3: return "TRANSPORT HELICOPTER";
			case 4: return "GUNSHIP";
			case 5: return "LIGHT ARMED VEHICLE";
			case 6: return "INFANTRY SQUAD";
		}
		return "TRANSPORT VEHICLE";
	}
	void ClearWidgets()
	{
		foreach (Widget widget : m_aWidgets)
		{
			if (widget)
				widget.RemoveFromHierarchy();
		}
		m_aWidgets.Clear();
		m_mContactCommands.Clear();
		m_iDrawnRevision = -1;
	}
	void Update(Widget parent, ResourceName layout, SCR_MapEntity mapEntity)
	{
		if (RplSession.Mode() == RplMode.Dedicated || !parent || !mapEntity || !GetGame().GetPlayerController())
			return;
		IEntity player = GetGame().GetPlayerController().GetControlledEntity();
		if (!AROR_QRFSpotting.IsResistance(player))
		{
			ClearWidgets();
			Clear();
			return;
		}
		while (m_aWidgets.Count() > s_aIds.Count())
		{
			int last = m_aWidgets.Count() - 1;
			if (m_aWidgets[last])
			{
				m_mContactCommands.Remove(m_aWidgets[last]);
				m_aWidgets[last].RemoveFromHierarchy();
			}
			m_aWidgets.Remove(last);
		}
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		float elapsed = now - s_fReceivedAt;
		float pixel = workspace.DPIScale(1);
		bool redraw = m_iDrawnRevision != s_iRevision || pixel != m_fDrawnPixelScale;
		bool refreshLabels = redraw || now >= m_fNextLabelsAt;
		if (refreshLabels)
			m_fNextLabelsAt = now + 1;
		for (int index = 0; index < s_aIds.Count(); index++)
		{
			bool created = index >= m_aWidgets.Count();
			if (index >= m_aWidgets.Count())
				m_aWidgets.Insert(workspace.CreateWidgets(layout, parent));
			Widget root = m_aWidgets[index];
			if (!root)
			{
				// A transient layout failure must not leave a permanently empty slot.
				root = workspace.CreateWidgets(layout, parent);
				m_aWidgets[index] = root;
				created = true;
				if (!root)
					continue;
			}
			float age = s_aAges[index] + elapsed;
			root.SetVisible(age < s_iLifetime);
			if (age >= s_iLifetime)
				continue;
			if (created)
			{
				FrameSlot.SetSize(root, 40, 40);
				FrameSlot.SetAlignment(root, 0.5, 0.5);
				root.SetZOrder(200);
			}
			ImageWidget image = ImageWidget.Cast(root.FindAnyWidget("Icon"));
			// Widgets are reused when snapshot ordering changes; refresh faction tint too.
			Color tint = AROR_UIColors.Get(AROR_UIColors.FACTION_US);
			if (s_aSides[index] == AROR_EFactionRole.INVADERS)
				tint = AROR_UIColors.Get(AROR_UIColors.FACTION_USSR);
			CanvasWidget canvas = CanvasWidget.Cast(root.FindAnyWidget("Frame"));
			if (image)
				image.SetVisible(false);
			if (canvas && (created || redraw))
			{
				array<ref CanvasWidgetCommand> commands;
				if (!m_mContactCommands.Find(root, commands))
				{
					commands = {};
					m_mContactCommands.Set(root, commands);
				}
				canvas.SetVisible(true);
				AROR_MapFrameIcon.DrawContact(canvas, commands, workspace.DPIScale(40), pixel, s_aKinds[index], s_aSides[index], tint);
			}
			RichTextWidget caption = RichTextWidget.Cast(root.FindAnyWidget("QRFLabel"));
			if (caption)
			{
				if (created)
				{
					caption.SetVisible(true);
					caption.SetExactFontSize(18);
					FrameSlot.SetPos(caption, 46, 0);
					FrameSlot.SetSize(caption, 420, 46);
				}
				caption.SetColor(tint);
				if (created || refreshLabels)
					caption.SetText(string.Format("%1 (last seen %2s)", Label(s_aKinds[index]), Math.Floor(age)));
			}
			root.SetOpacity(Math.Max(0.3, 1 - age / s_iLifetime));
			vector position = s_aPositions[index];
			int x, y;
			mapEntity.WorldToScreen(position[0], position[2], x, y, true);
			FrameSlot.SetPos(root, workspace.DPIUnscale(x), workspace.DPIUnscale(y));
		}
		m_iDrawnRevision = s_iRevision;
		m_fDrawnPixelScale = pixel;
	}
}
