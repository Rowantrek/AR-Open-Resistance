//! Client-side drawing of notifications and hints (driven by AROR_NotificationManagerComponent).
//! Headlines (alert banner, top-centre below the compass) and notifications (toast, right edge) share one queue and
//! show one at a time; headlines jump ahead of queued notifications. Hints (bottom pill) bypass the queue: a new hint
//! replaces the current one. Styling follows the Open Resistance UI design system (docs/UI_DESIGN.md).
//! The layout (AROR_Notifications.layout) only declares the widgets; positions, sizes and styling are set here.
class AROR_NotificationDisplay : Managed
{
	//! Above the HUD and the map
	protected static const int ROOT_Z_ORDER = 1000;
	protected static const float FADE_MS = 250;

	// Alert banner (headlines), top-centre below the compass, in reference pixels
	protected static const float HEADLINE_TOP = 96;
	protected static const float HEADLINE_WIDTH = 520;
	protected static const float HEADLINE_MAX_WIDTH = 900;
	protected static const float HEADLINE_HEADER_HEIGHT = 26;
	protected static const float HEADLINE_PADDING = 14;
	protected static const float HEADLINE_ICON_SIZE = 24;
	protected static const float HEADLINE_GAP = 10;
	protected static const int HEADLINE_FONT_SIZE = 26;		//!< alert-title
	protected static const int HEADLINE_SOURCE_FONT_SIZE = 13;	//!< label
	protected static const int HEADLINE_DETAIL_FONT_SIZE = 15;	//!< body-strong
	protected static const int HEADLINE_EFFECT_FONT_SIZE = 14;	//!< body
	protected static const string DEFAULT_HEADLINE_SOURCE = "HQ";	//!< Header band source when the sender gives no kicker

	// Toast (dispatch card), right edge below the REBINTEL status widget
	protected static const float CARD_MARGIN = 16;
	protected static const float CARD_TOP = 200;
	protected static const float CARD_HEIGHT = 52;
	protected static const float CARD_ICON_BLOCK = 36;
	protected static const float CARD_GLYPH_SIZE = 10;	//!< The square "ours / good news" glyph
	protected static const float CARD_ICON_SIZE = 20;		//!< icon-hud
	protected static const float CARD_PADDING = 12;
	protected static const float CARD_MIN_WIDTH = 300;
	protected static const float CARD_MAX_WIDTH = 420;	//!< Longer text wraps
	protected static const float CARD_MAX_LINES = 3;
	protected static const float CARD_KICKER_TOP = 8;
	protected static const float CARD_TEXT_TOP = 25;
	protected static const int CARD_KICKER_FONT_SIZE = 13;	//!< label
	protected static const int CARD_TEXT_FONT_SIZE = 15;		//!< body-strong

	// Hint pill
	protected static const float HINT_BOTTOM = 140;
	protected static const float HINT_HEIGHT = 36;
	protected static const float HINT_PADDING = 10;
	protected static const float HINT_BORDER = 2;
	protected static const float HINT_ICON_SIZE = 20;
	protected static const float HINT_GAP = 10;
	protected static const int HINT_FONT_SIZE = 15;
	protected static const int HINT_BADGE_FONT_SIZE = 15;

	protected Widget m_wRoot;

	protected Widget m_wHeadline;
	protected ImageWidget m_wHeadlineBand;
	protected ImageWidget m_wHeadlineLineTop;
	protected ImageWidget m_wHeadlineLineBottom;
	protected ImageWidget m_wHeadlineLineLeft;
	protected ImageWidget m_wHeadlineLineRight;
	protected ImageWidget m_wHeadlineHeader;
	protected RichTextWidget m_wHeadlineSource;
	protected ImageWidget m_wHeadlineIcon;
	protected RichTextWidget m_wHeadlineText;
	protected RichTextWidget m_wHeadlineDetail;
	protected RichTextWidget m_wHeadlineTime;
	protected RichTextWidget m_wHeadlineIntercept;
	protected RichTextWidget m_wHeadlineEffect;
	protected ImageWidget m_wHeadlineTimer;
	protected bool m_bHeadlineHasDetail;
	protected bool m_bHeadlineHasIntercept;
	protected bool m_bHeadlineHasEffect;
	protected bool m_bHeadlineLayoutDirty;
	protected float m_fHeadlineViewport;
	protected float m_fHeadlineWidth;
	protected float m_fHeadlineHeight;

