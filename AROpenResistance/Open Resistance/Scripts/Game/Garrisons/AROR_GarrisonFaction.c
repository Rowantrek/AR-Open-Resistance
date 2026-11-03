//! Defence recipes request types from the campaign role's selected faction preset. Legacy prefab fields are type
//! hints only. Soldiers and crews spawn as the role's base character dressed from its preset.
[BaseContainerProps()]
class AROR_GarrisonFaction
{
	[Attribute("-1", UIWidgets.ComboBox, desc: "Static weapon type from the selected faction preset; -1 uses the legacy prefab's type", enums: ParamEnumArray.FromEnum(AROR_EVehicleType))]
	AROR_EVehicleType m_eStaticWeaponType;
	[Attribute("", UIWidgets.ComboBox, desc: "Additional static weapon types from the selected faction preset, replacing the legacy variant list", enums: ParamEnumArray.FromEnum(AROR_EVehicleType))]
	ref array<AROR_EVehicleType> m_aStaticWeaponTypes;
	[Attribute("-1", UIWidgets.ComboBox, desc: "Unmanned armour type from the selected faction preset; -1 uses the legacy prefab's type", enums: ParamEnumArray.FromEnum(AROR_EVehicleType))]
	AROR_EVehicleType m_eParkedVehicleType;
	[Attribute("", UIWidgets.ComboBox, desc: "Manned defence vehicle types from the selected faction preset, replacing the legacy vehicle list", enums: ParamEnumArray.FromEnum(AROR_EVehicleType))]
	ref array<AROR_EVehicleType> m_aMannedVehicleTypes;

	[Attribute("1", UIWidgets.ComboBox, desc: "Campaign role served by this asset catalogue", enums: AROR_FactionRoles.MilitaryRoleParams())]
	AROR_EFactionRole m_eRole;
	[Attribute("AROR_OCCUPIERS", desc: "Configured campaign faction key for this role; must match the faction-role mapping")]
	string m_sFactionKey;
	[Attribute("US", desc: "Native source faction allowed during initial spawning only")]
	string m_sAssetFactionKey;
	[Attribute("{73530808E4B455BA}Prefabs/Weapons/Tripods/Tripod_M3_M2HB.et", UIWidgets.ResourceNamePicker, desc: "Legacy static weapon type hint, used when no explicit type is configured; never spawned directly", params: "et")]
	ResourceName m_sStaticMG;
	[Attribute("", desc: "Legacy static weapon type hints after the default MG; overridden by Static Weapon Types. Selected weapons require exactly one usable crew compartment.")]
	ref array<ResourceName> m_aStaticVariants;
	[Attribute("{0FBF8F010F81A4E5}Prefabs/Vehicles/Wheeled/LAV25/LAV25.et", UIWidgets.ResourceNamePicker, desc: "Legacy unmanned armour type hint; never spawned directly", params: "et")]
	ResourceName m_sParkedArmour;
	[Attribute("", desc: "Legacy manned defence type hints; overridden by Manned Vehicle Types. Every selected hull is fully crewed using native pilot/turret compartments.")]
	ref array<ResourceName> m_aMannedVehicles;

	int StaticVariantCount()
	{
		if (m_aStaticWeaponTypes && !m_aStaticWeaponTypes.IsEmpty())
			return 1 + m_aStaticWeaponTypes.Count();
		if (m_aStaticVariants)
			return 1 + m_aStaticVariants.Count();
		return 1;
	}

	int MannedVariantCount()
	{
		if (m_aMannedVehicleTypes && !m_aMannedVehicleTypes.IsEmpty())
			return m_aMannedVehicleTypes.Count();
		if (m_aMannedVehicles)
			return m_aMannedVehicles.Count();
		return 0;
	}
}
