//! One ambient patrol (see AROR_PatrolManagerComponent). Server only, never saved.
class AROR_Patrol
{
	int m_iId;
	AROR_EFactionRole m_eRole;
	AROR_EPatrolState m_eState;
	bool m_bMotorized;
	SCR_AIGroup m_Group;
	IEntity m_Vehicle;						//!< Null for foot patrols, and once a motorized patrol's vehicle is wrecked
	AIWaypoint m_Waypoint;
	ref array<vector> m_aRoute = {};		//!< Leg goals, fixed at spawn; never re-targeted at players
	ref array<IEntity> m_aAffiliated = {};	//!< Members already given the role's campaign faction
	int m_iLeg;
	int m_iExpected;						//!< Members requested (counts against the soldier budget)
	int m_iLiving;
	vector m_vPosition;						//!< Vehicle, else a living member; last known if none
	vector m_vProgressPosition;
	int m_iAge;								//!< Seconds since spawn
	int m_iStateAge;						//!< Seconds in the current state
	int m_iUnwatched;						//!< Seconds with no player within the despawn distance
	int m_iSinceCasualty = 100000;			//!< Seconds since a member was last lost
	int m_iProgressTimer;
	int m_iStalls;
}
