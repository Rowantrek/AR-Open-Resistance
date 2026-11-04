//! Faction preset weapon accessory types. A preset can list many accessories per type.
//! Fixed list used by the core: do not extend. Presets ignore entries whose type is not in this list.
enum AROR_EWeaponAccessoryType
{
	OPTIC,
	SUPPRESSOR,
	BAYONET,
	UNDERBARREL,
	LIGHT,
	OTHER	//!< Anything the importers cannot place (muzzle devices, grips, rail covers...)
}
