//! REBINTEL level-change alerts, raised locally on each client by the status widget (AROR_RebIntelHud).
//! A top-centre banner for 5 seconds: orange when Crackdown rises, amber when Readiness rises, green when either drops.
//! Header "REBINTEL // <scale>" with the game time, an intercept line in the army's own voice, the headline,
//! the change in the army's labels with the English line, and what it means for players.
class AROR_RebIntelAlerts
{
	protected static const float DURATION_SECONDS = 5;

	//------------------------------------------------------------------------------------------------
	static void Raise(AROR_EFactionRole role, AROR_EThreatScale scale, int previous, int level)
	{
		AROR_NotificationManagerComponent notifications = AROR_NotificationManagerComponent.GetInstance();
		if (!notifications)
			return;

		AROR_ThreatLabelSet labels = AROR_ThreatLabels.ForRole(role);
		bool raised = level > previous;

		int color = AROR_UIColors.REBEL;
		if (raised && scale == AROR_EThreatScale.CRACKDOWN)
			color = AROR_UIColors.ALERT_CRACKDOWN;
		else if (raised)
			color = AROR_UIColors.WARN;

		string source = "REBINTEL // " + ScaleWord(labels, scale);
		AROR_Notification notification = new AROR_Notification(AROR_ENotificationType.HEADLINE, labels.GetChangeHeadline(scale, raised), string.Empty, AROR_UIColors.Get(color), source);
		notification.SetIntercept(labels.GetIntercept(scale, raised));
		notification.SetDetail(string.Format("%1 > %2 · %3", labels.GetLabel(scale, previous), labels.GetLabel(scale, level), labels.GetLine(scale, level)));
		notification.SetEffect(Effect(scale, raised, level));
		notification.SetDurationSeconds(DURATION_SECONDS);
		notifications.ShowLocal(notification);
	}

	//------------------------------------------------------------------------------------------------
	//! CRACKDOWN, or the army's word for Readiness (THREATCON for the US Army)
	protected static string ScaleWord(AROR_ThreatLabelSet labels, AROR_EThreatScale scale)
	{
		if (scale == AROR_EThreatScale.CRACKDOWN)
			return "CRACKDOWN";
		if (labels.m_sReadinessHeading == "THREATCON")
			return "THREATCON";
		return "READINESS";
	}

	//------------------------------------------------------------------------------------------------
	//! What the new level means; a drop says so first
	protected static string Effect(AROR_EThreatScale scale, bool raised, int level)
	{
		string effect = AROR_ThreatLabels.GetEffect(scale, level);
		if (raised)
			return effect;
		if (scale == AROR_EThreatScale.CRACKDOWN)
			return "Pressure easing. " + effect;
		return "Enemy standing down. " + effect;
	}
}
