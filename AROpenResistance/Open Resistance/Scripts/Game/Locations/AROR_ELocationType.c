//! Types of map location. Extender mods can add types with `modded enum AROR_ELocationType`.
//! Append new values at the end so existing values stay stable.
enum AROR_ELocationType
{
	TOWN,
	HQ,
	AIRPORT,
	CITY,
	FACTORY,
	MILITARY_BASE,
	PORT,
	RESOURCES,
	OUTPOST,
	SUPPORT_CORRIDOR,
	HELIPORT
}
