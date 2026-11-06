//! Classifies Resistance arsenal prefabs into the preset categories and their types, for the arsenal's
//! category tabs and sub-type tabs. Built from the enabled content packs.
class AROR_ArsenalCatalog
{
	//! Category order of the tabs (matches the stock sort order)
	protected static const ref array<AROR_EFactionPresetCategory> CATEGORY_ORDER = {
		AROR_EFactionPresetCategory.GEAR,
		AROR_EFactionPresetCategory.EQUIPMENT,
		AROR_EFactionPresetCategory.WEAPONS,
		AROR_EFactionPresetCategory.WEAPON_ACCESSORIES,
		AROR_EFactionPresetCategory.AMMUNITION
	};

	//! Category value used for prefabs that are not in the preset
	static const int CATEGORY_OTHER = -1;
	//! Category value meaning "every category" (the first tab)
	static const int CATEGORY_ALL = -3;
	//! Type value meaning "every type" on the sub-type row
	static const int TYPE_ALL = -1;

	protected ref map<ResourceName, int> m_mCategory = new map<ResourceName, int>();
	protected ref map<ResourceName, int> m_mType = new map<ResourceName, int>();

	//------------------------------------------------------------------------------------------------
	//! Builds the lookup from the enabled content packs
	static AROR_ArsenalCatalog Build()
	{
		AROR_ArsenalCatalog catalog = new AROR_ArsenalCatalog();

		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
		if (!packs)
			return catalog;

		foreach (AROR_EFactionPresetCategory category : CATEGORY_ORDER)
		{
			array<AROR_ContentPackEntry> entries = {};
			packs.GetEnabledEntries(category, entries);
			foreach (AROR_ContentPackEntry entry : entries)
			{
				ResourceName prefab = entry.GetPrefab();
				if (prefab.IsEmpty() || catalog.m_mCategory.Contains(prefab))
					continue;

				catalog.m_mCategory.Set(prefab, category);
				catalog.m_mType.Set(prefab, entry.GetTypeValue());
			}
		}

		return catalog;
	}

	//------------------------------------------------------------------------------------------------
	//! Tab categories in display order; CATEGORY_OTHER is appended by the UI only when needed
	static void GetCategories(notnull array<int> outCategories)
	{
		outCategories.Clear();
		outCategories.Insert(CATEGORY_ALL);
		foreach (AROR_EFactionPresetCategory category : CATEGORY_ORDER)
		{
			outCategories.Insert(category);
		}
	}

	//------------------------------------------------------------------------------------------------
	int GetCategory(ResourceName prefab)
	{
		int category;
		if (m_mCategory.Find(prefab, category))
			return category;

		return CATEGORY_OTHER;
	}

	//------------------------------------------------------------------------------------------------
	int GetType(ResourceName prefab)
	{
		int type;
		if (m_mType.Find(prefab, type))
			return type;

		return TYPE_ALL;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab belongs to the category and, unless type is TYPE_ALL, to that type
	bool Matches(ResourceName prefab, int category, int type)
	{
		if (category == CATEGORY_ALL)
			return true;

		if (GetCategory(prefab) != category)
			return false;

		return type == TYPE_ALL || GetType(prefab) == type;
	}

	//------------------------------------------------------------------------------------------------
	//! Distinct types present among the given prefabs for a category, in first-seen order
	void GetTypes(notnull array<ResourceName> prefabs, int category, notnull array<int> outTypes)
	{
		outTypes.Clear();
		foreach (ResourceName prefab : prefabs)
		{
			if (GetCategory(prefab) != category)
				continue;

			int type = GetType(prefab);
			if (outTypes.Find(type) < 0)
				outTypes.Insert(type);
		}

		outTypes.Sort();
	}

	//------------------------------------------------------------------------------------------------
	//! Tab label for a category
	static string GetCategoryName(int category)
	{
		if (category == CATEGORY_ALL)
			return "All";

		switch (category)
		{
			case AROR_EFactionPresetCategory.GEAR: return "Gear";
			case AROR_EFactionPresetCategory.EQUIPMENT: return "Equipment";
			case AROR_EFactionPresetCategory.WEAPONS: return "Weapons";
			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES: return "Accessories";
			case AROR_EFactionPresetCategory.AMMUNITION: return "Ammunition";
		}

		return "Other";
	}

	//------------------------------------------------------------------------------------------------
	//! Sub-type tab label
	static string GetTypeName(int category, int type)
	{
		if (type == TYPE_ALL)
			return "All";

		return AROR_PresetTypes.GetTypeName(category, type);
	}
}
