[ComponentEditorProps(category: "OpenResistance/Earplugs", description: "Earplugs: F2 cuts the world sound volume. Client only. Attach to the game mode entity.")]
class AROR_EarplugComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Earplugs (see ARCHITECTURE.md -> Earplugs). The AROR_ToggleEarplugs input action (F2, rebindable; same context and
//! polling approach as the phone) multiplies the local player's SFX and Dialog master volume by a factor, and puts it
//! back when pressed again. Voice chat, music and UI are left alone. While the earplugs are in, a crossed-out ear sits
//! in the top left of the HUD (the "Earplugs" canvas of the notifications overlay, drawn here). Client only; nothing
//! is replicated or saved.
class AROR_EarplugComponent : SCR_BaseGameModeComponent
{
	protected static const string INPUT_CONTEXT = "AROR_Context";
	protected static const string INPUT_ACTION = "AROR_ToggleEarplugs";

	protected static const float ICON_SIZE = 32;	//!< Reference pixels
	protected static const float MARGIN = 16;		//!< space-4 from the screen edge
	protected static const float LINE_WIDTH = 2;
	protected static const int KEYLINE_COLOR = 0xFF080B09;

	[Attribute("0.25", desc: "Volume multiplier while the earplugs are in (0.25 = 75% quieter)", params: "0 1 0.05")]
	protected float m_fVolumeFactor;

	protected bool m_bActionDown;
	protected bool m_bEarplugsIn;
	protected float m_fSavedSfx;
	protected float m_fSavedDialog;

	protected CanvasWidget m_wIcon;
	protected ref array<ref CanvasWidgetCommand> m_aIconCommands = {};

	//------------------------------------------------------------------------------------------------
	//! Every frame on the client
	protected void Update()
	{
		InputManager inputManager = GetGame().GetInputManager();
		inputManager.ActivateContext(INPUT_CONTEXT);

		bool actionDown = inputManager.GetActionValue(INPUT_ACTION) > 0;
		if (actionDown == m_bActionDown)
			return;

		m_bActionDown = actionDown;
		if (actionDown)
			ToggleEarplugs();
	}

	//------------------------------------------------------------------------------------------------
	protected void ToggleEarplugs()
	{
		if (m_bEarplugsIn)
			Remove();
		else
			Insert();
	}

	//------------------------------------------------------------------------------------------------
	protected void Insert()
	{
		// Remember the player's own levels so they are restored exactly
		m_fSavedSfx = AudioSystem.GetMasterVolume(AudioSystem.SFX);
		m_fSavedDialog = AudioSystem.GetMasterVolume(AudioSystem.Dialog);
		AudioSystem.SetMasterVolume(AudioSystem.SFX, m_fSavedSfx * m_fVolumeFactor);
		AudioSystem.SetMasterVolume(AudioSystem.Dialog, m_fSavedDialog * m_fVolumeFactor);
		m_bEarplugsIn = true;
		ShowIndicator(true);
		Print("[AROR] Earplugs in", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Remove()
	{
		AudioSystem.SetMasterVolume(AudioSystem.SFX, m_fSavedSfx);
		AudioSystem.SetMasterVolume(AudioSystem.Dialog, m_fSavedDialog);
		m_bEarplugsIn = false;
		ShowIndicator(false);
		Print("[AROR] Earplugs out", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Show or hide the crossed-out ear; the icon is drawn on first use
	protected void ShowIndicator(bool show)
	{
		if (!m_wIcon)
		{
			if (!show)
				return;

			Widget root = AROR_NotificationManagerComponent.GetOverlayRoot();
			if (root)
				m_wIcon = CanvasWidget.Cast(root.FindAnyWidget("Earplugs"));

			if (!m_wIcon)
			{
				Print("[AROR] Notifications layout has no Earplugs canvas; earplug indicator skipped", LogLevel.WARNING);
				return;
			}

			WorkspaceWidget workspace = GetGame().GetWorkspace();
			float size = workspace.DPIScale(ICON_SIZE);
			FrameSlot.SetPos(m_wIcon, workspace.DPIScale(MARGIN), workspace.DPIScale(MARGIN));
			FrameSlot.SetSize(m_wIcon, size, size);
			DrawIcon(size, workspace.DPIScale(LINE_WIDTH));
		}

		m_wIcon.SetVisible(show);
	}

	//------------------------------------------------------------------------------------------------
	//! An ear on the HUD plate with a red diagonal through it
	protected void DrawIcon(float size, float line)
	{
		m_aIconCommands.Clear();

		PolygonDrawCommand plate = new PolygonDrawCommand();
		plate.m_iColor = AROR_UIColors.PLATE;
		plate.m_Vertices = {0, 0, size, 0, size, size, 0, size};
		m_aIconCommands.Insert(plate);

		// Outer rim then the inner curl, in 0..1 icon space
		array<float> rim = {0.30, 0.45, 0.32, 0.28, 0.45, 0.17, 0.60, 0.17, 0.72, 0.28, 0.74, 0.45, 0.66, 0.58, 0.60, 0.68, 0.56, 0.80, 0.46, 0.86, 0.38, 0.80};
		array<float> curl = {0.45, 0.46, 0.48, 0.34, 0.58, 0.34, 0.60, 0.46, 0.52, 0.57};
		m_aIconCommands.Insert(Line(Scale(rim, size), AROR_UIColors.TEXT, line, false));
		m_aIconCommands.Insert(Line(Scale(curl, size), AROR_UIColors.TEXT, line, false));

		// The slash gets a dark keyline so it reads over the ear
		array<float> slash = {0.16, 0.84, 0.84, 0.16};
		m_aIconCommands.Insert(Line(Scale(slash, size), KEYLINE_COLOR, line * 2.5, false));
		m_aIconCommands.Insert(Line(Scale(slash, size), AROR_UIColors.DANGER, line * 1.25, false));

		m_wIcon.SetDrawCommands(m_aIconCommands);
	}

	//------------------------------------------------------------------------------------------------
	protected static array<float> Scale(array<float> points, float size)
	{
		array<float> scaled = {};
		foreach (float value : points)
		{
			scaled.Insert(value * size);
		}
		return scaled;
	}

	//------------------------------------------------------------------------------------------------
	protected static LineDrawCommand Line(array<float> points, int color, float width, bool closed)
	{
		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color;
		line.m_fWidth = width;
		line.m_Vertices = points;
		line.m_bShouldEnclose = closed;
		return line;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || RplSession.Mode() == RplMode.Dedicated)
			return;

		GetGame().GetCallqueue().CallLater(Update, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Update);

		// Leave the player's volume as they set it
		if (m_bEarplugsIn)
			Remove();

		super.OnDelete(owner);
	}
}
