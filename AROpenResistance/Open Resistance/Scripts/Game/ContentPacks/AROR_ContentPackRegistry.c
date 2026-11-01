//! The list of content packs (see AROR_ContentPack). Packs are tried in this order: when two enabled packs list the
//! same prefab, or replace the same prefab, the earlier one wins.
//! Extender mods add their packs to this list with Override in addon, which stores only their additions.
//! TODO: verify with two addons that Override in addon merges additions into this plain array rather than replacing
//! it (same open question as AROR_FactionPresetRegistry).
[BaseContainerProps(configRoot: true)]
class AROR_ContentPackRegistry
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Content pack configs", params: "conf class=AROR_ContentPack")]
	protected ref array<ResourceName> m_aPacks;

	//------------------------------------------------------------------------------------------------
	//! Adds the pack config paths to outPacks
	void GetPackPaths(notnull array<ResourceName> outPacks)
	{
		if (m_aPacks)
			outPacks.InsertAll(m_aPacks);
	}
}
