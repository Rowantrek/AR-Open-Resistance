//! Faction preset ammunition types. A preset can list many items per type.
//! Fixed list used by the core: do not extend. Presets ignore entries whose type is not in this list.
enum AROR_EAmmunitionType
{
	MAGAZINE,
	BELT,
	ROCKET,
	SHELL,
	GRENADE_ROUND,
	MISSILE,
	BOMB
}
