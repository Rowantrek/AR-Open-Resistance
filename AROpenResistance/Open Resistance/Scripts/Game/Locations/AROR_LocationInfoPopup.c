//! On-map info card for a location, shown next to its icon (driven by AROR_LocationMapIconsComponent).
//! Name, type and controlling faction; settlements add population and a support bar per faction.
//! An opaque plate with a REBEL top band marking the selection, sized to be read at a glance.
//! The layout (AROR_MapInfoPopup.layout) only declares the widgets; positions, sizes and styling are set here.
class AROR_LocationInfoPopup : Managed
{
	// Card geometry in reference pixels
	protected static const float PADDING = 16;
	protected static const float BAND_HEIGHT = 4;
	protected static const float TITLE_HEIGHT = 34;
	protected static const float SUBTITLE_HEIGHT = 18;
	protected static const float FACT_HEIGHT = 26;
	protected static const float FACT_LABEL_WIDTH = 120;
	protected static const float HEADING_HEIGHT = 18;
	protected static const float ROW_HEIGHT = 44;		//!< Support row: name and value above, bar below
	protected static const float ROW_TEXT_HEIGHT = 22;
	protected static const float VALUE_WIDTH = 56;
	protected static const float BAR_HEIGHT = 8;

	protected static const int TITLE_FONT_SIZE = 28;
	protected static const int VALUE_FONT_SIZE = 18;
	protected static const int ROW_FONT_SIZE = 16;
	protected static const int LABEL_FONT_SIZE = 13;

	protected static const int SUPPORT_ROWS = 3;

	protected Widget m_wRoot;
	protected ImageWidget m_wBorder;
	protected ImageWidget m_wBackground;
	protected ImageWidget m_wAccent;
	protected TextWidget m_wTitle;
	protected TextWidget m_wSubtitle;
	protected ImageWidget m_wHeaderDivider;
	protected TextWidget m_wControlLabel;
	protected TextWidget m_wControl;
	protected ImageWidget m_wDivider;
	protected TextWidget m_wPopulationLabel;
	protected TextWidget m_wPopulation;
	protected TextWidget m_wSupportHeading;
	protected ref array<TextWidget> m_aSupportLabels = {};
	protected ref array<ImageWidget> m_aSupportTracks = {};
	protected ref array<ImageWidget> m_aSupportFills = {};
	protected ref array<TextWidget> m_aSupportValues = {};

	protected AROR_LocationComponent m_Location;
	protected float m_fWidth;
	protected float m_fHeight;
	protected ref Color m_BackgroundColor;

