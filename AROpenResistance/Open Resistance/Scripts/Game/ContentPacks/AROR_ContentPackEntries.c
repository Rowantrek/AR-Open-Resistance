//! One thing a content pack offers (see AROR_ContentPack): a prefab of a type with its price, the readiness at which
//! it becomes available and the import facilities needed to buy it. An entry can replace other prefabs (e.g. a weapon
//! mod's M16 variant replacing the base-game M16): while its pack is enabled, the replaced prefabs are not sold and
//! faction presets that list them get this prefab instead. Each category below adds its own type.
[BaseContainerProps(insertable: false)]
class AROR_ContentPackEntry
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Prefab", params: "et")]
	protected ResourceName m_sPrefab;

	[Attribute("0", desc: "Base cost in money", params: "0 inf")]
	protected int m_iCost;

	[Attribute("1", desc: "Readiness at which this asset becomes available", params: "1 5 1")]
	protected int m_iMinimumReadinessLevel;

	[Attribute("0", UIWidgets.ComboBox, desc: "Special location the role must also hold to import this asset: none, one seaport, or one heliport/airport", enums: ParamEnumArray.FromEnum(AROR_EImportPointRequirement))]
	protected AROR_EImportPointRequirement m_eImportPointRequirement;

	[Attribute("0", desc: "Import facilities (seaports, heliports and airports together) the role must hold to import this asset", params: "0 inf")]
	protected int m_iRequiredImportPoints;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefabs this entry replaces while its pack is enabled: they are not sold, and faction presets listing them use this prefab instead", params: "et")]
	protected ref array<ResourceName> m_aReplaces;

	[Attribute("", desc: "GUIDs of the addons this entry needs (as in their addon.gproj). The entry is ignored, including what it replaces, unless all are loaded. Empty: always available.")]
	protected ref array<string> m_aRequiredAddons;

	//------------------------------------------------------------------------------------------------
	ResourceName GetPrefab()
	{
		return m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! True when every addon this entry needs is in loadedAddons
	bool HasRequiredAddons(notnull array<string> loadedAddons)
	{
		return AROR_ContentPack.AreAddonsLoaded(m_aRequiredAddons, loadedAddons);
	}

	//------------------------------------------------------------------------------------------------
	int GetCost()
	{
		return m_iCost;
	}

	//------------------------------------------------------------------------------------------------
	int GetMinimumReadinessLevel()
	{
		return m_iMinimumReadinessLevel;
	}

	//------------------------------------------------------------------------------------------------
	AROR_EImportPointRequirement GetImportPointRequirement()
	{
		return m_eImportPointRequirement;
	}

	//------------------------------------------------------------------------------------------------
	int GetRequiredImportPoints()
	{
		return m_iRequiredImportPoints;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the role holds enough import points of the required kind (always true without a requirement)
	bool HasRequiredImportPoints(AROR_EFactionRole role)
	{
		return AROR_ImportPoints.IsMet(role, m_eImportPointRequirement, m_iRequiredImportPoints);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the prefabs this entry replaces; returns how many were added
	int GetReplaces(notnull array<ResourceName> outPrefabs)
	{
		if (!m_aReplaces)
			return 0;

		int added;
		foreach (ResourceName prefab : m_aReplaces)
		{
			if (!prefab.IsEmpty())
			{
				outPrefabs.Insert(prefab);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	AROR_EFactionPresetCategory GetCategory()
	{
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills an entry created at runtime (catalog generation, see AROR_ContentPack.GenerateFromCatalogs)
	void InitGenerated(ResourceName prefab, int type, int cost, int minimumReadinessLevel, int requiredImportPoints, AROR_EImportPointRequirement importPointRequirement)
	{
		m_sPrefab = prefab;
		m_iCost = cost;
		m_iMinimumReadinessLevel = minimumReadinessLevel;
		m_iRequiredImportPoints = requiredImportPoints;
		m_eImportPointRequirement = importPointRequirement;
		SetTypeValue(type);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetTypeValue(int type)
	{
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
	//! Type name for logs and labels, e.g. "RIFLE"
	string GetTypeName()
	{
		return AROR_PresetTypes.GetTypeName(GetCategory(), GetTypeValue());
	}
}

//! An entry that can use other items: a weapon (its magazines and attachments) or a vehicle (its ammunition and
//! munitions). The list is optional and informational: nothing reads it for gameplay yet. Only list what is not
//! already obvious; a future pass may fill it from the prefabs' own magazine wells and attachment slots.
[BaseContainerProps(insertable: false)]
class AROR_ContentPackCompatibleEntry : AROR_ContentPackEntry
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefabs this entry works with: magazines and attachments for a weapon, ammunition and munitions for a vehicle. Informational for now.", params: "et")]
	protected ref array<ResourceName> m_aCompatible;

	//------------------------------------------------------------------------------------------------
	//! Adds the listed compatible prefabs (empty ones skipped); returns how many were added
	int GetCompatible(notnull array<ResourceName> outPrefabs)
	{
		if (!m_aCompatible)
			return 0;

		int added;
		foreach (ResourceName prefab : m_aCompatible)
		{
			if (!prefab.IsEmpty())
			{
				outPrefabs.Insert(prefab);
				added++;
			}
		}

		return added;
	}
}

//! Empty vehicle (no crew)
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EVehicleType, "m_eType")]
class AROR_ContentPackVehicle : AROR_ContentPackCompatibleEntry
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

	//------------------------------------------------------------------------------------------------
	override protected void SetTypeValue(int type)
	{
		m_eType = type;
	}
}

//! Clothing and carried kit
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EGearType, "m_eType")]
class AROR_ContentPackGear : AROR_ContentPackEntry
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Gear type", enums: ParamEnumArray.FromEnum(AROR_EGearType))]
	protected AROR_EGearType m_eType;

	//------------------------------------------------------------------------------------------------
	override AROR_EFactionPresetCategory GetCategory()
	{
		return AROR_EFactionPresetCategory.GEAR;
	}

	//------------------------------------------------------------------------------------------------
	override int GetTypeValue()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	override protected void SetTypeValue(int type)
	{
		m_eType = type;
	}
}

//! Medical supplies, binoculars, radios and other equipment
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EEquipmentType, "m_eType")]
class AROR_ContentPackEquipment : AROR_ContentPackEntry
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Equipment type", enums: ParamEnumArray.FromEnum(AROR_EEquipmentType))]
	protected AROR_EEquipmentType m_eType;

	//------------------------------------------------------------------------------------------------
	override AROR_EFactionPresetCategory GetCategory()
	{
		return AROR_EFactionPresetCategory.EQUIPMENT;
	}

	//------------------------------------------------------------------------------------------------
	override int GetTypeValue()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	override protected void SetTypeValue(int type)
	{
		m_eType = type;
	}
}

