//! Resistance Storage arsenals list every stocked prefab (qty > 0) instead of the faction catalog.
modded class SCR_ArsenalComponent
{
	//! Sorted stock shared by every Resistance arsenal; rebuilt when the stock revision changes
	protected static ref array<ResourceName> s_aARORSortedPrefabs = {};
	protected static AROR_ResistanceStorageComponent s_ARORSortedSource;
	protected static int s_iARORSortedRevision = -1;

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

		// Every Resistance arsenal shows the same stock and the UI asks several times per refresh, so sort once per change
		if (storage != s_ARORSortedSource || storage.GetRevision() != s_iARORSortedRevision)
		{
			BuildSortedResistancePrefabs(storage, s_aARORSortedPrefabs);
			s_ARORSortedSource = storage;
			s_iARORSortedRevision = storage.GetRevision();
		}

		availablePrefabs.InsertAll(s_aARORSortedPrefabs);

		// Empty stock is a valid list — do not fall through to faction-catalog init
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Stocked prefabs (qty > 0) ordered Gear → Equipment → Weapons → Accessories → Ammo → other; then by prefab path.
	protected void BuildSortedResistancePrefabs(notnull AROR_ResistanceStorageComponent storage, notnull array<ResourceName> outPrefabs)
	{
		outPrefabs.Clear();

		array<ResourceName> prefabs = {};
		array<int> quantities = {};
		storage.GetEntries(prefabs, quantities);

		map<ResourceName, int> categoryRanks = new map<ResourceName, int>();
		BuildResistanceCategoryRanks(categoryRanks);

		// The full prefab name ends each key, so keys are unique and the sorted key maps back to its prefab
		array<string> sortKeys = {};
		map<string, ResourceName> keyToPrefab = new map<string, ResourceName>();
		int count = Math.Min(prefabs.Count(), quantities.Count());
		for (int i = 0; i < count; i++)
		{
			ResourceName prefab = prefabs[i];
			if (quantities[i] <= 0 || prefab.IsEmpty())
				continue;

			int rank;
			if (!categoryRanks.Find(prefab, rank))
				rank = 99;

			string rankStr = rank.ToString();
			if (rank < 10)
				rankStr = "0" + rankStr;

			string key = rankStr + "_" + PrefabPathKey(prefab) + "|" + prefab;
			sortKeys.Insert(key);
			keyToPrefab.Set(key, prefab);
		}

		sortKeys.Sort();

		foreach (string key : sortKeys)
		{
			outPrefabs.Insert(keyToPrefab.Get(key));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Matches admin Fill order (AROR_AdminMenuComponent.CollectResistanceStockPrefabs).
	protected void BuildResistanceCategoryRanks(notnull map<ResourceName, int> outRanks)
	{
		outRanks.Clear();

		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
		if (!packs)
			return;

		InsertCategoryRanks(packs, AROR_EFactionPresetCategory.GEAR, 0, outRanks);
		InsertCategoryRanks(packs, AROR_EFactionPresetCategory.EQUIPMENT, 1, outRanks);
		InsertCategoryRanks(packs, AROR_EFactionPresetCategory.WEAPONS, 2, outRanks);
		InsertCategoryRanks(packs, AROR_EFactionPresetCategory.WEAPON_ACCESSORIES, 3, outRanks);
		InsertCategoryRanks(packs, AROR_EFactionPresetCategory.AMMUNITION, 4, outRanks);
	}

	//------------------------------------------------------------------------------------------------
	protected void InsertCategoryRanks(notnull AROR_ContentPacksComponent packs, AROR_EFactionPresetCategory category, int rank, notnull map<ResourceName, int> outRanks)
	{
		array<AROR_ContentPackEntry> entries = {};
		packs.GetEnabledEntries(category, entries);
		foreach (AROR_ContentPackEntry entry : entries)
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
