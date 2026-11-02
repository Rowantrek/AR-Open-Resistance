//! One notification, built by the sender and passed to AROR_NotificationManagerComponent.Send_S.
//! Empty kicker, colour or sound fall back to the manager's defaults for the type.
class AROR_Notification : Managed
{
	protected AROR_ENotificationType m_eType;
	protected string m_sText;
	protected ResourceName m_sIcon;
	protected ref Color m_Color;
	protected string m_sKicker;
	protected string m_sSound;
	protected string m_sDetail;
	protected string m_sIntercept;
	protected string m_sEffect;
	protected float m_fDurationSeconds;

	//------------------------------------------------------------------------------------------------
	//! \param icon Texture (edds or imageset), white on transparent so it can be tinted; empty for none
	//! \param color Accent colour, e.g. the faction colour; null for the default
	//! \param kicker Small label above the text on a dispatch card, e.g. "Location captured" (unused by headlines)
	//! \param sound UI sound event (see SCR_SoundEvent); empty for the type's default
	void AROR_Notification(AROR_ENotificationType type, string text, ResourceName icon = string.Empty, Color color = null, string kicker = string.Empty, string sound = string.Empty)
	{
		m_eType = type;
		m_sText = text;
		m_sIcon = icon;
		m_Color = color;
		m_sKicker = kicker;
		m_sSound = sound;
	}

	//------------------------------------------------------------------------------------------------
	AROR_ENotificationType GetNotificationType()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	string GetText()
	{
		return m_sText;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetIcon()
	{
		return m_sIcon;
	}

	//------------------------------------------------------------------------------------------------
	Color GetColor()
	{
		return m_Color;
	}

	//------------------------------------------------------------------------------------------------
	string GetKicker()
	{
		return m_sKicker;
	}

	//------------------------------------------------------------------------------------------------
	string GetSound()
	{
		return m_sSound;
	}

	//------------------------------------------------------------------------------------------------
	void SetSound(string sound)
	{
		m_sSound = sound;
	}

	//------------------------------------------------------------------------------------------------
	// Headline extras: optional lines around the headline. Local notifications only; not sent by the server.
	//------------------------------------------------------------------------------------------------
	//! The where/what line under the headline
	void SetDetail(string detail)
	{
		m_sDetail = detail;
	}

	//------------------------------------------------------------------------------------------------
	string GetDetail()
	{
		return m_sDetail;
	}

	//------------------------------------------------------------------------------------------------
	//! Small line above the headline, e.g. "INTERCEPT · COMMAND NET"
	void SetIntercept(string intercept)
	{
		m_sIntercept = intercept;
	}

	//------------------------------------------------------------------------------------------------
	string GetIntercept()
	{
		return m_sIntercept;
	}

	//------------------------------------------------------------------------------------------------
	//! Sentence under the detail line: what this means for players
	void SetEffect(string effect)
	{
		m_sEffect = effect;
	}

	//------------------------------------------------------------------------------------------------
	string GetEffect()
	{
		return m_sEffect;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds on screen; 0 uses the manager's default for the type
	void SetDurationSeconds(float seconds)
	{
		m_fDurationSeconds = seconds;
	}

	//------------------------------------------------------------------------------------------------
	float GetDurationSeconds()
	{
		return m_fDurationSeconds;
	}
}
