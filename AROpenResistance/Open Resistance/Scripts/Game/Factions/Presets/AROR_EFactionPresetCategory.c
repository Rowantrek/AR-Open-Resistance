//! The categories of a faction preset (see AROR_FactionPreset).
//! Fixed list used by the core: do not extend.
enum AROR_EFactionPresetCategory
{
	UNITS,				//!< AI characters (AROR_EUnitType), at most one per type
	VEHICLES,			//!< Empty vehicles (AROR_EVehicleType)
	GEAR,				//!< Clothing and carried kit (AROR_EGearType)
	EQUIPMENT,			//!< Medical, binoculars, radios... (AROR_EEquipmentType)
	WEAPONS,			//!< Hand-held weapons (AROR_EWeaponType)
	WEAPON_ACCESSORIES,	//!< Optics, suppressors... (AROR_EWeaponAccessoryType)
	AMMUNITION			//!< Ammunition, including rocket pods and other munitions loaded onto vehicles and aircraft (AROR_EAmmunitionType)
}
