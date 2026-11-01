//! Registers this addon's content packs with Open Resistance (scripts of all addons combine, so this does not clash
//! with other compat addons the way overriding the registry config would)
//! (BaseContainerProps is repeated because a modded class does not keep the original's, and the registry is a config)
[BaseContainerProps(configRoot: true)]
modded class AROR_ContentPackRegistry
{
	//------------------------------------------------------------------------------------------------
	override void GetExtraPackPaths(notnull array<ResourceName> outPacks)
	{
		super.GetExtraPackPaths(outPacks);

		outPacks.Insert("{C24A4FD0486704D7}Configs/ContentPacks/WCS/AROR_ContentPack_WCS.conf");
	}
}
