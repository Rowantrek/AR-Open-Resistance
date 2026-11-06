//! The admin's choices for a new campaign while the setup screens are open (client only, owned by
//! AROR_SetupUIComponent so they survive the menu closing for HQ placement): an army (faction preset) per role, the
//! content packs on or off, campaign setting values and the resistance name. Sent to the server in one request once
//! the HQ is placed.
class AROR_SetupDraft
{
	//! Faction preset ID per role, indexed by AROR_EFactionRole
	protected ref array<string> m_aPresetIds = {};
	//! Available pack ID -> on in this campaign
	protected ref map<string, bool> m_mPackEnabled = new map<string, bool>();
	static const int STATE_OFF = 0;
	static const int STATE_PARTIAL = 1;
	static const int STATE_ON = 2;

	//! Group or section key (AROR_ContentPacksComponent.GetGroupKey / GetSectionKey) -> on in this campaign; missing means on
	protected ref map<string, bool> m_mSectionEnabled = new map<string, bool>();
	//! Setting ID -> value
	protected ref map<string, string> m_mSettingValues = new map<string, string>();
	protected string m_sName;

	//------------------------------------------------------------------------------------------------
	//! Starts over from the defaults: each role's default army, each available pack on its default, each setting on
	//! its default, the default resistance name
	void Reset()
	{
		m_aPresetIds.Clear();
		m_mPackEnabled.Clear();
		m_mSectionEnabled.Clear();
		m_mSettingValues.Clear();
		m_sName = "Resistance";
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (highCommand && !highCommand.GetDefaultResistanceName().IsEmpty())
			m_sName = highCommand.GetDefaultResistanceName();

		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		array<AROR_EFactionRole> roles = {};
		AROR_FactionRoles.GetAll(roles);
		foreach (AROR_EFactionRole role : roles)
		{
			if (presets)
				SetPresetId(role, presets.GetDefaultPresetId(role));
		}

		array<AROR_ContentPack> packs = {};
		GetAvailablePacks(packs);
		foreach (AROR_ContentPack pack : packs)
		{
			m_mPackEnabled.Set(pack.GetId(), pack.IsEnabledByDefault());
		}

		AROR_CampaignSettingsComponent settings = AROR_CampaignSettingsComponent.GetInstance();
		if (settings)
		{
			array<AROR_CampaignSetting> allSettings = {};
			settings.GetAllSettings(allSettings);
			foreach (AROR_CampaignSetting setting : allSettings)
			{
				m_mSettingValues.Set(setting.GetId(), setting.GetDefaultValue());
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The content packs the admin can choose from: loaded and with every addon they need (packs for missing mods
	//! are not shown)
	static int GetAvailablePacks(notnull array<AROR_ContentPack> outPacks)
	{
		AROR_ContentPacksComponent contentPacks = AROR_ContentPacksComponent.GetInstance();
		if (!contentPacks)
			return 0;

		array<AROR_ContentPack> packs = {};
		contentPacks.GetPacks(packs);
		int added;
		foreach (AROR_ContentPack pack : packs)
		{
			if (!contentPacks.IsAvailable(pack.GetId()))
				continue;

			outPacks.Insert(pack);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	// Armies
	//------------------------------------------------------------------------------------------------
	string GetPresetId(AROR_EFactionRole role)
	{
		if (m_aPresetIds.IsIndexValid(role))
			return m_aPresetIds[role];

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	void SetPresetId(AROR_EFactionRole role, string presetId)
	{
		while (m_aPresetIds.Count() <= role)
		{
			m_aPresetIds.Insert(string.Empty);
		}

		m_aPresetIds[role] = presetId;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetPresetIds()
	{
		return m_aPresetIds;
	}

	//------------------------------------------------------------------------------------------------
	// Content packs
	//------------------------------------------------------------------------------------------------
	bool IsPackEnabled(string packId)
	{
		return m_mPackEnabled.Get(packId);
	}

	//------------------------------------------------------------------------------------------------
	void SetPackEnabled(string packId, bool enabled)
	{
		m_mPackEnabled.Set(packId, enabled);
	}

	//------------------------------------------------------------------------------------------------
	int CountEnabledPacks()
	{
		int count;
		foreach (string packId, bool enabled : m_mPackEnabled)
		{
			if (enabled)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! A pack's groups, and the categories within them, are on unless turned off here. A group whose addons are
	//! missing is always off.
	bool IsGroupEnabled(notnull AROR_ContentPack pack, int groupIndex)
	{
		if (!IsGroupAvailable(pack, groupIndex))
			return false;

		return IsKeyEnabled(AROR_ContentPacksComponent.GetGroupKey(pack.GetId(), pack.GetGroupId(groupIndex)));
	}

	//------------------------------------------------------------------------------------------------
	bool IsSectionEnabled(notnull AROR_ContentPack pack, int groupIndex, AROR_EFactionPresetCategory category)
	{
		return IsKeyEnabled(AROR_ContentPacksComponent.GetSectionKey(pack.GetId(), pack.GetGroupId(groupIndex), category));
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsKeyEnabled(string key)
	{
		bool enabled;
		if (m_mSectionEnabled.Find(key, enabled))
			return enabled;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the group has all its addons loaded; otherwise it cannot be turned on
	static bool IsGroupAvailable(notnull AROR_ContentPack pack, int groupIndex)
	{
		AROR_ContentPacksComponent contentPacks = AROR_ContentPacksComponent.GetInstance();
		return contentPacks && contentPacks.IsGroupAvailable(pack, groupIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the group has entries of the category (only those are listed and can be turned off)
	static bool HasSection(notnull AROR_ContentPack pack, int groupIndex, AROR_EFactionPresetCategory category)
	{
		AROR_ContentPackGroup group = pack.GetGroup(groupIndex);
		if (!group)
			return false;

		array<AROR_ContentPackEntry> entries = {};
		return group.GetEntries(category, entries) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! STATE_ON when the group and all its categories are on, STATE_OFF when it or all of them are off (always for a
	//! group with a missing addon), otherwise STATE_PARTIAL
	int GetGroupState(notnull AROR_ContentPack pack, int groupIndex)
	{
		if (!IsGroupEnabled(pack, groupIndex))
			return STATE_OFF;

		int total;
		int on;
		for (int category = AROR_EFactionPresetCategory.VEHICLES; category <= AROR_EFactionPresetCategory.AMMUNITION; category++)
		{
			if (!HasSection(pack, groupIndex, category))
				continue;

			total++;
			if (IsSectionEnabled(pack, groupIndex, category))
				on++;
		}

		if (on == total)
			return STATE_ON;

		if (on == 0)
			return STATE_OFF;

		return STATE_PARTIAL;
	}

	//------------------------------------------------------------------------------------------------
	//! STATE_OFF when the pack is off, STATE_ON when all its groups are fully on, STATE_OFF when they are all off,
	//! otherwise STATE_PARTIAL (including groups left off because their addons are missing)
	int GetPackState(notnull AROR_ContentPack pack)
	{
		if (!IsPackEnabled(pack.GetId()))
			return STATE_OFF;

		int total;
		int on;
		int off;
		for (int g = 0; g < pack.GetGroupCount(); g++)
		{
			if (!HasAnyEntries(pack, g))
				continue;

			total++;
			int state = GetGroupState(pack, g);
			if (state == STATE_ON)
				on++;
			else if (state == STATE_OFF)
				off++;
		}

		if (on == total)
			return STATE_ON;

		if (off == total)
			return STATE_OFF;

		return STATE_PARTIAL;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the group has any entries to list
	static bool HasAnyEntries(notnull AROR_ContentPack pack, int groupIndex)
	{
		AROR_ContentPackGroup group = pack.GetGroup(groupIndex);
		if (!group)
			return false;

		array<AROR_ContentPackEntry> entries = {};
		return group.GetAllEntries(entries) > 0;
	}

	//------------------------------------------------------------------------------------------------
	void SetSectionEnabled(notnull AROR_ContentPack pack, int groupIndex, AROR_EFactionPresetCategory category, bool enabled)
	{
		m_mSectionEnabled.Set(AROR_ContentPacksComponent.GetSectionKey(pack.GetId(), pack.GetGroupId(groupIndex), category), enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! Click on a group: from fully on it turns off; otherwise it comes back fully on (its own switch first, then any
	//! turned-off categories). Does nothing for a group whose addons are missing.
	void ToggleGroup(notnull AROR_ContentPack pack, int groupIndex)
	{
		if (!IsGroupAvailable(pack, groupIndex))
			return;

		string groupKey = AROR_ContentPacksComponent.GetGroupKey(pack.GetId(), pack.GetGroupId(groupIndex));
		int state = GetGroupState(pack, groupIndex);
		if (state == STATE_ON)
		{
			m_mSectionEnabled.Set(groupKey, false);
		}
		else if (!IsKeyEnabled(groupKey))
		{
			m_mSectionEnabled.Set(groupKey, true);
		}
		else
		{
			for (int category = AROR_EFactionPresetCategory.VEHICLES; category <= AROR_EFactionPresetCategory.AMMUNITION; category++)
			{
				SetSectionEnabled(pack, groupIndex, category, true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Click on a pack: from fully on it turns off; from off it turns on keeping its group choices; when on but
	//! mixed or with everything off it resets every available group to fully on
	void TogglePack(notnull AROR_ContentPack pack)
	{
		string packId = pack.GetId();
		int state = GetPackState(pack);
		if (state == STATE_ON)
		{
			SetPackEnabled(packId, false);
			return;
		}

		if (!IsPackEnabled(packId))
		{
			SetPackEnabled(packId, true);
			return;
		}

		for (int g = 0; g < pack.GetGroupCount(); g++)
		{
			if (!IsGroupAvailable(pack, g))
				continue;

			m_mSectionEnabled.Set(AROR_ContentPacksComponent.GetGroupKey(packId, pack.GetGroupId(g)), true);
			for (int category = AROR_EFactionPresetCategory.VEHICLES; category <= AROR_EFactionPresetCategory.AMMUNITION; category++)
			{
				SetSectionEnabled(pack, g, category, true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when any group or category of an enabled pack has been turned off by the admin (not counting groups left
	//! off for missing addons)
	bool HasPartialChoices()
	{
		foreach (string key, bool enabled : m_mSectionEnabled)
		{
			if (!enabled)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The choices as overrides of the packs' defaults, as AROR_ContentPacksComponent stores them (turned-off sections
	//! go in the disabled list)
	void GetPackOverrides(notnull array<string> outDisabledIds, notnull array<string> outEnabledIds)
	{
		AROR_ContentPacksComponent contentPacks = AROR_ContentPacksComponent.GetInstance();
		if (!contentPacks)
			return;

		foreach (string sectionKey, bool sectionEnabled : m_mSectionEnabled)
		{
			if (!sectionEnabled)
				outDisabledIds.Insert(sectionKey);
		}

		foreach (string packId, bool enabled : m_mPackEnabled)
		{
			AROR_ContentPack pack = contentPacks.FindPack(packId);
			if (!pack || pack.IsEnabledByDefault() == enabled)
				continue;

			if (enabled)
				outEnabledIds.Insert(packId);
			else
				outDisabledIds.Insert(packId);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Campaign settings
	//------------------------------------------------------------------------------------------------
	string GetSettingValue(notnull AROR_CampaignSetting setting)
	{
		string value;
		if (m_mSettingValues.Find(setting.GetId(), value))
			return setting.Sanitize(value);

		return setting.GetDefaultValue();
	}

	//------------------------------------------------------------------------------------------------
	void SetSettingValue(notnull AROR_CampaignSetting setting, string value)
	{
		m_mSettingValues.Set(setting.GetId(), setting.Sanitize(value));
	}

	//------------------------------------------------------------------------------------------------
	void GetSettingValues(notnull array<string> outSettingIds, notnull array<string> outValues)
	{
		foreach (string settingId, string value : m_mSettingValues)
		{
			outSettingIds.Insert(settingId);
			outValues.Insert(value);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Name
	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	void SetName(string name)
	{
		m_sName = name;
	}
}
