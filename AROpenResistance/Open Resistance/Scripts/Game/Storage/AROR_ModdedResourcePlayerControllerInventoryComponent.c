//! Take/refund for arsenals with AROR_ResistanceArsenalComponent go through Resistance Storage
//! instead of catalog + supplies.
modded class SCR_ResourcePlayerControllerInventoryComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void RpcAsk_ArsenalRequestItem_(RplId resourceComponentRplId, RplId storageComponentRplId, ResourceName resourceNameItem, EResourceType resourceType)
	{
		if (TryResistanceArsenalRequest_S(resourceComponentRplId, storageComponentRplId, resourceNameItem, resourceType))
			return;

		super.RpcAsk_ArsenalRequestItem_(resourceComponentRplId, storageComponentRplId, resourceNameItem, resourceType);
	}

	//------------------------------------------------------------------------------------------------
	override protected void RpcAsk_ArsenalRefundItem_(RplId resourceComponentRplId, RplId inventoryItemRplId, EResourceType resourceType)
	{
		if (TryResistanceArsenalRefund_S(resourceComponentRplId, inventoryItemRplId, resourceType))
			return;

		super.RpcAsk_ArsenalRefundItem_(resourceComponentRplId, inventoryItemRplId, resourceType);
	}

	//------------------------------------------------------------------------------------------------
	//! true = handled (accepted or rejected); false = not a Resistance arsenal, use vanilla
	protected bool TryResistanceArsenalRequest_S(RplId resourceComponentRplId, RplId storageComponentRplId, ResourceName resourceNameItem, EResourceType resourceType)
	{
		if (!resourceComponentRplId.IsValid() || resourceNameItem.IsEmpty())
			return false;

		SCR_ResourceComponent resourceComponent = SCR_ResourceComponent.Cast(Replication.FindItem(resourceComponentRplId));
		if (!resourceComponent)
			return false;

		IEntity resourcesOwner = resourceComponent.GetOwner();
		if (!AROR_ResistanceArsenalComponent.IsResistanceArsenal(resourcesOwner))
			return false;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetOwner());
		if (!controller)
			return true;

		SCR_ChimeraCharacter buyer = SCR_ChimeraCharacter.Cast(controller.GetControlledEntity());
		if (!buyer)
			return true;

		SCR_InventoryStorageManagerComponent inventoryManagerComponent = SCR_InventoryStorageManagerComponent.Cast(buyer.GetCharacterController().GetInventoryStorageManager());
		if (!inventoryManagerComponent)
			return true;

		if (!storageComponentRplId.IsValid())
			return true;

		BaseInventoryStorageComponent storageComponent = BaseInventoryStorageComponent.Cast(Replication.FindItem(storageComponentRplId));
		if (!storageComponent)
			return true;

		IEntity storageParent = storageComponent.GetOwner();
		vector storagePosition = storageParent.GetOrigin();
		while (storageParent)
		{
			if (ChimeraCharacter.Cast(storageParent))
			{
				if (storageParent != buyer)
					return true;

				break;
			}

			storageParent = storageParent.GetParent();
		}

		if (!storageParent && vector.DistanceSq(buyer.GetOrigin(), storagePosition) > MAX_REFUNDING_DISTANCE_SQ)
			return true;

		if (vector.DistanceSq(resourcesOwner.GetOrigin(), storagePosition) > MAX_REFUNDING_DISTANCE_SQ)
			return true;

		if (!inventoryManagerComponent.ValidateStorageRequest(resourcesOwner))
			return true;

		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.FindArsenalComponent(resourcesOwner);
		if (!arsenalComponent)
			return true;

		AROR_ResistanceStorageComponent resistanceStorage = AROR_ResistanceStorageComponent.GetInstance();
		if (!resistanceStorage || !resistanceStorage.Take_S(resourceNameItem, 1))
			return true;

		// Spawned here rather than with TrySpawnPrefabToStorage so its default contents can be charged before handing it over
		IEntity item = SpawnArsenalItem(resourceNameItem, storagePosition);
		if (!item)
		{
			resistanceStorage.Add_S(resourceNameItem, 1);
			return true;
		}

		resistanceStorage.TakeNestedContents_S(item);

		if (inventoryManagerComponent.TryInsertItemInStorage(item, storageComponent, cb: new SCR_PrefabSpawnCallback(storageComponent)))
		{
			if (s_OnArsenalItemRequested)
				s_OnArsenalItemRequested.Invoke(resourceComponent, resourceNameItem, controller, storageComponent, resourceType, 0);
		}
		else
		{
			// Returns the item and whatever was charged for its contents, and deletes it
			resistanceStorage.AddEntity_S(item);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnArsenalItem(ResourceName prefab, vector position)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! true = handled; false = not a Resistance arsenal
	protected bool TryResistanceArsenalRefund_S(RplId resourceComponentRplId, RplId inventoryItemRplId, EResourceType resourceType)
	{
		if (!resourceComponentRplId.IsValid() || !inventoryItemRplId.IsValid())
			return false;

		SCR_ResourceComponent resourceComponent = SCR_ResourceComponent.Cast(Replication.FindItem(resourceComponentRplId));
		if (!resourceComponent)
			return false;

		IEntity resourcesOwner = resourceComponent.GetOwner();
		if (!AROR_ResistanceArsenalComponent.IsResistanceArsenal(resourcesOwner))
			return false;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetOwner());
		if (!controller)
			return true;

		SCR_ChimeraCharacter seller = SCR_ChimeraCharacter.Cast(controller.GetControlledEntity());
		if (!seller)
			return true;

		InventoryItemComponent inventoryItemComponent = InventoryItemComponent.Cast(Replication.FindItem(inventoryItemRplId));
		if (!inventoryItemComponent)
			return true;

		IEntity inventoryItemEntity = inventoryItemComponent.GetOwner();
		if (!inventoryItemEntity)
			return true;

		SCR_ItemAttributeCollection attributes = SCR_ItemAttributeCollection.Cast(inventoryItemComponent.GetAttributes());
		if (!attributes || !attributes.IsRefundable())
			return true;

		IEntity itemParent = inventoryItemEntity;
		while (itemParent)
		{
			if (ChimeraCharacter.Cast(itemParent))
			{
				if (itemParent != seller)
					return true;

				break;
			}

			itemParent = itemParent.GetParent();
		}

		vector itemPosition = inventoryItemEntity.GetOrigin();
		if (!itemParent && vector.DistanceSq(seller.GetOrigin(), itemPosition) > MAX_REFUNDING_DISTANCE_SQ)
			return true;

		InventoryStorageManagerComponent sellerInventoryManagerComponent = seller.GetCharacterController().GetInventoryStorageManager();
		if (!sellerInventoryManagerComponent || !sellerInventoryManagerComponent.ValidateStorageRequest(resourcesOwner))
			return true;

		if (vector.DistanceSq(resourcesOwner.GetOrigin(), itemPosition) > MAX_REFUNDING_DISTANCE_SQ)
			return true;

		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.FindArsenalComponent(resourcesOwner);
		if (!arsenalComponent || !arsenalComponent.IsRefundEnabled())
			return true;

		AROR_ResistanceStorageComponent resistanceStorage = AROR_ResistanceStorageComponent.GetInstance();
		if (!resistanceStorage)
			return true;

		SCR_ArsenalManagerComponent.OnItemRefunded_S(inventoryItemEntity, PlayerController.Cast(GetOwner()), arsenalComponent);

		// Unpacks nested contents into stock lines and deletes the entity
		resistanceStorage.AddEntity_S(inventoryItemEntity);
		return true;
	}
}
