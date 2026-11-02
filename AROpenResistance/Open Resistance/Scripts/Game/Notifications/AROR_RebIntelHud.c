//! Client-only REBINTEL status widget and level-change alerts (see ARCHITECTURE.md -> Notifications).
//! Shows both enemy armies on one plate in a top corner: per army its name in the faction colour, then a Readiness
//! and a Crackdown row in that army's own labels. Values and meters take the stage colour (green when quiet, red when
//! the Resistance is in trouble) and each army has a stripe in the colour of its worse level. Compact hides the
//! meters. A changed row flashes for a moment. Every level change also raises an alert banner when the player allows
//! pop-ups. Driven by AROR_NotificationManagerComponent; draws into the notifications overlay.
class AROR_RebIntelHud : Managed
{
	protected static const float MARGIN = 16;		//!< space-4 from the screen edge
	protected static const float PADDING = 12;		//!< space-3 inside the plate
	protected static const float TITLE_FONT = 12;
	protected static const float NAME_FONT = 14;
	protected static const float KEY_FONT = 13;		//!< label; caption is the smallest size allowed on the HUD
	protected static const float VALUE_FONT = 16;	//!< hud-value
	protected static const float ROW_HEIGHT = 22;
	protected static const float NAME_HEIGHT = 20;
	protected static const float FLAG_WIDTH = 22;
	protected static const float FLAG_HEIGHT = 14;
	protected static const float FLAG_GAP = 6;
	protected static const float BLOCK_GAP = 8;
	protected static const float STRIPE_WIDTH = 3;
	protected static const float COLUMN_GAP = 12;
	protected static const float SEGMENT_WIDTH = 9;
	protected static const float SEGMENT_HEIGHT = 8;
	protected static const float SEGMENT_GAP = 2;
	protected static const float MAX_VALUE_WIDTH = 170;	//!< Longer labels shrink, never below KEY_FONT
	protected static const int FLASH_MS = 1500;

	protected ref array<AROR_EFactionRole> m_aRoles = {AROR_EFactionRole.OCCUPIERS, AROR_EFactionRole.INVADERS};
	protected ref array<AROR_EThreatScale> m_aScales = {AROR_EThreatScale.READINESS, AROR_EThreatScale.CRACKDOWN};

	protected Widget m_wRoot;
	protected ImageWidget m_wPlate;
	protected RichTextWidget m_wTitle;
	protected ref array<ImageWidget> m_aStripes = {};
	protected ref array<RichTextWidget> m_aNames = {};
	protected ref array<CanvasWidget> m_aFlags = {};
	protected ref array<string> m_aFlagPresets = {"", ""};	//!< Army each flag was last drawn for
	//! Draw commands per flag canvas: the canvas does not keep its own copy
	protected ref array<ref array<ref CanvasWidgetCommand>> m_aFlagCommands = {new array<ref CanvasWidgetCommand>(), new array<ref CanvasWidgetCommand>()};
	// One entry per row, index role * 2 + scale
	protected ref array<ImageWidget> m_aFlashes = {};
	protected ref array<RichTextWidget> m_aKeys = {};
	protected ref array<RichTextWidget> m_aValues = {};
	protected ref array<ref array<ImageWidget>> m_aMeters = {};

	//! Last level seen per row; 0 until the first reading, which never alerts
	protected ref array<int> m_aSeen = {0, 0, 0, 0};
	protected ref array<int> m_aFlashUntil = {0, 0, 0, 0};

