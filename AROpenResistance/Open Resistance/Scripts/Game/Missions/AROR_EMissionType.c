//! Resistance mission types offered on the OPS board (see ARCHITECTURE.md -> Missions).
//! Active missions are saved with this value: append new types at the end so existing ones stay stable.
//! Extender mods can add types with `modded enum AROR_EMissionType` plus an AROR_Mission subclass returned from a
//! modded AROR_MissionBoardComponent.CreateMission.
enum AROR_EMissionType
{
	SUPPLY_THEFT,		//!< Steal a guarded enemy supply truck and drive it to a friendly location
	CONVOY_AMBUSH,		//!< Stop an enemy convoy before it reaches its destination
	PRISONER_RESCUE,	//!< Free detained sympathisers and escort them to a friendly location
	SABOTAGE,			//!< Destroy parked enemy supply vehicles at a military site
	ASSASSINATION,		//!< Kill a visiting enemy officer before he leaves
	CACHE_RECOVERY		//!< Find an old weapons cache inside a search area and secure it
}
