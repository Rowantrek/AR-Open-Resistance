//! A small card with a station's album art, shown for a few seconds to the player who tunes a radio. Client only.
//! The layout only declares the widgets; positions, sizes and colours are set here (see docs/UI_DESIGN.md).
class ORRadio_NowPlayingCard : Managed
{
	protected static const int ROOT_Z_ORDER = 1000;
	protected static const float FADE_MS = 250;

	// In reference pixels
	protected static const float CARD_LEFT = 16;
	protected static const float CARD_TOP = 200;
	protected static const float CARD_WIDTH = 340;
	protected static const float PADDING = 8;
	protected static const float ART_SIZE = 72;
	protected static const float TEXT_GAP = 12;
	protected static const float KICKER_TOP = 12;
	protected static const float STATION_TOP = 28;
	protected static const float TITLE_TOP = 52;
	protected static const int KICKER_FONT_SIZE = 13;	//!< label
	protected static const int STATION_FONT_SIZE = 18;	//!< section
	protected static const int TITLE_FONT_SIZE = 15;	//!< body-strong

	protected Widget m_wRoot;
	protected Widget m_wCard;
	protected ImageWidget m_wArt;
	protected TextWidget m_wKicker;
	protected TextWidget m_wStation;
	protected TextWidget m_wTitle;
	protected float m_fDurationMs;
	protected int m_iStartMs;
	protected bool m_bShowing;

	//------------------------------------------------------------------------------------------------
	void ORRadio_NowPlayingCard(ResourceName layout, float seconds)
	{
		m_fDurationMs = seconds * 1000;
		m_wRoot = GetGame().GetWorkspace().CreateWidgets(layout);
		if (!m_wRoot)
		{
			Print("[OR Radio] Now playing layout could not be created: " + layout, LogLevel.WARNING);
			return;
		}

		m_wCard = m_wRoot.FindAnyWidget("Card");
		m_wArt = ImageWidget.Cast(m_wRoot.FindAnyWidget("Art"));
		m_wKicker = TextWidget.Cast(m_wRoot.FindAnyWidget("Kicker"));
		m_wStation = TextWidget.Cast(m_wRoot.FindAnyWidget("Station"));
		m_wTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("Title"));
		ImageWidget plate = ImageWidget.Cast(m_wRoot.FindAnyWidget("Plate"));
		if (!m_wCard || !m_wArt || !m_wKicker || !m_wStation || !m_wTitle || !plate)
		{
			Print("[OR Radio] Now playing layout is missing widgets: " + layout, LogLevel.WARNING);
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
			return;
		}

		FrameSlot.SetAnchorMin(m_wRoot, 0, 0);
		FrameSlot.SetAnchorMax(m_wRoot, 1, 1);
		FrameSlot.SetOffsets(m_wRoot, 0, 0, 0, 0);
		m_wRoot.SetZOrder(ROOT_Z_ORDER);

		float height = ART_SIZE + PADDING * 2;
		FrameSlot.SetPos(m_wCard, CARD_LEFT, CARD_TOP);
		FrameSlot.SetSize(m_wCard, CARD_WIDTH, height);
		FrameSlot.SetPos(plate, 0, 0);
		FrameSlot.SetSize(plate, CARD_WIDTH, height);
		plate.SetColor(AROR_UIColors.Get(AROR_UIColors.PLATE));
		FrameSlot.SetPos(m_wArt, PADDING, PADDING);
		FrameSlot.SetSize(m_wArt, ART_SIZE, ART_SIZE);

		StyleText(m_wKicker, KICKER_FONT_SIZE, AROR_UIColors.REBEL, false);
		StyleText(m_wStation, STATION_FONT_SIZE, AROR_UIColors.TEXT, true);
		StyleText(m_wTitle, TITLE_FONT_SIZE, AROR_UIColors.TEXT_MUTED, false);
		m_wKicker.SetText("NOW PLAYING");
		m_wCard.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	void ~ORRadio_NowPlayingCard()
	{
		GetGame().GetCallqueue().Remove(Update);
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the card, or refreshes it without fading in again when it is already up
	void Show(string station, string title, ResourceName art)
	{
		if (!m_wRoot)
			return;

		bool hasArt = !art.IsEmpty();
		if (hasArt)
			m_wArt.LoadImageTexture(0, art);

		m_wArt.SetVisible(hasArt);
		float textLeft = PADDING;
		if (hasArt)
			textLeft += ART_SIZE + TEXT_GAP;

		FrameSlot.SetPos(m_wKicker, textLeft, KICKER_TOP);
		FrameSlot.SetPos(m_wStation, textLeft, STATION_TOP);
		FrameSlot.SetPos(m_wTitle, textLeft, TITLE_TOP);
		m_wStation.SetText(station);
		m_wTitle.SetText(title);

		m_iStartMs = System.GetTickCount();
		if (m_bShowing)
		{
			m_iStartMs = m_iStartMs - (int)FADE_MS;
			return;
		}

		m_bShowing = true;
		m_wCard.SetOpacity(0);
		m_wCard.SetVisible(true);
		GetGame().GetCallqueue().CallLater(Update, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	bool IsShowing()
	{
		return m_bShowing;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills in the title of the card already up, without restarting its timer
	void SetTitle(string title)
	{
		if (m_wTitle)
			m_wTitle.SetText(title);
	}

	//------------------------------------------------------------------------------------------------
	protected void Update()
	{
		float elapsed = System.GetTickCount() - m_iStartMs;
		if (elapsed >= m_fDurationMs)
		{
			m_bShowing = false;
			m_wCard.SetVisible(false);
			GetGame().GetCallqueue().Remove(Update);
			return;
		}

		m_wCard.SetOpacity(Math.Clamp(Math.Min(elapsed, m_fDurationMs - elapsed) / FADE_MS, 0, 1));
	}

	//------------------------------------------------------------------------------------------------
	protected void StyleText(TextWidget text, int fontSize, int colorToken, bool bold)
	{
		FrameSlot.SetSizeToContent(text, true);
		text.SetExactFontSize(fontSize);
		text.SetColor(AROR_UIColors.Get(colorToken));
		text.SetBold(bold);
	}
}
