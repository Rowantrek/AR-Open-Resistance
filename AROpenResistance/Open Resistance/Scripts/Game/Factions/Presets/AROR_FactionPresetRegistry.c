//! The list of faction presets available to campaigns (see AROR_FactionPreset). The core's own presets are the config
//! list; presets registered by extender mods (GetExtraPresetPaths) follow, in the order their addons' scripts load.
//! Extender mods register presets from script, so any number of addons can each add any number of presets without
//! sharing a config file: `modded class AROR_FactionPresetRegistry` overriding GetExtraPresetPaths.
[BaseContainerProps(configRoot: true)]
class AROR_FactionPresetRegistry
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Faction preset configs", params: "conf class=AROR_FactionPreset")]
	protected ref array<ResourceName> m_aPresets;

	//------------------------------------------------------------------------------------------------
	//! Extension point for extender mods: override, call super, then Insert each preset config path into outPresets
	//! (e.g. outPresets.Insert("{GUID}Configs/FactionPresets/MyPreset.conf")). An addon can add several presets.
	void GetExtraPresetPaths(notnull array<ResourceName> outPresets)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Extension point for extender mods: override, call super, then Insert each addition config path into outAdditions
	//! (e.g. outAdditions.Insert("{GUID}Configs/FactionPresets/MyAddition.conf")). An addition (AROR_FactionPresetAddition)
	//! adds units and vehicles to a preset that another addon owns; any number of addons can add to the same preset.
	void GetExtraAdditionPaths(notnull array<ResourceName> outAdditions)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the addition config paths to outAdditions, each once, in the order extender mods registered them
	void GetAdditionPaths(notnull array<ResourceName> outAdditions)
	{
		array<ResourceName> paths = {};
		GetExtraAdditionPaths(paths);

		foreach (ResourceName path : paths)
		{
			if (!path.IsEmpty() && !outAdditions.Contains(path))
				outAdditions.Insert(path);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the preset config paths to outPresets, each once: the config list, then the presets extender mods registered
	void GetPresetPaths(notnull array<ResourceName> outPresets)
	{
		array<ResourceName> paths = {};
		if (m_aPresets)
			paths.InsertAll(m_aPresets);

		GetExtraPresetPaths(paths);

		foreach (ResourceName path : paths)
		{
			if (!path.IsEmpty() && !outPresets.Contains(path))
				outPresets.Insert(path);
		}
	}
}
