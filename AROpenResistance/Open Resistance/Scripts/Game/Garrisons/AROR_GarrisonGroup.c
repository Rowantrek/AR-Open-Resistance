//! One queued native group, with roster indices mapped as members become available.
class AROR_GarrisonGroup
{
	int m_iId;
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	ref array<int> m_aSlots = {};
	int m_iQueuedAt;
	int m_iNextWake;
	bool m_bOrdered;
	bool m_bQueueClosed;
	bool m_bMountedDefence;
	int m_iNextSeatAttempt;
	int m_iSeatAttempts;
	int m_iNextFootOrder;
	int m_iPatrolLeg;
	int m_iContactSequence;
}
