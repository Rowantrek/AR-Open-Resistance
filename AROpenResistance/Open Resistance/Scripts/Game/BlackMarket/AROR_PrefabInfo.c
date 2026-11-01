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

		ResourceName image;
		SCR_UIInfo info = FindUiInfo(prefab, vehicle);
		if (info)
		{
			SCR_EditableEntityUIInfo editable = SCR_EditableEntityUIInfo.Cast(info);
			if (editable)
				image = editable.GetImage();

			if (image.IsEmpty())
				image = info.GetIconPath();
		}

		s_mImages.Set(key, image);
		return image;
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_UIInfo FindUiInfo(ResourceName prefab, bool vehicle)
	{
		FactionManager factions = GetGame().GetFactionManager();
		if (!factions || prefab.IsEmpty())
			return null;

		EEntityCatalogType type = EEntityCatalogType.ITEM;
		if (vehicle)
			type = EEntityCatalogType.VEHICLE;

		SCR_EntityCatalogManagerComponent catalogs = SCR_EntityCatalogManagerComponent.GetInstance();
		if (catalogs)
		{
			SCR_EntityCatalog general = catalogs.GetEntityCatalogOfType(type, false);
			if (general)
			{
				SCR_EntityCatalogEntry generalEntry = general.GetEntryWithPrefab(prefab);
				if (generalEntry)
					return generalEntry.GetEntityUiInfo();
			}
		}

		// Role identity factions need not own asset catalogues. Probe quietly while
		// preserving entries contributed by any loaded faction/content addon.
		for (int i = 0; i < factions.GetFactionsCount(); i++)
		{
			SCR_Faction faction = SCR_Faction.Cast(factions.GetFactionByIndex(i));
			if (!faction)
				continue;
			SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(type, false);
			if (!catalog)
				continue;
			SCR_EntityCatalogEntry entry = catalog.GetEntryWithPrefab(prefab);
			if (entry)
				return entry.GetEntityUiInfo();
		}
		return null;
	}
}
