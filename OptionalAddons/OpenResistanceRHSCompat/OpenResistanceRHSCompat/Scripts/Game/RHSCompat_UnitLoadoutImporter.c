#ifdef WORKBENCH
//! Lets the unit loadout import fill this addon's presets too, writing into the addon's own preset files
modded class AROR_UnitLoadoutImporter
{
	//------------------------------------------------------------------------------------------------
	override protected void GetExtraTargets(notnull array<string> presetIds, notnull array<string> folders, notnull array<string> files)
	{
		super.GetExtraTargets(presetIds, folders, files);

		presetIds.Insert("RHS_USAF_FORECON");
		folders.Insert("$OpenResistanceRHSCompat:Configs/FactionPresets/");
		files.Insert("AROR_Preset_RHS_USAF_FORECON.conf");

		presetIds.Insert("RHS_AFRF_MSV_VPKO3_EMR");
		folders.Insert("$OpenResistanceRHSCompat:Configs/FactionPresets/");
		files.Insert("AROR_Preset_RHS_AFRF_MSV_VPKO3_EMR.conf");
	}

	//------------------------------------------------------------------------------------------------
	//! Characters to read kits from for types the FORECON preset does not list yet
	override protected void GetExtraUnitPrefabs(string presetId, notnull map<AROR_EUnitType, ResourceName> outPrefabs)
	{
		super.GetExtraUnitPrefabs(presetId, outPrefabs);

		if (presetId == "RHS_USAF_FORECON")
			outPrefabs.Set(AROR_EUnitType.GRENADIER, "{786D962E5A78F0D8}Prefabs/Characters/Factions/BLUFOR/RHS_USAF/RHS_USAF_FORECON/Character_RHS_USAF_FORECON_GL.et");
	}

	//------------------------------------------------------------------------------------------------
	//! The numbered variants of the VPKO3 EMR characters, as extra kits of the unit picked for their type
	override protected void GetAlternateUnitPrefabs(string presetId, notnull array<AROR_EUnitType> outTypes, notnull array<ResourceName> outPrefabs)
	{
		super.GetAlternateUnitPrefabs(presetId, outTypes, outPrefabs);

		if (presetId != "RHS_AFRF_MSV_VPKO3_EMR")
			return;

		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.RIFLEMAN, "{35C3E1D9CA966CE9}", "Rifleman2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.RIFLEMAN, "{D05FAD20C673FB82}", "Rifleman3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.AUTORIFLEMAN, "{B63EC14C618F3671}", "AR2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.AUTORIFLEMAN, "{53A28DB56D6AA11A}", "AR3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.MACHINEGUNNER, "{9839621F07C21C97}", "MG2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.MACHINEGUNNER, "{7DA52EE60B278BFC}", "MG3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.GRENADIER, "{FAE5AB9F6DCB1F83}", "GL2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.GRENADIER, "{1F79E766612E88E8}", "GL3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.MEDIC, "{F556759CCA876610}", "Medic2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.MEDIC, "{10CA3965C662F17B}", "Medic3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.MARKSMAN, "{C1E5844AC2F8527C}", "Sharpshooter_2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.MARKSMAN, "{2479C8B3CE1DC517}", "Sharpshooter_3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.SNIPER, "{C1E5844AC2F8527C}", "Sharpshooter_2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.SNIPER, "{2479C8B3CE1DC517}", "Sharpshooter_3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.AT, "{C6F35395F3AB63C2}", "AT2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.AT, "{236F1F6CFF4EF4A9}", "AT3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.ENGINEER, "{4B582FCE388B5A2C}", "Sapper_2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.ENGINEER, "{AEC46337346ECD47}", "Sapper_3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.RADIO_OPERATOR, "{58517CDD56FCA85C}", "RTO_2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.RADIO_OPERATOR, "{BDCD30245A193F37}", "RTO_3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.TEAMLEADER, "{F5C0C7159CE28B1A}", "Sergeant_2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.TEAMLEADER, "{105C8BEC90071C71}", "Sergeant_3");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.SQUADLEADER, "{B11566B095AF0C72}", "SL2");
		AddMsvAlternate(outTypes, outPrefabs, AROR_EUnitType.SQUADLEADER, "{54892A49994A9B19}", "SL3");
	}

	//------------------------------------------------------------------------------------------------
	protected void AddMsvAlternate(notnull array<AROR_EUnitType> outTypes, notnull array<ResourceName> outPrefabs, AROR_EUnitType type, string guid, string suffix)
	{
		outTypes.Insert(type);
		outPrefabs.Insert(guid + "Prefabs/Characters/Factions/OPFOR/RHS_AFRF/MSV/VKPO3_EMR/Character_RHS_RF_MSV_VKPO_3.0_" + suffix + ".et");
	}
}
#endif
