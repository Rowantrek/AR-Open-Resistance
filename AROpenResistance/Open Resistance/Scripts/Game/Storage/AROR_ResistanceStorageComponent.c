[ComponentEditorProps(category: "OpenResistance/Storage", description: "Resistance shared item stock (prefab + quantity). Attach to the game mode entity.")]
class AROR_ResistanceStorageComponentClass : SCR_BaseGameModeComponentClass
{
}

void AROR_OnResistanceStorageChanged();
typedef func AROR_OnResistanceStorageChanged;

//! Resistance Storage (see ARCHITECTURE.md -> Resistance Storage): a campaign ledger of item prefabs and
//! quantities for the Resistance. The server owns the stock; parallel arrays replicate to every client.
//! Depositing an entity (AddEntity_S) unpacks nested inventory into separate stock lines, then deletes the entity.
//! Magazines are credited by their rounds, never as full magazines (see DepositCredit).
//! Saved by AROR_ResistanceStorageComponentSerializer. Slot/type/volume stay on the item when something later
//! spawns or equips it — this component only tracks prefab + count.
class AROR_ResistanceStorageComponent : SCR_BaseGameModeComponent
{
	protected static AROR_ResistanceStorageComponent s_Instance;

	//! Server and (after replication) client: prefab -> quantity. Zero qty is never kept.
	protected ref map<ResourceName, int> m_mStock = new map<ResourceName, int>();

	//! Server only: magazine prefab -> deposited rounds not yet adding up to a full magazine. Zero is never kept.
	protected ref map<ResourceName, int> m_mLooseRounds = new map<ResourceName, int>();

	[RplProp(onRplName: "OnStorageReplicated")]
	protected ref array<ResourceName> m_aPrefabIds = {};

	[RplProp(onRplName: "OnStorageReplicated")]
	protected ref array<int> m_aQuantities = {};

	protected ref ScriptInvokerBase<AROR_OnResistanceStorageChanged> m_OnStorageChanged = new ScriptInvokerBase<AROR_OnResistanceStorageChanged>();

