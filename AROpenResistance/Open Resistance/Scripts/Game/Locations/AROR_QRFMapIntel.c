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
	protected ref array<Widget> m_aWidgets = {};
	protected float m_fNextLabelsAt;

	static void Clear()
	{
		s_aIds.Clear();
		s_aPositions.Clear();
		s_aKinds.Clear();
		s_aSides.Clear();
		s_aAges.Clear();
	}
	static void Receive(array<string> ids, array<vector> positions, array<int> kinds, array<int> ages, int lifetime, array<int> sides)
	{
		if (RplSession.Mode() == RplMode.Dedicated || !ids || !positions || !kinds || !ages || !sides)
			return;
		if (ids.Count() > 32 || positions.Count() != ids.Count() || kinds.Count() != ids.Count() || ages.Count() != ids.Count() || sides.Count() != ids.Count())
			return;
		Clear();
		for (int index = 0; index < ids.Count(); index++)
		{
			if (kinds[index] < 0 || kinds[index] > 4 || ages[index] < 0 || ages[index] >= lifetime)
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
	}
	void Update(Widget parent, ResourceName layout, SCR_MapEntity mapEntity)
	{
		if (!parent || !mapEntity || !GetGame().GetPlayerController())
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
				m_aWidgets[last].RemoveFromHierarchy();
			m_aWidgets.Remove(last);
		}
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		float elapsed = now - s_fReceivedAt;
		bool refreshLabels = now >= m_fNextLabelsAt;
		if (refreshLabels)
			m_fNextLabelsAt = now + 1;
		for (int index = 0; index < s_aIds.Count(); index++)
		{
			bool created = index >= m_aWidgets.Count();
			if (index >= m_aWidgets.Count())
				m_aWidgets.Insert(workspace.CreateWidgets(layout, parent));
			Widget root = m_aWidgets[index];
			if (!root)
				continue;
			float age = s_aAges[index] + elapsed;
			root.SetVisible(age < s_iLifetime);
			if (age >= s_iLifetime)
				continue;
			if (created)
			{
				FrameSlot.SetSize(root, 40, 40);
				FrameSlot.SetAlignment(root, 0.5, 0.5);
			}
			ImageWidget image = ImageWidget.Cast(root.FindAnyWidget("Icon"));
			if (image && created)
			{
				image.LoadImageTexture(0, "{8EFE6E503C402610}UI/Textures/Map/military_base.edds");
			}
			// Widgets are reused when snapshot ordering changes; refresh faction tint too.
			Color tint = Color.FromRGBA(65, 145, 255, 255);
			if (s_aSides[index] == AROR_EFactionRole.INVADERS)
				tint = Color.FromRGBA(245, 65, 65, 255);
			if (image)
				image.SetColor(tint);
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
	}
}
