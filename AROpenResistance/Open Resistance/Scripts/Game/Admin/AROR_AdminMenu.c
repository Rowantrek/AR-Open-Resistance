//! The admin/testing menu: a full-screen menu (preset AROR_AdminMenu in chimeraMenus.conf) opened with Ctrl+T by
//! administrators. AROR_AdminApp draws it from the shared pooled-widget layout; tools that need the base-game map
//! (teleport, capture) close the menu first and are run by AROR_AdminMenuComponent. Esc or Ctrl+T closes it.
class AROR_AdminMenu : ChimeraMenuBase
{
	protected static AROR_AdminMenu s_Instance;

	protected ref AROR_AdminApp m_App;
	protected ImageWidget m_wDim;
	protected string m_sPendingTag;

	//! True while the open hotkey is held; starts true so the press that opened the menu does not close it
	protected bool m_bHotkeyDown = true;

	//------------------------------------------------------------------------------------------------
	static AROR_AdminMenu GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the menu, then asks the admin component to run the tag (a tool that needs the map)
	void CloseWith(string tag)
	{
		m_sPendingTag = tag;
		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBack(float value = 0.0, EActionTrigger reason = 0)
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		s_Instance = this;

		Widget root = GetRootWidget();
		m_wDim = ImageWidget.Cast(root.FindAnyWidget("AdminDim"));
		m_App = new AROR_AdminApp();
		if (!m_wDim || !m_App.Init(root, this))
		{
			Print("[AROR] Admin menu layout is missing widgets", LogLevel.ERROR);
			Close();
			return;
		}

		m_wDim.SetColor(Color.FromRGBA(0, 0, 0, 215));
		m_App.SetVisible(true, 1);
		GetGame().GetInputManager().AddActionListener("MenuBack", EActionTrigger.DOWN, OnBack);
		UpdateLayout();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		// The hotkey closes the menu. Read here as well as in AROR_AdminMenuComponent: while a menu is open the game
		// may leave our input context out, so keep it active and watch for the press from inside the menu.
		InputManager inputManager = GetGame().GetInputManager();
		inputManager.ActivateContext("AROR_Context");
		bool hotkeyDown = inputManager.GetActionValue("AROR_OpenAdminMenu") > 0 || inputManager.GetActionValue("AROR_OpenAdminMenuAlt") > 0;
		if (hotkeyDown != m_bHotkeyDown)
		{
			m_bHotkeyDown = hotkeyDown;
			Print(string.Format("[AROR] Admin menu sees hotkey %1", hotkeyDown), LogLevel.NORMAL);
			if (hotkeyDown)
			{
				Close();
				return;
			}
		}

		UpdateLayout();
	}

	//------------------------------------------------------------------------------------------------
	//! The 1700 x 900 admin canvas, scaled to fit the screen and centred
	protected void UpdateLayout()
	{
		if (!m_App || !m_wDim)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float screenWidth = workspace.DPIUnscale(workspace.GetWidth());
		float screenHeight = workspace.DPIUnscale(workspace.GetHeight());
		FrameSlot.SetPos(m_wDim, 0, 0);
		FrameSlot.SetSize(m_wDim, screenWidth, screenHeight);

		float scale = Math.Min(screenWidth / AROR_AdminApp.CANVAS_WIDTH, screenHeight / AROR_AdminApp.CANVAS_HEIGHT);
		m_App.Layout((screenWidth - AROR_AdminApp.CANVAS_WIDTH * scale) * 0.5, (screenHeight - AROR_AdminApp.CANVAS_HEIGHT * scale) * 0.5, scale);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetInputManager().RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnBack);

		if (m_App)
			m_App.Shutdown();

		m_App = null;
		if (s_Instance == this)
			s_Instance = null;

		super.OnMenuClose();

		AROR_AdminMenuComponent admin = AROR_AdminMenuComponent.GetInstance();
		if (admin && !m_sPendingTag.IsEmpty())
			admin.QueueAction(m_sPendingTag);
	}
}
