//! Dresses a character from a loadout (see AROR_UnitLoadout.c): clothing with its contents, then weapons with their
//! attachments and magazine, then loose items. Server only. Meant for gear-less characters; listed contents are
//! complete, so each container's own default contents are removed before it is filled. Anything that cannot be placed
//! is deleted and logged, so a partly broken kit still spawns what it can.
class AROR_LoadoutOutfitter
{
	//------------------------------------------------------------------------------------------------
	//! Server: applies the loadout to the character; returns how many listed things could not be placed (0 = complete)
	static int Apply_S(IEntity character, AROR_LoadoutBase loadout)
	{
		if (!Replication.IsServer() || !character || !loadout)
			return 0;

		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!inventory)
		{
			Print(string.Format("[AROR] Outfitter: %1 has no inventory manager", character), LogLevel.ERROR);
			return 1;
		}

		int failed;

		// Clothing first: it provides the storages the rest goes into
		BaseInventoryStorageComponent clothingStorage = BaseInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		array<AROR_LoadoutClothing> clothing = {};
		loadout.GetClothing(clothing);
		foreach (AROR_LoadoutClothing piece : clothing)
		{
			IEntity worn = SpawnNear(piece.GetPrefab(), character);
			if (!worn || !Insert(inventory, worn, clothingStorage))
			{
				failed += Fail(worn, piece.GetPrefab(), "clothing");
				continue;
			}

			array<AROR_LoadoutItem> contents = {};
			piece.GetContents(contents);
			failed += Fill(inventory, worn, contents);
		}

		BaseInventoryStorageComponent weaponStorage = BaseInventoryStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));
		array<AROR_LoadoutWeapon> weapons = {};
		loadout.GetWeapons(weapons);
		foreach (AROR_LoadoutWeapon weapon : weapons)
		{
			IEntity carried = SpawnNear(weapon.GetPrefab(), character);
			if (!carried || !Insert(inventory, carried, weaponStorage))
			{
				failed += Fail(carried, weapon.GetPrefab(), "weapon");
				continue;
			}

			// Parts go on once the weapon is in the character's inventory, so its storages are managed
			failed += FitWeapon(inventory, carried, weapon);
		}

		BaseInventoryStorageComponent equipmentStorage = BaseInventoryStorageComponent.Cast(character.FindComponent(SCR_EquipmentStorageComponent));
		array<AROR_LoadoutItem> items = {};
		loadout.GetItems(items);
		foreach (AROR_LoadoutItem item : items)
		{
			for (int copy = 0; copy < item.GetCount(); copy++)
			{
				IEntity loose = SpawnNear(item.GetPrefab(), character);
				if (!loose || !Insert(inventory, loose, equipmentStorage))
				{
					failed += Fail(loose, item.GetPrefab(), "item");
					continue;
				}

				array<AROR_LoadoutItem> contents = {};
				item.GetContents(contents);
				failed += Fill(inventory, loose, contents);
			}
		}

		return failed;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the character wears no clothing (e.g. a gear-less character nothing has dressed yet)
	static bool IsUndressed(IEntity character)
	{
		if (!character)
			return false;

		BaseInventoryStorageComponent clothingStorage = BaseInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!clothingStorage)
			return false;

		array<IEntity> worn = {};
		clothingStorage.GetAll(worn, false);
		return worn.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the container's own default contents, then puts each listed item (and its contents) inside it
	protected static int Fill(InventoryStorageManagerComponent inventory, IEntity container, array<AROR_LoadoutItem> contents)
	{
		array<BaseInventoryStorageComponent> storages = {};
		GetStorages(container, storages);
		ClearContents(storages);

		int failed;
		foreach (AROR_LoadoutItem item : contents)
		{
			for (int copy = 0; copy < item.GetCount(); copy++)
			{
				IEntity child = SpawnNear(item.GetPrefab(), container);
				if (!child || !InsertInAny(inventory, child, storages))
				{
					failed += Fail(child, item.GetPrefab(), "contents");
					continue;
				}

				array<AROR_LoadoutItem> nested = {};
				item.GetContents(nested);
				failed += Fill(inventory, child, nested);
			}
		}

		return failed;
	}

	//------------------------------------------------------------------------------------------------
	//! Mounts the weapon's attachments and swaps in its loaded magazine when one is listed
	protected static int FitWeapon(InventoryStorageManagerComponent inventory, IEntity weaponEntity, AROR_LoadoutWeapon weapon)
	{
		array<BaseInventoryStorageComponent> storages = {};
		GetStorages(weaponEntity, storages);

		int failed;
		array<ResourceName> attachments = {};
		weapon.GetAttachments(attachments);
		foreach (ResourceName attachment : attachments)
		{
			IEntity part = SpawnNear(attachment, weaponEntity);
			if (!part || !InsertInAny(inventory, part, storages))
				failed += Fail(part, attachment, "attachment");
		}

		ResourceName magazinePrefab = weapon.GetMagazine();
		if (magazinePrefab.IsEmpty())
			return failed;

		BaseWeaponComponent weaponComponent = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
		if (weaponComponent)
		{
			BaseMagazineComponent current = weaponComponent.GetCurrentMagazine();
			if (current && current.GetOwner())
				SCR_EntityHelper.DeleteEntityAndChildren(current.GetOwner());
		}

		IEntity magazine = SpawnNear(magazinePrefab, weaponEntity);
		if (!magazine || !InsertInAny(inventory, magazine, storages))
			failed += Fail(magazine, magazinePrefab, "magazine");

		return failed;
	}

	//------------------------------------------------------------------------------------------------
	//! Into the preferred storage if it takes the item, otherwise wherever the inventory finds room
	protected static bool Insert(InventoryStorageManagerComponent inventory, IEntity item, BaseInventoryStorageComponent preferred)
	{
		if (preferred && inventory.TryInsertItemInStorage(item, preferred))
			return true;

		return inventory.TryInsertItem(item);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool InsertInAny(InventoryStorageManagerComponent inventory, IEntity item, array<BaseInventoryStorageComponent> storages)
	{
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (inventory.TryInsertItemInStorage(item, storage))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GetStorages(IEntity owner, notnull array<BaseInventoryStorageComponent> outStorages)
	{
		array<Managed> components = {};
		owner.FindComponents(BaseInventoryStorageComponent, components);
		foreach (Managed managed : components)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (storage)
				outStorages.Insert(storage);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every item a container spawned with
	protected static void ClearContents(array<BaseInventoryStorageComponent> storages)
	{
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<IEntity> items = {};
			storage.GetAll(items, false);
			foreach (IEntity item : items)
			{
				if (item)
					SCR_EntityHelper.DeleteEntityAndChildren(item);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnNear(ResourceName prefab, IEntity anchor)
	{
		if (prefab.IsEmpty())
			return null;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = anchor.GetOrigin();
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a piece that could not be placed and removes it from the world; returns 1 to count it
	protected static int Fail(IEntity spawned, ResourceName prefab, string what)
	{
		Print(string.Format("[AROR] Outfitter: could not place %1 %2", what, prefab), LogLevel.WARNING);
		if (spawned)
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);

		return 1;
	}
}
