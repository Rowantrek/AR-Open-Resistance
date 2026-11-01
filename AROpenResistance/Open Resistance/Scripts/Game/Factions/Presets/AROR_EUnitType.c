//! Faction preset AI unit types. A preset has at most one unit per type.
//! Fixed list used by the core: do not extend. Presets ignore entries whose type is not in this list.
enum AROR_EUnitType
{
	RIFLEMAN,
	AUTORIFLEMAN,
	MACHINEGUNNER,
	GRENADIER,
	MEDIC,
	MARKSMAN,
	SNIPER,
	AT,
	AA,
	ENGINEER,
	RADIO_OPERATOR,
	TEAMLEADER,
	CREW,
	PILOT,
	SQUADLEADER,
	CIVILIAN,
	LAT	//!< Light anti-tank (disposable or short-range launcher); AT is the heavier launcher
}
