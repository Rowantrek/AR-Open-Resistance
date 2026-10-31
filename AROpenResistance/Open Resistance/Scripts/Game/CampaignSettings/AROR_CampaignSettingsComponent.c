[ComponentEditorProps(category: "OpenResistance/GameFlow", description: "Campaign settings: the categories and settings chosen at campaign setup, and this campaign's values. Attach to the game mode entity.")]
class AROR_CampaignSettingsComponentClass : SCR_BaseGameModeComponentClass
{
}

void AROR_OnCampaignSettingsChanged();
typedef func AROR_OnCampaignSettingsChanged;

//! Campaign settings (see ARCHITECTURE.md -> Game Flow -> Campaign Settings). The categories and their settings are
//! defined here on the game mode (extenders add theirs through prefab inheritance); the setup screen draws them on its
//! Settings step. The server owns the values: set when a campaign is created, replicated to every machine and saved
//! (AROR_CampaignSettingsComponentSerializer). Values are strings keyed by setting ID; a setting without a stored
//! value uses its default, and saved values for settings that are no longer defined are kept for later saves.
class AROR_CampaignSettingsComponent : SCR_BaseGameModeComponent
{
	[Attribute(desc: "Setting categories, in display order")]
	protected ref array<ref AROR_CampaignSettingCategory> m_aCategories;

	protected static const string ROW_SEPARATOR = "\n";
	protected static const string VALUE_SEPARATOR = "=";

	protected static AROR_CampaignSettingsComponent s_Instance;

	//! This campaign's values, rows of "id=value" joined by ROW_SEPARATOR (server-owned, replicated)
	[RplProp(onRplName: "OnValuesReplicated")]
	protected string m_sValues;

	//! Every machine: parsed m_sValues
	protected ref map<string, string> m_mValues = new map<string, string>();

	//! Every defined setting by ID, built on first lookup (FindSetting)
	protected ref map<string, AROR_CampaignSetting> m_mSettingsById;

	protected ref ScriptInvokerBase<AROR_OnCampaignSettingsChanged> m_OnSettingsChanged = new ScriptInvokerBase<AROR_OnCampaignSettingsChanged>();

	//------------------------------------------------------------------------------------------------
	static AROR_CampaignSettingsComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine; fires when the values change (campaign created, a save restored, replicated)
	ScriptInvokerBase<AROR_OnCampaignSettingsChanged> GetOnSettingsChanged()
	{
		return m_OnSettingsChanged;
	}

