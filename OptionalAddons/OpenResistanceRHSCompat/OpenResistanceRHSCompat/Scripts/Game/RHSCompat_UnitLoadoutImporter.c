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
	}

	//------------------------------------------------------------------------------------------------
	//! Characters to read kits from for types the FORECON preset does not list yet
	override protected void GetExtraUnitPrefabs(string presetId, notnull map<AROR_EUnitType, ResourceName> outPrefabs)
	{
		super.GetExtraUnitPrefabs(presetId, outPrefabs);

		if (presetId == "RHS_USAF_FORECON")
			outPrefabs.Set(AROR_EUnitType.GRENADIER, "{786D962E5A78F0D8}Prefabs/Characters/Factions/BLUFOR/RHS_USAF/RHS_USAF_FORECON/Character_RHS_USAF_FORECON_GL.et");
	}
}
#endif
