//! A square HICOMM control drawn from a border image, an inset fill, a label and an invisible button on top
class AROR_RebIntelControl : Managed
{
	protected ImageWidget m_wBorder;
	protected ImageWidget m_wFill;
	protected RichTextWidget m_wLabel;
	protected Widget m_wButton;

	//------------------------------------------------------------------------------------------------
	bool Bind(Widget root, string prefix)
	{
		m_wBorder = ImageWidget.Cast(root.FindAnyWidget(prefix + "Border"));
		m_wFill = ImageWidget.Cast(root.FindAnyWidget(prefix + "Fill"));
		m_wLabel = RichTextWidget.Cast(root.FindAnyWidget(prefix + "Label"));
		m_wButton = root.FindAnyWidget(prefix + "Button");
		if (!m_wBorder || !m_wFill || !m_wLabel || !m_wButton)
			return false;

		// Buttons only catch clicks; the engine draws them solid white unless cleared
		m_wButton.SetColor(Color.FromRGBA(0, 0, 0, 0));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	Widget GetButton()
	{
		return m_wButton;
	}

	//------------------------------------------------------------------------------------------------
	void SetText(string text)
	{
		m_wLabel.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	void SetVisible(bool visible)
	{
		m_wBorder.SetVisible(visible);
		m_wFill.SetVisible(visible);
		m_wLabel.SetVisible(visible);
		m_wButton.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	//! Selected: rebel fill with ink text. Otherwise a line-strong outline; disabled text is faint.
	void Style(bool selected, bool enabled = true)
	{
		int border = AROR_UIColors.LINE_STRONG;
		int fill = AROR_UIColors.SURFACE;
		int text = AROR_UIColors.REBEL;
		if (selected)
		{
			border = AROR_UIColors.REBEL;
			fill = AROR_UIColors.REBEL;
			text = AROR_UIColors.REBEL_INK;
		}
		else if (!enabled)
		{
			border = AROR_UIColors.LINE;
			text = AROR_UIColors.TEXT_FAINT;
		}
		m_wBorder.SetColor(AROR_UIColors.Get(border));
		m_wFill.SetColor(AROR_UIColors.Get(fill));
		m_wLabel.SetColor(AROR_UIColors.Get(text));
		m_wButton.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! Reference units (900 wide portrait screen), label centred from its estimated width
	void Layout(float x, float y, float width, float height, float fontSize, float scale)
	{
		AROR_RebIntelApp.Place(m_wBorder, x * scale, y * scale, width * scale, height * scale);
		AROR_RebIntelApp.Place(m_wFill, (x + 2) * scale, (y + 2) * scale, (width - 4) * scale, (height - 4) * scale);
		AROR_RebIntelApp.Place(m_wButton, x * scale, y * scale, width * scale, height * scale);
		float textWidth = AROR_RebIntelApp.EstimateWidth(m_wLabel.GetText(), fontSize);
		AROR_RebIntelApp.Label(m_wLabel, x + (width - textWidth) * 0.5, y + (height - fontSize * 1.25) * 0.5, textWidth + fontSize, fontSize * 1.4, fontSize, scale);
	}
}

//! One REBINTEL card: a Readiness or Crackdown level with its faction label, English line, meter and effect
class AROR_RebIntelCard : Managed
{
	static const int SEGMENTS = 5;

	AROR_EThreatScale m_eScale;
	Widget m_wFrame;
	ImageWidget m_wBorder;
	ImageWidget m_wFill;
	RichTextWidget m_wHeading;
	RichTextWidget m_wCount;
	ImageWidget m_wHighlight;
	RichTextWidget m_wValue;
	RichTextWidget m_wLine;
	RichTextWidget m_wEffect;
	ref array<ImageWidget> m_aMeter = {};
	int m_iLevel;	//!< 0 while there is no intelligence

	//------------------------------------------------------------------------------------------------
	bool Bind(Widget root, string prefix, AROR_EThreatScale scale)
	{
		m_eScale = scale;
		m_wFrame = root.FindAnyWidget(prefix);
		m_wBorder = ImageWidget.Cast(root.FindAnyWidget(prefix + "Border"));
		m_wFill = ImageWidget.Cast(root.FindAnyWidget(prefix + "Fill"));
		m_wHeading = RichTextWidget.Cast(root.FindAnyWidget(prefix + "Heading"));
		m_wCount = RichTextWidget.Cast(root.FindAnyWidget(prefix + "Count"));
		m_wHighlight = ImageWidget.Cast(root.FindAnyWidget(prefix + "Highlight"));
		m_wValue = RichTextWidget.Cast(root.FindAnyWidget(prefix + "Value"));
		m_wLine = RichTextWidget.Cast(root.FindAnyWidget(prefix + "Line"));
		m_wEffect = RichTextWidget.Cast(root.FindAnyWidget(prefix + "Effect"));
		for (int i = 1; i <= SEGMENTS; i++)
		{
			ImageWidget segment = ImageWidget.Cast(root.FindAnyWidget(prefix + "Meter" + i));
			if (!segment)
				return false;
			m_aMeter.Insert(segment);
		}
		return m_wFrame && m_wBorder && m_wFill && m_wHeading && m_wCount && m_wHighlight && m_wValue && m_wLine && m_wEffect;
	}
}

//! REBINTEL: what the enemy is doing, read from intercepted radio traffic (see ARCHITECTURE.md -> Phone).
//! Two tabs, one per enemy army, each with a Readiness and a Crackdown card labelled in that army's own terms.
//! Below them, the player's HUD widget and alert settings, saved locally (AROR_RebIntelSettings).
//! Read-only: Readiness and Crackdown are driven by the campaign on the server, never by players.
class AROR_RebIntelApp
{
	protected static const float CARD_X = 45;
	protected static const float CARD_WIDTH = 810;
	protected static const float CARD_HEIGHT = 350;
	protected static const float CARD_PADDING = 36;
	protected static const float VALUE_FONT = 50;
	protected static const float ROW_HEIGHT = 92;	//!< touch-min at the 900-wide reference
	protected static const float HALF_WIDTH = 397;
	protected static const float HALF_GAP = 16;

	protected Widget m_wRoot;
	protected ImageWidget m_wBackground;
	protected RichTextWidget m_wTitle;
	protected RichTextWidget m_wSubtitle;
	protected ImageWidget m_wDivider;
	protected RichTextWidget m_wHudHeading;
	protected ref AROR_RebIntelControl m_TabOccupiers = new AROR_RebIntelControl();
	protected ref AROR_RebIntelControl m_TabInvaders = new AROR_RebIntelControl();
	protected ref array<ref AROR_RebIntelCard> m_aCards = {};
	protected ref AROR_RebIntelControl m_Compact = new AROR_RebIntelControl();
	protected ref AROR_RebIntelControl m_Full = new AROR_RebIntelControl();
	protected ref AROR_RebIntelControl m_Left = new AROR_RebIntelControl();
	protected ref AROR_RebIntelControl m_Right = new AROR_RebIntelControl();
	protected ref array<string> m_aToggles = {"Hud", "Alerts"};
	protected ref array<ImageWidget> m_aToggleBorders = {};
	protected ref array<ImageWidget> m_aToggleFills = {};
	protected ref array<RichTextWidget> m_aToggleLabels = {};
	protected ref array<ImageWidget> m_aToggleTracks = {};
	protected ref array<ImageWidget> m_aToggleTrackFills = {};
	protected ref array<ImageWidget> m_aToggleKnobs = {};
	protected ref array<Widget> m_aToggleButtons = {};

	protected AROR_EFactionRole m_eRole;
	protected int m_iNextRefresh;

	//------------------------------------------------------------------------------------------------
	bool Init(Widget phoneRoot)
	{
		m_wRoot = phoneRoot.FindAnyWidget("RebIntelApp");
		m_wBackground = ImageWidget.Cast(phoneRoot.FindAnyWidget("RebIntelBackground"));
		m_wTitle = RichTextWidget.Cast(phoneRoot.FindAnyWidget("RebIntelTitle"));
		m_wSubtitle = RichTextWidget.Cast(phoneRoot.FindAnyWidget("RebIntelSubtitle"));
		m_wDivider = ImageWidget.Cast(phoneRoot.FindAnyWidget("RebIntelDivider"));
		m_wHudHeading = RichTextWidget.Cast(phoneRoot.FindAnyWidget("RebIntelHudHeading"));
		if (!m_wRoot || !m_wBackground || !m_wTitle || !m_wSubtitle || !m_wDivider || !m_wHudHeading)
			return false;
		if (!m_TabOccupiers.Bind(phoneRoot, "RebIntelTabOccupiers") || !m_TabInvaders.Bind(phoneRoot, "RebIntelTabInvaders"))
			return false;
		if (!m_Compact.Bind(phoneRoot, "RebIntelOptionCompact") || !m_Full.Bind(phoneRoot, "RebIntelOptionFull"))
			return false;
		if (!m_Left.Bind(phoneRoot, "RebIntelOptionLeft") || !m_Right.Bind(phoneRoot, "RebIntelOptionRight"))
			return false;

		AROR_RebIntelCard readiness = new AROR_RebIntelCard();
		AROR_RebIntelCard crackdown = new AROR_RebIntelCard();
		if (!readiness.Bind(phoneRoot, "RebIntelReadiness", AROR_EThreatScale.READINESS) || !crackdown.Bind(phoneRoot, "RebIntelCrackdown", AROR_EThreatScale.CRACKDOWN))
			return false;
		m_aCards.Insert(readiness);
		m_aCards.Insert(crackdown);

		foreach (string toggle : m_aToggles)
		{
			string prefix = "RebIntelToggle" + toggle;
			ImageWidget border = ImageWidget.Cast(phoneRoot.FindAnyWidget(prefix + "Border"));
			ImageWidget fill = ImageWidget.Cast(phoneRoot.FindAnyWidget(prefix + "Fill"));
			RichTextWidget label = RichTextWidget.Cast(phoneRoot.FindAnyWidget(prefix + "Label"));
			ImageWidget track = ImageWidget.Cast(phoneRoot.FindAnyWidget(prefix + "Track"));
			ImageWidget trackFill = ImageWidget.Cast(phoneRoot.FindAnyWidget(prefix + "TrackFill"));
			ImageWidget knob = ImageWidget.Cast(phoneRoot.FindAnyWidget(prefix + "Knob"));
			Widget button = phoneRoot.FindAnyWidget(prefix + "Button");
			if (!border || !fill || !label || !track || !trackFill || !knob || !button)
				return false;
			m_aToggleBorders.Insert(border);
			m_aToggleFills.Insert(fill);
			m_aToggleLabels.Insert(label);
			m_aToggleTracks.Insert(track);
			m_aToggleTrackFills.Insert(trackFill);
			m_aToggleKnobs.Insert(knob);
			button.SetColor(Color.FromRGBA(0, 0, 0, 0));
			m_aToggleButtons.Insert(button);
		}

		m_eRole = AROR_RebIntelSettings.GetTrackedRole();
		ApplyStyle();
		SetVisible(false, 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds this app's buttons so the phone forwards their clicks to OnButton
	void CollectButtons(notnull array<Widget> buttons)
	{
		buttons.Insert(m_TabOccupiers.GetButton());
		buttons.Insert(m_TabInvaders.GetButton());
		foreach (Widget toggle : m_aToggleButtons)
		{
			buttons.Insert(toggle);
		}
		buttons.Insert(m_Compact.GetButton());
		buttons.Insert(m_Full.GetButton());
		buttons.Insert(m_Left.GetButton());
		buttons.Insert(m_Right.GetButton());
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true if the click belonged to this app
	bool OnButton(string buttonName)
	{
		if (buttonName.IndexOf("RebIntel") != 0 || buttonName == "RebIntelTileButton")
			return false;

		if (buttonName == "RebIntelTabOccupiersButton")
			SelectRole(AROR_EFactionRole.OCCUPIERS);
		else if (buttonName == "RebIntelTabInvadersButton")
			SelectRole(AROR_EFactionRole.INVADERS);
		else if (buttonName == "RebIntelToggleHudButton")
			AROR_RebIntelSettings.SetHudShown(!AROR_RebIntelSettings.IsHudShown());
		else if (buttonName == "RebIntelToggleAlertsButton")
			AROR_RebIntelSettings.SetAlertsShown(!AROR_RebIntelSettings.AreAlertsShown());
		else if (buttonName == "RebIntelOptionCompactButton")
			AROR_RebIntelSettings.SetFull(false);
		else if (buttonName == "RebIntelOptionFullButton")
			AROR_RebIntelSettings.SetFull(true);
		else if (buttonName == "RebIntelOptionLeftButton")
			AROR_RebIntelSettings.SetLeft(true);
		else if (buttonName == "RebIntelOptionRightButton")
			AROR_RebIntelSettings.SetLeft(false);
		else
			return false;

		m_iNextRefresh = 0;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void SetVisible(bool visible, float opacity)
	{
		m_wRoot.SetVisible(visible);
		m_wRoot.SetOpacity(opacity);
		if (!visible)
			m_iNextRefresh = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Geometry follows the phone animation; data is read at most once per second while open.
	void Layout(float left, float top, float scale)
	{
		int now = GetGame().GetWorld().GetWorldTime();
		if (now >= m_iNextRefresh)
		{
			Refresh();
			m_iNextRefresh = now + 1000;
		}

		Place(m_wRoot, left, top + 70 * scale, 900 * scale, 1630 * scale);
		Place(m_wBackground, 0, 0, 900 * scale, 1630 * scale);
		Label(m_wTitle, 45, 36, 810, 80, 64, scale);
		Label(m_wSubtitle, 45, 118, 810, 40, 28, scale, true);
		m_TabOccupiers.Layout(CARD_X, 170, HALF_WIDTH, ROW_HEIGHT, 34, scale);
		m_TabInvaders.Layout(CARD_X + HALF_WIDTH + HALF_GAP, 170, HALF_WIDTH, ROW_HEIGHT, 34, scale);
		foreach (int i, AROR_RebIntelCard card : m_aCards)
		{
			LayoutCard(card, 285 + i * (CARD_HEIGHT + 20), scale);
		}

		Place(m_wDivider, CARD_X * scale, 1030 * scale, CARD_WIDTH * scale, 2 * scale);
		Label(m_wHudHeading, CARD_X, 1048, CARD_WIDTH, 40, 28, scale);
		foreach (int t, string toggle : m_aToggles)
		{
			LayoutToggle(t, 1100 + t * (ROW_HEIGHT + 14), scale);
		}
		m_Compact.Layout(CARD_X, 1312, HALF_WIDTH, ROW_HEIGHT, 32, scale);
		m_Full.Layout(CARD_X + HALF_WIDTH + HALF_GAP, 1312, HALF_WIDTH, ROW_HEIGHT, 32, scale);
		m_Left.Layout(CARD_X, 1418, HALF_WIDTH, ROW_HEIGHT, 32, scale);
		m_Right.Layout(CARD_X + HALF_WIDTH + HALF_GAP, 1418, HALF_WIDTH, ROW_HEIGHT, 32, scale);
	}

	//------------------------------------------------------------------------------------------------
	protected void LayoutCard(AROR_RebIntelCard card, float y, float scale)
	{
		Place(card.m_wFrame, CARD_X * scale, y * scale, CARD_WIDTH * scale, CARD_HEIGHT * scale);
		Place(card.m_wBorder, 0, 0, CARD_WIDTH * scale, CARD_HEIGHT * scale);
		Place(card.m_wFill, 2 * scale, 2 * scale, (CARD_WIDTH - 4) * scale, (CARD_HEIGHT - 4) * scale);

		// Heading row: label left, n/5 right
		float inner = CARD_WIDTH - CARD_PADDING * 2;
		float countWidth = EstimateWidth(card.m_wCount.GetText(), 32);
		float countX = CARD_WIDTH - CARD_PADDING - countWidth;
		float headingWidth = inner - 120;
		Label(card.m_wHeading, CARD_PADDING, 34, headingWidth, 44, 28, scale, true);
		Label(card.m_wCount, countX, 32, countWidth + 20, 44, 32, scale);

		// Value in the stage colour, with a matching stripe down the card's left edge
		Label(card.m_wValue, CARD_PADDING, 106, inner, 66, VALUE_FONT, scale, true);
		Place(card.m_wHighlight, 2 * scale, 2 * scale, 8 * scale, (CARD_HEIGHT - 4) * scale);
		Label(card.m_wLine, CARD_PADDING, 176, inner, 40, 30, scale);

		float gap = 12;
		float segment = (inner - gap * (AROR_RebIntelCard.SEGMENTS - 1)) / AROR_RebIntelCard.SEGMENTS;
		foreach (int i, ImageWidget meter : card.m_aMeter)
		{
			Place(meter, (CARD_PADDING + i * (segment + gap)) * scale, 226 * scale, segment * scale, 20 * scale);
		}
		Label(card.m_wEffect, CARD_PADDING, 262, inner, 80, 30, scale);
	}

	//------------------------------------------------------------------------------------------------
	//! Full-width row: label left, square switch right
	protected void LayoutToggle(int index, float y, float scale)
	{
		bool on = AROR_RebIntelSettings.IsHudShown();
		if (index == 1)
			on = AROR_RebIntelSettings.AreAlertsShown();

		float trackX = CARD_X + CARD_WIDTH - CARD_PADDING - 120;
		float trackY = y + (ROW_HEIGHT - 52) * 0.5;
		Place(m_aToggleBorders[index], CARD_X * scale, y * scale, CARD_WIDTH * scale, ROW_HEIGHT * scale);
		Place(m_aToggleFills[index], (CARD_X + 2) * scale, (y + 2) * scale, (CARD_WIDTH - 4) * scale, (ROW_HEIGHT - 4) * scale);
		Label(m_aToggleLabels[index], CARD_X + CARD_PADDING, y + (ROW_HEIGHT - 44) * 0.5, CARD_WIDTH - 240, 48, 34, scale);
		Place(m_aToggleTracks[index], trackX * scale, trackY * scale, 120 * scale, 52 * scale);
		Place(m_aToggleTrackFills[index], (trackX + 2) * scale, (trackY + 2) * scale, 116 * scale, 48 * scale);
		float knobX = trackX + 8;
		if (on)
			knobX = trackX + 120 - 52;
		Place(m_aToggleKnobs[index], knobX * scale, (trackY + 8) * scale, 44 * scale, 36 * scale);
		Place(m_aToggleButtons[index], CARD_X * scale, y * scale, CARD_WIDTH * scale, ROW_HEIGHT * scale);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refresh()
	{
		// The scheduler singleton deliberately exists only on the server. Clients read
		// its replicated component on the game mode, never a server-only singleton.
		AROR_OperationManagerComponent manager;
		AROR_InvaderCampaignComponent invasion;
		IEntity gameMode = GetGame().GetGameMode();
		if (gameMode)
		{
			manager = AROR_OperationManagerComponent.Cast(gameMode.FindComponent(AROR_OperationManagerComponent));
			invasion = AROR_InvaderCampaignComponent.Cast(gameMode.FindComponent(AROR_InvaderCampaignComponent));
		}

		m_TabOccupiers.SetText(ForceName(AROR_EFactionRole.OCCUPIERS));
		m_TabInvaders.SetText(ForceName(AROR_EFactionRole.INVADERS));
		m_TabOccupiers.Style(m_eRole == AROR_EFactionRole.OCCUPIERS);
		m_TabInvaders.Style(m_eRole == AROR_EFactionRole.INVADERS);
		m_wSubtitle.SetText(SubtitleText(manager, invasion, m_eRole));

		AROR_ThreatLabelSet labels = AROR_ThreatLabels.ForRole(m_eRole);
		bool ready = manager && manager.HasThreatState();
		foreach (AROR_RebIntelCard card : m_aCards)
		{
			card.m_iLevel = 0;
			if (ready)
				card.m_iLevel = manager.GetThreatLevel(m_eRole, card.m_eScale);
			RefreshCard(card, labels);
		}

		bool full = AROR_RebIntelSettings.IsFull();
		bool left = AROR_RebIntelSettings.IsLeft();
		m_Compact.Style(!full);
		m_Full.Style(full);
		m_Left.Style(left);
		m_Right.Style(!left);
		StyleToggle(0, AROR_RebIntelSettings.IsHudShown());
		StyleToggle(1, AROR_RebIntelSettings.AreAlertsShown());
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshCard(AROR_RebIntelCard card, AROR_ThreatLabelSet labels)
	{
		int level = card.m_iLevel;
		card.m_wHeading.SetText(labels.GetHeading(card.m_eScale) + " · " + labels.GetHeadingLine(card.m_eScale));
		if (level < 1)
		{
			card.m_wCount.SetText("--/5");
			card.m_wValue.SetText("--");
			card.m_wLine.SetText("Awaiting intelligence");
			card.m_wEffect.SetText(string.Empty);
		}
		else
		{
			card.m_wCount.SetText(string.Format("%1/5", level));
			card.m_wValue.SetText(labels.GetLabel(card.m_eScale, level));
			card.m_wLine.SetText(labels.GetLine(card.m_eScale, level));
			card.m_wEffect.SetText(AROR_ThreatLabels.GetEffect(card.m_eScale, level));
		}

		// Stage colour (green quiet ... red in trouble) on the value, the filled meter and the card's edge stripe
		int stageColor = AROR_UIColors.ForLevel(level);
		card.m_wHighlight.SetVisible(level >= 1);
		card.m_wHighlight.SetColor(AROR_UIColors.Get(stageColor));
		int valueColor = AROR_UIColors.TEXT_MUTED;
		if (level >= 1)
			valueColor = stageColor;
		card.m_wValue.SetColor(AROR_UIColors.Get(valueColor));

		foreach (int i, ImageWidget segment : card.m_aMeter)
		{
			int segmentColor = AROR_UIColors.LINE;
			if (i < level)
				segmentColor = stageColor;
			segment.SetColor(AROR_UIColors.Get(segmentColor));
		}

	}

	//------------------------------------------------------------------------------------------------
	//! On: rebel-dim track with the rebel knob right. Off: line-strong outline with the knob left.
	protected void StyleToggle(int index, bool on)
	{
		int track = AROR_UIColors.LINE_STRONG;
		int trackFill = AROR_UIColors.SURFACE;
		int knob = AROR_UIColors.LINE_STRONG;
		if (on)
		{
			track = AROR_UIColors.REBEL;
			trackFill = AROR_UIColors.REBEL_DIM;
			knob = AROR_UIColors.REBEL;
		}
		m_aToggleTracks[index].SetColor(AROR_UIColors.Get(track));
		m_aToggleTrackFills[index].SetColor(AROR_UIColors.Get(trackFill));
		m_aToggleKnobs[index].SetColor(AROR_UIColors.Get(knob));
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectRole(AROR_EFactionRole role)
	{
		m_eRole = role;
		AROR_RebIntelSettings.SetTrackedRole(role);
	}


	//------------------------------------------------------------------------------------------------
	protected void ApplyStyle()
	{
		m_wBackground.SetColor(AROR_UIColors.Get(AROR_UIColors.SURFACE));
		m_wDivider.SetColor(AROR_UIColors.Get(AROR_UIColors.LINE));
		Style(m_wTitle, "REBINTEL", AROR_UIColors.REBEL, true);
		Style(m_wSubtitle, string.Empty, AROR_UIColors.TEXT_MUTED, false);
		Style(m_wHudHeading, "HEADS-UP DISPLAY", AROR_UIColors.TEXT_MUTED, true);
		m_Compact.SetText("COMPACT");
		m_Full.SetText("FULL");
		m_Left.SetText("TOP LEFT");
		m_Right.SetText("TOP RIGHT");
		array<string> toggleLabels = {"Show widget on HUD", "Alert pop-ups"};
		foreach (int i, RichTextWidget label : m_aToggleLabels)
		{
			Style(label, toggleLabels[i], AROR_UIColors.TEXT, false);
			m_aToggleBorders[i].SetColor(AROR_UIColors.Get(AROR_UIColors.LINE));
			m_aToggleFills[i].SetColor(AROR_UIColors.Get(AROR_UIColors.SURFACE_RAISED));
		}
		foreach (AROR_RebIntelCard card : m_aCards)
		{
			card.m_wBorder.SetColor(AROR_UIColors.Get(AROR_UIColors.LINE));
			card.m_wFill.SetColor(AROR_UIColors.Get(AROR_UIColors.SURFACE_RAISED));
			Style(card.m_wHeading, string.Empty, AROR_UIColors.TEXT_MUTED, false);
			Style(card.m_wCount, string.Empty, AROR_UIColors.TEXT_MUTED, false);
			Style(card.m_wValue, string.Empty, AROR_UIColors.TEXT, true);
			Style(card.m_wLine, string.Empty, AROR_UIColors.TEXT_MUTED, false);
			Style(card.m_wEffect, string.Empty, AROR_UIColors.TEXT, false);
			card.m_wEffect.SetTextWrapping(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The army a role uses, e.g. "US ARMY"; falls back to the faction name, then the role
	protected static string ForceName(AROR_EFactionRole role)
	{
		string name;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets && presets.GetPreset(role))
			name = presets.GetPreset(role).GetName();
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (name.IsEmpty() && roles && roles.GetFaction(role))
			name = roles.GetFaction(role).GetFactionName();
		if (name.IsEmpty())
		{
			name = "Occupiers";
			if (role == AROR_EFactionRole.INVADERS)
				name = "Invaders";
		}
		name.ToUpper();
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Which enemy and whose table the cards show, plus anything that changes how to read them
	protected static string SubtitleText(AROR_OperationManagerComponent manager, AROR_InvaderCampaignComponent invasion, AROR_EFactionRole role)
	{
		string roleName = "Occupier";
		if (role == AROR_EFactionRole.INVADERS)
			roleName = "Invader";
		string text = roleName + ": " + ForceName(role);
		if (!manager || !manager.HasThreatState())
			return text + " · awaiting intelligence";
		if (role == AROR_EFactionRole.INVADERS && invasion && invasion.IsAdvanceActive())
			return text + " · territorial advance active";
		if (role == AROR_EFactionRole.INVADERS && !manager.AreInvaderOperationsEnabled())
			return text + " · responses disabled";
		return text + " · from intercepted radio traffic";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Style(RichTextWidget widget, string text, int color, bool bold)
	{
		widget.SetText(text);
		widget.SetColor(AROR_UIColors.Get(color));
		widget.SetBold(bold);
	}

	//------------------------------------------------------------------------------------------------
	//! Approximate rendered width in reference units; Cyrillic letters count once, not per UTF-8 byte
	static float EstimateWidth(string text, float fontSize)
	{
		return AROR_TextUtils.CharCount(text) * fontSize * 0.55;
	}

	//------------------------------------------------------------------------------------------------
	//! fit shrinks the font so long faction labels stay inside the given width
	static void Label(RichTextWidget widget, float x, float y, float width, float height, float size, float scale, bool fit = false)
	{
		if (fit)
		{
			float textWidth = EstimateWidth(widget.GetText(), size);
			if (textWidth > width)
				size = size * width / textWidth;
		}
		widget.SetExactFontSize(Math.Max(1, Math.Round(size * scale)));
		Place(widget, x * scale, y * scale, width * scale, height * scale);
	}

	//------------------------------------------------------------------------------------------------
	static void Place(Widget widget, float x, float y, float width, float height)
	{
		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, width, height);
	}
}