	//------------------------------------------------------------------------------------------------
	void AROR_LocationInfoPopup(ResourceName layout, notnull Widget parent, float width, Color backgroundColor)
	{
		m_fWidth = width;
		m_BackgroundColor = backgroundColor;

		m_wRoot = GetGame().GetWorkspace().CreateWidgets(layout, parent);
		if (!m_wRoot)
			return;

		m_wBorder = ImageWidget.Cast(m_wRoot.FindAnyWidget("Border"));
		m_wBackground = ImageWidget.Cast(m_wRoot.FindAnyWidget("Background"));
		m_wAccent = ImageWidget.Cast(m_wRoot.FindAnyWidget("Accent"));
		m_wTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("Title"));
		m_wSubtitle = TextWidget.Cast(m_wRoot.FindAnyWidget("Subtitle"));
		m_wHeaderDivider = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeaderDivider"));
		m_wControlLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("ControlLabel"));
		m_wControl = TextWidget.Cast(m_wRoot.FindAnyWidget("Control"));
		m_wDivider = ImageWidget.Cast(m_wRoot.FindAnyWidget("Divider"));
		m_wPopulationLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("PopulationLabel"));
		m_wPopulation = TextWidget.Cast(m_wRoot.FindAnyWidget("Population"));
		m_wSupportHeading = TextWidget.Cast(m_wRoot.FindAnyWidget("SupportHeading"));

		for (int i = 0; i < SUPPORT_ROWS; i++)
		{
			m_aSupportLabels.Insert(TextWidget.Cast(m_wRoot.FindAnyWidget("SupportLabel" + i)));
			m_aSupportTracks.Insert(ImageWidget.Cast(m_wRoot.FindAnyWidget("SupportTrack" + i)));
			m_aSupportFills.Insert(ImageWidget.Cast(m_wRoot.FindAnyWidget("SupportFill" + i)));
			m_aSupportValues.Insert(TextWidget.Cast(m_wRoot.FindAnyWidget("SupportValue" + i)));
		}

		if (!m_wBackground || !m_wTitle || !m_wSubtitle || !m_wControl)
			Print("[AROR] Map info popup layout is missing widgets (Background, Title, Subtitle, Control)", LogLevel.ERROR);

		ApplyStyle();
		m_wRoot.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	void ~AROR_LocationInfoPopup()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	//! Higher values draw above lower ones among the root's siblings
	void SetZOrder(int zOrder)
	{
		if (m_wRoot)
			m_wRoot.SetZOrder(zOrder);
	}

	//------------------------------------------------------------------------------------------------
	//! Card size in reference pixels; the height is valid after Show or Refresh
	float GetWidth()
	{
		return m_fWidth;
	}

	//------------------------------------------------------------------------------------------------
	float GetHeight()
	{
		return m_fHeight;
	}

	//------------------------------------------------------------------------------------------------
	AROR_LocationComponent GetLocation()
	{
		return m_Location;
	}

	//------------------------------------------------------------------------------------------------
	void Show(notnull AROR_LocationComponent location)
	{
		if (!m_wRoot)
			return;

		m_Location = location;
		Refresh();
		m_wRoot.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	void Hide()
	{
		m_Location = null;
		if (m_wRoot)
			m_wRoot.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Position in reference (unscaled) pixels
	void SetPosition(float x, float y)
	{
		if (m_wRoot)
			FrameSlot.SetPos(m_wRoot, x, y);
	}

	//------------------------------------------------------------------------------------------------
	//! Fill in the card from the location, e.g. after control or support changed
	void Refresh()
	{
		if (!m_Location || !m_wTitle || !m_wSubtitle || !m_wControl)
			return;

		AROR_EFactionRole controllingRole = m_Location.GetControllingRole();
		Color controlColor = GetRoleColor(controllingRole);
		if (!controlColor)
			controlColor = AROR_UIColors.Get(AROR_UIColors.TEXT);

		string title = m_Location.GetName();
		title.ToUpper();
		string type = AROR_TextUtils.FormatEnumName(typename.EnumToString(AROR_ELocationType, m_Location.GetLocationType()));
		type.ToUpper();
		string controller = GetRoleName(controllingRole);
		controller.ToUpper();

		m_wTitle.SetText(title);
		m_wSubtitle.SetText(type);
		m_wControl.SetText(controller);
		m_wControl.SetColor(controlColor);
		if (m_wControlLabel)
			m_wControlLabel.SetText("CONTROLLED BY");

		float contentWidth = m_fWidth - PADDING * 2;
		float factValueX = PADDING + FACT_LABEL_WIDTH;
		float factValueWidth = m_fWidth - factValueX - PADDING;

		float y = BAND_HEIGHT + 12;
		Place(m_wTitle, PADDING, y, contentWidth, TITLE_HEIGHT);
		y += TITLE_HEIGHT;
		Place(m_wSubtitle, PADDING, y, contentWidth, SUBTITLE_HEIGHT);
		y += SUBTITLE_HEIGHT + 10;
		Place(m_wHeaderDivider, 0, y, m_fWidth, 1);
		y += 1 + 12;

		Place(m_wControlLabel, PADDING, y, FACT_LABEL_WIDTH, FACT_HEIGHT);
		Place(m_wControl, factValueX, y, factValueWidth, FACT_HEIGHT);
		y += FACT_HEIGHT;

		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(m_Location.GetOwner().FindComponent(AROR_SettlementComponent));
		SetSettlementVisible(settlement != null);

		if (settlement)
		{
			if (m_wPopulationLabel)
				m_wPopulationLabel.SetText("POPULATION");
			if (m_wPopulation)
				m_wPopulation.SetText(string.Format("%1", settlement.GetPopulation()));
			Place(m_wPopulationLabel, PADDING, y, FACT_LABEL_WIDTH, FACT_HEIGHT);
			Place(m_wPopulation, factValueX, y, factValueWidth, FACT_HEIGHT);
			y += FACT_HEIGHT + 12;

			Place(m_wDivider, 0, y, m_fWidth, 1);
			y += 1 + 12;

			if (m_wSupportHeading)
				m_wSupportHeading.SetText("SUPPORT");
			Place(m_wSupportHeading, PADDING, y, contentWidth, HEADING_HEIGHT);
			y += HEADING_HEIGHT + 8;

			array<AROR_EFactionRole> roles = {};
			AROR_FactionRoles.GetMilitary(roles);
			foreach (int i, AROR_EFactionRole role : roles)
			{
				if (i >= SUPPORT_ROWS)
					break;

				LayoutSupportRow(i, role, settlement.GetSupport(role), PADDING, y, contentWidth);
				y += ROW_HEIGHT;
			}
			y -= 6;
		}

		y += PADDING - 4;
		m_fHeight = y;

		Place(m_wBorder, 0, 0, m_fWidth, y);
		Place(m_wBackground, 1, 1, m_fWidth - 2, y - 2);
		Place(m_wAccent, 1, 1, m_fWidth - 2, BAND_HEIGHT);
		FrameSlot.SetSize(m_wRoot, m_fWidth, y);
	}

	//------------------------------------------------------------------------------------------------
	//! One support row: faction name and percentage on top, a full-width bar below
	protected void LayoutSupportRow(int index, AROR_EFactionRole role, float percent, float left, float y, float width)
	{
		TextWidget label = m_aSupportLabels[index];
		ImageWidget track = m_aSupportTracks[index];
		ImageWidget fill = m_aSupportFills[index];
		TextWidget value = m_aSupportValues[index];
		if (!label || !track || !fill || !value)
			return;

		float barY = y + ROW_TEXT_HEIGHT + 4;

		label.SetText(GetRoleName(role));
		Place(label, left, y, width - VALUE_WIDTH, ROW_TEXT_HEIGHT);

		value.SetText(string.Format("%1", Math.Round(percent)) + "%");
		Place(value, left + width - VALUE_WIDTH, y, VALUE_WIDTH, ROW_TEXT_HEIGHT);

		Place(track, left, barY, width, BAR_HEIGHT);
		Place(fill, left, barY, width * Math.Clamp(percent, 0, 100) / 100, BAR_HEIGHT);

		Color color = GetRoleColor(role);
		if (color)
			fill.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetSettlementVisible(bool visible)
	{
		SetVisible(m_wDivider, visible);
		SetVisible(m_wPopulationLabel, visible);
		SetVisible(m_wPopulation, visible);
		SetVisible(m_wSupportHeading, visible);

		for (int i = 0; i < SUPPORT_ROWS; i++)
		{
			SetVisible(m_aSupportLabels[i], visible);
			SetVisible(m_aSupportTracks[i], visible);
			SetVisible(m_aSupportFills[i], visible);
			SetVisible(m_aSupportValues[i], visible);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Colours, fonts and shadows; positions are set in Refresh
	protected void ApplyStyle()
	{
		if (m_wBorder)
			m_wBorder.SetColor(AROR_UIColors.Get(AROR_UIColors.LINE_STRONG));
		if (m_wBackground && m_BackgroundColor)
			m_wBackground.SetColor(m_BackgroundColor);
		if (m_wAccent)
			m_wAccent.SetColor(AROR_UIColors.Get(AROR_UIColors.REBEL));

		if (m_wHeaderDivider)
			m_wHeaderDivider.SetColor(AROR_UIColors.Get(AROR_UIColors.LINE));
		if (m_wDivider)
			m_wDivider.SetColor(AROR_UIColors.Get(AROR_UIColors.LINE));

		Color text = AROR_UIColors.Get(AROR_UIColors.TEXT);
		Color muted = AROR_UIColors.Get(AROR_UIColors.TEXT_MUTED);

		StyleText(m_wTitle, TITLE_FONT_SIZE, text, true);
		StyleText(m_wSubtitle, LABEL_FONT_SIZE, muted, true);
		StyleText(m_wControlLabel, LABEL_FONT_SIZE, muted, true);
		StyleText(m_wControl, VALUE_FONT_SIZE, text, true);
		StyleText(m_wPopulationLabel, LABEL_FONT_SIZE, muted, true);
		StyleText(m_wPopulation, VALUE_FONT_SIZE, text, false);
		StyleText(m_wSupportHeading, LABEL_FONT_SIZE, muted, true);

		for (int i = 0; i < SUPPORT_ROWS; i++)
		{
			StyleText(m_aSupportLabels[i], ROW_FONT_SIZE, text, false);
			StyleText(m_aSupportValues[i], ROW_FONT_SIZE, text, false);

			if (m_aSupportTracks[i])
				m_aSupportTracks[i].SetColor(AROR_UIColors.Get(AROR_UIColors.LINE));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void StyleText(TextWidget text, int fontSize, Color color, bool bold)
	{
		if (!text)
			return;

		text.SetExactFontSize(fontSize);
		text.SetColor(color);
		text.SetBold(bold);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Place(Widget widget, float x, float y, float width, float height)
	{
		if (!widget)
			return;

		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, width, height);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetVisible(Widget widget, bool visible)
	{
		if (widget)
			widget.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	protected Faction GetRoleFaction(AROR_EFactionRole role)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return null;

		return roles.GetFaction(role);
	}

	//------------------------------------------------------------------------------------------------
	//! Faction name filling the role (from the faction manager), falling back to the role name
	protected string GetRoleName(AROR_EFactionRole role)
	{
		Faction faction = GetRoleFaction(role);
		if (faction)
			return faction.GetFactionName();

		return AROR_FactionRoles.GetDisplayName(role);
	}

	//------------------------------------------------------------------------------------------------
	protected Color GetRoleColor(AROR_EFactionRole role)
	{
		Faction faction = GetRoleFaction(role);
		if (faction)
			return faction.GetFactionColor();

		return null;
	}
}