	protected Widget m_wCard;
	protected ImageWidget m_wCardBackground;
	protected ImageWidget m_wCardIconBlock;
	protected ImageWidget m_wCardIcon;
	protected RichTextWidget m_wCardKicker;
	protected RichTextWidget m_wCardText;
	protected ImageWidget m_wCardTimer;

	protected Widget m_wHint;
	protected ImageWidget m_wHintBackground;
	protected ref array<ImageWidget> m_aHintBorders = {};
	protected ImageWidget m_wHintIcon;
	protected ImageWidget m_wHintBadge;			//!< "i" badge drawn when no hint icon texture is set
	protected RichTextWidget m_wHintBadgeText;
	protected RichTextWidget m_wHintText;

	protected float m_fHeadlineDurationMs;
	protected float m_fNotificationDurationMs;
	protected float m_fHintDurationMs;

	protected ref array<ref AROR_Notification> m_aQueue = {};
	protected ref AROR_Notification m_Current;
	protected int m_iCurrentStartMs;
	protected bool m_bCurrentHasIcon;
	protected int m_iCurrentFrames;

	//! Text widgets report their new size only after a layout pass, so the card is sized a couple of frames after SetText
	protected static const int MEASURE_DELAY_FRAMES = 2;
	protected bool m_bCardSized;
	protected float m_fCardWidth;
	protected float m_fCardHeight;
	protected float m_fCardTextHeight;

	protected bool m_bHintActive;
	protected bool m_bHintHasIcon;
	protected int m_iHintStartMs;

	protected bool m_bUpdating;

	//------------------------------------------------------------------------------------------------
	void AROR_NotificationDisplay(ResourceName layout, float headlineSeconds, float notificationSeconds, float hintSeconds)
	{
		m_fHeadlineDurationMs = headlineSeconds * 1000;
		m_fNotificationDurationMs = notificationSeconds * 1000;
		m_fHintDurationMs = hintSeconds * 1000;

		m_wRoot = GetGame().GetWorkspace().CreateWidgets(layout);
		if (!m_wRoot)
		{
			Print("[AROR] Notifications layout could not be created", LogLevel.ERROR);
			return;
		}

		m_wHeadline = m_wRoot.FindAnyWidget("Headline");
		m_wHeadlineBand = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeadlineBand"));
		m_wHeadlineLineTop = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeadlineLineTop"));
		m_wHeadlineLineBottom = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeadlineLineBottom"));
		m_wHeadlineLineLeft = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeadlineLineLeft"));
		m_wHeadlineLineRight = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeadlineLineRight"));
		m_wHeadlineHeader = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeadlineHeader"));
		m_wHeadlineSource = RichTextWidget.Cast(m_wRoot.FindAnyWidget("HeadlineSource"));
		m_wHeadlineIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeadlineIcon"));
		m_wHeadlineText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("HeadlineText"));
		m_wHeadlineDetail = RichTextWidget.Cast(m_wRoot.FindAnyWidget("HeadlineDetail"));
		m_wHeadlineTime = RichTextWidget.Cast(m_wRoot.FindAnyWidget("HeadlineTime"));
		m_wHeadlineIntercept = RichTextWidget.Cast(m_wRoot.FindAnyWidget("HeadlineIntercept"));
		m_wHeadlineEffect = RichTextWidget.Cast(m_wRoot.FindAnyWidget("HeadlineEffect"));
		m_wHeadlineTimer = ImageWidget.Cast(m_wRoot.FindAnyWidget("HeadlineTimer"));

