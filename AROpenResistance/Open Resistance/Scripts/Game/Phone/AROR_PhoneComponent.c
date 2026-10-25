[ComponentEditorProps(category: "OpenResistance/Phone", description: "Opens the phone with its keybind. Client only. Attach to the game mode entity.")]
class AROR_PhoneComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Opens and closes the phone (AROR_PhoneMenu) with the AROR_OpenPhone input action (rebindable).
//! The action lives in its own input context (AROR_PhoneContext, chimeraInputCommon.conf), activated every frame
//! here so the key works on foot and while the phone is open.
//! Ctrl+Y is also read straight from the keyboard (like the admin menu's Ctrl+T), because the input action did not
//! fire in Workbench testing (Ctrl+Y is also the editor's Redo shortcut).
class AROR_PhoneComponent : SCR_BaseGameModeComponent
{
	protected static const string INPUT_CONTEXT = "AROR_PhoneContext";
	protected static const string INPUT_ACTION = "AROR_OpenPhone";

	//! Ignore a second toggle this soon after the last one (the action and the keyboard check can both fire)
	protected static const int TOGGLE_COOLDOWN_MS = 250;

	protected int m_iLastToggleMs = -TOGGLE_COOLDOWN_MS;

	//------------------------------------------------------------------------------------------------
	//! Every frame on the client
	protected void Update()
	{
		GetGame().GetInputManager().ActivateContext(INPUT_CONTEXT);

		if (!Debug.KeyState(KeyCode.KC_Y))
			return;

		if (!Debug.KeyState(KeyCode.KC_LCONTROL) && !Debug.KeyState(KeyCode.KC_RCONTROL))
			return;

		Debug.ClearKey(KeyCode.KC_Y);
		TogglePhone();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPhoneAction(float value = 0.0, EActionTrigger reason = 0)
	{
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

		GetGame().GetInputManager().AddActionListener(INPUT_ACTION, EActionTrigger.DOWN, OnPhoneAction);
		GetGame().GetCallqueue().CallLater(Update, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Update);

		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.RemoveActionListener(INPUT_ACTION, EActionTrigger.DOWN, OnPhoneAction);

		super.OnDelete(owner);
	}
}
