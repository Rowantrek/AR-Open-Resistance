//! Loadouts (see ARCHITECTURE.md -> Faction presets): the full kit a faction preset gives one of its units (authored
//! inline on AROR_FactionPresetUnit, several as Readiness variants) or its players (one AROR_PlayerLoadout). Every
//! prefab here is read through AROR_ContentPacksComponent.Resolve, so an enabled pack's replacements reach kits too;
//! Content Pack readiness does not gate loadout items (the variant's own readiness decides).
//! This is data only: applying a loadout to a spawned character is the job of the spawning systems.
//! Contents (of clothing or of an item such as a pouch) are complete: whoever applies a loadout empties a container's
//! own default contents first, so a kit is exactly what is listed.

//! One item, possibly several of it: clothing contents (magazines, grenades, pouches) or loose items. An item that is
//! itself a container (a pouch, a backpack attachment) lists what it holds.
[BaseContainerProps(), SCR_BaseContainerResourceTitleField("m_sPrefab")]
class AROR_LoadoutItem
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Item prefab", params: "et")]
	protected ResourceName m_sPrefab;

	[Attribute("1", desc: "How many; each copy gets the contents below", params: "1 99 1")]
	protected int m_iCount;

	[Attribute(desc: "Items stored inside this one, e.g. magazines in a pouch")]
	protected ref array<ref AROR_LoadoutItem> m_aContents;

	//------------------------------------------------------------------------------------------------
	//! Adds the contents; returns how many were added
	int GetContents(notnull array<AROR_LoadoutItem> outItems)
	{
		return AROR_LoadoutBase.CollectItems(m_aContents, outItems);
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab to use: the listed one, or its replacement from an enabled content pack
	ResourceName GetPrefab()
	{
		return AROR_ContentPacksComponent.Resolve(m_sPrefab);
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetListedPrefab()
	{
		return m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	int GetCount()
	{
		return m_iCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs mistakes; context names the preset, unit and variant
	void Validate(string context)
	{
		if (m_sPrefab.IsEmpty())
			Print(string.Format("[AROR] %1: an item has no prefab", context), LogLevel.WARNING);

		if (m_iCount < 1)
			Print(string.Format("[AROR] %1: item '%2' has a count below 1", context, m_sPrefab), LogLevel.WARNING);

		AROR_LoadoutBase.ValidateItems(m_aContents, string.Format("%1, contents of '%2'", context, m_sPrefab));
	}
}

//! A piece of clothing (uniform, vest, helmet, backpack...) and what goes inside it. The loadout slot comes from the
//! prefab itself.
[BaseContainerProps(), SCR_BaseContainerResourceTitleField("m_sPrefab")]
class AROR_LoadoutClothing
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Clothing prefab; its own loadout slot decides where it is worn", params: "et")]
	protected ResourceName m_sPrefab;

	[Attribute(desc: "Items stored inside this piece, e.g. pouches on a vest; the complete contents")]
	protected ref array<ref AROR_LoadoutItem> m_aContents;

	//------------------------------------------------------------------------------------------------
	//! The prefab to use: the listed one, or its replacement from an enabled content pack
	ResourceName GetPrefab()
	{
		return AROR_ContentPacksComponent.Resolve(m_sPrefab);
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetListedPrefab()
	{
		return m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the contents; returns how many were added
	int GetContents(notnull array<AROR_LoadoutItem> outItems)
	{
		return AROR_LoadoutBase.CollectItems(m_aContents, outItems);
	}

	//------------------------------------------------------------------------------------------------
	void Validate(string context)
	{
		if (m_sPrefab.IsEmpty())
			Print(string.Format("[AROR] %1: a clothing piece has no prefab", context), LogLevel.WARNING);

		AROR_LoadoutBase.ValidateItems(m_aContents, string.Format("%1, contents of '%2'", context, m_sPrefab));
	}
}

//! A weapon with what is mounted on it. The weapon slot comes from the prefab itself; spare magazines are clothing
//! contents.
[BaseContainerProps(), SCR_BaseContainerResourceTitleField("m_sPrefab")]
class AROR_LoadoutWeapon
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Weapon prefab; its own weapon slot decides where it is carried", params: "et")]
	protected ResourceName m_sPrefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Attachments mounted on the weapon (optic, suppressor, light, bipod...)", params: "et")]
	protected ref array<ResourceName> m_aAttachments;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Magazine loaded in the weapon; empty keeps the weapon prefab's own", params: "et")]
	protected ResourceName m_sMagazine;

	//------------------------------------------------------------------------------------------------
	//! The prefab to use: the listed one, or its replacement from an enabled content pack
	ResourceName GetPrefab()
	{
		return AROR_ContentPacksComponent.Resolve(m_sPrefab);
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetListedPrefab()
	{
		return m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the attachments, after content pack replacements; returns how many were added
	int GetAttachments(notnull array<ResourceName> outPrefabs)
	{
		if (!m_aAttachments)
			return 0;

		int added;
		foreach (ResourceName attachment : m_aAttachments)
		{
			if (attachment.IsEmpty())
				continue;

			outPrefabs.Insert(AROR_ContentPacksComponent.Resolve(attachment));
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded magazine after content pack replacements, or empty to keep the weapon's own
	ResourceName GetMagazine()
	{
		if (m_sMagazine.IsEmpty())
			return string.Empty;

		return AROR_ContentPacksComponent.Resolve(m_sMagazine);
	}

	//------------------------------------------------------------------------------------------------
	void Validate(string context)
	{
		if (m_sPrefab.IsEmpty())
			Print(string.Format("[AROR] %1: a weapon has no prefab", context), LogLevel.WARNING);

		if (m_aAttachments && m_aAttachments.Contains(string.Empty))
			Print(string.Format("[AROR] %1: weapon '%2' has an empty attachment", context, m_sPrefab), LogLevel.WARNING);
	}
}

//! One complete kit: clothing with contents, weapons and loose items. Shared by unit loadouts (AROR_UnitLoadout, with
//! Readiness variants) and player loadouts (AROR_PlayerLoadout, a single kit).
[BaseContainerProps(insertable: false)]
class AROR_LoadoutBase
{
	[Attribute(desc: "Clothing worn, each with its contents")]
	protected ref array<ref AROR_LoadoutClothing> m_aClothing;

	[Attribute(desc: "Weapons carried, each with its attachments")]
	protected ref array<ref AROR_LoadoutWeapon> m_aWeapons;

	[Attribute(desc: "Loose items placed wherever they fit")]
	protected ref array<ref AROR_LoadoutItem> m_aItems;

	//------------------------------------------------------------------------------------------------
	//! Adds the clothing; returns how many were added
	int GetClothing(notnull array<AROR_LoadoutClothing> outClothing)
	{
		if (!m_aClothing)
			return 0;

		int added;
		foreach (AROR_LoadoutClothing clothing : m_aClothing)
		{
			if (clothing && !clothing.GetListedPrefab().IsEmpty())
			{
				outClothing.Insert(clothing);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the weapons; returns how many were added
	int GetWeapons(notnull array<AROR_LoadoutWeapon> outWeapons)
	{
		if (!m_aWeapons)
			return 0;

		int added;
		foreach (AROR_LoadoutWeapon weapon : m_aWeapons)
		{
			if (weapon && !weapon.GetListedPrefab().IsEmpty())
			{
				outWeapons.Insert(weapon);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the loose items; returns how many were added
	int GetItems(notnull array<AROR_LoadoutItem> outItems)
	{
		return CollectItems(m_aItems, outItems);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs mistakes in the kit; context names the preset and the loadout
	void Validate(string context)
	{
		if (m_aClothing)
		{
			foreach (AROR_LoadoutClothing clothing : m_aClothing)
			{
				if (clothing)
					clothing.Validate(context);
			}
		}

		if (m_aWeapons)
		{
			foreach (AROR_LoadoutWeapon weapon : m_aWeapons)
			{
				if (weapon)
					weapon.Validate(context);
			}
		}

		ValidateItems(m_aItems, context);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the non-empty items of a list; returns how many were added
	static int CollectItems(array<ref AROR_LoadoutItem> items, notnull array<AROR_LoadoutItem> outItems)
	{
		if (!items)
			return 0;

		int added;
		foreach (AROR_LoadoutItem item : items)
		{
			if (item && !item.GetListedPrefab().IsEmpty() && item.GetCount() > 0)
			{
				outItems.Insert(item);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	static void ValidateItems(array<ref AROR_LoadoutItem> items, string context)
	{
		if (!items)
			return;

		foreach (AROR_LoadoutItem item : items)
		{
			if (item)
				item.Validate(context);
		}
	}
}

//! A unit's kit as one Readiness variant (see AROR_FactionPresetUnit). Of a unit's loadouts, those with the highest
//! minimum readiness at or below the role's current readiness are used, picked at random by weight.
[BaseContainerProps()]
class AROR_UnitLoadout : AROR_LoadoutBase
{
	[Attribute("1", desc: "Readiness from which this loadout is used; a higher variant replaces it once reached", params: "1 5 1")]
	protected int m_iMinimumReadiness;

	[Attribute("1", desc: "Relative chance among loadouts of the same readiness", params: "1 100 1")]
	protected int m_iWeight;

	//------------------------------------------------------------------------------------------------
	int GetMinimumReadiness()
	{
		return m_iMinimumReadiness;
	}

	//------------------------------------------------------------------------------------------------
	int GetWeight()
	{
		return m_iWeight;
	}

	//------------------------------------------------------------------------------------------------
	override void Validate(string context)
	{
		if (m_iMinimumReadiness < 1 || m_iMinimumReadiness > 5)
			Print(string.Format("[AROR] %1: minimum readiness %2 is outside 1-5", context, m_iMinimumReadiness), LogLevel.WARNING);

		if (m_iWeight < 1)
			Print(string.Format("[AROR] %1: weight is below 1, so it is never picked", context), LogLevel.WARNING);

		super.Validate(context);
	}

	//------------------------------------------------------------------------------------------------
	//! The loadout to use at a readiness: of the loadouts with the highest minimum readiness at or below it, one picked
	//! at random by weight. If every loadout needs a higher readiness, the lowest tier is used. Null when there are none.
	static AROR_UnitLoadout Pick(array<ref AROR_UnitLoadout> loadouts, int readiness)
	{
		if (!loadouts)
			return null;

		// Find the tier: the highest minimum at or below the readiness, else the lowest minimum overall
		int bestTier = -1;
		int lowestTier = int.MAX;
		foreach (AROR_UnitLoadout loadout : loadouts)
		{
			if (!loadout || loadout.m_iWeight < 1)
				continue;

			int tier = loadout.m_iMinimumReadiness;
			if (tier < lowestTier)
				lowestTier = tier;

			if (tier <= readiness && tier > bestTier)
				bestTier = tier;
		}

		if (bestTier < 0)
			bestTier = lowestTier;

		if (bestTier == int.MAX)
			return null;

		int totalWeight;
		foreach (AROR_UnitLoadout loadout : loadouts)
		{
			if (loadout && loadout.m_iWeight > 0 && loadout.m_iMinimumReadiness == bestTier)
				totalWeight += loadout.m_iWeight;
		}

		int roll = Math.RandomInt(0, totalWeight);
		foreach (AROR_UnitLoadout loadout : loadouts)
		{
			if (!loadout || loadout.m_iWeight < 1 || loadout.m_iMinimumReadiness != bestTier)
				continue;

			roll -= loadout.m_iWeight;
			if (roll < 0)
				return loadout;
		}

		return null;
	}
}

//! The starting kit for players of a role (see AROR_FactionPreset): one kit, no Readiness variants
[BaseContainerProps()]
class AROR_PlayerLoadout : AROR_LoadoutBase
{
}
