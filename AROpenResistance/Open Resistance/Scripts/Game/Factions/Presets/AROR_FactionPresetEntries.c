//! One thing a faction preset makes available: a prefab of a type (see AROR_FactionPreset).
//! Each category below adds its own type. Prices, readiness and import requirements are not here: they belong to
//! content packs (see AROR_ContentPack). Presets only list what exists; the systems using them decide what to pick.
[BaseContainerProps(insertable: false)]
class AROR_FactionPresetEntry
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Prefab", params: "et")]
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	//! The prefab to use: the listed one, or its replacement from an enabled content pack (e.g. a weapon mod's variant)
	ResourceName GetPrefab()
	{
		return AROR_ContentPacksComponent.Resolve(m_sPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab exactly as listed in the preset, ignoring content pack replacements
	ResourceName GetListedPrefab()
	{
		return m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	AROR_EFactionPresetCategory GetCategory()
	{
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The entry's type within its category, as the int value of that category's type enum
	int GetTypeValue()
	{
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! False for types outside the core list (e.g. added by a modded enum); such entries are ignored
	bool IsKnownType()
	{
		return AROR_PresetTypes.IsKnownType(GetCategory(), GetTypeValue());
	}

	//------------------------------------------------------------------------------------------------
	//! Type name for logs, e.g. "RIFLEMAN"
	string GetTypeName()
	{
		return AROR_PresetTypes.GetTypeName(GetCategory(), GetTypeValue());
	}
}

//! AI character. A preset has at most one unit per type. The prefab is optional: empty uses the role's default for the
//! type (AROR_DefaultUnitPrefabs), so a preset can give a unit a loadout without naming a character. Loadouts are
//! Readiness variants of the unit's kit; with none, the character keeps its prefab's own kit.
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EUnitType, "m_eType")]
class AROR_FactionPresetUnit : AROR_FactionPresetEntry
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Unit type (one unit per type in a preset)", enums: ParamEnumArray.FromEnum(AROR_EUnitType))]
	protected AROR_EUnitType m_eType;

	[Attribute(desc: "Kit variants by Readiness. Empty keeps the character prefab's own kit; when set, use a character without gear (e.g. the role default).")]
	protected ref array<ref AROR_UnitLoadout> m_aLoadouts;

	//------------------------------------------------------------------------------------------------
	//! True when the unit names its own character instead of the role default
	bool HasPrefab()
	{
		return !m_sPrefab.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	bool HasLoadouts()
	{
		return m_aLoadouts && !m_aLoadouts.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The loadout to use at a readiness (see AROR_UnitLoadout.Pick), or null to keep the prefab's own kit
	AROR_UnitLoadout PickLoadout(int readiness)
	{
		return AROR_UnitLoadout.Pick(m_aLoadouts, readiness);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs loadout mistakes; presetId names the preset in the log
	void ValidateLoadouts(string presetId)
	{
		if (!m_aLoadouts)
			return;

		bool hasFirstTier;
		foreach (int index, AROR_UnitLoadout loadout : m_aLoadouts)
		{
			if (!loadout)
				continue;

			if (loadout.GetMinimumReadiness() <= 1)
				hasFirstTier = true;

			loadout.Validate(string.Format("Faction preset '%1', %2 loadout %3", presetId, GetTypeName(), index + 1));
		}

		if (!hasFirstTier && !m_aLoadouts.IsEmpty())
			Print(string.Format("[AROR] Faction preset '%1': %2 has no readiness 1 loadout; its lowest one is used below that", presetId, GetTypeName()), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override AROR_EFactionPresetCategory GetCategory()
	{
		return AROR_EFactionPresetCategory.UNITS;
	}

	//------------------------------------------------------------------------------------------------
	override int GetTypeValue()
	{
		return m_eType;
	}
}

//! Empty vehicle (no crew)
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EVehicleType, "m_eType")]
class AROR_FactionPresetVehicle : AROR_FactionPresetEntry
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Vehicle type", enums: ParamEnumArray.FromEnum(AROR_EVehicleType))]
	protected AROR_EVehicleType m_eType;

	//------------------------------------------------------------------------------------------------
	override AROR_EFactionPresetCategory GetCategory()
	{
		return AROR_EFactionPresetCategory.VEHICLES;
	}

	//------------------------------------------------------------------------------------------------
	override int GetTypeValue()
	{
		return m_eType;
	}
}
