//! Special location an entry needs the role to hold, on top of its import facility count. Extender mods can add
//! values with `modded enum AROR_EImportPointRequirement`. Append new values at the end so existing values stay stable.
enum AROR_EImportPointRequirement
{
	NONE,
	PORT,	//!< At least one held seaport
	AIR		//!< At least one held heliport or airport
}
