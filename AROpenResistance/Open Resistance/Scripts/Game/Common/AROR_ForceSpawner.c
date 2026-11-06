//! Server helpers for spawning AI forces: groups, vehicles, waypoints, clear and road positions, and living counts.
//! Shared by Missions and High Command (and available to extender mods). Soldiers are always requested by unit type
//! and go through AROR_FactionPresetsComponent.FillGroup_S, so they spawn as the role's base character and are dressed
//! from the role's faction preset loadout at its current Readiness.
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
	//! An AI group of the role with one member per unit type, each spawned as the role's base character and dressed
	//! from its preset loadout at the role's current Readiness (through the engine's queue, not all this frame).
	//! Null if the group prefab, faction or any character cannot be set up.
	static SCR_AIGroup SpawnGroup(ResourceName groupPrefab, AROR_EFactionRole role, notnull array<AROR_EUnitType> types, vector position)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (types.IsEmpty() || !presets)
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

		int slots = presets.FillGroup_S(group, role, types);
		if (slots < 1)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return null;
		}

		group.RequestSpawn(slots);
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
}