	//------------------------------------------------------------------------------------------------
	void AROR_RebIntelHud(notnull Widget overlayRoot)
	{
		m_wRoot = overlayRoot.FindAnyWidget("Status");
		m_wPlate = ImageWidget.Cast(overlayRoot.FindAnyWidget("StatusPlate"));
		m_wTitle = RichTextWidget.Cast(overlayRoot.FindAnyWidget("StatusTitle"));
		foreach (AROR_EFactionRole role : m_aRoles)
		{
			string army = "Status" + RoleName(role);
			m_aStripes.Insert(ImageWidget.Cast(overlayRoot.FindAnyWidget(army + "Stripe")));
			m_aNames.Insert(RichTextWidget.Cast(overlayRoot.FindAnyWidget(army + "Name")));
			m_aFlags.Insert(CanvasWidget.Cast(overlayRoot.FindAnyWidget(army + "Flag")));
			foreach (AROR_EThreatScale scale : m_aScales)
			{
				string row = army + ScaleName(scale);
				m_aFlashes.Insert(ImageWidget.Cast(overlayRoot.FindAnyWidget(row + "Flash")));
				m_aKeys.Insert(RichTextWidget.Cast(overlayRoot.FindAnyWidget(row + "Key")));
				m_aValues.Insert(RichTextWidget.Cast(overlayRoot.FindAnyWidget(row + "Value")));
				array<ImageWidget> meter = {};
				for (int i = 1; i <= 5; i++)
				{
					meter.Insert(ImageWidget.Cast(overlayRoot.FindAnyWidget(row + "Meter" + i)));
				}
				m_aMeters.Insert(meter);
			}
		}

		if (!IsComplete())
		{
			Print("[AROR] Notifications layout is missing the REBINTEL status widgets (Status group)", LogLevel.ERROR);
			m_wRoot = null;
			return;
		}

		m_wPlate.SetColor(AROR_UIColors.Get(AROR_UIColors.PLATE));
		Style(m_wTitle, TITLE_FONT, AROR_UIColors.TEXT_MUTED, true);
		m_wTitle.SetText("REBINTEL");
		foreach (RichTextWidget name : m_aNames)
		{
			Style(name, NAME_FONT, AROR_UIColors.TEXT, true);
		}
		foreach (int r, RichTextWidget key : m_aKeys)
		{
			Style(key, KEY_FONT, AROR_UIColors.TEXT_MUTED, false);
			Style(m_aValues[r], VALUE_FONT, AROR_UIColors.TEXT, true);
		}
		m_wRoot.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! A few times per second: alerts for any level change, then the widget
	void Update()
	{
		if (!m_wRoot)
			return;

		AROR_OperationManagerComponent manager;
		IEntity gameMode = GetGame().GetGameMode();
		if (gameMode)
			manager = AROR_OperationManagerComponent.Cast(gameMode.FindComponent(AROR_OperationManagerComponent));
		if (!manager || !manager.HasThreatState())
		{
			m_wRoot.SetVisible(false);
			return;
		}

		int now = System.GetTickCount();
		foreach (int r, AROR_EFactionRole role : m_aRoles)
		{
			foreach (int s, AROR_EThreatScale scale : m_aScales)
			{
				int index = r * 2 + s;
				int level = manager.GetThreatLevel(role, scale);
				int previous = m_aSeen[index];
				m_aSeen[index] = level;
				if (previous == 0 || previous == level)
					continue;

				m_aFlashUntil[index] = now + FLASH_MS;
				if (AROR_RebIntelSettings.AreAlertsShown())
					AROR_RebIntelAlerts.Raise(role, scale, previous, level);
			}
		}

		bool visible = AROR_RebIntelSettings.IsHudShown() && !IsHudHidden();
		m_wRoot.SetVisible(visible);
		if (visible)
			Draw(now);
	}

	//------------------------------------------------------------------------------------------------
	protected void Draw(int now)
	{
		bool full = AROR_RebIntelSettings.IsFull();

		// Texts first, so the key and value columns can be sized to the longest entries
		float keyColumn, valueColumn, nameWidth;
		array<float> valueFonts = {};
		foreach (int r, AROR_EFactionRole role : m_aRoles)
		{
			AROR_ThreatLabelSet labels = AROR_ThreatLabels.ForRole(role);
			m_aNames[r].SetText(ArmyName(role));
			m_aNames[r].SetColor(AROR_UIColors.Get(ArmyColor(role)));
			nameWidth = Math.Max(nameWidth, TextWidth(m_aNames[r].GetText(), NAME_FONT));
			DrawFlag(r, role);

			int worst = 0;
			foreach (int s, AROR_EThreatScale scale : m_aScales)
			{
				int index = r * 2 + s;
				int level = m_aSeen[index];
				worst = Math.Max(worst, level);
				int stage = AROR_UIColors.ForLevel(level);

				m_aKeys[index].SetText(labels.GetHeading(scale));
				m_aValues[index].SetText(ShortLabel(labels, scale, level));
				m_aValues[index].SetColor(AROR_UIColors.Get(stage));
				keyColumn = Math.Max(keyColumn, TextWidth(m_aKeys[index].GetText(), KEY_FONT));

				float valueFont = VALUE_FONT;
				float valueWidth = TextWidth(m_aValues[index].GetText(), VALUE_FONT);
				if (valueWidth > MAX_VALUE_WIDTH)
				{
					valueFont = Math.Max(KEY_FONT, VALUE_FONT * MAX_VALUE_WIDTH / valueWidth);
					valueWidth = TextWidth(m_aValues[index].GetText(), valueFont);
				}
				m_aValues[index].SetExactFontSize(Math.Round(valueFont));
				valueFonts.Insert(valueFont);
				valueColumn = Math.Max(valueColumn, valueWidth);

				foreach (int i, ImageWidget segment : m_aMeters[index])
				{
					segment.SetVisible(full);
					int segmentColor = AROR_UIColors.LINE;
					if (i < level)
						segmentColor = stage;
					segment.SetColor(AROR_UIColors.Get(segmentColor));
				}

				// A changed row flashes in its new stage colour
				ImageWidget flash = m_aFlashes[index];
				flash.SetVisible(now < m_aFlashUntil[index]);
				flash.SetColor(AROR_HiCommMap.Rgb(stage & 0xFFFFFF, 0.28));
			}
			m_aStripes[r].SetColor(AROR_UIColors.Get(AROR_UIColors.ForLevel(worst)));
		}

		float meterWidth;
		if (full)
			meterWidth = COLUMN_GAP + SEGMENT_WIDTH * 5 + SEGMENT_GAP * 4;
		float rowLeft = PADDING + STRIPE_WIDTH + 8;
		float width = Math.Max(rowLeft + keyColumn + COLUMN_GAP + valueColumn + meterWidth + PADDING, rowLeft + FLAG_WIDTH + FLAG_GAP + nameWidth + PADDING);
		width = Math.Max(width, 190);

		// Lay out: title, then per army its name and two rows beside a stage stripe
		float y = PADDING - 2;
		FrameSlot.SetPos(m_wTitle, PADDING, y);
		FrameSlot.SetSize(m_wTitle, width - PADDING * 2, TITLE_FONT * 1.4);
		y += TITLE_FONT * 1.4 + 4;
		foreach (int r, AROR_EFactionRole role : m_aRoles)
		{
			float blockTop = y;
			Place(m_aFlags[r], rowLeft, y + (NAME_HEIGHT - FLAG_HEIGHT) * 0.5, FLAG_WIDTH, FLAG_HEIGHT);
			Place(m_aNames[r], rowLeft + FLAG_WIDTH + FLAG_GAP, y, width - rowLeft - FLAG_WIDTH - FLAG_GAP - PADDING, NAME_HEIGHT);
			y += NAME_HEIGHT;
			foreach (int s, AROR_EThreatScale scale : m_aScales)
			{
				int index = r * 2 + s;
				Place(m_aFlashes[index], PADDING + STRIPE_WIDTH, y, width - PADDING * 2 - STRIPE_WIDTH, ROW_HEIGHT);
				Place(m_aKeys[index], rowLeft, y + (ROW_HEIGHT - KEY_FONT * 1.3) * 0.5, keyColumn + 4, KEY_FONT * 1.4);
				float valueFont = valueFonts[index];
				Place(m_aValues[index], rowLeft + keyColumn + COLUMN_GAP, y + (ROW_HEIGHT - valueFont * 1.3) * 0.5, valueColumn + 8, valueFont * 1.4);
				float meterLeft = width - PADDING - (SEGMENT_WIDTH * 5 + SEGMENT_GAP * 4);
				foreach (int i, ImageWidget segment : m_aMeters[index])
				{
					Place(segment, meterLeft + i * (SEGMENT_WIDTH + SEGMENT_GAP), y + (ROW_HEIGHT - SEGMENT_HEIGHT) * 0.5, SEGMENT_WIDTH, SEGMENT_HEIGHT);
				}
				y += ROW_HEIGHT;
			}
			Place(m_aStripes[r], PADDING, blockTop + 2, STRIPE_WIDTH, y - blockTop - 4);
			y += BLOCK_GAP;
		}
		float height = y - BLOCK_GAP + PADDING - 2;

		float anchor = 1;
		float x = -MARGIN;
		if (AROR_RebIntelSettings.IsLeft())
		{
			anchor = 0;
			x = MARGIN;
		}
		FrameSlot.SetAnchor(m_wRoot, anchor, 0);
		FrameSlot.SetAlignment(m_wRoot, anchor, 0);
		FrameSlot.SetPos(m_wRoot, x, MARGIN);
		FrameSlot.SetSize(m_wRoot, width, height);
		Place(m_wPlate, 0, 0, width, height);
	}

	//------------------------------------------------------------------------------------------------
	//! The army's national flag beside its name; redrawn only when the army holding the role changes
	protected void DrawFlag(int index, AROR_EFactionRole role)
	{
		string presetId;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets)
			presetId = presets.GetPresetId(role);
		if (presetId == m_aFlagPresets[index] && !presetId.IsEmpty())
			return;

		m_aFlagPresets[index] = presetId;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		AROR_ArmyFlag.Draw(m_aFlags[index], m_aFlagCommands[index], workspace.DPIScale(FLAG_WIDTH), workspace.DPIScale(FLAG_HEIGHT), presetId, AROR_UIColors.Get(ArmyColor(role)));
	}

