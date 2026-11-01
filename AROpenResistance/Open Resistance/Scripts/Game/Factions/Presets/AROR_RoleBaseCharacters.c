//! The gear-less character a role's AI spawns as; its preset unit loadout dresses it (see AROR_FactionPresetsComponent)
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EFactionRole, "m_eRole")]
class AROR_RoleBaseCharacter
{
	[Attribute("1", UIWidgets.ComboBox, desc: "Campaign role", enums: ParamEnumArray.FromEnum(AROR_EFactionRole))]
	protected AROR_EFactionRole m_eRole;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Character prefab without gear, carrying the role's faction", params: "et")]
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	AROR_EFactionRole GetRole()
	{
		return m_eRole;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetPrefab()
	{
		return m_sPrefab;
	}
}

//! Base AI character per role (Configs/FactionPresets/AROR_RoleBaseCharacters.conf). Every AI soldier of a role spawns
//! as this character whatever its unit type, then is dressed from its preset unit loadout. Keyed by role, not preset,
//! because one preset can serve several roles. A role without one (Civilians) uses its preset units' own prefabs.
//! Extender mods replace the list with Override in addon; a later entry for the same role wins.
[BaseContainerProps(configRoot: true)]
class AROR_RoleBaseCharacters
{
	[Attribute(desc: "Base AI character per role")]
	protected ref array<ref AROR_RoleBaseCharacter> m_aCharacters;

	//------------------------------------------------------------------------------------------------
	//! The role's base character, or empty if it has none
	ResourceName GetPrefab(AROR_EFactionRole role)
	{
		if (!m_aCharacters)
			return string.Empty;

		ResourceName prefab;
		foreach (AROR_RoleBaseCharacter character : m_aCharacters)
		{
			if (character && character.GetRole() == role && !character.GetPrefab().IsEmpty())
				prefab = character.GetPrefab();
		}

		return prefab;
	}
}
