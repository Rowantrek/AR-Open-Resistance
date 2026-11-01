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
	[Attribute("0", desc: "Append the role's faction preset static MGs, armed cars and APCs to the static and manned choices (after the authored ones, so saved variants keep their meaning). If the role uses another army's preset, the authored assets are dropped and soldiers and everything else come from the preset.")]
	bool m_bUseFactionPreset;

	//! Set by preset resolution: parked armour choices, the authored armour first, then the preset's APCs and IFVs.
	//! Null on an unresolved binding, which parks m_sParkedArmour only.
	ref array<ResourceName> m_aParkedArmourChoices;

	//! The binding to use with this preset: itself when presets are off or the preset is missing, else a resolved copy
	AROR_GarrisonFaction ResolveWithPreset(AROR_FactionPreset preset)
	{
		if (!m_bUseFactionPreset || !preset)
			return this;

		AROR_GarrisonFaction resolved = new AROR_GarrisonFaction();
		resolved.m_eRole = m_eRole;
		resolved.m_sFactionKey = m_sFactionKey;

		// Authored assets are native to m_sAssetFactionKey; another army's preset replaces them all
		string presetAssets = preset.GetCatalogFactionKey();
		bool sameArmy = presetAssets.IsEmpty() || presetAssets == m_sAssetFactionKey;
		resolved.m_sAssetFactionKey = m_sAssetFactionKey;
		resolved.m_aInfantryVariants = {};
		resolved.m_aStaticVariants = {};
		resolved.m_aMannedVehicles = {};
		if (sameArmy)
		{
			resolved.m_sStaticMG = m_sStaticMG;
			resolved.m_sParkedArmour = m_sParkedArmour;
			if (m_aStaticVariants)
				resolved.m_aStaticVariants.Copy(m_aStaticVariants);
			if (m_aMannedVehicles)
				resolved.m_aMannedVehicles.Copy(m_aMannedVehicles);
		}
		else
		{
			resolved.m_sAssetFactionKey = presetAssets;
		}

		resolved.m_sInfantry = PresetUnit(preset, AROR_EUnitType.RIFLEMAN, m_sInfantry, sameArmy);
		// Specialists keep their authored order: automatic rifleman, grenadier, leader
		array<AROR_EUnitType> specialists = {AROR_EUnitType.AUTORIFLEMAN, AROR_EUnitType.GRENADIER, AROR_EUnitType.LEADER};
		foreach (int i, AROR_EUnitType specialist : specialists)
		{
			ResourceName authored;
			if (m_aInfantryVariants && i < m_aInfantryVariants.Count())
				authored = m_aInfantryVariants[i];
			resolved.m_aInfantryVariants.Insert(PresetUnit(preset, specialist, authored, sameArmy));
		}
		// AT and AA specialists (variants 4 and 5) take a team's rifleman place; a preset without AA soldiers uses AT
		ResourceName antiTank = PresetUnit(preset, AROR_EUnitType.AT, string.Empty, sameArmy);
		ResourceName antiAir = antiTank;
		if (preset.GetUnit(AROR_EUnitType.AA))
			antiAir = PresetUnit(preset, AROR_EUnitType.AA, string.Empty, sameArmy);
		resolved.m_aInfantryVariants.Insert(antiTank);
		resolved.m_aInfantryVariants.Insert(antiAir);
		resolved.m_sMarksman = PresetUnit(preset, AROR_EUnitType.MARKSMAN, PresetUnit(preset, AROR_EUnitType.SNIPER, m_sMarksman, sameArmy), sameArmy);
		resolved.m_sCrew = PresetUnit(preset, AROR_EUnitType.CREW, m_sCrew, sameArmy);

		array<ResourceName> statics = {};
		PresetVehicles(preset, AROR_EVehicleType.STATIC_MG_LOW, statics);
		PresetVehicles(preset, AROR_EVehicleType.STATIC_MG_HIGH, statics);
		foreach (ResourceName weapon : statics)
		{
			if (resolved.m_sStaticMG.IsEmpty())
				resolved.m_sStaticMG = weapon;
			else if (weapon != resolved.m_sStaticMG && !resolved.m_aStaticVariants.Contains(weapon))
				resolved.m_aStaticVariants.Insert(weapon);
		}

		array<ResourceName> hulls = {};
		PresetVehicles(preset, AROR_EVehicleType.ARMED_CAR, hulls);
		PresetVehicles(preset, AROR_EVehicleType.ARMED_TRUCK, hulls);
		PresetVehicles(preset, AROR_EVehicleType.APC, hulls);
		PresetVehicles(preset, AROR_EVehicleType.IFV, hulls);
		foreach (ResourceName hull : hulls)
		{
			if (!resolved.m_aMannedVehicles.Contains(hull))
				resolved.m_aMannedVehicles.Insert(hull);
		}

		// Parked armour is picked per garrison from these, so content mods (e.g. WCS) vary it too
		resolved.m_aParkedArmourChoices = {};
		if (!resolved.m_sParkedArmour.IsEmpty())
			resolved.m_aParkedArmourChoices.Insert(resolved.m_sParkedArmour);
		array<ResourceName> armour = {};
		PresetVehicles(preset, AROR_EVehicleType.APC, armour);
		PresetVehicles(preset, AROR_EVehicleType.IFV, armour);
		foreach (ResourceName parked : armour)
		{
			if (!resolved.m_aParkedArmourChoices.Contains(parked))
				resolved.m_aParkedArmourChoices.Insert(parked);
		}
		if (resolved.m_sParkedArmour.IsEmpty() && !resolved.m_aParkedArmourChoices.IsEmpty())
			resolved.m_sParkedArmour = resolved.m_aParkedArmourChoices[0];

		return resolved;
	}

	//! Same army: the authored unit (deliberate picks), the preset's when none is authored. Another army, or a preset
	//! that dresses its own infantry (its soldiers are the role defaults dressed by Readiness): the preset's unit of the
	//! type, falling back to its rifleman.
	protected ResourceName PresetUnit(AROR_FactionPreset preset, AROR_EUnitType type, ResourceName authored, bool sameArmy)
	{
		bool keepAuthored = sameArmy && !preset.DressesInfantry();
		if (keepAuthored && !authored.IsEmpty())
			return authored;
		ResourceName character = UnitCharacter(preset, type);
		if (!character.IsEmpty())
			return character;
		if (keepAuthored)
			return authored;
		return UnitCharacter(preset, AROR_EUnitType.RIFLEMAN);
	}

	//! The preset unit's character for this binding's role: its own, or the role default when it names none
	protected ResourceName UnitCharacter(AROR_FactionPreset preset, AROR_EUnitType type)
	{
		AROR_FactionPresetUnit unit = preset.GetUnit(type);
		if (!unit)
			return string.Empty;
		if (unit.HasPrefab())
			return unit.GetPrefab();
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return string.Empty;
		return presets.GetDefaultUnitPrefab(m_eRole, type);
	}

	//! Appends the preset's vehicles of the type, in preset order (stable while the loaded mods stay the same)
	protected static void PresetVehicles(AROR_FactionPreset preset, AROR_EVehicleType type, notnull array<ResourceName> outPrefabs)
	{
		array<AROR_FactionPresetEntry> entries = {};
		preset.GetEntries(AROR_EFactionPresetCategory.VEHICLES, type, entries);
		foreach (AROR_FactionPresetEntry entry : entries)
		{
			if (!entry.GetPrefab().IsEmpty())
				outPrefabs.Insert(entry.GetPrefab());
		}
	}
}
