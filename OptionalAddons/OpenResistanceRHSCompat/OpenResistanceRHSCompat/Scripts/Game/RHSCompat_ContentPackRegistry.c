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

		outPacks.Insert("{AFD01836405F0CF2}Configs/ContentPacks/AROR_ContentPack_RHS.conf");
	}
}
