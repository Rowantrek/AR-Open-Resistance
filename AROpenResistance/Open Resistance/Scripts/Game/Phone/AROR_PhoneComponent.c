[ComponentEditorProps(category: "OpenResistance/Phone", description: "Opens the phone with its keybind. Client only. Attach to the game mode entity.")]
class AROR_PhoneComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Opens and closes the phone (AROR_PhoneMenu) with the AROR_OpenPhone input action (rebindable).
//! The action lives in its own input context (AROR_Context, chimeraInputCommon.conf), activated every frame
//! here so the key works on foot and while the phone is open.
//! Ctrl+Y is also read straight from the keyboard (like the admin menu's Ctrl+T), because the input action did not
//! fire in Workbench testing (Ctrl+Y is also the editor's Redo shortcut).
class AROR_PhoneComponent : SCR_BaseGameModeComponent
{
	protected static const string INPUT_CONTEXT = "AROR_Context";
	protected static const string INPUT_ACTION = "AROR_OpenPhone";

	//! Suppress transient input changes while native menu contexts switch
	protected static const int TOGGLE_COOLDOWN_MS = 250;

	protected int m_iLastToggleMs = -TOGGLE_COOLDOWN_MS;
	protected bool m_bHotkeyDown;
	protected static AROR_PhoneComponent s_Instance;
	protected ref AROR_PhoneSessionState m_Session;

	//! Client session survives menu destruction and player respawn, but not a world/server change.
	static AROR_PhoneSessionState GetSession()
	{
		if (!s_Instance)
			return null;
		return s_Instance.m_Session;
	}

	//------------------------------------------------------------------------------------------------
	//! Menu updates use the same latch when gameplay callqueue/input processing is suspended.
	static void PollInput()
	{
		if (s_Instance)
			s_Instance.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! One rising edge across the rebindable actions and the Workbench keyboard fallback.
	protected void Update()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;
		inputManager.ActivateContext(INPUT_CONTEXT);

		bool keyboardDown = Debug.KeyState(KeyCode.KC_Y) && (Debug.KeyState(KeyCode.KC_LCONTROL) || Debug.KeyState(KeyCode.KC_RCONTROL));
		bool hotkeyDown = inputManager.GetActionValue(INPUT_ACTION) > 0 || inputManager.GetActionValue("AROR_OpenPhoneAlt") > 0 || keyboardDown || Debug.KeyState(KeyCode.KC_F9);
		if (hotkeyDown == m_bHotkeyDown)
			return;

		m_bHotkeyDown = hotkeyDown;
		if (hotkeyDown)
			TogglePhone();
	}

	//------------------------------------------------------------------------------------------------
	//! Close the open phone, or open it when no other menu is open and the player is alive
	protected void TogglePhone()
	{
		int now = System.GetTickCount();
		if (now - m_iLastToggleMs < TOGGLE_COOLDOWN_MS)
			return;

		m_iLastToggleMs = now;

		AROR_PhoneMenu phone = AROR_PhoneMenu.GetInstance();
		if (phone)
		{
			phone.RequestClose();
			return;
		}

		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager.IsAnyMenuOpen() || !IsLocalPlayerAlive())
			return;

		menuManager.OpenMenu(ChimeraMenuPreset.AROR_PhoneMenu);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsLocalPlayerAlive()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(playerController.GetControlledEntity());
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && !controller.IsDead();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || RplSession.Mode() == RplMode.Dedicated)
			return;

		m_Session = new AROR_PhoneSessionState();
		s_Instance = this;
		Print("[AROR] Phone hotkey ready (shared input polling)", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(Update, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;
		m_Session = null;
		GetGame().GetCallqueue().Remove(Update);

		super.OnDelete(owner);
	}
}
