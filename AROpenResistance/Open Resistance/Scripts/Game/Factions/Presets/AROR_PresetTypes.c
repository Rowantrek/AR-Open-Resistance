//! The type enum behind each preset category (see AROR_EFactionPresetCategory), shared by faction preset entries and
//! content pack entries so both agree on which types are known and how they are named.
class AROR_PresetTypes
{
	//------------------------------------------------------------------------------------------------
	//! False for types outside the core list (e.g. added by a modded enum); such entries are ignored
	static bool IsKnownType(AROR_EFactionPresetCategory category, int type)
	{
		if (type < 0)
			return false;

		switch (category)
		{
			case AROR_EFactionPresetCategory.UNITS:
				return type <= AROR_EUnitType.CIVILIAN;
			case AROR_EFactionPresetCategory.VEHICLES:
				return type <= AROR_EVehicleType.STATIC_ARTILLERY;
			case AROR_EFactionPresetCategory.GEAR:
				return type <= AROR_EGearType.OTHER;
			case AROR_EFactionPresetCategory.EQUIPMENT:
				return type <= AROR_EEquipmentType.OTHER;
			case AROR_EFactionPresetCategory.WEAPONS:
				return type <= AROR_EWeaponType.MARKSMAN_RIFLE;
			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES:
				return type <= AROR_EWeaponAccessoryType.OTHER;
			case AROR_EFactionPresetCategory.AMMUNITION:
				return type <= AROR_EAmmunitionType.BOMB;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Type name for logs and labels, e.g. "RIFLEMAN"
	static string GetTypeName(AROR_EFactionPresetCategory category, int type)
	{
		switch (category)
		{
			case AROR_EFactionPresetCategory.UNITS:
				return typename.EnumToString(AROR_EUnitType, type);
			case AROR_EFactionPresetCategory.VEHICLES:
				return typename.EnumToString(AROR_EVehicleType, type);
			case AROR_EFactionPresetCategory.GEAR:
				return typename.EnumToString(AROR_EGearType, type);
			case AROR_EFactionPresetCategory.EQUIPMENT:
				return typename.EnumToString(AROR_EEquipmentType, type);
			case AROR_EFactionPresetCategory.WEAPONS:
				return typename.EnumToString(AROR_EWeaponType, type);
			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES:
				return typename.EnumToString(AROR_EWeaponAccessoryType, type);
			case AROR_EFactionPresetCategory.AMMUNITION:
				return typename.EnumToString(AROR_EAmmunitionType, type);
		}

		return string.Empty;
	}
}
