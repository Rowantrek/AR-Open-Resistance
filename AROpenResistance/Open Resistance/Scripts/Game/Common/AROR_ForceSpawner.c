//! Server helpers for spawning AI forces outside Operations: faction preset lookups, groups, vehicles, seating,
//! waypoints and road/clear positions. Shared by Missions and Patrols so both use one implementation.
class AROR_ForceSpawner
{
	//------------------------------------------------------------------------------------------------
	static string FactionKey(AROR_EFactionRole role)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return string.Empty;

		return roles.GetFactionKey(role);
	}

	//------------------------------------------------------------------------------------------------
	//! A unit of the type from the role's faction preset, falling back to its rifleman. A unit that names no character
	//! uses the role's default for its type (dressed from the unit's loadouts as it spawns, see AROR_AIOutfitter).
	static ResourceName UnitPrefab(AROR_EFactionRole role, AROR_EUnitType type)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return string.Empty;

		AROR_FactionPreset preset = presets.GetPreset(role);
		if (!preset)
			return string.Empty;

		if (!preset.GetUnit(type))
			type = AROR_EUnitType.RIFLEMAN;

		if (!preset.GetUnit(type))
			return string.Empty;

		return presets.GetUnitPrefab(role, type);
	}

	//------------------------------------------------------------------------------------------------
	//! A random vehicle of the type from the role's faction preset available at the readiness, or empty if none
	static ResourceName VehiclePrefab(AROR_EFactionRole role, AROR_EVehicleType type, int readiness = 5)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return string.Empty;

		AROR_FactionPreset preset = presets.GetPreset(role);
		if (!preset)
			return string.Empty;

		array<AROR_FactionPresetEntry> entries = {};
		preset.GetEntries(AROR_EFactionPresetCategory.VEHICLES, type, entries);
		for (int i = entries.Count() - 1; i >= 0; i--)
		{
			if (!entries[i] || entries[i].GetPrefab().IsEmpty() || AROR_ContentPacksComponent.GetMinimumReadinessOf(entries[i].GetPrefab()) > readiness)
				entries.Remove(i);
		}

		if (entries.IsEmpty())
			return string.Empty;

		return entries.GetRandomElement().GetPrefab();
	}

	//------------------------------------------------------------------------------------------------
	//! A team for the role: a leader and riflemen with an automatic rifleman and a machine gunner mixed in, plus AT or AA
	//! specialists for the enemy roles (AROR_InfantryRoster)
	static void BuildTeam(AROR_EFactionRole role, int count, notnull array<ResourceName> outMembers)
	{
		outMembers.Clear();
		for (int i = 0; i < count; i++)
		{
			AROR_EUnitType type = AROR_EUnitType.RIFLEMAN;
			if (i == 0)
				type = AROR_EUnitType.LEADER;
			else if (i == 2)
				type = AROR_EUnitType.AUTORIFLEMAN;
			else if (i == 4)
				type = AROR_EUnitType.MACHINEGUNNER;

			ResourceName prefab = UnitPrefab(role, type);
			if (!prefab.IsEmpty())
				outMembers.Insert(prefab);
		}

		AROR_InfantryRoster.AddSpecialists_S(role, outMembers);
	}

	//------------------------------------------------------------------------------------------------
	//! An AI group of the role with these members (spawned through the engine's queue, not all this frame)
	static SCR_AIGroup SpawnGroup(ResourceName groupPrefab, AROR_EFactionRole role, notnull array<ResourceName> members, vector position)
	{
		if (members.IsEmpty())
			return null;

		string factionKey = FactionKey(role);
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
		if (!faction)
			return null;

		SCR_AIGroup.IgnoreSpawning(true);
		IEntity entity = SpawnPrefab(groupPrefab, position, 0);
		SCR_AIGroup.IgnoreSpawning(false);

		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);

			return null;
		}

		bool assigned;
		if (!group.GetFaction())
			assigned = group.InitFactionKey(factionKey);
		else
			assigned = group.SetFaction(faction);

		if (!assigned)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return null;
		}

		group.SetCanDeleteIfNoPlayer(false);
		group.SetPrivate(true);
		group.SetPrivacyChangeable(false);
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);

		if (!group.m_aUnitPrefabSlots)
			group.m_aUnitPrefabSlots = {};

		group.m_aUnitPrefabSlots.Clear();
		foreach (ResourceName member : members)
		{
			group.m_aUnitPrefabSlots.Insert(member);
		}

		group.RequestSpawn(members.Count());
		return group;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle affiliated with the role (yaw in degrees)
	static IEntity SpawnVehicle(AROR_EFactionRole role, ResourceName prefab, vector position, float yaw)
	{
		IEntity vehicle = SpawnPrefab(prefab, position, yaw);
		if (!vehicle)
			return null;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		string factionKey = FactionKey(role);
		if (affiliation && !factionKey.IsEmpty())
			affiliation.SetAffiliatedFactionByKey(factionKey);

		return vehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a prefab on the terrain surface (yaw in degrees)
	static IEntity SpawnPrefab(ResourceName prefab, vector position, float yaw)
	{
		if (prefab.IsEmpty())
			return null;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		BaseWorld world = GetGame().GetWorld();
		position[1] = world.GetSurfaceY(position[0], position[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector rotation[3];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), rotation);
		params.Transform[0] = rotation[0];
		params.Transform[1] = rotation[1];
		params.Transform[2] = rotation[2];
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(resource, world, params);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the group a waypoint of the prefab at the position
	static AIWaypoint AddWaypoint(SCR_AIGroup group, ResourceName waypointPrefab, vector position, float radius)
	{
		if (!group)
			return null;

		AIWaypoint waypoint = AIWaypoint.Cast(SpawnPrefab(waypointPrefab, position, 0));
		if (!waypoint)
			return null;

		waypoint.SetCompletionRadius(radius);
		group.AddWaypoint(waypoint);
		return waypoint;
	}

	//------------------------------------------------------------------------------------------------
	//! A clear spot on dry land within radius of centre with room for an object of the clearance radius
	static bool FindClearPosition(vector centre, float radius, float clearance, out vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		for (int attempt = 0; attempt < 6; attempt++)
		{
			vector found;
			if (!SCR_WorldTools.FindEmptyTerrainPosition(found, centre, radius, clearance, 3))
				continue;

			if (world.GetSurfaceY(found[0], found[2]) <= world.GetOceanHeight(found[0], found[2]) + 0.5)
				continue;

			position = found;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A reachable road point near a position (searching towards goal)
	static bool FindRoadPosition(vector near, vector goal, out vector position)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return false;

		BaseWorld world = GetGame().GetWorld();
		near[1] = world.GetSurfaceY(near[0], near[2]);
		goal[1] = world.GetSurfaceY(goal[0], goal[2]);
		return aiWorld.GetRoadNetworkManager().GetReachableWaypointInRoad(near, goal, 150, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Living members of a group; new members are given the role's campaign faction once (presets spawn with their
	//! native faction)
	static int CountAlive(SCR_AIGroup group, AROR_EFactionRole role, notnull array<IEntity> affiliated)
	{
		if (!group)
			return 0;

		string factionKey = FactionKey(role);
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		int alive;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!IsAlive(entity))
				continue;

			alive++;
			if (affiliated.Contains(entity))
				continue;

			affiliated.Insert(entity);
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
			if (affiliation && !factionKey.IsEmpty() && affiliation.GetAffiliatedFactionKey() != factionKey)
				affiliation.SetAffiliatedFactionByKey(factionKey);
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	//! Living members of a group (no affiliation changes)
	static int CountLiving(SCR_AIGroup group, notnull array<IEntity> outMembers)
	{
		outMembers.Clear();
		if (!group)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (IsAlive(entity))
				outMembers.Insert(entity);
		}

		return outMembers.Count();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAlive(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && !controller.IsDead();
	}

	//------------------------------------------------------------------------------------------------
	//! Gone or wrecked
	static bool IsDestroyed(IEntity entity)
	{
		if (!entity)
			return true;

		DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		return damage && damage.IsDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle with a player in one of its seats
	static bool HasPlayerAboard(IEntity entity)
	{
		if (!entity)
			return false;

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return false;

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		PlayerManager playerManager = GetGame().GetPlayerManager();
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && slot.GetOccupant() && playerManager.GetPlayerIdFromControlledEntity(slot.GetOccupant()) > 0)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Driver and turret seats
	static int CountCrewSeats(IEntity vehicle)
	{
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return 1;

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		int seats = 1;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && TurretCompartmentSlot.Cast(slot) && slot.IsCompartmentAccessible())
				seats++;
		}

		return seats;
	}

	//------------------------------------------------------------------------------------------------
	static int CountCargoSeats(IEntity vehicle)
	{
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return 0;

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		int seats;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && CargoCompartmentSlot.Cast(slot) && slot.IsCompartmentAccessible())
				seats++;
		}

		return seats;
	}

	//------------------------------------------------------------------------------------------------
	//! Seats a group's members in a vehicle (driver first, then turrets, then cargo), teleporting them in. Called
	//! every update until it returns true: seat requests can be dropped, and members spawn over several frames.
	//! initialPlacement: the first seating of a vehicle spawned away from players. Places into the seat's own entity
	//! with no door action, also while the actor is paused, as Operations does (the door-closing request is refused
	//! for vehicles spawned away from players).
	static bool SeatGroup(SCR_AIGroup group, IEntity vehicle, int expectedMembers, bool initialPlacement = false)
	{
		if (!group || !vehicle)
			return false;

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage || !usage.GetPilotCompartmentSlot())
			return false;

		array<IEntity> members = {};
		CountLiving(group, members);
		if (members.Count() < expectedMembers)
			return false;

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		array<BaseCompartmentSlot> ordered = {usage.GetPilotCompartmentSlot()};
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && TurretCompartmentSlot.Cast(slot) && !ordered.Contains(slot))
				ordered.Insert(slot);
		}

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && CargoCompartmentSlot.Cast(slot) && !ordered.Contains(slot))
				ordered.Insert(slot);
		}

		bool allSeated = true;
		foreach (IEntity member : members)
		{
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(member.FindComponent(CompartmentAccessComponent));
			if (!access)
			{
				allSeated = false;
				continue;
			}

			BaseCompartmentSlot current = access.GetCompartment();
			if (access.IsInCompartment() && current && current.GetVehicle() == vehicle)
				continue;

			allSeated = false;
			if (access.IsGettingIn() || access.IsGettingOut() || access.IsInCompartment())
				continue;

			foreach (BaseCompartmentSlot seat : ordered)
			{
				if (!seat.IsCompartmentAccessible() || seat.IsOccupied() || seat.GetOccupant())
					continue;

				if (initialPlacement)
				{
					IEntity seatEntity = seat.GetOwner();
					if (!seatEntity)
						seatEntity = vehicle;

					if (!access.GetInVehicle(seatEntity, seat, true, -1, ECloseDoorAfterActions.INVALID, true))
						access.GetInVehicle(seatEntity, seat, true, -1, ECloseDoorAfterActions.INVALID, false);
				}
				else
				{
					access.GetInVehicle(vehicle, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
				}

				break;
			}
		}

		if (!allSeated || !usage.GetPilotCompartmentSlot().GetOccupant())
			return false;

		SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
		if (utility)
			utility.AddUsableVehicle(usage);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes a group and its members, living or not. Deleting the group alone also cancels its outstanding native
	//! spawn requests, but leaves already spawned members behind.
	static void DeleteGroupAndMembers(SCR_AIGroup group)
	{
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		array<IEntity> members = {};
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetControlledEntity())
				members.Insert(agent.GetControlledEntity());
		}

		SCR_EntityHelper.DeleteEntityAndChildren(group);
		foreach (IEntity member : members)
		{
			if (member)
				SCR_EntityHelper.DeleteEntityAndChildren(member);
		}
	}
}
