//! Faction preset gear types (clothing and carried kit). A preset can list many items per type.
//! Fixed list used by the core: do not extend. Presets ignore entries whose type is not in this list.
enum AROR_EGearType
{
	UNIFORM,
	VEST,
	HELMET,
	HEADGEAR,
	BACKPACK,
	FACEWEAR,
	OTHER
}