	//------------------------------------------------------------------------------------------------
	// Definitions
	//------------------------------------------------------------------------------------------------
	//! Adds every category in display order; returns how many were added
	int GetCategories(notnull array<AROR_CampaignSettingCategory> outCategories)
	{
		if (!m_aCategories)
			return 0;

		int added;
		foreach (AROR_CampaignSettingCategory category : m_aCategories)
		{
			if (!category)
				continue;

			outCategories.Insert(category);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every defined setting, category by category; returns how many were added
	int GetAllSettings(notnull array<AROR_CampaignSetting> outSettings)
	{
		array<AROR_CampaignSettingCategory> categories = {};
		GetCategories(categories);

		int added;
		foreach (AROR_CampaignSettingCategory category : categories)
		{
			added += category.GetSettings(outSettings);
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Settings are defined on the component and do not change at runtime, so the lookup is built once
	AROR_CampaignSetting FindSetting(string settingId)
	{
		if (!m_mSettingsById)
		{
			m_mSettingsById = new map<string, AROR_CampaignSetting>();
			array<AROR_CampaignSetting> settings = {};
			GetAllSettings(settings);
			foreach (AROR_CampaignSetting setting : settings)
			{
				if (setting && !m_mSettingsById.Contains(setting.GetId()))
					m_mSettingsById.Insert(setting.GetId(), setting);
			}
		}

		return m_mSettingsById.Get(settingId);
	}

	//------------------------------------------------------------------------------------------------
	// Values (every machine)
	//------------------------------------------------------------------------------------------------
	//! This campaign's value for the setting (its default if none is stored); empty for an unknown setting
	string GetValue(string settingId)
	{
		AROR_CampaignSetting setting = FindSetting(settingId);
		if (!setting)
			return string.Empty;

		string value;
		if (m_mValues.Find(settingId, value))
			return setting.Sanitize(value);

		return setting.GetDefaultValue();
	}

	//------------------------------------------------------------------------------------------------
	bool GetBool(string settingId)
	{
		return GetValue(settingId) == "1";
	}

	//------------------------------------------------------------------------------------------------
	int GetInt(string settingId)
	{
		return GetValue(settingId).ToInt();
	}

	//------------------------------------------------------------------------------------------------
	// Server
	//------------------------------------------------------------------------------------------------
	//! Server only. A new campaign's values from the setup screen: every defined setting gets the given value if valid,
	//! otherwise its default. IDs of settings that are not defined are ignored.
	void SetValues_S(array<string> settingIds, array<string> values)
	{
		if (!Replication.IsServer())
			return;

		map<string, string> given = ToMap(settingIds, values);
		map<string, string> result = new map<string, string>();
		array<AROR_CampaignSetting> settings = {};
		GetAllSettings(settings);
		foreach (AROR_CampaignSetting setting : settings)
		{
			result.Set(setting.GetId(), setting.Sanitize(given.Get(setting.GetId())));
		}

		Apply_S(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Values restored from a save: as SetValues_S, but values for settings that are no longer defined are
	//! kept, so a later save still holds them if the setting comes back
	void RestoreValues_S(array<string> settingIds, array<string> values)
	{
		if (!Replication.IsServer())
			return;

		map<string, string> result = ToMap(settingIds, values);
		array<AROR_CampaignSetting> settings = {};
		GetAllSettings(settings);
		foreach (AROR_CampaignSetting setting : settings)
		{
			result.Set(setting.GetId(), setting.Sanitize(result.Get(setting.GetId())));
		}

		Apply_S(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every stored value (for the serializer)
	void GetValues(notnull array<string> outSettingIds, notnull array<string> outValues)
	{
		foreach (string settingId, string value : m_mValues)
		{
			outSettingIds.Insert(settingId);
			outValues.Insert(value);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply_S(notnull map<string, string> values)
	{
		string joined;
		foreach (string settingId, string value : values)
		{
			if (!IsStorable(settingId) || !IsStorable(value))
				continue;

			if (!joined.IsEmpty())
				joined += ROW_SEPARATOR;
			joined += settingId + VALUE_SEPARATOR + value;
		}

		m_sValues = joined;
		Replication.BumpMe();
		OnValuesReplicated();
		Print(string.Format("[AROR] Campaign settings: %1 values set", m_mValues.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnValuesReplicated()
	{
		m_mValues.Clear();
		array<string> rows = {};
		m_sValues.Split(ROW_SEPARATOR, rows, true);
		foreach (string row : rows)
		{
			int split = row.IndexOf(VALUE_SEPARATOR);
			if (split > 0)
				m_mValues.Set(row.Substring(0, split), row.Substring(split + 1, row.Length() - split - 1));
		}

		m_OnSettingsChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Pairs the two lists by index; entries without a partner are dropped
	protected static map<string, string> ToMap(array<string> settingIds, array<string> values)
	{
		map<string, string> result = new map<string, string>();
		if (!settingIds || !values)
			return result;

		for (int i = 0, count = Math.Min(settingIds.Count(), values.Count()); i < count; i++)
		{
			if (!settingIds[i].IsEmpty())
				result.Set(settingIds[i], values[i]);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! IDs and values must not contain the separators of the replicated string
	protected static bool IsStorable(string text)
	{
		return text.IndexOf(ROW_SEPARATOR) < 0 && text.IndexOf(VALUE_SEPARATOR) < 0;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
