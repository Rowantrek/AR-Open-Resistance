//! One individual per managed group permits independent orders without changing native player groups.
class AROR_Recruit
{
	int m_iId;
	int m_iOwnerId;
	string m_sOwnerKey;
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	IEntity m_Character;
	IEntity m_Vehicle;
	int m_iOrder = AROR_ERecruitAction.FOLLOW;
	int m_iRaisedAt;
	int m_iFundsCost;
	int m_iManpowerCost;
	int m_iBoardDeadline;
	bool m_bFormed;
	vector m_vTarget;
	ref array<IEntity> m_aAffiliated = {};
}
