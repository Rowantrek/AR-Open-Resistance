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
	//! The choices as overrides of the packs' defaults, as AROR_ContentPacksComponent stores them
	void GetPackOverrides(notnull array<string> outDisabledIds, notnull array<string> outEnabledIds)
	{
		AROR_ContentPacksComponent contentPacks = AROR_ContentPacksComponent.GetInstance();
		if (!contentPacks)
			return;

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
