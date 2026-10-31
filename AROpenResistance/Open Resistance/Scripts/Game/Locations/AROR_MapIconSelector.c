//! Corner-bracket selector drawn around a map icon (driven by AROR_LocationMapIconsComponent).
//! The layout (AROR_MapIconSelector.layout) declares eight bars (two per corner); sizes, positions and colour are set here.
class AROR_MapIconSelector : Managed
{
	protected static const float THICKNESS = 2;

	//! Bracket arm length as a fraction of the selector size
	protected static const float ARM_FRACTION = 0.3;

	protected Widget m_wRoot;
	protected ref array<Widget> m_aBars = {};

	//------------------------------------------------------------------------------------------------
	void AROR_MapIconSelector(ResourceName layout, notnull Widget parent, Color color)
	{
		m_wRoot = GetGame().GetWorkspace().CreateWidgets(layout, parent);
		if (!m_wRoot)
			return;

		array<string> names = {"TopLeftH", "TopLeftV", "TopRightH", "TopRightV", "BottomLeftH", "BottomLeftV", "BottomRightH", "BottomRightV"};
		foreach (string name : names)
		{
			Widget bar = m_wRoot.FindAnyWidget(name);
			if (!bar)
			{
				Print(string.Format("[AROR] Map icon selector layout has no widget named '%1'", name), LogLevel.ERROR);
				continue;
			}

			if (color)
				bar.SetColor(color);

			m_aBars.Insert(bar);
		}

		m_wRoot.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	void ~AROR_MapIconSelector()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	//! Centre and size in reference (unscaled) pixels; opacity 0-1
	void Show(float centerX, float centerY, float size, float opacity)
	{
		if (!m_wRoot || m_aBars.Count() != 8)
			return;

		FrameSlot.SetPos(m_wRoot, centerX - size * 0.5, centerY - size * 0.5);
		FrameSlot.SetSize(m_wRoot, size, size);

		float arm = size * ARM_FRACTION;
		float far = size - THICKNESS;

		// Top-left
		Place(m_aBars[0], 0, 0, arm, THICKNESS);
		Place(m_aBars[1], 0, 0, THICKNESS, arm);
		// Top-right
		Place(m_aBars[2], size - arm, 0, arm, THICKNESS);
		Place(m_aBars[3], far, 0, THICKNESS, arm);
		// Bottom-left
		Place(m_aBars[4], 0, far, arm, THICKNESS);
		Place(m_aBars[5], 0, size - arm, THICKNESS, arm);
		// Bottom-right
		Place(m_aBars[6], size - arm, far, arm, THICKNESS);
		Place(m_aBars[7], far, size - arm, THICKNESS, arm);

		m_wRoot.SetOpacity(opacity);
		m_wRoot.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Higher values draw above lower ones among the root's siblings
	void SetZOrder(int zOrder)
	{
		if (m_wRoot)
			m_wRoot.SetZOrder(zOrder);
	}

	//------------------------------------------------------------------------------------------------
	void Hide()
	{
		if (m_wRoot)
			m_wRoot.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Place(Widget widget, float x, float y, float width, float height)
	{
		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, width, height);
	}
}
