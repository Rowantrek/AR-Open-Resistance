//! A content pack (see ARCHITECTURE.md -> Content Packs): the items and vehicles one content source (the base game, a
//! mod such as RHS) offers, each with a price, a minimum readiness and import requirements, in the same fixed
//! categories as faction presets (no units). Entries sit in groups, one per set of addons they need (usually one
//! group per mod), so a pack can cover a mod family and only the entries of loaded mods are offered. A pack can also
//! need addons itself: it is then only available while all of them are loaded. The admin enables or disables
//! available packs per campaign. One `.conf` per pack, listed in AROR_ContentPackRegistry.
[BaseContainerProps(configRoot: true)]
class AROR_ContentPack
{
	[Attribute("", desc: "Unique ID, e.g. BASE_GAME. Saved with campaigns: never change it once released.")]
	protected string m_sId;

	[Attribute("", desc: "Name shown to the admin when enabling packs")]
	protected string m_sName;

	[Attribute("", desc: "GUIDs of the addons this whole pack needs (as in their addon.gproj). Empty: always available. To cover a mod family, leave this empty and put each mod's GUID on its own group.")]
	protected ref array<string> m_aRequiredAddons;

	[Attribute("1", desc: "Enabled when available, unless the admin turned it off in this campaign")]
	protected bool m_bEnabledByDefault;

	[Attribute(desc: "Entries grouped by the addons they need")]
	protected ref array<ref AROR_ContentPackGroup> m_aGroups;

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	bool IsEnabledByDefault()
	{
		return m_bEnabledByDefault;
	}

	//------------------------------------------------------------------------------------------------
	//! True when every required addon is in loadedAddons (GUIDs, compared case-insensitively). Groups have their own
	//! requirements on top (AROR_ContentPackGroup), so one pack can cover many optional mods.
	bool HasRequiredAddons(notnull array<string> loadedAddons)
	{
		return AreAddonsLoaded(m_aRequiredAddons, loadedAddons);
	}

	//------------------------------------------------------------------------------------------------
	//! True when every GUID in required (null or empty: none) is in loadedAddons, compared case-insensitively
	static bool AreAddonsLoaded(array<string> requiredAddons, notnull array<string> loadedAddons)
	{
		if (!requiredAddons)
			return true;

		foreach (string required : requiredAddons)
		{
			if (required.IsEmpty())
				continue;

			string wanted = required;
			wanted.ToUpper();
			bool found;
			foreach (string loaded : loadedAddons)
			{
				string guid = loaded;
				guid.ToUpper();
				if (guid == wanted)
				{
					found = true;
					break;
				}
			}

			if (!found)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	int GetGroupCount()
	{
		if (!m_aGroups)
			return 0;

		return m_aGroups.Count();
	}

	//------------------------------------------------------------------------------------------------
	AROR_ContentPackGroup GetGroup(int index)
	{
		if (!m_aGroups || !m_aGroups.IsIndexValid(index))
			return null;

		return m_aGroups[index];
	}

	//------------------------------------------------------------------------------------------------
	//! The group's ID within the pack, used to save which groups the admin turned off: its label (without the
	//! characters the saved lists use as separators), or its position if it has none. Renaming a group or reordering
	//! unlabelled ones resets the admin's choice for them.
	string GetGroupId(int index)
	{
		AROR_ContentPackGroup group = GetGroup(index);
		string id;
		if (group)
			id = group.GetLabel();

		if (id.IsEmpty())
			return "#" + index;

		id.Replace(",", "_");
		id.Replace(":", "_");
		return id;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every entry of the category with a known type, whatever its addons; returns how many were added
	int GetAllEntries(AROR_EFactionPresetCategory category, notnull array<AROR_ContentPackEntry> outEntries)
	{
		if (!m_aGroups)
			return 0;

		int added;
		foreach (AROR_ContentPackGroup group : m_aGroups)
		{
			if (group)
				added += group.GetEntries(category, outEntries);
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the entries of the category with a known type whose group has all its addons loaded. Entries of groups with
	//! a missing addon are not added but counted in skipped (which is added to, not reset). Returns how many were added.
	int GetAvailableEntries(AROR_EFactionPresetCategory category, notnull array<string> loadedAddons, notnull array<AROR_ContentPackEntry> outEntries, inout int skipped)
	{
		if (!m_aGroups)
			return 0;

		int added;
		foreach (AROR_ContentPackGroup group : m_aGroups)
		{
			if (!group)
				continue;

			if (group.HasRequiredAddons(loadedAddons))
			{
				added += group.GetEntries(category, outEntries);
			}
			else
			{
				array<AROR_ContentPackEntry> unavailable = {};
				skipped += group.GetEntries(category, unavailable);
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs configuration mistakes: missing ID, unknown types, empty prefabs. Returns false if the pack is unusable.
	bool Validate(string source)
	{
		if (m_sId.IsEmpty())
		{
			Print(string.Format("[AROR] Content pack '%1' has no ID and is ignored", source), LogLevel.ERROR);
			return false;
		}

		if (!m_aGroups)
			return true;

		int empty;
		foreach (AROR_ContentPackGroup group : m_aGroups)
		{
			if (!group)
				continue;

			array<AROR_ContentPackEntry> entries = {};
			group.GetAllEntries(entries);
			foreach (AROR_ContentPackEntry entry : entries)
			{
				if (entry.GetPrefab().IsEmpty())
					empty++;

				if (!entry.IsKnownType())
					Print(string.Format("[AROR] Content pack '%1' has a %2 entry with an unknown type (%3); it is ignored", m_sId, typename.EnumToString(AROR_EFactionPresetCategory, entry.GetCategory()), entry.GetTypeValue()), LogLevel.WARNING);
			}
		}

		if (empty > 0)
			Print(string.Format("[AROR] Content pack '%1' has %2 entries without a prefab", m_sId, empty), LogLevel.WARNING);

		return true;
	}
}
