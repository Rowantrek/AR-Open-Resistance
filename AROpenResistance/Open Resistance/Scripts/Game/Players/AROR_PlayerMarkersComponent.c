//! One connected player as every machine sees it (parsed from the replicated player board)
class AROR_PlayerMarkerInfo
{
	int m_iPlayerId;
	float m_fX;
	float m_fZ;
	bool m_bUnconscious;
}

[ComponentEditorProps(category: "OpenResistance/Players", description: "Publishes every living player's position and consciousness for the map markers. Attach to the game mode entity.")]
class AROR_PlayerMarkersComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Player map markers, data side. Clients only stream nearby characters, so the server publishes a board of all
//! living players (id, position, unconscious) about once a second; every machine parses it and AROR_PlayerMapMarkers
//! draws it. Names are looked up from the player id on the drawing machine. Not saved.
class AROR_PlayerMarkersComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", desc: "Seconds between player board updates", params: "0.25 10 0.25")]
	protected float m_fPublishSeconds;

	protected static const string ROW_SEPARATOR = ";";
	protected static const string FIELD_SEPARATOR = "|";
	protected static const int FIELD_COUNT = 4;

	protected static AROR_PlayerMarkersComponent s_Instance;

	[RplProp(onRplName: "OnBoardReplicated")]
	protected string m_sBoard;

	protected ref array<ref AROR_PlayerMarkerInfo> m_aPlayers = {};

	//------------------------------------------------------------------------------------------------
	static AROR_PlayerMarkersComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: living players as of the last board
	int GetPlayers(notnull array<AROR_PlayerMarkerInfo> outPlayers)
	{
		outPlayers.Clear();
		foreach (AROR_PlayerMarkerInfo info : m_aPlayers)
		{
			outPlayers.Insert(info);
		}
		return outPlayers.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rebuild the board and replicate it if it changed
	protected void Publish_S()
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		string board;
		foreach (int playerId : playerIds)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
			if (!character)
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller)
				continue;

			ECharacterLifeState state = controller.GetLifeState();
			if (state == ECharacterLifeState.DEAD)
				continue;

			vector position = character.GetOrigin();
			int unconscious;
			if (state == ECharacterLifeState.INCAPACITATED)
				unconscious = 1;
			int x = Math.Round(position[0]);
			int z = Math.Round(position[2]);
			board += string.Format("%1|%2|%3|%4", playerId, x, z, unconscious) + ROW_SEPARATOR;
		}

		if (board == m_sBoard)
			return;

		m_sBoard = board;
		Replication.BumpMe();
		OnBoardReplicated();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBoardReplicated()
	{
		m_aPlayers.Clear();
		array<string> rows = {};
		m_sBoard.Split(ROW_SEPARATOR, rows, true);
		foreach (string row : rows)
		{
			array<string> fields = {};
			row.Split(FIELD_SEPARATOR, fields, false);
			if (fields.Count() != FIELD_COUNT)
				continue;

			AROR_PlayerMarkerInfo info = new AROR_PlayerMarkerInfo();
			info.m_iPlayerId = fields[0].ToInt();
			info.m_fX = fields[1].ToFloat();
			info.m_fZ = fields[2].ToFloat();
			info.m_bUnconscious = fields[3].ToInt() != 0;
			m_aPlayers.Insert(info);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(Publish_S, m_fPublishSeconds * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Publish_S);
		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
