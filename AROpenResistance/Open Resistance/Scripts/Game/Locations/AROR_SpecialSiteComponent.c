[ComponentEditorProps(category: "OpenResistance/Locations", description: "Special site: a built-up, high-value installation. Pays its holder extra income, is dressed with a steel-plated yard and base buildings, has helipads with parked helicopters while an AI army holds it, and keeps soldiers and a truck moving around it. Put on a location next to its garrison.")]
class AROR_SpecialSiteComponentClass : ScriptComponentClass {}

//! One piece of a special site's dressing, placed in the site's own frame (x right, z forward of the location entity)
[BaseContainerProps()]
class AROR_SpecialSiteProp
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefab or composition to place", params: "et")]
	ResourceName m_sPrefab;
	[Attribute("0 0 0", desc: "Offset from the location entity in its own frame (x right, z forward), metres. Height is ignored: placed on the ground.")]
	vector m_vOffset;
	[Attribute("0", desc: "Turn relative to the location entity, degrees", params: "-360 360 1")]
	float m_fYaw;
	[Attribute("0", desc: "A helipad: while an AI army holds the site, one of its helicopters is parked on it")]
	bool m_bHelipad;
	[Attribute("1", desc: "Soldiers at the site walk over to it while they go about the base")]
	bool m_bWorkPoint;
}

//! A high-value site (Chicken: Stone Ridge Base).
//! Every machine: at play start the authored sandbags are removed and the base is built locally from fixed offsets
//! (a plated yard and the props list), so every player sees the same base without replicating hundreds of props.
//! Server: extra income for the holder; while an AI army holds it and players are near, a parked helicopter on each
//! helipad, soldiers walking between the buildings, fire teams walking a loop outside the walls and a truck driving
//! laps round the base. Lost helicopters, teams and the truck return after the respawn time. Nothing is saved.
class AROR_SpecialSiteComponent : ScriptComponent
{
	[Attribute("1500", desc: "Extra money paid to the holder every income payment", params: "0 100000 50", category: "Income")]
	protected int m_iMoneyPerPayment;
	[Attribute("3", desc: "Extra Resistance manpower every income payment while the Resistance holds it", params: "0 50 1", category: "Income")]
	protected int m_iManpowerPerPayment;

	[Attribute("1", desc: "Remove the location's authored sandbag walls and positions (direct children whose prefab is a sandbag) at play start", category: "Layout")]
	protected bool m_bRemoveSandbags;
	[Attribute("{CABCA96DA5450B05}Prefabs/Structures/Military/Camps/AirfieldMatting_01/AirfieldMatting_US_01_long.et", UIWidgets.ResourceNamePicker, desc: "Ground plating laid edge to edge over the yard (empty: none). Covers the grass.", params: "et", category: "Layout")]
	protected ResourceName m_sFloorPrefab;
	[Attribute("-37 0 -43", desc: "Yard plating: corner in the site's frame, metres", category: "Layout")]
	protected vector m_vFloorMin;
	[Attribute("31 0 40", desc: "Yard plating: opposite corner in the site's frame, metres", category: "Layout")]
	protected vector m_vFloorMax;
	[Attribute(desc: "Buildings, props and helipads placed at play start", category: "Layout")]
	protected ref array<ref AROR_SpecialSiteProp> m_aProps;

	[Attribute("", UIWidgets.ComboBox, desc: "Helicopter parked on each helipad while an AI army holds the site, in pad order (empty: transport helicopters). Taken from the holder's faction preset.", enums: ParamEnumArray.FromEnum(AROR_EVehicleType), category: "Activity")]
	protected ref array<AROR_EVehicleType> m_aHelicopterTypes;
	[Attribute("3", desc: "Pairs of soldiers walking between the base's buildings", params: "0 8 1", category: "Activity")]
	protected int m_iWorkerPairs;
	[Attribute("2", desc: "Fire teams walking a loop outside the walls", params: "0 6 1", category: "Activity")]
	protected int m_iFootPatrols;
	[Attribute("75", desc: "Radius of the loop round the base walked by the fire teams and driven by the truck, metres", params: "20 400 5", category: "Activity")]
	protected float m_fLoopRadius;
	[Attribute("1", desc: "A truck (or car) driving laps round the base", category: "Activity")]
	protected bool m_bSupplyTruck;
	[Attribute("25", desc: "The truck's top speed, km/h", params: "5 80 1", category: "Activity")]
	protected float m_fTruckSpeed;

