//! Server: dresses AI soldiers from their faction preset's unit loadouts (see AROR_UnitLoadout) as they spawn. Only
//! soldiers whose preset unit names no character are dressed: they spawn as the role's gear-less default for the unit
//! type and get the loadout picked by the role's current Readiness. Units with their own kitted character keep it.
//! A content mod can still put kit on the gear-less defaults through their base-game parents (WCS_Squads gives the
//! base riflemen gloves, a backpack and pouches), so whatever the character wears or carries is removed first and the
//! kit is exactly the loadout. Dressed soldiers are not re-armed by AROR_SoldierArmory: their loadout is authoritative.
//! Hooked from the modded SCR_AIGroup (AROR_ModdedAIGroup.c), so every AI group is covered in one place.
class AROR_AIOutfitter
{
	protected static const int STRIP_DELAY_MS = 100;	//!< After the agent is added, so the character's own items have spawned
	protected static const int DRESS_DELAY_MS = 250;	//!< After stripping, so the freed slots take the new clothing

	protected static ref AROR_AIOutfitter s_Instance;
	protected ref array<IEntity> m_aDressed = {};

	//------------------------------------------------------------------------------------------------
	protected static AROR_AIOutfitter Get()
	{
		if (!s_Instance)
			s_Instance = new AROR_AIOutfitter();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an AI agent joined a group
	static void OnAgentAdded_S(AIAgent agent)
	{
		if (!agent || !Replication.IsServer() || !GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(Get().Strip_S, STRIP_DELAY_MS, false, agent);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the character was dressed from a preset loadout
	static bool WasDressed(IEntity character)
	{
		return character && s_Instance && s_Instance.m_aDressed.Contains(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void Strip_S(AIAgent agent)
	{
		if (!agent)
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (!character || !AROR_ForceSpawner.IsAlive(character) || WasDressed(character))
			return;
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) > 0)
			return;

		AROR_UnitLoadout loadout = PickLoadout(character);
		if (!loadout)
			return;

		for (int i = m_aDressed.Count() - 1; i >= 0; i--)
		{
			if (!m_aDressed[i])
				m_aDressed.Remove(i);
		}
		m_aDressed.Insert(character);

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (inventory)
		{
			Empty(inventory, BaseInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent)));
			Empty(inventory, BaseInventoryStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent)));
			Empty(inventory, BaseInventoryStorageComponent.Cast(character.FindComponent(SCR_EquipmentStorageComponent)));
		}

		GetGame().GetCallqueue().CallLater(Dress_S, DRESS_DELAY_MS, false, character, loadout);
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes what the storage holds (worn clothing with its contents, carried weapons, gadgets) through the inventory
	protected void Empty(SCR_InventoryStorageManagerComponent inventory, BaseInventoryStorageComponent storage)
	{
		if (!storage)
			return;

		array<IEntity> items = {};
		storage.GetAll(items, false);
		foreach (IEntity item : items)
		{
			if (item)
				inventory.TryDeleteItem(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Dress_S(IEntity character, AROR_UnitLoadout loadout)
	{
		if (!character || !loadout || !AROR_ForceSpawner.IsAlive(character))
			return;

		int failed = AROR_LoadoutOutfitter.Apply_S(character, loadout);
		if (failed > 0)
			Print(string.Format("[AROR] AI loadout applied to %1 with %2 pieces missing", SCR_ResourceNameUtils.GetPrefabName(character), failed), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! The loadout for the character: from its role's preset unit whose default character it is, when that unit names
	//! no character of its own and has loadouts. Null otherwise.
	protected AROR_UnitLoadout PickLoadout(SCR_ChimeraCharacter character)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		AROR_EFactionRole role;
		if (!presets || !roles || !roles.GetRoleOfFaction(character.GetFaction(), role))
			return null;

		AROR_EUnitType type;
		if (!presets.FindUnitTypeOf(role, SCR_ResourceNameUtils.GetPrefabName(character), type))
			return null;

		AROR_FactionPreset preset = presets.GetPreset(role);
		if (!preset)
			return null;

		AROR_FactionPresetUnit unit = preset.GetUnit(type);
		if (!unit || unit.HasPrefab() || !unit.HasLoadouts())
			return null;

		return presets.GetUnitLoadout(role, type);
	}
}
