//! A set of content pack entries that need the same addons (see AROR_ContentPack). Entries of any category sit in one
//! list, each with its own type. The group's entries are offered only while every addon it names is loaded, so one pack
//! can cover a mod family: one group per sub-mod. A group with no addons is always available (e.g. the base game).
[BaseContainerProps(), BaseContainerCustomTitleField("m_sLabel")]
class AROR_ContentPackGroup
{
	[Attribute("", desc: "Name shown in the editor, e.g. the mod these entries come from")]
	protected string m_sLabel;

	[Attribute("", desc: "GUIDs of the addons these entries need (as in their addon.gproj). The entries are ignored, including what they replace, unless all are loaded. Empty: always available.")]
	protected ref array<string> m_aRequiredAddons;

	[Attribute(desc: "Vehicles, gear, equipment, weapons, weapon accessories and ammunition; each entry picks its own type")]
	protected ref array<ref AROR_ContentPackEntry> m_aEntries;

	//------------------------------------------------------------------------------------------------
	string GetLabel()
	{
		return m_sLabel;
	}

	//------------------------------------------------------------------------------------------------
	//! True when every addon this group needs is in loadedAddons
	bool HasRequiredAddons(notnull array<string> loadedAddons)
	{
		return AROR_ContentPack.AreAddonsLoaded(m_aRequiredAddons, loadedAddons);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every entry of the category with a known type; returns how many were added
	int GetEntries(AROR_EFactionPresetCategory category, notnull array<AROR_ContentPackEntry> outEntries)
	{
		if (!m_aEntries)
			return 0;

		int added;
		foreach (AROR_ContentPackEntry entry : m_aEntries)
		{
			if (entry && entry.GetCategory() == category && entry.IsKnownType())
			{
				outEntries.Insert(entry);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every entry, whatever its category or type
	int GetAllEntries(notnull array<AROR_ContentPackEntry> outEntries)
	{
		if (!m_aEntries)
			return 0;

		int added;
		foreach (AROR_ContentPackEntry entry : m_aEntries)
		{
			if (entry)
			{
				outEntries.Insert(entry);
				added++;
			}
		}

		return added;
	}
}