	//------------------------------------------------------------------------------------------------
	//! "THREATCON NORMAL" beside the THREATCON key reads as "NORMAL"
	protected static string ShortLabel(AROR_ThreatLabelSet labels, AROR_EThreatScale scale, int level)
	{
		string label = labels.GetLabel(scale, level);
		string prefix = labels.GetHeading(scale) + " ";
		if (label.IndexOf(prefix) == 0)
			return label.Substring(prefix.Length(), label.Length() - prefix.Length());
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! The army a role uses, e.g. "US ARMY"; falls back to the role
	static string ArmyName(AROR_EFactionRole role)
	{
		string name;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets && presets.GetPreset(role))
			name = presets.GetPreset(role).GetName();
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
	//! Faction colour token for the army a role uses
	static int ArmyColor(AROR_EFactionRole role)
	{
		string presetId;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets)
			presetId = presets.GetPresetId(role);
		if (presetId == "USSR_ARMY")
			return AROR_UIColors.FACTION_USSR;
		if (presetId == "US_ARMY")
			return AROR_UIColors.FACTION_US;
		return AROR_UIColors.TEXT;
	}

	//------------------------------------------------------------------------------------------------
	//! Hidden while the map, the phone, the inventory or any other menu is open, and when not playing a character
	protected static bool IsHudHidden()
	{
		if (GetGame().GetMenuManager().IsAnyMenuOpen())
			return true;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			return true;

		PlayerController playerController = GetGame().GetPlayerController();
		return !playerController || !playerController.GetControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsComplete()
	{
		if (!m_wRoot || !m_wPlate || !m_wTitle)
			return false;
		foreach (int r, ImageWidget stripe : m_aStripes)
		{
			if (!stripe || !m_aNames[r] || !m_aFlags[r])
				return false;
		}
		foreach (int i, RichTextWidget key : m_aKeys)
		{
			if (!key || !m_aValues[i] || !m_aFlashes[i])
				return false;
			foreach (ImageWidget segment : m_aMeters[i])
			{
				if (!segment)
					return false;
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string RoleName(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.INVADERS)
			return "Invaders";
		return "Occupiers";
	}

	//------------------------------------------------------------------------------------------------
	protected static string ScaleName(AROR_EThreatScale scale)
	{
		if (scale == AROR_EThreatScale.READINESS)
			return "Readiness";
		return "Crackdown";
	}

	//------------------------------------------------------------------------------------------------
	protected static float TextWidth(string text, float fontSize)
	{
		return AROR_TextUtils.CharCount(text) * fontSize * 0.58;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Style(TextWidget text, float fontSize, int color, bool bold)
	{
		text.SetExactFontSize(fontSize);
		text.SetColor(AROR_UIColors.Get(color));
		text.SetBold(bold);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Place(Widget widget, float x, float y, float width, float height)
	{
		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, width, height);
	}
}
