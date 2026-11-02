//! Resistance Storage arsenals list every stocked prefab (qty > 0) instead of the faction catalog.
modded class SCR_ArsenalComponent
{
	//------------------------------------------------------------------------------------------------
	//! Resistance stock is the budget — never gate the HQ arsenal on vanilla supplies.
	override bool IsArsenalUsingSupplies()
	{
		if (AROR_ResistanceArsenalComponent.IsResistanceArsenal(GetOwner()))
			return false;

		return super.IsArsenalUsingSupplies();
	}

	//------------------------------------------------------------------------------------------------
	//! Without SCR_ArsenalManagerComponent, vanilla IsArsenalTypeEnabled_Static is false and disables
	//! every box on init. Resistance arsenals stay available regardless of manager flags.
	override bool IsArsenalEnabledByType()
	{
		if (AROR_ResistanceArsenalComponent.IsResistanceArsenal(GetOwner()))
			return true;

		return super.IsArsenalEnabledByType();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetAvailablePrefabs(out notnull array<ResourceName> availablePrefabs)
	{
		if (!AROR_ResistanceArsenalComponent.IsResistanceArsenal(GetOwner()))
			return super.GetAvailablePrefabs(availablePrefabs);

		AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.GetInstance();
		if (!storage)
			return true;

		array<ResourceName> prefabs = {};
		array<int> quantities = {};
		storage.GetEntries(prefabs, quantities);

		int count = Math.Min(prefabs.Count(), quantities.Count());
		for (int i = 0; i < count; i++)
		{
			if (quantities[i] > 0 && !prefabs[i].IsEmpty())
				availablePrefabs.Insert(prefabs[i]);
		}

		SortResistancePrefabs(availablePrefabs);

		// Empty stock is a valid list — do not fall through to faction-catalog init
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Gear → Equipment → Weapons → Accessories → Ammo → other; then by prefab path.
	protected void SortResistancePrefabs(notnull array<ResourceName> prefabs)
	{
		int n = prefabs.Count();
		if (n < 2)
			return;

		map<ResourceName, int> categoryRanks = new map<ResourceName, int>();
		BuildResistanceCategoryRanks(categoryRanks);

		array<string> sortKeys = {};
		foreach (ResourceName prefab : prefabs)
		{
			int rank = 99;
			if (!categoryRanks.Find(prefab, rank))
				rank = 99;

			string rankStr = rank.ToString();
			if (rank < 10)
				rankStr = "0" + rankStr;

			sortKeys.Insert(rankStr + "_" + PrefabPathKey(prefab));
		}

		for (int i = 0; i < n - 1; i++)
		{
			for (int j = 0; j < n - i - 1; j++)
			{
				if (sortKeys[j].Compare(sortKeys[j + 1]) > 0)
				{
					string keyTmp = sortKeys[j];
					sortKeys[j] = sortKeys[j + 1];
					sortKeys[j + 1] = keyTmp;

					ResourceName prefabTmp = prefabs[j];
					prefabs[j] = prefabs[j + 1];
					prefabs[j + 1] = prefabTmp;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Matches admin Fill order (AROR_AdminMenuComponent.CollectResistanceStockPrefabs).
	protected void BuildResistanceCategoryRanks(notnull map<ResourceName, int> outRanks)
	{
		outRanks.Clear();

		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return;

		AROR_FactionPreset preset = presets.GetPreset(AROR_EFactionRole.RESISTANCE);
		if (!preset)
			return;

		InsertCategoryRanks(preset, AROR_EFactionPresetCategory.GEAR, 0, outRanks);
		InsertCategoryRanks(preset, AROR_EFactionPresetCategory.EQUIPMENT, 1, outRanks);
		InsertCategoryRanks(preset, AROR_EFactionPresetCategory.WEAPONS, 2, outRanks);
		InsertCategoryRanks(preset, AROR_EFactionPresetCategory.WEAPON_ACCESSORIES, 3, outRanks);
		InsertCategoryRanks(preset, AROR_EFactionPresetCategory.AMMUNITION, 4, outRanks);
	}

	//------------------------------------------------------------------------------------------------
	protected void InsertCategoryRanks(notnull AROR_FactionPreset preset, AROR_EFactionPresetCategory category, int rank, notnull map<ResourceName, int> outRanks)
	{
		array<AROR_FactionPresetEntry> entries = {};
		preset.GetAllEntries(category, entries);
		foreach (AROR_FactionPresetEntry entry : entries)
		{
			ResourceName prefab = entry.GetPrefab();
			int existingRank;
			if (!prefab.IsEmpty() && !outRanks.Find(prefab, existingRank))
				outRanks.Set(prefab, rank);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Path after the GUID brace so similar folders group together.
	protected string PrefabPathKey(ResourceName prefab)
	{
		string s = prefab;
		int brace = s.IndexOf("}");
		if (brace >= 0 && brace + 1 < s.Length())
			return s.Substring(brace + 1, s.Length() - brace - 1);

		return s;
	}

	//------------------------------------------------------------------------------------------------
	override void RefreshArsenal(bool init = false, SCR_Faction faction = null)
	{
		if (!AROR_ResistanceArsenalComponent.IsResistanceArsenal(GetOwner()))
		{
			super.RefreshArsenal(init, faction);
			return;
		}

		ClearArsenal();

		array<ResourceName> arsenalPrefabs = {};
		GetAvailablePrefabs(arsenalPrefabs);

		// Always notify (including empty) so the arsenal inventory clears when stock is gone
		ScriptInvokerArsenalUpdated onUpdated = GetOnArsenalUpdated();
		if (onUpdated)
			onUpdated.Invoke(arsenalPrefabs);

		// Same client sync as vanilla RefreshArsenal; clients rebuild via GetAvailablePrefabs (stock replicates)
		if (!init && Replication.IsServer())
			Rpc(RPC_OnArsenalUpdated, GetSupportedArsenalItemTypes(), GetSupportedArsenalItemModes());
	}
}
