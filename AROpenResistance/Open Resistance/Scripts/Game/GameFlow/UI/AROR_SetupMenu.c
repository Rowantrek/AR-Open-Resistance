//! Campaign setup screen for the server admin (see ARCHITECTURE.md -> Game Flow -> Setup UI): a full-screen menu
//! (preset AROR_SetupMenu in chimeraMenus.conf). AROR_SetupApp draws it from the shared pooled-widget layout on a
//! 1700 x 900 canvas; the state it shows (screen, save list, server message, new campaign draft) lives on
//! AROR_SetupUIComponent. Placing the HQ or loading a save closes the menu and hands the action to the component next
//! frame. Esc steps back one screen; setup itself cannot be dismissed.
class AROR_SetupMenu : ChimeraMenuBase
{
	//! Between a save's fields as the server sends them: name | save point | date | compatible (1/0)
	static const string FIELD_SEPARATOR = "|";

	protected static AROR_SetupMenu s_Open;

	protected ref AROR_SetupApp m_App;
	protected ImageWidget m_wDim;

	// Action handed to the setup component once the menu has closed
	protected string m_sPendingDialog;
	protected string m_sPendingTag;
	protected int m_iPendingValue;

	//------------------------------------------------------------------------------------------------
	//! The open setup menu, or null
	static AROR_SetupMenu GetOpen()
	{
		return s_Open;
	}

	//------------------------------------------------------------------------------------------------
	//! The setup component's state changed (screen, save list or message): show it
	void SyncFromSetup()
	{
		if (m_App)
			m_App.SyncFromSetup();
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the menu, then asks the setup component to run the action
	void CloseWith(string dialogId, string tag, int value)
	{
		if (!m_sPendingTag.IsEmpty())
			return;

		m_sPendingDialog = dialogId;
		m_sPendingTag = tag;
		m_iPendingValue = value;
		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBack(float value = 0.0, EActionTrigger reason = 0)
	{
		if (m_App)
			m_App.Back();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		Widget root = GetRootWidget();
		m_wDim = ImageWidget.Cast(root.FindAnyWidget("SetupDim"));
		m_App = new AROR_SetupApp();
		if (!m_wDim || !m_App.Init(root, this))
		{
			Print("[AROR] Setup menu layout is missing widgets", LogLevel.ERROR);
			m_App = null;
			Close();
			return;
		}

		m_wDim.SetColor(AROR_UIColors.Get(AROR_UIColors.SURFACE));
		m_App.SetVisible(true, 1);
		GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, OnBack);
		s_Open = this;
		m_App.SyncFromSetup();
		UpdateLayout();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		UpdateLayout();
	}

	//------------------------------------------------------------------------------------------------
	//! The 1700 x 900 canvas, scaled to fit the screen and centred
	protected void UpdateLayout()
	{
		if (!m_App || !m_wDim)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float screenWidth = workspace.DPIUnscale(workspace.GetWidth());
		float screenHeight = workspace.DPIUnscale(workspace.GetHeight());
		FrameSlot.SetPos(m_wDim, 0, 0);
		FrameSlot.SetSize(m_wDim, screenWidth, screenHeight);

		float scale = Math.Min(screenWidth / AROR_SetupApp.CANVAS_WIDTH, screenHeight / AROR_SetupApp.CANVAS_HEIGHT);
		m_App.Layout((screenWidth - AROR_SetupApp.CANVAS_WIDTH * scale) * 0.5, (screenHeight - AROR_SetupApp.CANVAS_HEIGHT * scale) * 0.5, scale);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetInputManager().RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnBack);

		if (m_App)
			m_App.Shutdown();

		m_App = null;
		if (s_Open == this)
			s_Open = null;

		super.OnMenuClose();

		AROR_SetupUIComponent setup = AROR_SetupUIComponent.GetInstance();
		if (setup && !m_sPendingTag.IsEmpty())
			setup.QueueDialogAction(m_sPendingDialog, m_sPendingTag, m_iPendingValue);
	}
}
