[ComponentEditorProps(category: "OpenResistance/Garrisons", description: "An outpost store: stocked once for the owning army when the garrison is first raised, then a finite container that is never refilled. Reports its physical inventory once after startup.")]
class AROR_OutpostSupplyComponentClass : ScriptComponentClass {}

//! Inventory belongs to the container prefab the store is placed on, which must be a real (openable) container with
//! an inventory manager. The garrison stocks it once (Stock_S) when its roster is first generated; that event is saved
//! with the garrison, so a loaded campaign never restocks a looted store.
class AROR_OutpostSupplyComponent : ScriptComponent
{
	[Attribute("Equipment", desc: "Store category: Weapons, Ammunition, Equipment (non-medical) or Medical. Picks the kind of item it is stocked with.")]
	protected string m_sCategory;

	[Attribute("6", desc: "Items placed in the store, once, when its garrison is first raised; picked at random from the category's items", params: "0 50 1")]
	protected int m_iStockCount;

	//------------------------------------------------------------------------------------------------
	//! Fill the store for the army (currently nothing: see StockPool). Returns the number of items placed.
	int Stock_S(AROR_EFactionRole role)
	{
		if (!Replication.IsServer() || m_iStockCount <= 0)
			return 0;
		IEntity owner = GetOwner();
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!owner || !presets)
			return 0;
		AROR_FactionPreset preset = presets.GetPreset(role);
		array<ResourceName> pool = {};
		if (preset)
			StockPool(preset, role, pool);
		if (pool.IsEmpty())
		{
			Print(string.Format("[AROR OUTPOST STORE] site=%1 category=%2 not stocked: no item source for outpost stores yet", LocationId(), m_sCategory), LogLevel.WARNING);
			return 0;
		}
		InventoryStorageManagerComponent inventory = FindInventory(owner);
		if (!inventory)
		{
			Print(string.Format("[AROR OUTPOST STORE] site=%1 category=%2 not stocked: the container has no inventory manager (a decorative prop, not an openable container)", LocationId(), m_sCategory), LogLevel.ERROR);
			return 0;
		}
		int placed;
		for (int pick = 0; pick < m_iStockCount; pick++)
		{
			Resource resource = Resource.Load(pool[Math.RandomInt(0, pool.Count())]);
			if (!resource || !resource.IsValid())
				continue;
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			owner.GetWorldTransform(params.Transform);
			IEntity item = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
			if (!item)
				continue;
			if (inventory.TryInsertItem(item))
				placed++;
			else
				SCR_EntityHelper.DeleteEntityAndChildren(item);
		}
		Print(string.Format("[AROR OUTPOST STORE] site=%1 category=%2 stocked %3 of %4 items from preset %5", LocationId(), m_sCategory, placed, m_iStockCount, preset.GetId()), LogLevel.NORMAL);
		return placed;
	}

	//------------------------------------------------------------------------------------------------
	//! The items this store's category draws from that the army's readiness allows.
	//! TODO: faction presets no longer list items, so the pool is empty and stores stay unstocked until this draws from
	//! content packs (or the army's unit loadouts).
	protected void StockPool(notnull AROR_FactionPreset preset, AROR_EFactionRole role, notnull array<ResourceName> pool)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! The container's inventory manager, on the box itself or one of its parts
	protected InventoryStorageManagerComponent FindInventory(IEntity owner)
	{
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(owner.FindComponent(InventoryStorageManagerComponent));
		IEntity part = owner.GetChildren();
		while (!inventory && part)
		{
			inventory = InventoryStorageManagerComponent.Cast(part.FindComponent(InventoryStorageManagerComponent));
			part = part.GetSibling();
		}
		return inventory;
	}

	//------------------------------------------------------------------------------------------------
	protected string LocationId()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return "unregistered";
		IEntity parent = owner.GetParent();
		for (int depth = 0; parent && depth < 16; depth++)
		{
			AROR_LocationComponent location = AROR_LocationComponent.Cast(parent.FindComponent(AROR_LocationComponent));
			if (location)
				return location.GetId();
			parent = parent.GetParent();
		}
		return "unregistered";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckContents_S()
	{
		if (!Replication.IsServer())
			return;
		IEntity owner = GetOwner();
		if (!owner)
			return;
		string locationId = LocationId();
		// Standalone boxes carry storage components only (the inventory manager lives on the character opening them),
		// on the box itself or on a child entity
		array<Managed> storages = {};
		owner.FindComponents(BaseInventoryStorageComponent, storages);
		IEntity part = owner.GetChildren();
		while (part)
		{
			array<Managed> partStorages = {};
			part.FindComponents(BaseInventoryStorageComponent, partStorages);
			storages.InsertAll(partStorages);
			part = part.GetSibling();
		}
		if (storages.IsEmpty())
		{
			Print(string.Format("[AROR OUTPOST STORE] site=%1 category=%2 has no native storage; container access/content requires correction", locationId, m_sCategory), LogLevel.ERROR);
			return;
		}
		array<IEntity> items = {};
		foreach (Managed managed : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage)
				continue;
			array<IEntity> stored = {};
			storage.GetAll(stored, false);
			items.InsertAll(stored);
		}
		if (items.IsEmpty())
			Print(string.Format("[AROR OUTPOST STORE] site=%1 category=%2 observedItems=0; inspect native contents or prior looting/save restoration; no refill performed", locationId, m_sCategory), LogLevel.WARNING);
		else
			Print(string.Format("[AROR OUTPOST STORE] site=%1 category=%2 observedItems=%3; native finite inventory, no automatic restock", locationId, m_sCategory, items.Count()), LogLevel.NORMAL);
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;
		GetGame().GetCallqueue().CallLater(CheckContents_S, 5000, false);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckContents_S);
		super.OnDelete(owner);
	}
}
