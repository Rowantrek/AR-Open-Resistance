//! Server-owned recruitment transaction. The native player group owns commands and membership after joining.
class AROR_SquadRecruit
{
	int m_iId;
	SCR_AIGroup m_Master;
	SCR_AIGroup m_SpawnGroup;
	SCR_ChimeraCharacter m_Character;
	float m_fStarted;
	bool m_bJoined;
}

//! Compact phone roster entry; works even when the character is outside the client's streaming range.
class AROR_SquadMemberInfo
{
	int m_iId;
	int m_iGroupId;
	string m_sState;
}
