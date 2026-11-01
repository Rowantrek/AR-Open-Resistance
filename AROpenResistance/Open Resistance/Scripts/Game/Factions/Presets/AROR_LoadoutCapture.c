//! Workbench capture settings of a faction preset (see AROR_UnitLoadoutImporter): which characters' kits to read into
//! which of the preset's units, at which Readiness, and the preset file to write them to. Lets a content pack's preset
//! (e.g. the WCS armies) take its loadouts from the mod's own kitted characters without anyone typing them in.
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EUnitType, "m_eType")]
class AROR_LoadoutCaptureSource
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Preset unit that receives the kit", enums: ParamEnumArray.FromEnum(AROR_EUnitType))]
	protected AROR_EUnitType m_eType;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Kitted character whose kit is read", params: "et")]
	protected ResourceName m_sCharacter;

	[Attribute("1", desc: "Readiness of the loadout written; a unit that already has a loadout at this readiness is kept", params: "1 5 1")]
	protected int m_iReadiness;

	//------------------------------------------------------------------------------------------------
	AROR_EUnitType GetUnitType()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetCharacter()
	{
		return m_sCharacter;
	}

	//------------------------------------------------------------------------------------------------
	int GetReadiness()
	{
		return m_iReadiness;
	}
}

[BaseContainerProps()]
class AROR_LoadoutCapture
{
	[Attribute("", desc: "The preset's own file as a filesystem path, e.g. $OpenResistanceWCS:Configs/FactionPresets/WCS/AROR_Preset_US_Army_WCS.conf")]
	protected string m_sFile;

	[Attribute(desc: "Characters to read, one loadout each")]
	protected ref array<ref AROR_LoadoutCaptureSource> m_aSources;

	//------------------------------------------------------------------------------------------------
	string GetFile()
	{
		return m_sFile;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the sources; returns how many were added
	int GetSources(notnull array<AROR_LoadoutCaptureSource> outSources)
	{
		if (!m_aSources)
			return 0;

		int added;
		foreach (AROR_LoadoutCaptureSource source : m_aSources)
		{
			if (source && !source.GetCharacter().IsEmpty())
			{
				outSources.Insert(source);
				added++;
			}
		}

		return added;
	}
}