		m_wCard = m_wRoot.FindAnyWidget("Card");
		m_wCardBackground = ImageWidget.Cast(m_wRoot.FindAnyWidget("CardBackground"));
		m_wCardIconBlock = ImageWidget.Cast(m_wRoot.FindAnyWidget("CardIconBlock"));
		m_wCardIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("CardIcon"));
		m_wCardKicker = RichTextWidget.Cast(m_wRoot.FindAnyWidget("CardKicker"));
		m_wCardText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("CardText"));
		m_wCardTimer = ImageWidget.Cast(m_wRoot.FindAnyWidget("CardTimer"));

		m_wHint = m_wRoot.FindAnyWidget("Hint");
		m_wHintBackground = ImageWidget.Cast(m_wRoot.FindAnyWidget("HintBackground"));
		m_wHintIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("HintIcon"));
		m_wHintBadge = ImageWidget.Cast(m_wRoot.FindAnyWidget("HintBadge"));
		m_wHintBadgeText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("HintBadgeText"));
		m_wHintText = RichTextWidget.Cast(m_wRoot.FindAnyWidget("HintText"));

		array<string> borderNames = {"HintBorderTop", "HintBorderBottom", "HintBorderLeft", "HintBorderRight"};
		foreach (string borderName : borderNames)
		{
			ImageWidget border = ImageWidget.Cast(m_wRoot.FindAnyWidget(borderName));
			if (border)
				m_aHintBorders.Insert(border);
		}

		if (!m_wHeadline || !m_wHeadlineText || !m_wCard || !m_wCardText || !m_wHint || !m_wHintText)
		{
			Print("[AROR] Notifications layout is missing widgets (Headline, Card, Hint groups)", LogLevel.ERROR);
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
			return;
		}

		ApplyLayout();
		ApplyStyle();

		m_wHeadline.SetVisible(false);
		m_wCard.SetVisible(false);
		m_wHint.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	void ~AROR_NotificationDisplay()
	{
		GetGame().GetCallqueue().Remove(Update);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	//! The overlay root, for other HUD elements drawn in the same layout (the REBINTEL status widget)
	Widget GetRoot()
	{
		return m_wRoot;
	}

	//------------------------------------------------------------------------------------------------
	//! Queue a notification; headlines go ahead of queued notifications but behind queued headlines
	void Enqueue(notnull AROR_Notification notification)
	{
		if (!m_wRoot)
			return;

		int index = m_aQueue.Count();
		if (notification.GetNotificationType() == AROR_ENotificationType.HEADLINE)
		{
			index = 0;
			while (index < m_aQueue.Count() && m_aQueue[index].GetNotificationType() == AROR_ENotificationType.HEADLINE)
				index++;
		}

		m_aQueue.InsertAt(notification, index);

		if (!m_Current)
			ShowNext();
	}

	//------------------------------------------------------------------------------------------------
	//! Show a hint now, replacing any current one
	void ShowHint(string text, notnull Color color, ResourceName icon, string sound)
	{
		if (!m_wRoot)
			return;

		m_wHintText.SetText(text);

		// A texture icon when one is set, otherwise the "i" badge
		m_bHintHasIcon = SetIcon(m_wHintIcon, icon);
		SetColor(m_wHintIcon, color);
		SetColor(m_wHintBadge, color);
		SetVisible(m_wHintBadge, !m_bHintHasIcon);
		SetVisible(m_wHintBadgeText, !m_bHintHasIcon);

		foreach (ImageWidget border : m_aHintBorders)
		{
			border.SetColor(color);
		}

		m_iHintStartMs = System.GetTickCount();
		m_bHintActive = true;
		m_wHint.SetOpacity(0);
		m_wHint.SetVisible(true);

		PlaySound(sound);
		StartUpdating();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowNext()
	{
		m_Current = null;
		m_wHeadline.SetVisible(false);
		m_wCard.SetVisible(false);

		if (m_aQueue.IsEmpty())
			return;

		m_Current = m_aQueue[0];
		m_aQueue.RemoveOrdered(0);

		Color color = m_Current.GetColor();
		if (m_Current.GetNotificationType() == AROR_ENotificationType.HEADLINE)
		{
			// ToUpper modifies the string in place
			string text = m_Current.GetText();
			text.ToUpper();
			m_wHeadlineText.SetText(text);
			m_bHeadlineLayoutDirty = true;

			// Header band in the priority colour with the source in ink; the detail line is optional
			string source = m_Current.GetKicker();
			if (source.IsEmpty())
				source = DEFAULT_HEADLINE_SOURCE;
			source.ToUpper();
			if (m_wHeadlineSource)
			{
				m_wHeadlineSource.SetText(source);
				m_wHeadlineSource.SetColor(InkFor(color));
			}
			m_bHeadlineHasDetail = SetOptionalText(m_wHeadlineDetail, m_Current.GetDetail());
			m_bHeadlineHasIntercept = SetOptionalText(m_wHeadlineIntercept, m_Current.GetIntercept());
			m_bHeadlineHasEffect = SetOptionalText(m_wHeadlineEffect, m_Current.GetEffect());
			if (m_wHeadlineTime)
			{
				m_wHeadlineTime.SetText(GetGameTimeText());
				m_wHeadlineTime.SetColor(InkFor(color));
			}

			// Rises read in white; good news (rebel-green banners) reads in green
			if (color.G() > color.R() * 1.5 && color.G() > color.B())
				m_wHeadlineText.SetColor(color);
			else
				m_wHeadlineText.SetColor(AROR_UIColors.Get(AROR_UIColors.TEXT));

			m_bCurrentHasIcon = SetIcon(m_wHeadlineIcon, m_Current.GetIcon());
			SetColor(m_wHeadlineIcon, color);
			SetColor(m_wHeadlineHeader, color);
			SetColor(m_wHeadlineLineTop, color);
			SetColor(m_wHeadlineLineBottom, color);
			SetColor(m_wHeadlineLineLeft, color);
			SetColor(m_wHeadlineLineRight, color);
			SetColor(m_wHeadlineTimer, color);

			m_wHeadline.SetOpacity(0);
			m_wHeadline.SetVisible(true);
		}
		else
		{
			string kicker = m_Current.GetKicker();
			kicker.ToUpper();
			if (m_wCardKicker)
				m_wCardKicker.SetText(kicker);

			// Single line first, to measure; SizeCard switches to wrapping if it is too long
			m_wCardText.SetTextWrapping(false);
			FrameSlot.SetSizeToContent(m_wCardText, true);
			m_wCardText.SetText(m_Current.GetText());
			m_bCardSized = false;

			// The sender's icon tinted in the accent colour, or the plain square glyph when it has none
			m_bCurrentHasIcon = SetIcon(m_wCardIcon, m_Current.GetIcon());
			SetVisible(m_wCardIconBlock, !m_bCurrentHasIcon);
			SetColor(m_wCardIcon, color);
			SetColor(m_wCardIconBlock, color);
			SetColor(m_wCardTimer, color);

			m_wCard.SetOpacity(0);
			m_wCard.SetVisible(true);
		}

		m_iCurrentStartMs = System.GetTickCount();
		m_iCurrentFrames = 0;
		PlaySound(m_Current.GetSound());
		StartUpdating();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartUpdating()
	{
		if (m_bUpdating)
			return;

		m_bUpdating = true;
		GetGame().GetCallqueue().CallLater(Update, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while something is on screen: fades, timer bars, and sizes that depend on the text
	protected void Update()
	{
		int now = System.GetTickCount();

		if (m_Current)
			UpdateCurrent(now);

		if (m_bHintActive)
			UpdateHint(now);

		if (!m_Current && !m_bHintActive)
		{
			m_bUpdating = false;
			GetGame().GetCallqueue().Remove(Update);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCurrent(int now)
	{
		bool isHeadline = m_Current.GetNotificationType() == AROR_ENotificationType.HEADLINE;
		float duration = m_fNotificationDurationMs;
		if (isHeadline)
			duration = m_fHeadlineDurationMs;
		if (m_Current.GetDurationSeconds() > 0)
			duration = m_Current.GetDurationSeconds() * 1000;

		float elapsed = now - m_iCurrentStartMs;
		if (elapsed >= duration)
		{
			ShowNext();
			return;
		}

		m_iCurrentFrames++;
		float opacity = GetFadeOpacity(elapsed, duration);
		float remaining = 1 - elapsed / duration;

		if (isHeadline)
		{
			m_wHeadline.SetOpacity(opacity);
			LayoutHeadline(remaining);
			return;
		}

		if (!m_bCardSized && m_iCurrentFrames > MEASURE_DELAY_FRAMES)
			SizeCard();

		// Stay invisible until sized, so the previous message's size never shows
		if (!m_bCardSized)
		{
			m_wCard.SetOpacity(0);
			return;
		}

		m_wCard.SetOpacity(opacity);
		LayoutCard(remaining);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHint(int now)
	{
		float elapsed = now - m_iHintStartMs;
		if (elapsed >= m_fHintDurationMs)
		{
			m_bHintActive = false;
			m_wHint.SetVisible(false);
			return;
		}

		m_wHint.SetOpacity(GetFadeOpacity(elapsed, m_fHintDurationMs));
		LayoutHint();
	}

	//------------------------------------------------------------------------------------------------
	protected static float GetFadeOpacity(float elapsed, float duration)
	{
		float fadeIn = elapsed / FADE_MS;
		float fadeOut = (duration - elapsed) / FADE_MS;
		return Math.Clamp(Math.Min(fadeIn, fadeOut), 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! One UTF-8-aware conservative estimate for both width and wrapping. This is not a font metric.
	//! Never read GetScreenSize here: after Place(), it is our assigned frame, not natural text width.
	protected static float EstimateHeadlineLineWidth(string line, int fontSize)
	{
		return AROR_TextUtils.CharCount(line) * fontSize * 0.75;
	}

	//! Natural text width independent of the previous frame's constrained widget size.
	protected static float HeadlineTextWidth(TextWidget text, int fontSize)
	{
		if (!text)
			return 0;
		array<string> lines = {};
		text.GetText().Split("\n", lines, false);
		float width;
		foreach (string line : lines)
			width = Math.Max(width, EstimateHeadlineLineWidth(line, fontSize));
		return width;
	}

	//! Reserve wrapping space before laying out the next row; never measure our assigned width.
	protected static float PlaceHeadlineText(TextWidget text, int fontSize, float x, float y, float width)
	{
		if (!text)
			return 0;
		array<string> lines = {};
		text.GetText().Split("\n", lines, false);
		int rows;
		foreach (string line : lines)
			rows += Math.Max(1, Math.Ceil(EstimateHeadlineLineWidth(line, fontSize) / Math.Max(1, width)));
		float height = Math.Max(1, rows) * fontSize * 1.3;
		FrameSlot.SetSizeToContent(text, false);
		text.SetTextWrapping(true);
		Place(text, x, y, width, height);
		return height;
	}

	//------------------------------------------------------------------------------------------------
	//! Banner: expand for the title too, then wrap all rows within the viewport.
	protected void LayoutHeadline(float remaining)
	{
		float screenWidth, screenHeight;
		m_wRoot.GetScreenSize(screenWidth, screenHeight);
		float maximum = HEADLINE_MAX_WIDTH;
		if (screenWidth > 0)
			maximum = Math.Min(maximum, Math.Max(160, GetGame().GetWorkspace().DPIUnscale(screenWidth) - 48));
		if (!m_bHeadlineLayoutDirty && maximum == m_fHeadlineViewport)
		{
			Place(m_wHeadlineTimer, 1, m_fHeadlineHeight - 3, (m_fHeadlineWidth - 2) * remaining, 2);
			return;
		}
		float width = HEADLINE_WIDTH;
		float iconSpace;
		if (m_bCurrentHasIcon)
			iconSpace = HEADLINE_ICON_SIZE + HEADLINE_GAP;
		float textLeft = HEADLINE_PADDING + iconSpace;
		float naturalWidth = HeadlineTextWidth(m_wHeadlineText, HEADLINE_FONT_SIZE);
		if (m_bHeadlineHasDetail)
			naturalWidth = Math.Max(naturalWidth, HeadlineTextWidth(m_wHeadlineDetail, HEADLINE_DETAIL_FONT_SIZE));
		if (m_bHeadlineHasIntercept)
			naturalWidth = Math.Max(naturalWidth, HeadlineTextWidth(m_wHeadlineIntercept, HEADLINE_SOURCE_FONT_SIZE));
		if (m_bHeadlineHasEffect)
			naturalWidth = Math.Max(naturalWidth, HeadlineTextWidth(m_wHeadlineEffect, HEADLINE_EFFECT_FONT_SIZE));
		float timeWidth = HeadlineTextWidth(m_wHeadlineTime, HEADLINE_SOURCE_FONT_SIZE) * 1.2;
		float sourceWidth = HeadlineTextWidth(m_wHeadlineSource, HEADLINE_SOURCE_FONT_SIZE);
		width = Math.Max(width, naturalWidth + textLeft + HEADLINE_PADDING);
		width = Math.Max(width, sourceWidth + timeWidth + HEADLINE_PADDING * 3);
		width = Math.Min(width, maximum);
		float contentWidth = width - textLeft - HEADLINE_PADDING;
		float sourceHeight = PlaceHeadlineText(m_wHeadlineSource, HEADLINE_SOURCE_FONT_SIZE, HEADLINE_PADDING, 4, Math.Max(32, width - timeWidth - HEADLINE_PADDING * 3));
		float headerHeight = Math.Max(HEADLINE_HEADER_HEIGHT, sourceHeight + 8);
		float y = headerHeight + HEADLINE_PADDING * 0.5;
		if (m_bHeadlineHasIntercept)
		{
			y += PlaceHeadlineText(m_wHeadlineIntercept, HEADLINE_SOURCE_FONT_SIZE, textLeft, y, contentWidth);
		}
		float textHeight = PlaceHeadlineText(m_wHeadlineText, HEADLINE_FONT_SIZE, textLeft, y, contentWidth);
		Place(m_wHeadlineIcon, HEADLINE_PADDING, y + (textHeight - HEADLINE_ICON_SIZE) * 0.5, HEADLINE_ICON_SIZE, HEADLINE_ICON_SIZE);
		y += textHeight;
		if (m_bHeadlineHasDetail)
		{
			y += PlaceHeadlineText(m_wHeadlineDetail, HEADLINE_DETAIL_FONT_SIZE, textLeft, y + 2, contentWidth) + 2;
		}
		if (m_bHeadlineHasEffect)
		{
			y += PlaceHeadlineText(m_wHeadlineEffect, HEADLINE_EFFECT_FONT_SIZE, textLeft, y + 2, contentWidth) + 2;
		}
		float height = y + HEADLINE_PADDING;
		if (m_wHeadlineTime)
			PlaceHeadlineText(m_wHeadlineTime, HEADLINE_SOURCE_FONT_SIZE, width - HEADLINE_PADDING - timeWidth, 4, Math.Max(1, timeWidth));

		FrameSlot.SetSize(m_wHeadline, width, height);
		Place(m_wHeadlineBand, 0, 0, width, height);
		Place(m_wHeadlineHeader, 0, 0, width, headerHeight);
		Place(m_wHeadlineLineTop, 0, 0, width, 1);
		Place(m_wHeadlineLineBottom, 0, height - 1, width, 1);
		Place(m_wHeadlineLineLeft, 0, 0, 1, height);
		Place(m_wHeadlineLineRight, width - 1, 0, 1, height);

		// Drains from the right along the bottom edge
		Place(m_wHeadlineTimer, 1, height - 3, (width - 2) * remaining, 2);
		m_fHeadlineViewport = maximum;
		m_fHeadlineWidth = width;
		m_fHeadlineHeight = height;
		m_bHeadlineLayoutDirty = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Once per card: width follows the text up to CARD_MAX_WIDTH, then the text wraps (up to CARD_MAX_LINES lines)
	protected void SizeCard()
	{
		float textWidth, lineHeight, kickerWidth, kickerHeight;
		MeasureText(m_wCardText, CARD_TEXT_FONT_SIZE, textWidth, lineHeight);
		if (m_wCardKicker)
			MeasureText(m_wCardKicker, CARD_KICKER_FONT_SIZE, kickerWidth, kickerHeight);

		float maxTextWidth = CARD_MAX_WIDTH - CARD_ICON_BLOCK - CARD_PADDING * 2;
		float contentWidth = Math.Max(textWidth, kickerWidth);
		float lines = 1;

		if (textWidth > maxTextWidth)
		{
			// Word wrapping breaks lines early, so allow some slack when counting them
			lines = Math.Min(Math.Ceil(textWidth * 1.15 / maxTextWidth), CARD_MAX_LINES);
			contentWidth = maxTextWidth;

			FrameSlot.SetSizeToContent(m_wCardText, false);
			m_wCardText.SetTextWrapping(true);
			FrameSlot.SetSize(m_wCardText, maxTextWidth, lines * lineHeight);
		}

		m_fCardTextHeight = lines * lineHeight;
		m_fCardWidth = Math.Clamp(CARD_ICON_BLOCK + CARD_PADDING + contentWidth + CARD_PADDING, CARD_MIN_WIDTH, CARD_MAX_WIDTH);
		m_fCardHeight = Math.Max(CARD_HEIGHT, CARD_TEXT_TOP + m_fCardTextHeight + CARD_PADDING);
		m_bCardSized = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Square glyph (or the sender's icon) on the left, kicker above the text
	protected void LayoutCard(float remaining)
	{
		float width = m_fCardWidth;
		float height = m_fCardHeight;
		float textLeft = CARD_ICON_BLOCK + CARD_PADDING;

		FrameSlot.SetSize(m_wCard, width, height);
		Place(m_wCardBackground, 0, 0, width, height);
		Place(m_wCardIconBlock, CARD_PADDING + (CARD_ICON_BLOCK - CARD_PADDING - CARD_GLYPH_SIZE) * 0.5, (height - CARD_GLYPH_SIZE) * 0.5, CARD_GLYPH_SIZE, CARD_GLYPH_SIZE);
		Place(m_wCardIcon, CARD_PADDING + (CARD_ICON_BLOCK - CARD_PADDING - CARD_ICON_SIZE) * 0.5, (height - CARD_ICON_SIZE) * 0.5, CARD_ICON_SIZE, CARD_ICON_SIZE);

		if (m_wCardKicker)
			FrameSlot.SetPos(m_wCardKicker, textLeft, CARD_KICKER_TOP);

		FrameSlot.SetPos(m_wCardText, textLeft, CARD_TEXT_TOP);
		Place(m_wCardTimer, CARD_ICON_BLOCK, height - 2, (width - CARD_ICON_BLOCK) * remaining, 2);
	}

	//------------------------------------------------------------------------------------------------
	//! Pill at the bottom centre, outlined in the hint colour, with the icon (or "i" badge) before the text
	protected void LayoutHint()
	{
		float textWidth, textHeight;
		MeasureText(m_wHintText, HINT_FONT_SIZE, textWidth, textHeight);

		float iconSpace = HINT_ICON_SIZE + HINT_GAP;
		float width = HINT_PADDING + iconSpace + textWidth + HINT_PADDING + HINT_GAP * 0.5;

		FrameSlot.SetSize(m_wHint, width, HINT_HEIGHT);
		Place(m_wHintBackground, 0, 0, width, HINT_HEIGHT);

		if (m_aHintBorders.Count() == 4)
		{
			Place(m_aHintBorders[0], 0, 0, width, HINT_BORDER);
			Place(m_aHintBorders[1], 0, HINT_HEIGHT - HINT_BORDER, width, HINT_BORDER);
			Place(m_aHintBorders[2], 0, 0, HINT_BORDER, HINT_HEIGHT);
			Place(m_aHintBorders[3], width - HINT_BORDER, 0, HINT_BORDER, HINT_HEIGHT);
		}

		float iconTop = (HINT_HEIGHT - HINT_ICON_SIZE) * 0.5;
		Place(m_wHintIcon, HINT_PADDING, iconTop, HINT_ICON_SIZE, HINT_ICON_SIZE);
		Place(m_wHintBadge, HINT_PADDING, iconTop, HINT_ICON_SIZE, HINT_ICON_SIZE);

		if (m_wHintBadgeText)
		{
			float badgeTextWidth, badgeTextHeight;
			MeasureText(m_wHintBadgeText, HINT_BADGE_FONT_SIZE, badgeTextWidth, badgeTextHeight);
			FrameSlot.SetPos(m_wHintBadgeText, HINT_PADDING + (HINT_ICON_SIZE - badgeTextWidth) * 0.5, iconTop + (HINT_ICON_SIZE - badgeTextHeight) * 0.5);
		}

		FrameSlot.SetPos(m_wHintText, HINT_PADDING + iconSpace, (HINT_HEIGHT - textHeight) * 0.5);
	}

	//------------------------------------------------------------------------------------------------
	//! Anchors for the three groups; sizes follow the content and are set while shown
	protected void ApplyLayout()
	{
		FrameSlot.SetAnchorMin(m_wRoot, 0, 0);
		FrameSlot.SetAnchorMax(m_wRoot, 1, 1);
		FrameSlot.SetOffsets(m_wRoot, 0, 0, 0, 0);
		m_wRoot.SetZOrder(ROOT_Z_ORDER);

		FrameSlot.SetAnchor(m_wHeadline, 0.5, 0);
		FrameSlot.SetAlignment(m_wHeadline, 0.5, 0);
		FrameSlot.SetPos(m_wHeadline, 0, HEADLINE_TOP);

		FrameSlot.SetAnchor(m_wCard, 1, 0);
		FrameSlot.SetAlignment(m_wCard, 1, 0);
		FrameSlot.SetPos(m_wCard, -CARD_MARGIN, CARD_TOP);

		FrameSlot.SetAnchor(m_wHint, 0.5, 1);
		FrameSlot.SetAlignment(m_wHint, 0.5, 1);
		FrameSlot.SetPos(m_wHint, 0, -HINT_BOTTOM);

		array<Widget> texts = {m_wHeadlineText, m_wHeadlineSource, m_wHeadlineDetail, m_wHeadlineTime, m_wHeadlineIntercept, m_wHeadlineEffect, m_wCardKicker, m_wCardText, m_wHintText, m_wHintBadgeText};
		foreach (Widget text : texts)
		{
			if (text)
				FrameSlot.SetSizeToContent(text, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Colours and fonts that do not change per message: flat plates, no shadows (see the UI design system)
	protected void ApplyStyle()
	{
		SetColor(m_wHeadlineBand, AROR_UIColors.Get(AROR_UIColors.PLATE_STRONG));
		SetColor(m_wCardBackground, AROR_UIColors.Get(AROR_UIColors.PLATE));
		SetColor(m_wHintBackground, AROR_UIColors.Get(AROR_UIColors.PLATE));
		StyleText(m_wHeadlineText, HEADLINE_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.TEXT), true);
		StyleText(m_wHeadlineSource, HEADLINE_SOURCE_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.REBEL_INK), true);
		StyleText(m_wHeadlineDetail, HEADLINE_DETAIL_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.TEXT), true);
		StyleText(m_wHeadlineTime, HEADLINE_SOURCE_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.REBEL_INK), true);
		StyleText(m_wHeadlineIntercept, HEADLINE_SOURCE_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.TEXT_MUTED), false);
		StyleText(m_wHeadlineEffect, HEADLINE_EFFECT_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.TEXT), false);
		StyleText(m_wCardKicker, CARD_KICKER_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.TEXT_MUTED), true);
		StyleText(m_wCardText, CARD_TEXT_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.TEXT), true);
		StyleText(m_wHintText, HINT_FONT_SIZE, AROR_UIColors.Get(AROR_UIColors.TEXT), true);

		// Dark "i" on the coloured badge
		if (m_wHintBadgeText)
		{
			m_wHintBadgeText.SetText("i");
			m_wHintBadgeText.SetExactFontSize(HINT_BADGE_FONT_SIZE);
			m_wHintBadgeText.SetColor(AROR_UIColors.Get(AROR_UIColors.WARN_INK));
			m_wHintBadgeText.SetBold(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Dark ink on light priority colours (rebel, warn, danger), light text on dark sender colours
	protected static Color InkFor(Color fill)
	{
		float luminance = 0.2126 * fill.R() + 0.7152 * fill.G() + 0.0722 * fill.B();
		// Widget colours are linear, so mid-tones such as orange sit near 0.2
		if (luminance > 0.18)
			return AROR_UIColors.Get(AROR_UIColors.REBEL_INK);
		return AROR_UIColors.Get(AROR_UIColors.TEXT);
	}

	//------------------------------------------------------------------------------------------------
	//! Single-line text size in reference pixels. GetTextSize returns 0 in game, so this reads the laid-out
	//! (size-to-content) widget instead, and estimates from the font size before the first layout pass.
	protected static void MeasureText(notnull TextWidget text, int fontSize, out float width, out float height)
	{
		float screenWidth, screenHeight;
		text.GetScreenSize(screenWidth, screenHeight);
		if (screenWidth > 0 && screenHeight > 0)
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			width = workspace.DPIUnscale(screenWidth);
			height = workspace.DPIUnscale(screenHeight);
			return;
		}

		string content = text.GetText();
		width = content.Length() * fontSize * 0.55;
		height = fontSize * 1.3;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the text and shows the widget only when there is any. Returns true if shown.
	protected static bool SetOptionalText(TextWidget text, string content)
	{
		if (!text)
			return false;

		bool shown = !content.IsEmpty();
		text.SetText(content);
		text.SetVisible(shown);
		return shown;
	}

	//------------------------------------------------------------------------------------------------
	//! In-game time of day, "21:40", for the banner header
	protected static string GetGameTimeText()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world || !world.GetTimeAndWeatherManager())
			return string.Empty;

		int hours, minutes, seconds;
		world.GetTimeAndWeatherManager().GetHoursMinutesSeconds(hours, minutes, seconds);
		return hours.ToString(2) + ":" + minutes.ToString(2);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetVisible(Widget widget, bool visible)
	{
		if (widget)
			widget.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the icon, or hides the widget when there is none. Returns true if an icon is shown.
	protected static bool SetIcon(ImageWidget image, ResourceName icon)
	{
		if (!image)
			return false;

		bool hasIcon = !icon.IsEmpty();
		if (hasIcon)
			image.LoadImageTexture(0, icon);

		image.SetVisible(hasIcon);
		return hasIcon;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlaySound(string sound)
	{
		// Forced: unforced UI sounds are skipped while another UI sound plays (e.g. the click that closed a menu)
		if (!sound.IsEmpty())
			SCR_UISoundEntity.SoundEvent(sound, true);
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
	protected static void SetColor(Widget widget, Color color)
	{
		if (widget && color)
			widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Place(Widget widget, float x, float y, float width, float height)
	{
		if (!widget)
			return;

		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, width, height);
	}
}
