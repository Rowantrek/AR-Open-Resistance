//! The list of content packs (see AROR_ContentPack). Packs are tried in this order: when two enabled packs list the
//! same prefab, or replace the same prefab, the earlier one wins. The core's own packs come first (the config list),
//! then packs registered by extender mods (GetExtraPackPaths), in the order their addons' scripts load.
//! Extender mods register packs from script, so any number of addons can each add any number of packs without
//! sharing a config file: `modded class AROR_ContentPackRegistry` overriding GetExtraPackPaths.
[BaseContainerProps(configRoot: true)]
class AROR_ContentPackRegistry
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Content pack configs", params: "conf class=AROR_ContentPack")]
	protected ref array<ResourceName> m_aPacks;

	//------------------------------------------------------------------------------------------------
	//! Extension point for extender mods: override, call super, then Insert each pack config path into outPacks
	//! (e.g. outPacks.Insert("{GUID}Configs/ContentPacks/MyPack.conf")). An addon can add several packs.
	void GetExtraPackPaths(notnull array<ResourceName> outPacks)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the pack config paths to outPacks, each once: the config list, then the packs extender mods registered
	void GetPackPaths(notnull array<ResourceName> outPacks)
	{
		array<ResourceName> paths = {};
		if (m_aPacks)
			paths.InsertAll(m_aPacks);

		GetExtraPackPaths(paths);

		foreach (ResourceName path : paths)
		{
			if (!path.IsEmpty() && !outPacks.Contains(path))
				outPacks.Insert(path);
		}
	}
}
