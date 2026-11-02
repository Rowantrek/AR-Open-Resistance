//! A group of campaign settings, shown as one tab on the setup screen's Settings step
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class AROR_CampaignSettingCategory
{
	[Attribute("", desc: "Unique ID, e.g. GAMEPLAY")]
	protected string m_sId;

	[Attribute("", desc: "Name shown on the setup screen")]
	protected string m_sName;

	[Attribute(desc: "Settings in this category, in display order")]
	protected ref array<ref AROR_CampaignSetting> m_aSettings;

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
	//! Adds the category's settings in display order; returns how many were added
	int GetSettings(notnull array<AROR_CampaignSetting> outSettings)
	{
		if (!m_aSettings)
			return 0;

		int added;
		foreach (AROR_CampaignSetting setting : m_aSettings)
		{
			if (!setting)
				continue;

			outSettings.Insert(setting);
			added++;
		}

		return added;
	}
}
