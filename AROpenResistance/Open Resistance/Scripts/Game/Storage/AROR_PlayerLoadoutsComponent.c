enum AROR_ELoadoutSlotIndex
{
	SLOT_1 = 0,
	SLOT_2 = 1,
	SLOT_3 = 2
}

[ComponentEditorProps(category: "OpenResistance/Storage", description: "Per-player arsenal loadout slots (3). Attach to the game mode entity.")]
class AROR_PlayerLoadoutsComponentClass : SCR_BaseGameModeComponentClass
{
}

//! One saved loadout slot: vanilla arsenal JSON for re-equip, plus flat prefab counts for Resistance Storage math.
class AROR_LoadoutSlotData
{
	string m_sLoadoutJson;
	ref array<ResourceName> m_aPrefabs = {};
	ref array<int> m_aQuantities = {};

	//------------------------------------------------------------------------------------------------
	bool IsEmpty()
	{
		return m_sLoadoutJson.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		m_sLoadoutJson = string.Empty;
		m_aPrefabs.Clear();
		m_aQuantities.Clear();
	}

	//------------------------------------------------------------------------------------------------
	void CopyFrom(notnull AROR_LoadoutSlotData other)
	{
		m_sLoadoutJson = other.m_sLoadoutJson;
		m_aPrefabs.Clear();
		m_aQuantities.Clear();
		foreach (ResourceName prefab : other.m_aPrefabs)
			m_aPrefabs.Insert(prefab);
		foreach (int qty : other.m_aQuantities)
			m_aQuantities.Insert(qty);
	}
}

//! Three slots for one player identity.
class AROR_PlayerLoadoutSet
{
	static const int SLOT_COUNT = 3;

	ref array<ref AROR_LoadoutSlotData> m_aSlots = {};

	//------------------------------------------------------------------------------------------------
	void AROR_PlayerLoadoutSet()
	{
		for (int i = 0; i < SLOT_COUNT; i++)
			m_aSlots.Insert(new AROR_LoadoutSlotData());
	}

	//------------------------------------------------------------------------------------------------
	AROR_LoadoutSlotData GetSlot(int slotIndex)
	{
		if (slotIndex < 0 || slotIndex >= m_aSlots.Count())
			return null;

		return m_aSlots[slotIndex];
	}
}

//! Player loadout slots (see ARCHITECTURE.md -> Resistance Storage): three identity-keyed kits saved with the
//! campaign. Save snapshots worn gear; Load deposits current gear into Resistance Storage and equips the kit
//! from stock. Slot occupancy is pushed only to the owning client (like personal money).
class AROR_PlayerLoadoutsComponent : SCR_BaseGameModeComponent
{
	protected static AROR_PlayerLoadoutsComponent s_Instance;

	//! Server: identity key -> three slots
	protected ref map<string, ref AROR_PlayerLoadoutSet> m_mLoadouts = new map<string, ref AROR_PlayerLoadoutSet>();

	//! Client: bit 0..2 set when that local slot is filled
	protected int m_iLocalFilledMask;

