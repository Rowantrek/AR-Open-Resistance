[ComponentEditorProps(category: "OpenResistance/Notifications", description: "Sends notifications and hints to players and draws them. Attach to the game mode entity.")]
class AROR_NotificationManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Notifications and hints (see ARCHITECTURE.md -> Notifications).
//! Notifications (headlines and dispatch cards) are sent by the server to everyone or to one player, and queue on screen.
//! Hints are personal: shown locally by a client, or sent by the server to one player (e.g. when it rejects an action).
//! Anything can send: core systems, extender mods and admin tools.
class AROR_NotificationManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("{4F1F368135E5E570}UI/Layouts/Notifications/AROR_Notifications.layout", UIWidgets.ResourceNamePicker, desc: "Layout with the Headline, Card and Hint widget groups", params: "layout")]
	protected ResourceName m_sLayout;

	[Attribute("6", desc: "Seconds a headline stays on screen", category: "Headline")]
	protected float m_fHeadlineDuration;

	[Attribute(SCR_SoundEvent.TASK_SUCCEED, desc: "Default UI sound event for headlines (see SCR_SoundEvent)", category: "Headline")]
	protected string m_sHeadlineSound;

	[Attribute("5", desc: "Seconds a notification stays on screen", category: "Notification")]
	protected float m_fNotificationDuration;

	[Attribute(SCR_SoundEvent.TASK_CREATED, desc: "Default UI sound event for notifications (see SCR_SoundEvent)", category: "Notification")]
	protected string m_sNotificationSound;

	[Attribute("Campaign update", desc: "Label above the text when the sender gives none", category: "Notification")]
	protected string m_sDefaultKicker;

	[Attribute("0.498 0.89 0.604 1", UIWidgets.ColorPicker, desc: "Accent colour when the sender gives none (design system rebel)", category: "Notification")]
	protected ref Color m_DefaultColor;

	[Attribute("3", desc: "Seconds a hint stays on screen", category: "Hint")]
	protected float m_fHintDuration;

	[Attribute(SCR_SoundEvent.HINT, desc: "Default UI sound event for hints (see SCR_SoundEvent); empty for none", category: "Hint")]
	protected string m_sHintSound;

	[Attribute("0.949 0.722 0.294 1", UIWidgets.ColorPicker, desc: "Hint colour when the sender gives none (design system warn)", category: "Hint")]
	protected ref Color m_HintColor;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Icon shown on every hint, white on transparent so it can be tinted; empty for the built-in \"i\" badge", params: "edds imageset", category: "Hint")]
	protected ResourceName m_sHintIcon;

	protected static AROR_NotificationManagerComponent s_Instance;

	protected ref AROR_NotificationDisplay m_Display;

	protected static const int HUD_UPDATE_MS = 250;
	protected ref AROR_RebIntelHud m_RebIntelHud;

	//------------------------------------------------------------------------------------------------
	static AROR_NotificationManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// Sending (static API)
	//------------------------------------------------------------------------------------------------
	//! Server only. Send a headline or notification.
	//! \param playerId Receiving player when the audience is PLAYER
	static void Send_S(notnull AROR_Notification notification, AROR_ENotificationAudience audience = AROR_ENotificationAudience.EVERYONE, int playerId = 0)
	{
		if (!Replication.IsServer())
		{
			Print("[AROR] Notifications can only be sent by the server", LogLevel.WARNING);
			return;
		}

		if (!s_Instance)
		{
			Print(string.Format("[AROR] Notification '%1' dropped: no AROR_NotificationManagerComponent on the game mode", notification.GetText()), LogLevel.WARNING);
			return;
		}

		s_Instance.SendNotification(notification, audience, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client. Show a hint on this machine only (dropped while the local player is dead or not spawned).
	//! \param color Null for the default (amber)
	//! \param sound UI sound event; empty for the default
	static void ShowHint(string text, Color color = null, string sound = string.Empty)
	{
		if (s_Instance)
			s_Instance.DisplayHint(text, color, sound);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Show a hint on one player's screen, e.g. why the server rejected their action.
	static void SendHint_S(int playerId, string text, Color color = null, string sound = string.Empty)
	{
		if (!Replication.IsServer())
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
			return;

		int packedColor;
		bool hasColor = color != null;
		if (hasColor)
			packedColor = color.PackToInt();

		playerController.AROR_ShowHint_S(text, hasColor, packedColor, sound);
	}

	//------------------------------------------------------------------------------------------------
	// Server
	//------------------------------------------------------------------------------------------------
	//! Fills in defaults so every client shows the same thing, then delivers
	protected void SendNotification(AROR_Notification notification, AROR_ENotificationAudience audience, int playerId)
	{
		AROR_ENotificationType type = notification.GetNotificationType();

		string kicker = notification.GetKicker();
		if (kicker.IsEmpty() && type == AROR_ENotificationType.NOTIFICATION)
			kicker = m_sDefaultKicker;

		Color color = notification.GetColor();
		if (!color)
			color = m_DefaultColor;

		string sound = notification.GetSound();
		if (sound.IsEmpty())
		{
			if (type == AROR_ENotificationType.HEADLINE)
				sound = m_sHeadlineSound;
			else
				sound = m_sNotificationSound;
		}

		int packedColor = color.PackToInt();
		string icon = notification.GetIcon();

		if (audience == AROR_ENotificationAudience.PLAYER)
		{
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (playerController)
				playerController.AROR_ShowNotification_S(type, notification.GetText(), icon, packedColor, kicker, sound);

			return;
		}

		// EVERYONE
		Rpc(RpcDo_Notification, type, notification.GetText(), icon, packedColor, kicker, sound);

		// Broadcasts do not reach the sender: show it for a hosting player too
		if (RplSession.Mode() != RplMode.Dedicated)
			ReceiveNotification(type, notification.GetText(), icon, packedColor, kicker, sound);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Notification(int type, string text, string icon, int packedColor, string kicker, string sound)
	{
		ReceiveNotification(type, text, icon, packedColor, kicker, sound);
	}

	//------------------------------------------------------------------------------------------------
	// Client
	//------------------------------------------------------------------------------------------------
	//! Client: a notification arrived (broadcast, or through the player controller for single players)
	void ReceiveNotification(int type, string text, string icon, int packedColor, string kicker, string sound)
	{
		AROR_NotificationDisplay display = GetDisplay();
		if (!display)
			return;

		display.Enqueue(new AROR_Notification(type, text, icon, Color.FromInt(packedColor), kicker, sound));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: queue a notification raised on this machine (e.g. REBINTEL level changes); empty sound uses the type's default
	void ShowLocal(notnull AROR_Notification notification)
	{
		AROR_NotificationDisplay display = GetDisplay();
		if (!display)
			return;

		if (notification.GetSound().IsEmpty())
		{
			if (notification.GetNotificationType() == AROR_ENotificationType.HEADLINE)
				notification.SetSound(m_sHeadlineSound);
			else
				notification.SetSound(m_sNotificationSound);
		}

		display.Enqueue(notification);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: show a hint locally (also the end point of SendHint_S)
	void DisplayHint(string text, Color color, string sound)
	{
		if (!IsLocalPlayerAlive())
			return;

		AROR_NotificationDisplay display = GetDisplay();
		if (!display)
			return;

		if (!color)
			color = m_HintColor;

		if (sound.IsEmpty())
			sound = m_sHintSound;

		display.ShowHint(text, color, m_sHintIcon, sound);
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
	//! Created on first use; never on a dedicated server
	protected AROR_NotificationDisplay GetDisplay()
	{
		if (!m_Display && RplSession.Mode() != RplMode.Dedicated)
			m_Display = new AROR_NotificationDisplay(m_sLayout, m_fHeadlineDuration, m_fNotificationDuration, m_fHintDuration);

		return m_Display;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the full-screen HUD overlay that other HUD elements are created inside (e.g. the personal marker),
	//! or null on a dedicated server
	Widget GetHudRoot()
	{
		AROR_NotificationDisplay display = GetDisplay();
		if (!display)
			return null;

		return display.GetRoot();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;
		if (RplSession.Mode() != RplMode.Dedicated)
			GetGame().GetCallqueue().CallLater(UpdateRebIntelHud, HUD_UPDATE_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the REBINTEL status widget and its level-change alerts, a few times per second
	protected void UpdateRebIntelHud()
	{
		if (!m_RebIntelHud)
		{
			AROR_NotificationDisplay display = GetDisplay();
			if (!display || !display.GetRoot())
				return;
			m_RebIntelHud = new AROR_RebIntelHud(display.GetRoot());
		}

		m_RebIntelHud.Update();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(UpdateRebIntelHud);
		m_RebIntelHud = null;
		m_Display = null;

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
