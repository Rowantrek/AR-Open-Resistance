//! Orders a leader can give a squad from HICOMM Strategic Command
enum AROR_EHighCommandOrder
{
	NONE,	//!< No order yet (fresh squads wait at the HQ)
	ATTACK,	//!< Attack an enemy-held location
	DEFEND,	//!< Defend a Resistance-held location
	MOVE,	//!< Move to a map position, then stand by there (becomes HOLD on arrival)
	HOLD	//!< Stand by at a position, defending it
}

//! Server: one raised squad. Session-only (never saved); its cost is refunded when the campaign ends.
class AROR_HighCommandSquad
{
	int m_iId;
	int m_iOwnerId;		//!< Player ID of the commanding leader; 0 = unassigned (the host can take command)
	int m_iType;		//!< Index into the squad catalogue
	string m_sCallsign;
	int m_iFundsCost;	//!< Paid when raised, kept so the refund matches even if the catalogue changes
	int m_iManpowerCost;
	int m_iSize;
	int m_iRaisedAt;	//!< World time (ms)

	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	AROR_EHighCommandOrder m_eOrder;
	string m_sTargetId;	//!< Stable location ID of an attack/defend target
	vector m_vTarget;	//!< Order position: the location centre, Move point or Hold position

	bool m_bFormed;		//!< Members have spawned at least once
	bool m_bAssaulting;	//!< Attack order: reached the target and attacking (before that it is moving there)
	int m_iAlive;
	vector m_vPosition;
	ref array<IEntity> m_aAffiliated = {};	//!< Members already given the Resistance faction
}

//! Client: a squad as read from the replicated HICOMM snapshot
class AROR_HighCommandSquadInfo
{
	int m_iId;
	int m_iOwnerId;
	int m_iType;
	string m_sCallsign;
	AROR_EHighCommandOrder m_eOrder;
	string m_sTargetId;
	int m_iAlive;
	int m_iSize;
	bool m_bFormed;
	vector m_vPosition;
	vector m_vTarget;
}
