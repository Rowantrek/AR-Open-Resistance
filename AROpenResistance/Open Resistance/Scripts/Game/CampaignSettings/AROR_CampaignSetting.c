//! One campaign setting (see ARCHITECTURE.md -> Game Flow -> Campaign Settings), defined on
//! AROR_CampaignSettingsComponent inside a category. The admin chooses its value at campaign setup; the value is
//! stored, replicated and saved as a string under the setting's ID, so adding a setting never changes the save format.
//! Subclasses decide which values are valid, how they show and how the setup screen steps through them.
[BaseContainerProps(insertable: false)]
class AROR_CampaignSetting
{
	[Attribute("", desc: "Unique ID, e.g. HARD_CORE_MEDICAL. Saved with campaigns: never change it once released.")]
	protected string m_sId;

	[Attribute("", desc: "Name shown on the setup screen")]
	protected string m_sName;

	[Attribute("", desc: "One-line explanation shown under the name")]
	protected string m_sDescription;

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
	string GetDescription()
	{
		return m_sDescription;
	}

	//------------------------------------------------------------------------------------------------
	//! Value a new campaign starts with, and the fallback for a missing or invalid value
	string GetDefaultValue()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	bool IsValidValue(string value)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Text the setup screen shows for the value
	string FormatValue(string value)
	{
		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! The value one step on (direction > 0) or back (direction < 0), as the setup screen's arrows change it
	string StepValue(string value, int direction)
	{
		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! The value if valid, otherwise the default
	string Sanitize(string value)
	{
		if (IsValidValue(value))
			return value;

		return GetDefaultValue();
	}
}

//------------------------------------------------------------------------------------------------
//! On or off. Values "1" / "0".
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sId")]
class AROR_CampaignSettingBool : AROR_CampaignSetting
{
	[Attribute("0", desc: "On in a new campaign")]
	protected bool m_bDefault;

	//------------------------------------------------------------------------------------------------
	override string GetDefaultValue()
	{
		if (m_bDefault)
			return "1";

		return "0";
	}

	//------------------------------------------------------------------------------------------------
	override bool IsValidValue(string value)
	{
		return value == "1" || value == "0";
	}

	//------------------------------------------------------------------------------------------------
	override string FormatValue(string value)
	{
		if (value == "1")
			return "ON";

		return "OFF";
	}

	//------------------------------------------------------------------------------------------------
	override string StepValue(string value, int direction)
	{
		if (value == "1")
			return "0";

		return "1";
	}
}

//------------------------------------------------------------------------------------------------
//! One of a list of options. The value is the option's ID.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sId")]
class AROR_CampaignSettingChoice : AROR_CampaignSetting
{
	[Attribute("", desc: "Option IDs, saved with campaigns: never change one once released")]
	protected ref array<string> m_aOptionIds;

	[Attribute("", desc: "Option names shown on the setup screen, in the same order as the IDs (the ID shows if missing)")]
	protected ref array<string> m_aOptionNames;

	[Attribute("0", desc: "Index of the option a new campaign starts with")]
	protected int m_iDefaultIndex;

	//------------------------------------------------------------------------------------------------
	override string GetDefaultValue()
	{
		if (!m_aOptionIds || m_aOptionIds.IsEmpty())
			return string.Empty;

		if (m_aOptionIds.IsIndexValid(m_iDefaultIndex))
			return m_aOptionIds[m_iDefaultIndex];

		return m_aOptionIds[0];
	}

	//------------------------------------------------------------------------------------------------
	override bool IsValidValue(string value)
	{
		return m_aOptionIds && m_aOptionIds.Contains(value);
	}

	//------------------------------------------------------------------------------------------------
	override string FormatValue(string value)
	{
		if (!m_aOptionIds)
			return value;

		int index = m_aOptionIds.Find(value);
		if (m_aOptionNames && m_aOptionNames.IsIndexValid(index) && !m_aOptionNames[index].IsEmpty())
			return m_aOptionNames[index];

		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! Wraps around at both ends
	override string StepValue(string value, int direction)
	{
		if (!m_aOptionIds || m_aOptionIds.IsEmpty())
			return value;

		int count = m_aOptionIds.Count();
		int index = Math.Max(m_aOptionIds.Find(value), 0);
		if (direction > 0)
			index = (index + 1) % count;
		else if (direction < 0)
			index = (index + count - 1) % count;

		return m_aOptionIds[index];
	}
}

//------------------------------------------------------------------------------------------------
//! A whole number within a range, changed in steps
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sId")]
class AROR_CampaignSettingNumber : AROR_CampaignSetting
{
	[Attribute("0", desc: "Value in a new campaign")]
	protected int m_iDefault;

	[Attribute("0", desc: "Lowest value")]
	protected int m_iMin;

	[Attribute("100", desc: "Highest value")]
	protected int m_iMax;

	[Attribute("1", desc: "Change per arrow press", params: "1 inf 1")]
	protected int m_iStep;

	[Attribute("", desc: "Shown after the value, e.g. \" min\" or \"%\"")]
	protected string m_sSuffix;

	//------------------------------------------------------------------------------------------------
	override string GetDefaultValue()
	{
		return Math.ClampInt(m_iDefault, m_iMin, m_iMax).ToString();
	}

	//------------------------------------------------------------------------------------------------
	override bool IsValidValue(string value)
	{
		if (value.IsEmpty() || value.ToInt().ToString() != value)
			return false;

		int number = value.ToInt();
		return number >= m_iMin && number <= m_iMax;
	}

	//------------------------------------------------------------------------------------------------
	override string FormatValue(string value)
	{
		return value + m_sSuffix;
	}

	//------------------------------------------------------------------------------------------------
	override string StepValue(string value, int direction)
	{
		int number = Sanitize(value).ToInt();
		if (direction > 0)
			number += Math.Max(m_iStep, 1);
		else if (direction < 0)
			number -= Math.Max(m_iStep, 1);

		return Math.ClampInt(number, m_iMin, m_iMax).ToString();
	}
}