	[Attribute("1300", desc: "Helicopters and soldiers turn out while a player is this close, metres", params: "300 3000 50", category: "Activation")]
	protected float m_fActivationDistance;
	[Attribute("1600", desc: "...and are removed once no player has been this close for half a minute, metres", params: "400 4000 50", category: "Activation")]
	protected float m_fRetireDistance;
	[Attribute("1800", desc: "Seconds before a lost helicopter, team or truck is replaced", params: "120 14400 60", category: "Activation")]
	protected int m_iRespawnSeconds;
	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty AI group prefab", params: "et", category: "Activation")]
	protected ResourceName m_sGroupPrefab;
	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, params: "et", category: "Activation")]
	protected ResourceName m_sMoveWaypointPrefab;

	protected static const int TICK_SECONDS = 2;
	protected static const int RETIRE_DELAY = 30;
	protected static const int LOOP_POINTS = 8;
	protected static const int MAX_FLOOR_TILES = 900;
	protected static const float PAD_MOVED = 30;		//!< A helicopter this far off its pad has been taken
	protected static const float SPAWN_CLEARANCE = 40;	//!< Soldiers and vehicles appear at the spot furthest from players, at least this far if possible

	protected AROR_LocationComponent m_Location;
	protected bool m_bBuilt;
	protected bool m_bActive;
	protected int m_iClock;
	protected int m_iFarSince = -1;
	protected AROR_EFactionRole m_eRole;
	protected ref array<IEntity> m_aDressing = {};		//!< Every machine: props and plating placed here (local entities)
	protected ref array<vector> m_aPads = {};
	protected ref array<float> m_aPadYaws = {};
	protected ref array<IEntity> m_aPadEntities = {};
	protected ref array<vector> m_aWorkPoints = {};
	protected ref array<IEntity> m_aHelicopters = {};	//!< Server, parallel to the pads; null when none stands there
	protected ref array<int> m_aHelicopterLostAt = {};	//!< Parallel: clock when its helicopter was lost, -1 none
	protected ref array<ref AROR_SpecialSiteGroup> m_aPatrols = {};
	protected ref array<ref AROR_SpecialSiteGroup> m_aWorkers = {};
	protected ref array<int> m_aPatrolLostAt = {};
	protected ref array<int> m_aWorkerLostAt = {};
	protected ref AROR_SpecialSiteTruck m_Truck;
	protected int m_iTruckLostAt = -1;

	//------------------------------------------------------------------------------------------------
	int GetMoneyPerPayment() { return m_iMoneyPerPayment; }
	int GetManpowerPerPayment() { return m_iManpowerPerPayment; }

	//------------------------------------------------------------------------------------------------
	//! Every machine, once: sandbags out, plating and buildings in. Fixed offsets, so all machines build the same base.
	protected void Build()
	{
		if (m_bBuilt)
			return;
		m_bBuilt = true;

		IEntity owner = GetOwner();
		if (m_bRemoveSandbags)
			RemoveSandbags(owner);

		int tiles = LayFloor(owner);
		int props = PlaceProps(owner);
		Print(string.Format("[AROR SITE] %1 built: %2 plates, %3 props, %4 helipads", m_Location.GetId(), tiles, props, m_aPads.Count()), LogLevel.NORMAL);
	}

	protected void RemoveSandbags(IEntity owner)
	{
		array<IEntity> doomed = {};
		IEntity child = owner.GetChildren();
		while (child)
		{
			EntityPrefabData data = child.GetPrefabData();
			if (data)
			{
				string name = data.GetPrefabName();
				name.ToLower();
				if (name.Contains("sandbag"))
					doomed.Insert(child);
			}
			child = child.GetSibling();
		}

		foreach (IEntity sandbag : doomed)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(sandbag);
		}
	}

	//! Plates laid edge to edge over the yard rectangle, measured from the prefab itself, each levelled on the ground
	protected int LayFloor(IEntity owner)
	{
		if (m_sFloorPrefab.IsEmpty())
			return 0;

		float yaw = owner.GetYawPitchRoll()[0];
		IEntity probe = SpawnLocal(m_sFloorPrefab, owner.GetOrigin(), yaw);
		if (!probe)
			return 0;

		vector mins, maxs;
		probe.GetBounds(mins, maxs);
		SCR_EntityHelper.DeleteEntityAndChildren(probe);
		float sizeX = maxs[0] - mins[0];
		float sizeZ = maxs[2] - mins[2];
		if (sizeX < 0.5 || sizeZ < 0.5)
			return 0;

		vector centreOffset = (mins + maxs) * 0.5;
		centreOffset[1] = 0;
		vector shift = owner.VectorToParent(centreOffset);
		float left = Math.Min(m_vFloorMin[0], m_vFloorMax[0]);
		float right = Math.Max(m_vFloorMin[0], m_vFloorMax[0]);
		float back = Math.Min(m_vFloorMin[2], m_vFloorMax[2]);
		float front = Math.Max(m_vFloorMin[2], m_vFloorMax[2]);
		int columns = Math.Ceil((right - left) / sizeX);
		int rows = Math.Ceil((front - back) / sizeZ);
		BaseWorld world = GetGame().GetWorld();
		int laid;
		for (int column = 0; column < columns; column++)
		{
			for (int row = 0; row < rows; row++)
			{
				if (laid >= MAX_FLOOR_TILES)
					return laid;

				vector local = Vector(left + (column + 0.5) * sizeX, 0, back + (row + 0.5) * sizeZ);
				vector centre = owner.CoordToParent(local);
				// Level on the average of its corners so it neither floats nor sinks on a gentle slope
				float height;
				for (int corner = 0; corner < 4; corner++)
				{
					float cornerX = sizeX * 0.5;
					if (corner == 1 || corner == 2)
						cornerX = -cornerX;
					float cornerZ = sizeZ * 0.5;
					if (corner >= 2)
						cornerZ = -cornerZ;
					vector at = owner.CoordToParent(local + Vector(cornerX, 0, cornerZ));
					height += world.GetSurfaceY(at[0], at[2]);
				}

				vector origin = centre - shift;
				origin[1] = height * 0.25 + 0.02;
				IEntity tile = SpawnLocal(m_sFloorPrefab, origin, yaw);
				if (!tile)
					return laid;

				m_aDressing.Insert(tile);
				laid++;
			}
		}

		return laid;
	}

	protected int PlaceProps(IEntity owner)
	{
		if (!m_aProps)
			return 0;

		float siteYaw = owner.GetYawPitchRoll()[0];
		vector centre = owner.GetOrigin();
		BaseWorld world = GetGame().GetWorld();
		int placed;
		foreach (AROR_SpecialSiteProp prop : m_aProps)
		{
			if (!prop || prop.m_sPrefab.IsEmpty())
				continue;

			vector local = prop.m_vOffset;
			local[1] = 0;
			vector position = owner.CoordToParent(local);
			position[1] = world.GetSurfaceY(position[0], position[2]);
			float yaw = siteYaw + prop.m_fYaw;
			IEntity entity = SpawnLocal(prop.m_sPrefab, position, yaw);
			if (!entity)
			{
				Print(string.Format("[AROR SITE] %1 could not place %2", m_Location.GetId(), prop.m_sPrefab), LogLevel.WARNING);
				continue;
			}

			m_aDressing.Insert(entity);
			placed++;
			if (prop.m_bHelipad)
			{
				m_aPads.Insert(position);
				m_aPadYaws.Insert(yaw);
				m_aPadEntities.Insert(entity);
				m_aHelicopters.Insert(null);
				m_aHelicopterLostAt.Insert(-1);
			}

			if (prop.m_bWorkPoint)
			{
				// Beside it, on the side facing the middle of the base
				vector toCentre = centre - position;
				toCentre[1] = 0;
				float distance = toCentre.Length();
				if (distance > 1)
					position = position + toCentre * (Math.Min(6, distance) / distance);
				m_aWorkPoints.Insert(position);
			}
		}

		return placed;
	}

	//! Spawned on this machine only (yaw in degrees, position used as given)
	protected static IEntity SpawnLocal(ResourceName prefab, vector position, float yaw)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector rotation[3];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), rotation);
		params.Transform[0] = rotation[0];
		params.Transform[1] = rotation[1];
		params.Transform[2] = rotation[2];
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Server
	protected void Tick()
	{
		if (!Replication.IsServer() || !m_Location || !m_bBuilt)
			return;

		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!commanders || !commanders.IsCampaignActive())
			return;

		m_iClock += TICK_SECONDS;
		vector centre = m_Location.GetCenter();
		if (!m_bActive)
		{
			if (NearestPlayer(centre) <= m_fActivationDistance)
			{
				m_bActive = true;
				m_iFarSince = -1;
				Print(string.Format("[AROR SITE] %1 active (holder role %2)", m_Location.GetId(), m_Location.GetControllingRole()), LogLevel.NORMAL);
			}
			return;
		}

		if (NearestPlayer(centre) <= m_fRetireDistance)
			m_iFarSince = -1;
		else if (m_iFarSince < 0)
			m_iFarSince = m_iClock;
		else if (m_iClock - m_iFarSince >= RETIRE_DELAY)
		{
			Retire();
			return;
		}

		AROR_EFactionRole role = m_Location.GetControllingRole();
		if (role != m_eRole)
		{
			// A new holder: the old army's helicopters and soldiers are left where they are, no longer this site's
			Forget();
			m_eRole = role;
		}

		if (!AROR_GarrisonPolicy.IsEnemyRole(role))
			return;

		UpdateHelicopters(role);
		UpdateGroups(role, m_aWorkers, m_aWorkerLostAt, m_iWorkerPairs, true);
		UpdateGroups(role, m_aPatrols, m_aPatrolLostAt, m_iFootPatrols, false);
		UpdateTruck(role);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHelicopters(AROR_EFactionRole role)
	{
		foreach (int index, vector pad : m_aPads)
		{
			IEntity helicopter = m_aHelicopters[index];
			if (helicopter)
			{
				bool gone = AROR_ForceSpawner.IsDestroyed(helicopter) || AROR_ResistanceFleet.IsClaimed(helicopter) || vector.DistanceXZ(helicopter.GetOrigin(), pad) > PAD_MOVED;
				if (!gone)
					continue;

				Print(string.Format("[AROR SITE] %1 helicopter on pad %2 lost or taken", m_Location.GetId(), index), LogLevel.NORMAL);
				m_aHelicopters[index] = null;
				m_aHelicopterLostAt[index] = m_iClock;
				continue;
			}

			int lost = m_aHelicopterLostAt[index];
			if (lost >= 0 && m_iClock - lost < m_iRespawnSeconds)
				continue;

			// Not in front of a player standing on the pad
			if (NearestPlayer(pad) < 15)
				continue;

			ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(role, HelicopterType(index));
			if (prefab.IsEmpty())
				prefab = AROR_ForceSpawner.VehiclePrefab(role, AROR_EVehicleType.HELI_TRANSPORT);
			if (prefab.IsEmpty())
			{
				Print(string.Format("[AROR SITE] %1 pad %2: the holder's preset has no helicopter", m_Location.GetId(), index), LogLevel.WARNING);
				m_aHelicopterLostAt[index] = m_iClock;
				continue;
			}

			IEntity spawned = AROR_ForceSpawner.SpawnVehicle(role, prefab, pad, m_aPadYaws[index]);
			if (!spawned)
			{
				m_aHelicopterLostAt[index] = m_iClock;
				continue;
			}

			RaiseOntoDeck(spawned, pad);
			m_aHelicopters[index] = spawned;
			m_aHelicopterLostAt[index] = -1;
			Print(string.Format("[AROR SITE] %1 pad %2: %3 parked", m_Location.GetId(), index, prefab), LogLevel.NORMAL);
		}
	}

	protected AROR_EVehicleType HelicopterType(int index)
	{
		if (m_aHelicopterTypes && index < m_aHelicopterTypes.Count())
			return m_aHelicopterTypes[index];

		return AROR_EVehicleType.HELI_TRANSPORT;
	}

	//! Spawned on the terrain; lifted onto whatever deck the pad has
	protected void RaiseOntoDeck(IEntity vehicle, vector pad)
	{
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(pad[0], pad[2]);
		TraceParam trace = new TraceParam();
		trace.Start = Vector(pad[0], ground + 3, pad[2]);
		trace.End = Vector(pad[0], ground - 0.5, pad[2]);
		trace.Flags = TraceFlags.ENTS;
		trace.Exclude = vehicle;
		float fraction = world.TraceMove(trace, null);
		if (fraction >= 1 || !m_aPadEntities.Contains(trace.TraceEnt))
			return;

		float deck = trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction;
		if (deck <= ground + 0.05)
			return;

		vector origin = vehicle.GetOrigin();
		origin[1] = origin[1] + (deck - ground);
		vehicle.SetOrigin(origin);
		vehicle.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Workers (pairs walking between the base's buildings) or patrols (fire teams walking the outer loop)
	protected void UpdateGroups(AROR_EFactionRole role, array<ref AROR_SpecialSiteGroup> groups, array<int> lostAt, int wanted, bool workers)
	{
		if (workers && m_aWorkPoints.IsEmpty())
			return;

		for (int index = groups.Count() - 1; index >= 0; index--)
		{
			AROR_SpecialSiteGroup entry = groups[index];
			array<IEntity> living = {};
			int alive = AROR_ForceSpawner.CountLiving(entry.m_Group, living);
			if (!entry.m_Group || (alive == 0 && (entry.m_bSeen || m_iClock - entry.m_iRaisedAt > 60)))
			{
				// Also after a failed spawn, so a broken preset never retries every tick
				lostAt.Insert(m_iClock);
				entry.Remove_S();
				groups.Remove(index);
				continue;
			}

			if (alive > 0)
				entry.m_bSeen = true;

			bool arrived = alive > 0 && vector.DistanceXZ(living[0].GetOrigin(), entry.m_vGoal) < 8;
			if (m_iClock < entry.m_iNextOrderAt && !arrived)
				continue;

			if (workers)
			{
				// Stop a while at each building, then on to another
				if (arrived && entry.m_iLingerUntil == 0)
				{
					entry.m_iLingerUntil = m_iClock + Math.RandomInt(20, 50);
					continue;
				}
				if (entry.m_iLingerUntil > 0 && m_iClock < entry.m_iLingerUntil)
					continue;

				entry.m_iLingerUntil = 0;
				entry.m_vGoal = m_aWorkPoints.GetRandomElement();
				entry.m_iNextOrderAt = m_iClock + 90;
			}
			else
			{
				entry.m_iPoint = (entry.m_iPoint + entry.m_iDirection + LOOP_POINTS) % LOOP_POINTS;
				entry.m_vGoal = LoopPoint(entry.m_iPoint);
				entry.m_iNextOrderAt = m_iClock + 120;
			}
			entry.Order_S(m_sMoveWaypointPrefab, entry.m_vGoal, 4);
		}

		for (int lost = lostAt.Count() - 1; lost >= 0; lost--)
		{
			if (m_iClock - lostAt[lost] >= m_iRespawnSeconds)
				lostAt.Remove(lost);
		}

		if (groups.Count() + lostAt.Count() >= wanted)
			return;

		// One new group per tick, at the spot furthest from any player
		array<vector> spots = {};
		if (workers)
			spots.InsertAll(m_aWorkPoints);
		else
		{
			for (int point = 0; point < LOOP_POINTS; point++)
			{
				spots.Insert(LoopPoint(point));
			}
		}

		int start = FurthestFromPlayers(spots);
		vector spawn = spots[start];
		vector clear;
		if (AROR_ForceSpawner.FindClearPosition(spawn, 10, 1, clear))
			spawn = clear;

		array<AROR_EUnitType> units = {};
		if (workers)
		{
			units.Insert(AROR_EUnitType.RIFLEMAN);
			units.Insert(AROR_EUnitType.RIFLEMAN);
		}
		else
			AROR_Formations.GetUnits(AROR_EFormation.RIFLE_FIRE_TEAM, units);

		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, role, units, spawn);
		if (!group)
			return;

		AROR_SpecialSiteGroup added = new AROR_SpecialSiteGroup();
		added.m_Group = group;
		added.m_iRaisedAt = m_iClock;
		added.m_iPoint = start;
		added.m_vGoal = spawn;
		added.m_iDirection = 1;
		if (groups.Count() % 2 == 1)
			added.m_iDirection = -1;
		added.m_iNextOrderAt = m_iClock + 6;
		groups.Insert(added);
		string kind = "foot patrol";
		if (workers)
			kind = "work detail";
		Print(string.Format("[AROR SITE] %1 %2 raised for role %3", m_Location.GetId(), kind, role), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! A truck with a driver and a passenger driving laps of the outer loop
	protected void UpdateTruck(AROR_EFactionRole role)
	{
		if (m_Truck)
		{
			array<IEntity> crew = {};
			int alive = AROR_ForceSpawner.CountLiving(m_Truck.m_Group, crew);
			bool lost = !m_Truck.m_Vehicle || AROR_ForceSpawner.IsDestroyed(m_Truck.m_Vehicle) || AROR_ResistanceFleet.IsClaimed(m_Truck.m_Vehicle);
			if (m_Truck.m_bSeated && alive == 0)
				lost = true;
			if (!m_Truck.m_bSeated && m_iClock - m_Truck.m_iRaisedAt > 90)
			{
				Print(string.Format("[AROR SITE] %1 truck crew could not board; removed", m_Location.GetId()), LogLevel.WARNING);
				lost = true;
			}

			if (lost)
			{
				m_Truck.Remove_S();
				m_Truck = null;
				m_iTruckLostAt = m_iClock;
				return;
			}

			if (!m_Truck.m_bSeated)
			{
				int expected = 2;
				if (m_iClock - m_Truck.m_iRaisedAt > 30)
					expected = Math.Max(alive, 1);
				if (AROR_ForceSpawner.SeatGroup(m_Truck.m_Group, m_Truck.m_Vehicle, expected))
				{
					m_Truck.m_bSeated = true;
					m_Truck.m_iNextOrderAt = m_iClock;
				}
				return;
			}

			bool arrived = vector.DistanceXZ(m_Truck.m_Vehicle.GetOrigin(), m_Truck.m_vGoal) < 25;
			if (m_iClock >= m_Truck.m_iNextOrderAt || arrived)
			{
				m_Truck.m_iPoint = (m_Truck.m_iPoint + 1) % LOOP_POINTS;
				m_Truck.m_vGoal = LoopPoint(m_Truck.m_iPoint);
				m_Truck.Order_S(m_sMoveWaypointPrefab, m_Truck.m_vGoal, 20);
				m_Truck.m_iNextOrderAt = m_iClock + 120;
				AROR_ForceSpawner.SetCruiseSpeed(m_Truck.m_Vehicle, m_Truck.m_Group, m_fTruckSpeed);
			}
			return;
		}

		if (!m_bSupplyTruck || (m_iTruckLostAt >= 0 && m_iClock - m_iTruckLostAt < m_iRespawnSeconds))
			return;

		ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(role, AROR_EVehicleType.MOTOR_TRUCK);
		if (prefab.IsEmpty())
			prefab = AROR_ForceSpawner.VehiclePrefab(role, AROR_EVehicleType.MOTOR_CAR);
		if (prefab.IsEmpty())
		{
			m_iTruckLostAt = m_iClock;
			return;
		}

		array<vector> spots = {};
		for (int point = 0; point < LOOP_POINTS; point++)
		{
			spots.Insert(LoopPoint(point));
		}
		int start = FurthestFromPlayers(spots);
		vector position;
		float yaw;
		if (!AROR_ForceSpawner.FindRoadsidePosition(spots[start], 40, false, position, yaw))
		{
			if (!AROR_ForceSpawner.FindClearPosition(spots[start], 25, 5, position))
				return;
			yaw = Math.RandomFloat(0, 360);
		}

		IEntity vehicle = AROR_ForceSpawner.SpawnVehicle(role, prefab, position, yaw);
		if (!vehicle)
		{
			m_iTruckLostAt = m_iClock;
			return;
		}

		array<AROR_EUnitType> units = {AROR_EUnitType.CREW, AROR_EUnitType.RIFLEMAN};
		vector side = position;
		vector clear;
		if (AROR_ForceSpawner.FindClearPosition(position, 10, 1, clear))
			side = clear;
		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, role, units, side);
		if (!group)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			m_iTruckLostAt = m_iClock;
			return;
		}

		m_Truck = new AROR_SpecialSiteTruck();
		m_Truck.m_Vehicle = vehicle;
		m_Truck.m_Group = group;
		m_Truck.m_iRaisedAt = m_iClock;
		m_Truck.m_iPoint = start;
		m_Truck.m_vGoal = position;
		Print(string.Format("[AROR SITE] %1 truck raised for role %2: %3", m_Location.GetId(), role, prefab), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected vector LoopPoint(int point)
	{
		float bearing = point * Math.PI2 / LOOP_POINTS;
		vector spot = m_Location.GetCenter() + Vector(Math.Sin(bearing), 0, Math.Cos(bearing)) * m_fLoopRadius;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		return spot;
	}

	//! Distance to the nearest player (a large number when there is none)
	protected float NearestPlayer(vector position)
	{
		float nearest = 100000;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (player)
				nearest = Math.Min(nearest, vector.DistanceXZ(player.GetOrigin(), position));
		}

		return nearest;
	}

	//! Index of the spot furthest from every player (players often stand inside the base, so never wait for a spot
	//! nobody can see)
	protected int FurthestFromPlayers(notnull array<vector> spots)
	{
		int best;
		float bestDistance = -1;
		foreach (int index, vector spot : spots)
		{
			float distance = NearestPlayer(spot);
			if (distance > bestDistance)
			{
				best = index;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Nobody near: soldiers and the truck go, and helicopters still on their pads (unclaimed) are put away
	protected void Retire()
	{
		m_bActive = false;
		foreach (AROR_SpecialSiteGroup patrol : m_aPatrols)
		{
			patrol.Remove_S();
		}
		m_aPatrols.Clear();

		foreach (AROR_SpecialSiteGroup worker : m_aWorkers)
		{
			worker.Remove_S();
		}
		m_aWorkers.Clear();

		if (m_Truck)
			m_Truck.Remove_S();
		m_Truck = null;

		foreach (int index, IEntity helicopter : m_aHelicopters)
		{
			if (helicopter && !AROR_ForceSpawner.IsDestroyed(helicopter) && !AROR_ResistanceFleet.IsClaimed(helicopter) && vector.DistanceXZ(helicopter.GetOrigin(), m_aPads[index]) <= PAD_MOVED)
				SCR_EntityHelper.DeleteEntityAndChildren(helicopter);
			m_aHelicopters[index] = null;
		}

		Print(string.Format("[AROR SITE] %1 retired", m_Location.GetId()), LogLevel.NORMAL);
	}

	//! Control changed while players are here: stop managing the old holder's helicopters and soldiers
	protected void Forget()
	{
		m_aPatrols.Clear();
		m_aWorkers.Clear();
		m_aPatrolLostAt.Clear();
		m_aWorkerLostAt.Clear();
		m_Truck = null;
		m_iTruckLostAt = -1;
		for (int index = 0; index < m_aHelicopters.Count(); index++)
		{
			m_aHelicopters[index] = null;
			m_aHelicopterLostAt[index] = -1;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Location = AROR_LocationComponent.Cast(owner.FindComponent(AROR_LocationComponent));
		if (!GetGame().InPlayMode())
			return;

		if (!m_Location)
		{
			Print("[AROR SITE] Special site needs a location component; disabled", LogLevel.ERROR);
			return;
		}

		// Every machine builds the base once the world has settled
		GetGame().GetCallqueue().CallLater(Build, 1000);
		if (!Replication.IsServer())
			return;

		m_eRole = m_Location.GetControllingRole();
		GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Build);
		GetGame().GetCallqueue().Remove(Tick);
		if (Replication.IsServer() && m_bActive)
			Retire();
		foreach (IEntity dressing : m_aDressing)
		{
			if (dressing)
				SCR_EntityHelper.DeleteEntityAndChildren(dressing);
		}
		m_aDressing.Clear();
		super.OnDelete(owner);
	}
}

//! Soldiers at a special site: a work detail walking between buildings or a fire team walking the outer loop
class AROR_SpecialSiteGroup
{
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	int m_iRaisedAt;
	int m_iPoint;
	int m_iDirection = 1;
	int m_iNextOrderAt;
	int m_iLingerUntil;
	bool m_bSeen;	//!< Its members have spawned; from then on none living means wiped out
	vector m_vGoal;

	void Order_S(ResourceName prefab, vector goal, float radius)
	{
		if (!m_Group)
			return;

		if (m_Waypoint)
		{
			m_Group.RemoveWaypoint(m_Waypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		}

		m_Waypoint = AROR_ForceSpawner.AddWaypoint(m_Group, prefab, goal, radius);
	}

	void Remove_S()
	{
		array<IEntity> living = {};
		AROR_ForceSpawner.CountLiving(m_Group, living);
		foreach (IEntity member : living)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(member);
		}

		if (m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
		if (m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		m_Group = null;
		m_Waypoint = null;
	}
}

//! The truck driving laps round a special site
class AROR_SpecialSiteTruck : AROR_SpecialSiteGroup
{
	IEntity m_Vehicle;
	bool m_bSeated;

	override void Remove_S()
	{
		super.Remove_S();
		if (m_Vehicle && !AROR_ForceSpawner.IsDestroyed(m_Vehicle) && !AROR_ResistanceFleet.IsClaimed(m_Vehicle))
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
		m_Vehicle = null;
	}
}
