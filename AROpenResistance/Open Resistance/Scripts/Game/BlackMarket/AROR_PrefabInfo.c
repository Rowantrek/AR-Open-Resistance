//! Display name and picture of a prefab for the shop apps, read from the base game's entity catalogues (the same data
//! the Game Master and arsenal use). Cached per prefab. Falls back to the prefab's file name and no picture, so a
//! prefab missing from every catalogue still shows up.
class AROR_PrefabInfo
{
	protected static ref map<string, string> s_mNames = new map<string, string>();
	protected static ref map<string, string> s_mImages = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	//! Human readable name, e.g. "UAZ-469"; the catalogue's localised name when there is one
	static string GetName(ResourceName prefab, bool vehicle)
	{
		string key = prefab;
		string cached;
		if (s_mNames.Find(key, cached))
			return cached;

		string name;
		SCR_UIInfo info = FindUiInfo(prefab, vehicle);
		if (info)
			name = info.GetName();

		if (name.IsEmpty())
			name = AROR_GarageSnapshotHelper.PrefabDisplayName(prefab);
		else if (name.Get(0) == "#")
			name = WidgetManager.Translate(name);

		s_mNames.Set(key, name);
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Picture to show in a list row (the Content Browser image, else the inventory icon); empty when none is known
	static ResourceName GetImage(ResourceName prefab, bool vehicle)
	{
		string key = prefab;
		string cached;
		if (s_mImages.Find(key, cached))
			return cached;

		EEntityCatalogType type = EEntityCatalogType.ITEM;
		if (vehicle)
			type = EEntityCatalogType.VEHICLE;

		ResourceName image = ReadImage(FindUiInfoOfType(prefab, type));
		s_mImages.Set(key, image);
		return image;
	}

	//------------------------------------------------------------------------------------------------
	//! A character prefab's picture from the character catalogues (e.g. for the leaderboard's infantry icon)
	static ResourceName GetCharacterImage(ResourceName prefab)
	{
		string key = prefab;
		string cached;
		if (s_mImages.Find(key, cached))
			return cached;

		ResourceName image = ReadImage(FindUiInfoOfType(prefab, EEntityCatalogType.CHARACTER));
		s_mImages.Set(key, image);
		return image;
	}

	//------------------------------------------------------------------------------------------------
	protected static ResourceName ReadImage(SCR_UIInfo info)
	{
		ResourceName image;
		if (info)
		{
			SCR_EditableEntityUIInfo editable = SCR_EditableEntityUIInfo.Cast(info);
			if (editable)
				image = editable.GetImage();

			if (image.IsEmpty())
				image = info.GetIconPath();
		}

		return image;
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_UIInfo FindUiInfo(ResourceName prefab, bool vehicle)
	{
		EEntityCatalogType type = EEntityCatalogType.ITEM;
		if (vehicle)
			type = EEntityCatalogType.VEHICLE;

		return FindUiInfoOfType(prefab, type);
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_UIInfo FindUiInfoOfType(ResourceName prefab, EEntityCatalogType type)
	{
		SCR_EntityCatalogManagerComponent catalogs = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!catalogs || prefab.IsEmpty())
			return null;

		SCR_EntityCatalogEntry entry = catalogs.GetEntryWithPrefabFromAnyCatalog(type, prefab, null);
		if (!entry)
			return null;

		return entry.GetEntityUiInfo();
	}
}
