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
	//! A spot on the nearest road to near (within searchRadius), lined up with it. yaw (degrees) runs along the
	//! road. kerb: parked at the roadside, nose in the direction of traffic for that side (right-hand traffic);
	//! otherwise on the carriageway. False when no road is that close or the spot is wet or blocked.
	static bool FindRoadsidePosition(vector near, float searchRadius, bool kerb, out vector position, out float yaw)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return false;
		BaseRoad road;
		float roadDistance;
		aiWorld.GetRoadNetworkManager().GetClosestRoad(near, road, roadDistance);
		if (!road || roadDistance > searchRadius)
			return false;
		array<vector> points = {};
		road.GetPoints(points);
		if (points.Count() < 2)
			return false;

		// The road segment nearest the point, and the foot of the point on it
		int best = -1;
		float bestDistance = float.MAX;
		vector bestFoot;
		for (int index = 0; index + 1 < points.Count(); index++)
		{
			vector start = points[index];
			vector segment = points[index + 1] - start;
			segment[1] = 0;
			float lengthSq = segment[0] * segment[0] + segment[2] * segment[2];
			if (lengthSq < 0.25)
				continue;
			float along = ((near[0] - start[0]) * segment[0] + (near[2] - start[2]) * segment[2]) / lengthSq;
			along = Math.Clamp(along, 0, 1);
			vector foot = start + segment * along;
			float footDistance = vector.DistanceXZ(foot, near);
			if (footDistance < bestDistance)
			{
				bestDistance = footDistance;
				best = index;
				bestFoot = foot;
			}
		}
		if (best < 0)
			return false;

		vector direction = points[best + 1] - points[best];
		direction[1] = 0;
		direction.Normalize();
		vector right = Vector(direction[2], 0, -direction[0]);
		float side = 1;
		if (Math.RandomInt(0, 2) == 0)
			side = -1;
		vector heading = direction * side;
		float offset = 0;
		if (kerb)
		{
			float width = road.GetWidth();
			if (width <= 0)
				width = 6;
			offset = width * 0.5 + 1.2;
		}
		vector spot = bestFoot + right * (offset * side);
		BaseWorld world = GetGame().GetWorld();
		spot[1] = world.GetSurfaceY(spot[0], spot[2]);
		if (spot[1] <= world.GetOceanHeight(spot[0], spot[2]) + 0.5)
			return false;
		if (!IsSpotClear(spot, 1.3, 2.2))
			return false;
		position = spot;
		yaw = heading.ToYaw();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the nearest road's carriageway comes within clearance of the point (the road's half width is added
	//! to the distance the road network reports from its centre line). Roads are terrain, so traces never see them.
	static bool IsNearRoad(vector point, float clearance)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return false;
		BaseRoad road;
		float distance;
		aiWorld.GetRoadNetworkManager().GetClosestRoad(point, road, distance);
		if (!road)
			return false;
		float halfWidth = road.GetWidth() * 0.5;
		if (halfWidth <= 0)
			halfWidth = 3;
		return distance < halfWidth + clearance;
	}

	//------------------------------------------------------------------------------------------------
	//! Nothing standing within halfWidth of the point up to height (terrain excluded)
	static bool IsSpotClear(vector point, float halfWidth, float height)
	{
		BaseWorld world = GetGame().GetWorld();
		TraceBox trace = new TraceBox();
		trace.Start = Vector(point[0], world.GetSurfaceY(point[0], point[2]) + 0.4, point[2]);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-halfWidth, 0, -halfWidth);
		trace.Maxs = Vector(halfWidth, height, halfWidth);
		trace.Flags = TraceFlags.ENTS;
		return world.TraceMove(trace, null) >= 1;
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
	//! A random vehicle of the type from the role's faction preset that its readiness allows (any of the type if none
	//! is allowed yet), or empty if it has none
	static ResourceName VehiclePrefab(AROR_EFactionRole role, AROR_EVehicleType type)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return string.Empty;

		AROR_FactionPreset preset = presets.GetPreset(role);
		if (!preset)
			return string.Empty;

		array<AROR_FactionPresetEntry> entries = {};
		preset.GetEntries(AROR_EFactionPresetCategory.VEHICLES, type, entries);
		if (entries.IsEmpty())
			return string.Empty;

		array<AROR_FactionPresetEntry> allowed = {};
		foreach (AROR_FactionPresetEntry candidate : entries)
		{
			if (AROR_ContentPacksComponent.IsAvailableAtReadiness(candidate.GetPrefab(), role))
				allowed.Insert(candidate);
		}

		if (!allowed.IsEmpty())
			return AROR_FactionPresetsComponent.PickVehicleEntry(allowed);

		return AROR_FactionPresetsComponent.PickVehicleEntry(entries);
	}

	//------------------------------------------------------------------------------------------------
	//! Seats a group's members in a vehicle (driver first, then turrets, then cargo), teleporting them in. Call
	//! until it returns true: seat requests can be dropped, and members spawn over several frames.
	static bool SeatGroup(SCR_AIGroup group, IEntity vehicle, int expectedMembers)
	{
		if (!group || !vehicle)
			return false;

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage || !usage.GetPilotCompartmentSlot())
			return false;

		array<IEntity> members = {};
		AROR_ForceSpawner.CountLiving(group, members);
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

				access.GetInVehicle(vehicle, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
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
}