	//------------------------------------------------------------------------------------------------
	static AROR_ResistanceStorageComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine; fires on the server after a mutate and on clients when the replicated stock arrives
	ScriptInvokerBase<AROR_OnResistanceStorageChanged> GetOnStorageChanged()
	{
		return m_OnStorageChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine
	int GetQuantity(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return 0;

		int qty;
		if (m_mStock.Find(prefab, qty))
			return qty;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: parallel lists of every stocked prefab and its quantity
	void GetEntries(notnull array<ResourceName> prefabs, notnull array<int> quantities)
	{
		prefabs.Clear();
		quantities.Clear();

		foreach (ResourceName prefab, int qty : m_mStock)
		{
			prefabs.Insert(prefab);
			quantities.Insert(qty);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True while nothing is stocked and no loose rounds are held (nothing to save)
	bool IsEmpty()
	{
		return m_mStock.IsEmpty() && m_mLooseRounds.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: parallel lists of every magazine prefab with loose rounds and the rounds held
	void GetLooseRounds(notnull array<ResourceName> prefabs, notnull array<int> rounds)
	{
		prefabs.Clear();
		rounds.Clear();

		foreach (ResourceName prefab, int held : m_mLooseRounds)
		{
			prefabs.Insert(prefab);
			rounds.Insert(held);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Adds amount of the prefab; no-op if amount <= 0 or prefab is empty.
	void Add_S(ResourceName prefab, int amount)
	{
		if (!Replication.IsServer() || amount <= 0 || prefab.IsEmpty())
			return;

		AdjustStock(prefab, amount);
		Publish_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Adds amount of each prefab (empty entries skipped); one replication at the end.
	void AddMany_S(notnull array<ResourceName> prefabs, int amount)
	{
		if (!Replication.IsServer() || amount <= 0)
			return;

		bool changed = false;
		foreach (ResourceName prefab : prefabs)
		{
			if (prefab.IsEmpty())
				continue;

			AdjustStock(prefab, amount);
			changed = true;
		}

		if (changed)
			Publish_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Takes the amount if stock covers it; returns false (and takes nothing) otherwise.
	bool Take_S(ResourceName prefab, int amount)
	{
		if (!Replication.IsServer() || amount <= 0 || prefab.IsEmpty())
			return false;

		if (GetQuantity(prefab) < amount)
			return false;

		AdjustStock(prefab, -amount);
		Publish_S();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Unpacks nested inventory into separate stock lines (vest cargo, weapon attachments,
	//! magazines, etc.), credits each item (see DepositCredit), then deletes the root entity. Rejects entities
	//! without an InventoryItemComponent. Item health is not preserved.
	bool AddEntity_S(IEntity item)
	{
		if (!Replication.IsServer() || !item)
			return false;

		InventoryItemComponent invItem = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!invItem)
			return false;

		array<IEntity> entities = {};
		CollectItemEntities(item, entities);

		foreach (IEntity entity : entities)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
			if (prefab.IsEmpty())
				continue;

			int credit = DepositCredit(entity, prefab, m_mLooseRounds);
			if (credit > 0)
				AdjustStock(prefab, credit);
		}

		Publish_S();
		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whole items per prefab that depositing these entities (and everything nested in them) would add
	//! now, without changing anything. Lets a swap check stock before it deposits anything.
	void GetDepositYield(notnull array<IEntity> items, notnull map<ResourceName, int> outYield)
	{
		map<ResourceName, int> looseRounds = new map<ResourceName, int>();
		foreach (ResourceName loosePrefab, int held : m_mLooseRounds)
			looseRounds.Set(loosePrefab, held);

		array<IEntity> entities = {};
		foreach (IEntity item : items)
			CollectItemEntities(item, entities);

		foreach (IEntity entity : entities)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
			if (prefab.IsEmpty())
				continue;

			int credit = DepositCredit(entity, prefab, looseRounds);
			if (credit <= 0)
				continue;

			int qty;
			outYield.Find(prefab, qty);
			outYield.Set(prefab, qty + credit);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whole items one deposited entity is worth. A magazine is worth its rounds: they join the prefab's loose
	//! rounds, and each full magazine's worth becomes one stocked magazine. An emptied magazine adds nothing and two
	//! half-empty ones add one, so taking a full magazine and returning it empty cannot create ammunition.
	//! Everything else is worth one. Updates looseRounds.
	protected static int DepositCredit(IEntity entity, ResourceName prefab, notnull map<ResourceName, int> looseRounds)
	{
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(entity.FindComponent(BaseMagazineComponent));
		if (!magazine)
			return 1;

		int capacity = magazine.GetMaxAmmoCount();
		if (capacity <= 0)
			return 1;

		int rounds;
		looseRounds.Find(prefab, rounds);
		int loaded = magazine.GetAmmoCount();
		if (loaded > 0)
			rounds += loaded;

		int whole = rounds / capacity;
		rounds -= whole * capacity;
		if (rounds > 0)
			looseRounds.Set(prefab, rounds);
		else
			looseRounds.Remove(prefab);

		return whole;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: replace stock from a save (used by the serializer)
	void RestoreEntries_S(notnull array<ResourceName> prefabs, notnull array<int> quantities)
	{
		if (!Replication.IsServer())
			return;

		m_mStock.Clear();
		int count = Math.Min(prefabs.Count(), quantities.Count());
		for (int i = 0; i < count; i++)
		{
			ResourceName prefab = prefabs[i];
			int qty = quantities[i];
			if (prefab.IsEmpty() || qty <= 0)
				continue;

			m_mStock.Set(prefab, qty);
		}

		Publish_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: replace loose magazine rounds from a save (used by the serializer). Not replicated.
	void RestoreLooseRounds_S(notnull array<ResourceName> prefabs, notnull array<int> rounds)
	{
		if (!Replication.IsServer())
			return;

		m_mLooseRounds.Clear();
		int count = Math.Min(prefabs.Count(), rounds.Count());
		for (int i = 0; i < count; i++)
		{
			if (prefabs[i].IsEmpty() || rounds[i] <= 0)
				continue;

			m_mLooseRounds.Set(prefabs[i], rounds[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AdjustStock(ResourceName prefab, int delta)
	{
		int qty = GetQuantity(prefab) + delta;
		if (qty <= 0)
			m_mStock.Remove(prefab);
		else
			m_mStock.Set(prefab, qty);
	}

	//------------------------------------------------------------------------------------------------
	protected void Publish_S()
	{
		if (!Replication.IsServer())
			return;

		m_aPrefabIds.Clear();
		m_aQuantities.Clear();

		foreach (ResourceName prefab, int qty : m_mStock)
		{
			m_aPrefabIds.Insert(prefab);
			m_aQuantities.Insert(qty);
		}

		Replication.BumpMe();
		m_OnStorageChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStorageReplicated()
	{
		m_mStock.Clear();
		int count = Math.Min(m_aPrefabIds.Count(), m_aQuantities.Count());
		for (int i = 0; i < count; i++)
		{
			ResourceName prefab = m_aPrefabIds[i];
			int qty = m_aQuantities[i];
			if (prefab.IsEmpty() || qty <= 0)
				continue;

			m_mStock.Set(prefab, qty);
		}

		m_OnStorageChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Root and every nested inventory item, each once
	protected void CollectItemEntities(IEntity root, notnull array<IEntity> outEntities)
	{
		if (!root || outEntities.Contains(root))
			return;

		InventoryItemComponent invItem = InventoryItemComponent.Cast(root.FindComponent(InventoryItemComponent));
		if (!invItem)
			return;

		outEntities.Insert(root);

		array<Managed> components = {};
		root.FindComponents(BaseInventoryStorageComponent, components);
		foreach (Managed managed : components)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage)
				continue;

			array<IEntity> nested = {};
			storage.GetAll(nested, false);
			foreach (IEntity child : nested)
			{
				CollectItemEntities(child, outEntities);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
