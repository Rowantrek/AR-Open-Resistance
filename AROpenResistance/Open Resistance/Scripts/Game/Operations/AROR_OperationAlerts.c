//! One outstanding QRF or attack as every machine sees it (parsed from the replicated threat board)
class AROR_OperationThreatInfo
{
	AROR_EOperationType m_eType;
	AROR_EFactionRole m_eRole;
	string m_sTargetId;
	AROR_EOperationState m_eState;
	int m_iEtaSeconds = -1;	//!< Seconds left when the board was received; -1 before dispatch
}

//! Player-facing operation alerts: a headline with an arrival estimate when a QRF or attack is dispatched, and a
//! replicated board of outstanding responses for the map markers (AROR_OperationMapMarkers). Server writes it,
//! every machine parses it; arrival countdowns run locally from when the board arrived.
modded class AROR_OperationManagerComponent
{
	protected static const int THREAT_BOARD_RESYNC = 15;	//!< Seconds between republishing countdowns that drift
	protected static const string THREAT_ROW = ";";
	protected static const string THREAT_FIELD = "|";

	[RplProp(onRplName: "OnThreatBoardReplicated")]
	protected string m_sThreatBoard;

	protected string m_sThreatBoardStructure;
	protected int m_iThreatBoardResync;
	protected ref array<ref AROR_OperationThreatInfo> m_aThreatInfos = {};
	protected int m_iThreatBoardReceivedAt;

	//------------------------------------------------------------------------------------------------
	//! Every machine: outstanding QRFs and attacks
	int GetThreats(notnull array<AROR_OperationThreatInfo> outThreats)
	{
		outThreats.Clear();
		foreach (AROR_OperationThreatInfo info : m_aThreatInfos)
		{
			outThreats.Insert(info);
		}
		return outThreats.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: arrival seconds still to run, or -1 before dispatch
	int GetThreatEtaRemaining(notnull AROR_OperationThreatInfo info)
	{
		if (info.m_iEtaSeconds < 0)
			return -1;
		int elapsed = (System.GetTickCount() - m_iThreatBoardReceivedAt) / 1000;
		return Math.Max(info.m_iEtaSeconds - elapsed, 0);
	}

	//------------------------------------------------------------------------------------------------
	override protected void NotifyChanged(AROR_Operation operation)
	{
		super.NotifyChanged(operation);
		if (!Replication.IsServer() || !operation || !operation.UsesTacticalResponse())
			return;
		if (operation.GetState() == AROR_EOperationState.ACTIVE)
			AnnounceDispatch_S(operation);
		PublishThreatBoard_S(true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void Tick()
	{
		super.Tick();
		if (!Replication.IsServer() || s_Instance != this)
			return;
		m_iThreatBoardResync--;
		PublishThreatBoard_S(false);
	}

	//------------------------------------------------------------------------------------------------
	//! "NATO QRF DISPATCHED / Levie Military Base · ETA ~4 MIN" (attacks: "... ATTACK INCOMING / ...")
	protected void AnnounceDispatch_S(AROR_Operation operation)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_LocationComponent target;
		if (locations)
			target = locations.GetLocationById(operation.GetTargetLocationId());
		if (!target)
			return;
		string text = DispatchHeadline(ThreatArmyName(operation.GetRole()), operation.GetType(), target.GetName(), operation.GetEtaRemaining(m_iClock));
		int color = AROR_UIColors.WARN;
		if (operation.GetType() == AROR_EOperationType.ATTACK)
			color = AROR_UIColors.DANGER;
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.HEADLINE, text, string.Empty, AROR_UIColors.Get(color), "Enemy movement"));
		Print(string.Format("[AROR ALERT] %1: %2", operation.GetId(), text), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Short army name for alerts and markers: "NATO" for the US Army, "USSR" for the Soviet Army, otherwise the
	//! preset's own name
	static string ThreatArmyName(AROR_EFactionRole role)
	{
		string presetId;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets)
			presetId = presets.GetPresetId(role);
		if (presetId == "US_ARMY")
			return "NATO";
		if (presetId == "USSR_ARMY")
			return "USSR";
		return AROR_RebIntelHud.ArmyName(role);
	}

	//------------------------------------------------------------------------------------------------
	static string DispatchHeadline(string army, AROR_EOperationType type, string targetName, int etaSeconds)
	{
		string text = army + " QRF DISPATCHED / " + targetName;
		if (type == AROR_EOperationType.ATTACK)
			text = army + " ATTACK INCOMING / " + targetName;
		string eta = AROR_OperationEta.Describe(etaSeconds);
		if (!eta.IsEmpty())
			text += "  ·  " + eta;
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Row per outstanding QRF/attack: type|role|target|state|eta. force: publish now; otherwise only when the
	//! set of operations changed, or every resync interval so late joiners' countdowns stay close.
	protected void PublishThreatBoard_S(bool force)
	{
		string board;
		string structure;
		foreach (AROR_Operation operation : m_aOperations)
		{
			if (operation.IsTerminal() || !operation.UsesTacticalResponse())
				continue;
			string row = string.Format("%1|%2|%3|%4", operation.GetType(), operation.GetRole(), operation.GetTargetLocationId(), operation.GetState());
			structure += row + THREAT_ROW;
			board += string.Format("%1|%2", row, operation.GetEtaRemaining(m_iClock)) + THREAT_ROW;
		}
		if (!force && structure == m_sThreatBoardStructure && m_iThreatBoardResync > 0)
			return;
		if (!force && structure.IsEmpty() && m_sThreatBoardStructure.IsEmpty())
			return;
		m_sThreatBoardStructure = structure;
		m_iThreatBoardResync = THREAT_BOARD_RESYNC;
		if (board == m_sThreatBoard)
			return;
		m_sThreatBoard = board;
		Replication.BumpMe();
		OnThreatBoardReplicated();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnThreatBoardReplicated()
	{
		m_aThreatInfos.Clear();
		m_iThreatBoardReceivedAt = System.GetTickCount();
		array<string> rows = {};
		m_sThreatBoard.Split(THREAT_ROW, rows, true);
		foreach (string row : rows)
		{
			array<string> fields = {};
			row.Split(THREAT_FIELD, fields, false);
			if (fields.Count() != 5)
				continue;
			AROR_OperationThreatInfo info = new AROR_OperationThreatInfo();
			info.m_eType = fields[0].ToInt();
			info.m_eRole = fields[1].ToInt();
			info.m_sTargetId = fields[2];
			info.m_eState = fields[3].ToInt();
			info.m_iEtaSeconds = fields[4].ToInt();
			m_aThreatInfos.Insert(info);
		}
	}
}