	//------------------------------------------------------------------------------------------------
	static AROR_PlayerLoadoutsComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: whether the local player's slot is filled (for action CanBePerformed)
	bool IsLocalSlotFilled(int slotIndex)
	{
		if (slotIndex < 0 || slotIndex >= AROR_PlayerLoadoutSet.SLOT_COUNT)
			return false;

		return (m_iLocalFilledMask & (1 << slotIndex)) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether that player's slot has a saved kit
	bool IsSlotFilled_S(int playerId, int slotIndex)
	{
		if (!Replication.IsServer() || slotIndex < 0 || slotIndex >= AROR_PlayerLoadoutSet.SLOT_COUNT)
			return false;

		string key = GetPlayerKey(playerId);
		if (key.IsEmpty())
			return false;

		AROR_PlayerLoadoutSet loadoutSet;
		if (!m_mLoadouts.Find(key, loadoutSet) || !loadoutSet)
			return false;

		AROR_LoadoutSlotData slot = loadoutSet.GetSlot(slotIndex);
		return slot && !slot.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Action UI: server checks authoritative data; clients use the synced local mask
	bool IsSlotFilledForUser(IEntity user, int slotIndex)
	{
		if (Replication.IsServer())
		{
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
			return IsSlotFilled_S(playerId, slotIndex);
		}

		return IsLocalSlotFilled(slotIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: server pushed the local player's filled-slot mask
	void OnLocalSlotsReceived(int filledMask)
	{
		m_iLocalFilledMask = filledMask;
	}

	//------------------------------------------------------------------------------------------------
	//! Stable key: Bohemia identity ID, or "player_" + id in Workbench / offline
	protected static string GetPlayerKey(int playerId)
	{
		if (playerId <= 0)
			return string.Empty;

		BackendApi backend = GetGame().GetBackendApi();
		if (backend)
		{
			string identity = backend.GetPlayerIdentityId(playerId);
			if (!identity.IsEmpty())
				return identity;
		}

		return "player_" + playerId;
	}

	//------------------------------------------------------------------------------------------------
	protected AROR_PlayerLoadoutSet GetOrCreateSet(string key)
	{
		AROR_PlayerLoadoutSet loadoutSet;
		if (m_mLoadouts.Find(key, loadoutSet))
			return loadoutSet;

		loadoutSet = new AROR_PlayerLoadoutSet();
		m_mLoadouts.Set(key, loadoutSet);
		return loadoutSet;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: bitmask of filled slots for this player
	protected int GetFilledMask_S(string key)
	{
		AROR_PlayerLoadoutSet loadoutSet;
		if (!m_mLoadouts.Find(key, loadoutSet))
			return 0;

		int mask;
		for (int i = 0; i < AROR_PlayerLoadoutSet.SLOT_COUNT; i++)
		{
			AROR_LoadoutSlotData slot = loadoutSet.GetSlot(i);
			if (slot && !slot.IsEmpty())
				mask |= (1 << i);
		}

		return mask;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: push filled-slot mask to the owning client
	protected void SendSlots(int playerId)
	{
		string key = GetPlayerKey(playerId);
		if (key.IsEmpty())
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (playerController)
			playerController.AROR_SendLoadoutSlots_S(GetFilledMask_S(key));
	}

	//------------------------------------------------------------------------------------------------
	//! Serializer / restore: true while no player has any filled slot
	bool IsEmpty()
	{
		foreach (string key, AROR_PlayerLoadoutSet loadoutSet : m_mLoadouts)
		{
			if (!loadoutSet)
				continue;

			for (int i = 0; i < AROR_PlayerLoadoutSet.SLOT_COUNT; i++)
			{
				AROR_LoadoutSlotData slot = loadoutSet.GetSlot(i);
				if (slot && !slot.IsEmpty())
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Serializer: flatten filled slots into parallel arrays
	void GetEntries(notnull array<string> playerKeys, notnull array<int> slotIndices, notnull array<string> loadoutJsons,
		notnull array<string> itemPlayerKeys, notnull array<int> itemSlotIndices,
		notnull array<ResourceName> itemPrefabs, notnull array<int> itemQuantities)
	{
		playerKeys.Clear();
		slotIndices.Clear();
		loadoutJsons.Clear();
		itemPlayerKeys.Clear();
		itemSlotIndices.Clear();
		itemPrefabs.Clear();
		itemQuantities.Clear();

		foreach (string key, AROR_PlayerLoadoutSet loadoutSet : m_mLoadouts)
		{
			if (!loadoutSet)
				continue;

			for (int i = 0; i < AROR_PlayerLoadoutSet.SLOT_COUNT; i++)
			{
				AROR_LoadoutSlotData slot = loadoutSet.GetSlot(i);
				if (!slot || slot.IsEmpty())
					continue;

				playerKeys.Insert(key);
				slotIndices.Insert(i);
				loadoutJsons.Insert(slot.m_sLoadoutJson);

				int prefabCount = Math.Min(slot.m_aPrefabs.Count(), slot.m_aQuantities.Count());
				for (int p = 0; p < prefabCount; p++)
				{
					if (slot.m_aPrefabs[p].IsEmpty() || slot.m_aQuantities[p] <= 0)
						continue;

					itemPlayerKeys.Insert(key);
					itemSlotIndices.Insert(i);
					itemPrefabs.Insert(slot.m_aPrefabs[p]);
					itemQuantities.Insert(slot.m_aQuantities[p]);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Serializer: replace all slots from a save
	void RestoreEntries_S(notnull array<string> playerKeys, notnull array<int> slotIndices, notnull array<string> loadoutJsons,
		notnull array<string> itemPlayerKeys, notnull array<int> itemSlotIndices,
		notnull array<ResourceName> itemPrefabs, notnull array<int> itemQuantities)
	{
		if (!Replication.IsServer())
			return;

		m_mLoadouts.Clear();

		int slotCount = Math.Min(Math.Min(playerKeys.Count(), slotIndices.Count()), loadoutJsons.Count());
		for (int i = 0; i < slotCount; i++)
		{
			string key = playerKeys[i];
			int slotIndex = slotIndices[i];
			string json = loadoutJsons[i];
			if (key.IsEmpty() || json.IsEmpty() || slotIndex < 0 || slotIndex >= AROR_PlayerLoadoutSet.SLOT_COUNT)
				continue;

			AROR_PlayerLoadoutSet loadoutSet = GetOrCreateSet(key);
			AROR_LoadoutSlotData slot = loadoutSet.GetSlot(slotIndex);
			if (!slot)
				continue;

			slot.Clear();
			slot.m_sLoadoutJson = json;
		}

		int itemCount = Math.Min(Math.Min(Math.Min(itemPlayerKeys.Count(), itemSlotIndices.Count()), itemPrefabs.Count()), itemQuantities.Count());
		for (int i = 0; i < itemCount; i++)
		{
			string key = itemPlayerKeys[i];
			int slotIndex = itemSlotIndices[i];
			ResourceName prefab = itemPrefabs[i];
			int qty = itemQuantities[i];
			if (key.IsEmpty() || prefab.IsEmpty() || qty <= 0 || slotIndex < 0 || slotIndex >= AROR_PlayerLoadoutSet.SLOT_COUNT)
				continue;

			AROR_PlayerLoadoutSet loadoutSet;
			if (!m_mLoadouts.Find(key, loadoutSet) || !loadoutSet)
				continue;

			AROR_LoadoutSlotData slot = loadoutSet.GetSlot(slotIndex);
			if (!slot || slot.IsEmpty())
				continue;

			slot.m_aPrefabs.Insert(prefab);
			slot.m_aQuantities.Insert(qty);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: snapshot the user's worn gear into the slot
	bool SaveSlot_S(int playerId, IEntity userEntity, int slotIndex)
	{
		if (!Replication.IsServer())
			return false;

		if (slotIndex < 0 || slotIndex >= AROR_PlayerLoadoutSet.SLOT_COUNT)
			return false;

		string key = GetPlayerKey(playerId);
		if (key.IsEmpty() || !userEntity)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(userEntity);
		if (!character)
			return false;

		string factionKey = SCR_PlayerArsenalLoadout.ARSENALLOADOUT_FACTIONKEY_NONE;
		Faction characterFaction = character.GetFaction();
		if (characterFaction)
			factionKey = characterFaction.GetFactionKey();

		JsonSaveContext saveContext = new JsonSaveContext();
		if (!saveContext.WriteValue(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_FACTION_KEY, factionKey))
			return false;

		if (!SCR_PlayerArsenalLoadout.ReadLoadoutString(character, saveContext))
			return false;

		string json = saveContext.SaveToString();
		if (json.IsEmpty())
			return false;

		array<ResourceName> prefabs = {};
		array<int> quantities = {};
		CollectFlatPrefabCounts(character, prefabs, quantities);
		if (prefabs.IsEmpty())
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Nothing to save");
			return false;
		}

		AROR_PlayerLoadoutSet loadoutSet = GetOrCreateSet(key);
		AROR_LoadoutSlotData slot = loadoutSet.GetSlot(slotIndex);
		if (!slot)
			return false;

		slot.Clear();
		slot.m_sLoadoutJson = json;
		foreach (ResourceName prefab : prefabs)
			slot.m_aPrefabs.Insert(prefab);
		foreach (int qty : quantities)
			slot.m_aQuantities.Insert(qty);

		SendSlots(playerId);
		AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("Loadout %1 saved", slotIndex + 1));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: deposit worn gear into Resistance Storage, take the slot kit from stock, re-equip
	bool LoadSlot_S(int playerId, IEntity userEntity, int slotIndex)
	{
		if (!Replication.IsServer())
			return false;

		if (slotIndex < 0 || slotIndex >= AROR_PlayerLoadoutSet.SLOT_COUNT)
			return false;

		string key = GetPlayerKey(playerId);
		if (key.IsEmpty() || !userEntity)
			return false;

		AROR_PlayerLoadoutSet loadoutSet;
		if (!m_mLoadouts.Find(key, loadoutSet) || !loadoutSet)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Loadout slot empty");
			return false;
		}

		AROR_LoadoutSlotData slot = loadoutSet.GetSlot(slotIndex);
		if (!slot || slot.IsEmpty())
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Loadout slot empty");
			return false;
		}

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(userEntity);
		if (!character)
			return false;

		AROR_ResistanceStorageComponent storage = AROR_ResistanceStorageComponent.GetInstance();
		if (!storage)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Resistance Storage unavailable");
			return false;
		}

		map<ResourceName, int> required = new map<ResourceName, int>();
		int reqCount = Math.Min(slot.m_aPrefabs.Count(), slot.m_aQuantities.Count());
		for (int i = 0; i < reqCount; i++)
		{
			ResourceName prefab = slot.m_aPrefabs[i];
			int qty = slot.m_aQuantities[i];
			if (prefab.IsEmpty() || qty <= 0)
				continue;

			int existing;
			required.Find(prefab, existing);
			required.Set(prefab, existing + qty);
		}

		if (required.IsEmpty())
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Loadout slot empty");
			return false;
		}

		map<ResourceName, int> current = new map<ResourceName, int>();
		CollectFlatPrefabCountsToMap(character, current);

		foreach (ResourceName prefab, int need : required)
		{
			int have = storage.GetQuantity(prefab);
			int worn;
			current.Find(prefab, worn);
			if (have + worn < need)
			{
				AROR_NotificationManagerComponent.SendHint_S(playerId, "Not enough stock for this loadout");
				return false;
			}
		}

		array<IEntity> rootItems = {};
		CollectRootInventoryItems(character, rootItems);
		foreach (IEntity item : rootItems)
		{
			if (item)
				storage.AddEntity_S(item);
		}

		array<ResourceName> takenPrefabs = {};
		array<int> takenQuantities = {};
		foreach (ResourceName prefab, int need : required)
		{
			if (!storage.Take_S(prefab, need))
			{
				for (int t = 0; t < takenPrefabs.Count(); t++)
					storage.Add_S(takenPrefabs[t], takenQuantities[t]);

				AROR_NotificationManagerComponent.SendHint_S(playerId, "Not enough stock for this loadout");
				return false;
			}

			takenPrefabs.Insert(prefab);
			takenQuantities.Insert(need);
		}

		JsonLoadContext loadContext = new JsonLoadContext();
		if (!loadContext.LoadFromString(slot.m_sLoadoutJson))
		{
			for (int t = 0; t < takenPrefabs.Count(); t++)
				storage.Add_S(takenPrefabs[t], takenQuantities[t]);

			AROR_NotificationManagerComponent.SendHint_S(playerId, "Failed to apply loadout");
			return false;
		}

		string factionKey;
		loadContext.ReadValue(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_FACTION_KEY, factionKey);

		if (!SCR_PlayerArsenalLoadout.ApplyLoadoutString(character, loadContext))
		{
			for (int t = 0; t < takenPrefabs.Count(); t++)
				storage.Add_S(takenPrefabs[t], takenQuantities[t]);

			AROR_NotificationManagerComponent.SendHint_S(playerId, "Failed to apply loadout");
			return false;
		}

		AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("Loadout %1 loaded", slotIndex + 1));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab counts for every nested inventory item on the character (Storage fidelity)
	protected void CollectFlatPrefabCounts(IEntity character, notnull array<ResourceName> prefabs, notnull array<int> quantities)
	{
		prefabs.Clear();
		quantities.Clear();

		map<ResourceName, int> counts = new map<ResourceName, int>();
		CollectFlatPrefabCountsToMap(character, counts);

		foreach (ResourceName prefab, int qty : counts)
		{
			prefabs.Insert(prefab);
			quantities.Insert(qty);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectFlatPrefabCountsToMap(IEntity character, notnull map<ResourceName, int> counts)
	{
		array<IEntity> rootItems = {};
		CollectRootInventoryItems(character, rootItems);

		array<IEntity> allItems = {};
		foreach (IEntity root : rootItems)
			CollectItemEntities(root, allItems);

		foreach (IEntity entity : allItems)
		{
			ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
			if (prefab.IsEmpty())
				continue;

			int qty;
			counts.Find(prefab, qty);
			counts.Set(prefab, qty + 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Top-level equipped items on the character (not nested cargo). Storage types match vanilla arsenal loadouts.
	protected void CollectRootInventoryItems(IEntity character, notnull array<IEntity> outItems)
	{
		outItems.Clear();
		if (!character)
			return;

		array<Managed> components = {};
		character.FindComponents(BaseInventoryStorageComponent, components);
		foreach (Managed managed : components)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage || !ShouldCollectFromStorage(storage))
				continue;

			array<IEntity> nested = {};
			storage.GetAll(nested, false);
			foreach (IEntity item : nested)
			{
				if (!item || outItems.Contains(item))
					continue;

				if (!InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent)))
					continue;

				outItems.Insert(item);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShouldCollectFromStorage(BaseInventoryStorageComponent storage)
	{
		typename storageType = storage.Type();
		return storageType == SCR_CharacterInventoryStorageComponent
			|| storageType == SCR_UniversalInventoryStorageComponent
			|| storageType == EquipedWeaponStorageComponent
			|| storageType == ClothNodeStorageComponent
			|| storageType == SCR_EquipmentStorageComponent
			|| storageType == SCR_WeaponAttachmentsStorageComponent
			|| storageType == SCR_FilteredInventoryStorageComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Root and every nested inventory item, each once (same idea as Resistance Storage)
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
				CollectItemEntities(child, outEntities);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);

		if (Replication.IsServer())
			SendSlots(playerId);
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
