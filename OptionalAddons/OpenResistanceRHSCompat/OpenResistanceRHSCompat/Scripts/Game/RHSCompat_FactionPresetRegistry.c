//! Registers this addon's faction presets with Open Resistance (scripts of all addons combine, so this does not clash
//! with other compat addons the way overriding the registry config would)
//! (BaseContainerProps is repeated because a modded class does not keep the original's, and the registry is a config)
[BaseContainerProps(configRoot: true)]
modded class AROR_FactionPresetRegistry
{
	//------------------------------------------------------------------------------------------------
	override void GetExtraPresetPaths(notnull array<ResourceName> outPresets)
	{
		super.GetExtraPresetPaths(outPresets);

		outPresets.Insert("{0B9D15CDFDB3EDDF}Configs/FactionPresets/AROR_Preset_RHS_USAF_FORECON.conf");
	}
}
