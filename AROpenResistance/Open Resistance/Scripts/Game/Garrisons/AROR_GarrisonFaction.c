//! Asset bindings are data, independent of the campaign role's faction identity.
[BaseContainerProps()]
class AROR_GarrisonFaction
{
	[Attribute("1", UIWidgets.ComboBox, desc: "Campaign role served by this asset catalogue", enums: AROR_FactionRoles.MilitaryRoleParams())]
	AROR_EFactionRole m_eRole;
	[Attribute("AROR_OCCUPIERS", desc: "Configured campaign faction key for this role; must match the faction-role mapping")]
	string m_sFactionKey;
	[Attribute("US", desc: "Native source faction allowed during initial spawning only")]
	string m_sAssetFactionKey;
	[Attribute("{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et", UIWidgets.ResourceNamePicker, desc: "Infantry character used by this role's garrison teams", params: "et")]
	ResourceName m_sInfantry;
	[Attribute("", desc: "Optional fireteam specialists after the rifleman: automatic rifleman, grenadier and leader. Logical variant choices are saved; empty retains rifleman-only teams.")]
	ref array<ResourceName> m_aInfantryVariants;
	[Attribute("{0F6689B491641155}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Sniper.et", UIWidgets.ResourceNamePicker, desc: "Marksman posted alone on each guard tower (sentry post); empty uses the infantry character", params: "et")]
	ResourceName m_sMarksman;
	[Attribute("{E1CB513B8B9B08F4}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Crew.et", UIWidgets.ResourceNamePicker, desc: "Character used to crew static weapons and manned vehicles", params: "et")]
	ResourceName m_sCrew;
	[Attribute("{73530808E4B455BA}Prefabs/Weapons/Tripods/Tripod_M3_M2HB.et", UIWidgets.ResourceNamePicker, desc: "Static machine-gun prefab; requires one usable crew compartment", params: "et")]
	ResourceName m_sStaticMG;
	[Attribute("", desc: "Optional static weapon variants after the default MG. Fresh garrisons distribute their authored gun positions across this list; each weapon must have exactly one usable crew compartment.")]
	ref array<ResourceName> m_aStaticVariants;
	[Attribute("{0FBF8F010F81A4E5}Prefabs/Vehicles/Wheeled/LAV25/LAV25.et", UIWidgets.ResourceNamePicker, desc: "Unmanned armour parked inside larger military garrisons", params: "et")]
	ResourceName m_sParkedArmour;
	[Attribute("", desc: "Random manned defence choices. Every selected hull is fully crewed using native pilot/turret compartments.")]
	ref array<ResourceName> m_aMannedVehicles;
}
