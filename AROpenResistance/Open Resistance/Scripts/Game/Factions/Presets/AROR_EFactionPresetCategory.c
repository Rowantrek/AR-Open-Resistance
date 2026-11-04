//! The categories of things the game fields or sells. Faction presets (AROR_FactionPreset) only use UNITS and VEHICLES;
//! content packs (AROR_ContentPack) use every category except UNITS.
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
