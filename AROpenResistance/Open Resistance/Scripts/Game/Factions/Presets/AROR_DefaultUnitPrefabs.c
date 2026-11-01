//! The character a role uses for a unit type when its faction preset names none (see AROR_FactionPresetUnit)
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EUnitType, "m_eType")]
class AROR_DefaultUnitPrefab
{
	[Attribute("1", UIWidgets.ComboBox, desc: "Campaign role", enums: ParamEnumArray.FromEnum(AROR_EFactionRole))]
	protected AROR_EFactionRole m_eRole;

	[Attribute("0", UIWidgets.ComboBox, desc: "Unit type", enums: ParamEnumArray.FromEnum(AROR_EUnitType))]
	protected AROR_EUnitType m_eType;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Character prefab, normally without gear (the preset loadout dresses it)", params: "et")]
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	AROR_EFactionRole GetRole()
	{
		return m_eRole;
	}

	//------------------------------------------------------------------------------------------------
	AROR_EUnitType GetUnitType()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetPrefab()
	{
		return m_sPrefab;
	}
}

//! Default unit characters per role and unit type (Configs/FactionPresets/AROR_DefaultUnitPrefabs.conf). Keyed by role,
//! not preset, because one preset can serve several roles. Extender mods replace or extend the list with Override in
//! addon; a later entry for the same role and type wins.
[BaseContainerProps(configRoot: true)]
class AROR_DefaultUnitPrefabs
{
	[Attribute(desc: "Default character per role and unit type")]
	protected ref array<ref AROR_DefaultUnitPrefab> m_aUnits;

	//------------------------------------------------------------------------------------------------
	//! The default character for the role and type, or empty if there is none
	ResourceName GetPrefab(AROR_EFactionRole role, AROR_EUnitType type)
	{
		if (!m_aUnits)
			return string.Empty;

		ResourceName prefab;
		foreach (AROR_DefaultUnitPrefab unit : m_aUnits)
		{
			if (unit && unit.GetRole() == role && unit.GetUnitType() == type && !unit.GetPrefab().IsEmpty())
				prefab = unit.GetPrefab();
		}

		return prefab;
	}
}
