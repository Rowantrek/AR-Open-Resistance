//! One mission as every machine sees it, parsed from AROR_MissionBoardComponent's replicated snapshot
class AROR_MissionInfo
{
	int m_iId;
	AROR_EMissionType m_eType;
	bool m_bActive;
	string m_sCodename;
	string m_sLocationId;
	vector m_vPosition;
	float m_fAreaRadius;
	string m_sDestinationId;
	vector m_vDestination;
	bool m_bHasDestination;
	AROR_EFactionRole m_eEnemyRole;
	int m_iFunds;
	int m_iManpower;
	int m_iTimeLimit;		//!< Seconds
	int m_iSeconds;			//!< When the snapshot arrived: seconds left on the board (offers) or to complete (active)
	int m_iRisk;
	int m_iGuards;
	string m_sLeaderName;
	int m_iLeaderId;		//!< Session owner; 0 after load means any current leader
	int m_iPhase;
	string m_sProgress;
}
