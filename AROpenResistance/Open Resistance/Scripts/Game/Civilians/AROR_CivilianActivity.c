//! A pedestrian or an occupied car belonging to one settlement's ambient population budget.
class AROR_CivilianActivity
{
	string m_sLocationId;
	SCR_AIGroup m_Group;
	IEntity m_Vehicle;
	AIWaypoint m_Waypoint;
	ref array<IEntity> m_aAffiliated = {};
	ref array<IEntity> m_aMembers = {};
	int m_iExpected;
	int m_iAge;
	int m_iStateAge;
	int m_iUnwatched;
	int m_iStalled;
	int m_iStalls;
	bool m_bReady;
	bool m_bRetiring;
	bool m_bClaimedVehicle;
	vector m_vCentre;
	vector m_vPosition;
	vector m_vTarget;
	vector m_vProgress;
}
