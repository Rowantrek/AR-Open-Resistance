//! Server: arms AI soldiers of the enemy roles (Occupiers, Invaders) from their role's faction preset as they spawn,
//! so weapons that content mods add to the faction catalogs (e.g. WCS, through catalog augmentation) reach garrisons,
//! QRFs, patrols and missions. The soldier's primary rifle, machine gun or sniper rifle is replaced by a random preset
//! weapon of the same type available at the role's Readiness, and their spare magazines by the new weapon's default
//! magazine. Launchers, sidearms and everything else stay as the unit prefab defines them.
//! Hooked from the modded SCR_AIGroup (AROR_ModdedAIGroup.c), so every AI group is covered in one place.
class AROR_SoldierArmory
{
	protected static const int REARM_DELAY_MS = 1500;	//!< After the agent is added, so the unit's own loadout has spawned

	protected static ref AROR_SoldierArmory s_Instance;
	protected ref array<ref AROR_SoldierArmorySwapCallback> m_aPendingSwaps = {};
	protected ref array<IEntity> m_aRearmed = {};

	//------------------------------------------------------------------------------------------------
	protected static AROR_SoldierArmory Get()
	{
		if (!s_Instance)
			s_Instance = new AROR_SoldierArmory();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an AI agent joined a group. Soldiers of the enemy roles are re-armed shortly after.
	static void OnAgentAdded_S(AIAgent agent)
	{
		if (!agent || !Replication.IsServer() || !GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(Get().Rearm_S, REARM_DELAY_MS, false, agent);
	}

	//------------------------------------------------------------------------------------------------
	//! The enemy role whose faction the character belongs to; false for the Resistance, civilians and players
	protected static bool EnemyRoleOf(IEntity character, out AROR_EFactionRole role)
	{
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;

		string factionKey = affiliation.GetAffiliatedFactionKey();
		if (factionKey.IsEmpty())
			return false;

		if (factionKey == AROR_ForceSpawner.FactionKey(AROR_EFactionRole.OCCUPIERS))
		{
			role = AROR_EFactionRole.OCCUPIERS;
			return true;
		}

		if (factionKey == AROR_ForceSpawner.FactionKey(AROR_EFactionRole.INVADERS))
		{
			role = AROR_EFactionRole.INVADERS;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Preset weapon type for an equipped weapon, or -1 for weapons that are not swapped
	protected static int PresetWeaponType(EWeaponType weaponType)
	{
		switch (weaponType)
		{
			case EWeaponType.WT_RIFLE: return AROR_EWeaponType.ASSAULT_RIFLE;
			case EWeaponType.WT_MACHINEGUN: return AROR_EWeaponType.MACHINEGUN;
			case EWeaponType.WT_SNIPERRIFLE: return AROR_EWeaponType.SNIPER_RIFLE;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! A random preset weapon of the type available at the role's Readiness (riflemen also draw carbines), or empty
	protected static ResourceName PickWeapon(AROR_EFactionRole role, int presetType)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return string.Empty;

		AROR_FactionPreset preset = presets.GetPreset(role);
		if (!preset)
			return string.Empty;

		int readiness = 5;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
			readiness = operations.GetReadinessLevel(role);

		array<AROR_FactionPresetEntry> entries = {};
		preset.GetEntries(AROR_EFactionPresetCategory.WEAPONS, presetType, entries);
		if (presetType == AROR_EWeaponType.ASSAULT_RIFLE)
			preset.GetEntries(AROR_EFactionPresetCategory.WEAPONS, AROR_EWeaponType.CARBINE, entries);

		array<ResourceName> candidates = {};
		foreach (AROR_FactionPresetEntry entry : entries)
		{
			if (!entry.GetPrefab().IsEmpty() && AROR_ContentPacksComponent.GetMinimumReadinessOf(entry.GetPrefab()) <= readiness)
				candidates.Insert(entry.GetPrefab());
		}

		if (candidates.IsEmpty())
			return string.Empty;

		return candidates.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	protected void Rearm_S(AIAgent agent)
	{
		if (!agent)
			return;

		IEntity character = agent.GetControlledEntity();
		if (!Replication.IsServer() || !character || !AROR_ForceSpawner.IsAlive(character))
			return;
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) > 0)
			return;
		// Soldiers dressed from a preset loadout carry exactly its weapons
		if (AROR_AIOutfitter.WasDressed(character))
			return;
		for (int i = m_aRearmed.Count() - 1; i >= 0; i--)
		{
			if (!m_aRearmed[i])
				m_aRearmed.Remove(i);
		}
		if (m_aRearmed.Contains(character))
			return;

		AROR_EFactionRole role;
		if (!EnemyRoleOf(character, role))
			return;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!inventory || !weaponManager)
			return;
		m_aRearmed.Insert(character);

		array<WeaponSlotComponent> slots = {};
		weaponManager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			IEntity weapon = slot.GetWeaponEntity();
			if (!weapon)
				continue;

			BaseWeaponComponent weaponComponent = BaseWeaponComponent.Cast(weapon.FindComponent(BaseWeaponComponent));
			if (!weaponComponent)
				continue;

			int presetType = PresetWeaponType(weaponComponent.GetWeaponType());
			if (presetType < 0)
				continue;

			// The first rifle, machine gun or sniper rifle is the primary weapon; only that one is swapped
			ResourceName picked = PickWeapon(role, presetType);
			if (!picked.IsEmpty() && picked != weapon.GetPrefabData().GetPrefabName())
				ReplaceWeapon_S(character, inventory, weapon, weaponComponent, picked);

			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ReplaceWeapon_S(IEntity character, SCR_InventoryStorageManagerComponent inventory, IEntity oldWeapon, BaseWeaponComponent oldComponent, ResourceName picked)
	{
		InventoryItemComponent item = InventoryItemComponent.Cast(oldWeapon.FindComponent(InventoryItemComponent));
		if (!item || !item.GetParentSlot())
			return;

		InventoryStorageSlot parentSlot = item.GetParentSlot();
		BaseInventoryStorageComponent storage = parentSlot.GetStorage();
		int slotId = parentSlot.GetID();

		IEntity newWeapon = AROR_ForceSpawner.SpawnPrefab(picked, character.GetOrigin(), 0);
		if (!newWeapon)
			return;

		BaseWeaponComponent newComponent = BaseWeaponComponent.Cast(newWeapon.FindComponent(BaseWeaponComponent));
		BaseMuzzleComponent newMuzzle;
		if (newComponent)
			newMuzzle = newComponent.GetCurrentMuzzle();
		ResourceName newMagazine;
		if (newMuzzle)
			newMagazine = newMuzzle.GetDefaultMagazineOrProjectileName();
		if (newMagazine.IsEmpty())
		{
			// Without a known magazine the soldier could not reload: keep the unit's own weapon
			SCR_EntityHelper.DeleteEntityAndChildren(newWeapon);
			return;
		}

		// Spare magazines of the old weapon, replaced below unless the new weapon takes the same ones
		BaseMagazineWell oldWell;
		BaseMuzzleComponent oldMuzzle = oldComponent.GetCurrentMuzzle();
		if (oldMuzzle)
			oldWell = oldMuzzle.GetMagazineWell();
		BaseMagazineWell newWell = newMuzzle.GetMagazineWell();
		bool sameMagazines = oldWell && newWell && oldWell.Type() == newWell.Type();

		// Replace through the native inventory transaction. Never delete the equipped
		// gun first while AI/WCS may still be running its weapon behaviour.
		if (!inventory.CanReplaceItem(newWeapon, storage, slotId))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(newWeapon);
			return;
		}
		AROR_SoldierArmorySwapCallback callback = new AROR_SoldierArmorySwapCallback();
		callback.m_Armory = this;
		callback.m_Character = character;
		callback.m_Inventory = inventory;
		callback.m_OldWeapon = oldWeapon;
		callback.m_NewWeapon = newWeapon;
		callback.m_OldWell = oldWell;
		callback.m_sNewMagazine = newMagazine;
		callback.m_bSameMagazines = sameMagazines;
		m_aPendingSwaps.Insert(callback);
		if (!inventory.TryReplaceItem(newWeapon, storage, slotId, callback))
			FinishSwap_S(callback, false);
	}

	//! Only the native completion callback may change magazines or select the new weapon.
	void FinishSwap_S(AROR_SoldierArmorySwapCallback callback, bool success)
	{
		if (!Replication.IsServer() || !callback || !m_aPendingSwaps.Contains(callback))
			return;
		SCR_InventoryStorageManagerComponent inventory = callback.m_Inventory;
		IEntity newWeapon = callback.m_NewWeapon;
		if (success && inventory && newWeapon && inventory.Contains(newWeapon))
		{
			IEntity oldWeapon = callback.m_OldWeapon;
			if (oldWeapon && !inventory.Contains(oldWeapon))
			{
				InventoryItemComponent oldItem = InventoryItemComponent.Cast(oldWeapon.FindComponent(InventoryItemComponent));
				if (oldItem && !oldItem.GetParentSlot())
					SCR_EntityHelper.DeleteEntityAndChildren(oldWeapon);
			}
			IEntity character = callback.m_Character;
			if (AROR_ForceSpawner.IsAlive(character) && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) == 0)
			{
				if (!callback.m_bSameMagazines && callback.m_OldWell)
					ReplaceMagazines_S(inventory, callback.m_OldWell, callback.m_sNewMagazine);
				ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
				BaseWeaponComponent weapon = BaseWeaponComponent.Cast(newWeapon.FindComponent(BaseWeaponComponent));
				if (chimera && chimera.GetCharacterController() && weapon)
					chimera.GetCharacterController().SelectWeapon(weapon);
			}
		}
		else if (newWeapon)
		{
			InventoryItemComponent item = InventoryItemComponent.Cast(newWeapon.FindComponent(InventoryItemComponent));
			// Never delete an item that another inventory has acquired in the meantime.
			if (item && !item.GetParentSlot())
				SCR_EntityHelper.DeleteEntityAndChildren(newWeapon);
			Print("[AROR ARMORY] Native weapon replacement failed; original weapon was not pre-deleted", LogLevel.WARNING);
		}
		m_aPendingSwaps.RemoveItem(callback);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the spare magazines that fit the old weapon and adds as many of the new weapon's default magazine
	protected void ReplaceMagazines_S(SCR_InventoryStorageManagerComponent inventory, BaseMagazineWell oldWell, ResourceName newMagazine)
	{
		array<typename> query = {};
		query.Insert(MagazineComponent);
		array<IEntity> magazines = {};
		inventory.FindItemsWithComponents(magazines, query, EStoragePurpose.PURPOSE_DEPOSIT);

		int removed;
		foreach (IEntity magazine : magazines)
		{
			BaseMagazineComponent magazineComponent = BaseMagazineComponent.Cast(magazine.FindComponent(BaseMagazineComponent));
			if (!magazineComponent || !magazineComponent.GetMagazineWell() || magazineComponent.GetMagazineWell().Type() != oldWell.Type())
				continue;

			if (inventory.TryDeleteItem(magazine))
				removed++;
		}

		if (removed > 0)
			inventory.TrySpawnPrefabToStorage(newMagazine, null, -1, EStoragePurpose.PURPOSE_DEPOSIT, null, removed);
	}
}
