//! Lifecycle of an ambient patrol (see AROR_PatrolManagerComponent)
enum AROR_EPatrolState
{
	SPAWNING,	//!< Members requested, not all materialised yet
	SEATING,	//!< Motorized: crew and passengers being placed in the vehicle
	MOVING,		//!< Walking or driving its route
	LEAVING		//!< Route done or lifetime over: no more orders, waits to despawn
}
