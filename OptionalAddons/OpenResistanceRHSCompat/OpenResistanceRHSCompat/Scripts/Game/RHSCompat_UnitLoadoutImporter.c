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
}
#endif
