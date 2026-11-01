//! Client map layer for player annotations; widgets and draw commands live only while a map is open.
class AROR_PlayerMapMarkers
{
	protected ref array<Widget> m_aWidgets = {};
	protected ref array<Widget> m_aPlates = {};
	protected ref array<ref array<ref CanvasWidgetCommand>> m_aPlateCommands = {};
	protected ref array<string> m_aCaptions = {};
	protected ref array<int> m_aColors = {};

	void ClearWidgets()
	{
		foreach (Widget widget : m_aWidgets)
		{
			if (widget)
				widget.RemoveFromHierarchy();
		}
		foreach (Widget plate : m_aPlates)
		{
			if (plate) plate.RemoveFromHierarchy();
		}
		m_aWidgets.Clear();
		m_aPlates.Clear();
		m_aPlateCommands.Clear();
		m_aCaptions.Clear();
		m_aColors.Clear();
	}

	void Update(Widget parent, ResourceName layout, SCR_MapEntity mapEntity)
	{
		if (!parent || !mapEntity)
			return;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		int count = AROR_FieldClientState.s_aMarkers.Count();
		while (m_aWidgets.Count() > count)
		{
			int last = m_aWidgets.Count() - 1;
			if (m_aWidgets[last]) m_aWidgets[last].RemoveFromHierarchy();
			if (m_aPlates[last]) m_aPlates[last].RemoveFromHierarchy();
			m_aWidgets.Remove(last);
			m_aPlates.Remove(last);
			m_aPlateCommands.Remove(last);
			m_aCaptions.Remove(last);
			m_aColors.Remove(last);
		}
		foreach (int index, AROR_FieldMarkerInfo marker : AROR_FieldClientState.s_aMarkers)
		{
			if (index >= m_aWidgets.Count())
			{
				Widget plate = workspace.CreateWidgets(layout, parent);
				if (plate)
				{
					Widget plateIcon = plate.FindAnyWidget("Icon");
					Widget plateLabel = plate.FindAnyWidget("QRFLabel");
					if (plateIcon) plateIcon.SetVisible(false);
					if (plateLabel) plateLabel.SetVisible(false);
					FrameSlot.SetAlignment(plate, 0, 0.5);
				}
				m_aPlates.Insert(plate);
				m_aPlateCommands.Insert(new array<ref CanvasWidgetCommand>());
				m_aCaptions.Insert(string.Empty);
				m_aColors.Insert(0);
				m_aWidgets.Insert(workspace.CreateWidgets(layout, parent));
			}
			Widget root = m_aWidgets[index];
			if (!root) continue;
			Color color = AROR_UIColors.Get(AROR_UIColors.REBEL);
			if (marker.m_iKind == 1) color = AROR_UIColors.Get(AROR_UIColors.DANGER);
			if (marker.m_iScope == AROR_EMarkerScope.PRIVATE && marker.m_iKind != 1)
				color = AROR_UIColors.Get(AROR_UIColors.TEXT);
			ImageWidget image = ImageWidget.Cast(root.FindAnyWidget("Icon"));
			if (image)
			{
				image.SetColor(color);
				image.SetRotation(0);
				if (marker.m_iKind == 1) image.SetRotation(45);
			}
			string kind = AROR_FieldClientState.KindName(marker.m_iKind);
			if (marker.m_iKind == 4) kind += " #" + marker.m_iId;
			string text = string.Format("%1 %2  [%3]", kind, marker.m_sCaption, AROR_FieldClientState.ScopeName(marker.m_iScope));
			RichTextWidget caption = RichTextWidget.Cast(root.FindAnyWidget("QRFLabel"));
			if (caption)
			{
				caption.SetVisible(true);
				caption.SetExactFontSize(17);
				caption.SetColor(color);
				caption.SetBold(true);
				if (m_aCaptions[index] != text || m_aColors[index] != color.PackToInt())
				{
					caption.SetText(text);
					m_aCaptions[index] = text;
					m_aColors[index] = color.PackToInt();
					Widget markerPlate = m_aPlates[index];
					if (markerPlate)
					{
						float width = AROR_MapLabelPlate.Width(text, 17);
						FrameSlot.SetSize(markerPlate, width, 30);
						CanvasWidget canvas = CanvasWidget.Cast(markerPlate.FindAnyWidget("Frame"));
						if (canvas) AROR_MapLabelPlate.Draw(canvas, m_aPlateCommands[index], width, 30, color.PackToInt());
					}
				}
				FrameSlot.SetPos(caption, 24 + AROR_MapLabelPlate.TextInset(), 0);
				FrameSlot.SetSize(caption, 600, 32);
			}
			int x, y;
			mapEntity.WorldToScreen(marker.m_vPosition[0], marker.m_vPosition[2], x, y, true);
			if (m_aPlates[index])
				FrameSlot.SetPos(m_aPlates[index], workspace.DPIUnscale(x) + 15, workspace.DPIUnscale(y));
			FrameSlot.SetAlignment(root, 0.5, 0.5);
			FrameSlot.SetSize(root, 18, 18);
			FrameSlot.SetPos(root, workspace.DPIUnscale(x), workspace.DPIUnscale(y));
		}
	}
}