//! Hand-held weapon
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EWeaponType, "m_eType")]
class AROR_ContentPackWeapon : AROR_ContentPackCompatibleEntry
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Weapon type", enums: ParamEnumArray.FromEnum(AROR_EWeaponType))]
	protected AROR_EWeaponType m_eType;

	//------------------------------------------------------------------------------------------------
	override AROR_EFactionPresetCategory GetCategory()
	{
		return AROR_EFactionPresetCategory.WEAPONS;
	}

	//------------------------------------------------------------------------------------------------
	override int GetTypeValue()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	override protected void SetTypeValue(int type)
	{
		m_eType = type;
	}
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EWeaponAccessoryType, "m_eType")]
class AROR_ContentPackWeaponAccessory : AROR_ContentPackEntry
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Weapon accessory type", enums: ParamEnumArray.FromEnum(AROR_EWeaponAccessoryType))]
	protected AROR_EWeaponAccessoryType m_eType;

	//------------------------------------------------------------------------------------------------
	override AROR_EFactionPresetCategory GetCategory()
	{
		return AROR_EFactionPresetCategory.WEAPON_ACCESSORIES;
	}

	//------------------------------------------------------------------------------------------------
	override int GetTypeValue()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	override protected void SetTypeValue(int type)
	{
		m_eType = type;
	}
}

//! Ammunition, including munitions loaded onto vehicles and aircraft as items (e.g. a helicopter rocket pod)
[BaseContainerProps(), SCR_BaseContainerCustomTitleEnum(AROR_EAmmunitionType, "m_eType")]
class AROR_ContentPackAmmunition : AROR_ContentPackEntry
{
	[Attribute("0", UIWidgets.ComboBox, desc: "Ammunition type", enums: ParamEnumArray.FromEnum(AROR_EAmmunitionType))]
	protected AROR_EAmmunitionType m_eType;

	//------------------------------------------------------------------------------------------------
	override AROR_EFactionPresetCategory GetCategory()
	{
		return AROR_EFactionPresetCategory.AMMUNITION;
	}

	//------------------------------------------------------------------------------------------------
	override int GetTypeValue()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	override protected void SetTypeValue(int type)
	{
		m_eType = type;
	}
}
