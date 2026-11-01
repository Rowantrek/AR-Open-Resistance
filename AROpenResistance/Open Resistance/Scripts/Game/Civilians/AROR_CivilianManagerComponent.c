[ComponentEditorProps(category: "OpenResistance/Civilians", description: "Server-owned ambient town pedestrians and occupied civilian road traffic.")]
class AROR_CivilianManagerComponentClass : SCR_BaseGameModeComponentClass {}

class AROR_CivilianManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", desc: "Enable ambient civilians")]
	protected bool m_bEnabled;
	[Attribute("1000", desc: "Town/city activity and player activation radius in metres", params: "200 2000 50")]
	protected float m_fTownRadius;
	[Attribute("8", desc: "Pedestrians per active town", params: "0 20 1")]
	protected int m_iPedestriansPerTown;
	[Attribute("2", desc: "Occupied civilian cars per active town", params: "0 6 1")]
	protected int m_iCarsPerTown;
	[Attribute("48", desc: "Global people cap, including reserved drivers/passengers", params: "0 150 1")]
	protected int m_iPeopleLimit;
	[Attribute("10", desc: "Global ambient vehicle cap", params: "0 30 1")]
	protected int m_iVehicleLimit;
	[Attribute("125", desc: "Minimum pedestrian spawn distance from every player", params: "50 500 5")]
	protected float m_fFootSpawnDistance;
	[Attribute("250", desc: "Minimum car spawn distance from every player", params: "100 750 5")]
	protected float m_fCarSpawnDistance;
	[Attribute("1300", desc: "Retain activity while players are within this distance of its town", params: "500 3000 50")]
	protected float m_fRetentionDistance;
	[Attribute("60", desc: "Seconds unseen before ambient cleanup", params: "15 300 5")]
	protected int m_iCleanupDelay;
	[Attribute("90", desc: "Seconds allowed for queued spawning and seating", params: "15 180 5")]
	protected int m_iSpawnTimeout;
	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, params: "et")]
	protected ResourceName m_sGroupPrefab;
	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, params: "et")]
	protected ResourceName m_sMovePrefab;
	protected ref array<ref AROR_CivilianActivity> m_aActivities = {};
	protected ref array<vector> m_aPlayers = {};
	protected int m_iTownCursor;
	protected static const int UPDATE_SECONDS = 5;
	protected static const int MAX_STALLS = 3;

	protected void Update_S()
	{
		if (!Replication.IsServer())
			return;
		AROR_GameFlowComponent flow = AROR_GameFlowComponent.GetInstance();
		if (!flow || !flow.IsPlaythroughReady())
			return;
		RefreshPlayers();
		for (int i = m_aActivities.Count() - 1; i >= 0; i--)
		{
			if (!UpdateActivity(m_aActivities[i]))
				m_aActivities.Remove(i);
		}
		if (m_bEnabled && !m_aPlayers.IsEmpty())
			TryPopulate(); // At most one activity per tick across the whole server.
	}

	protected void RefreshPlayers()
	{
		m_aPlayers.Clear();
		array<int> ids = {};
		PlayerManager manager = GetGame().GetPlayerManager();
		manager.GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity entity = manager.GetPlayerControlledEntity(id);
			if (entity)
				m_aPlayers.Insert(entity.GetOrigin());
		}
	}

	protected bool PlayerNear(vector position, float distance)
	{
		foreach (vector player : m_aPlayers)
		{
			if (vector.DistanceXZ(position, player) <= distance)
				return true;
		}
		return false;
	}

	//! Keep dead bodies and stalled cars accounted for until they can be removed away from players.
	protected bool VisibleToPlayers(AROR_CivilianActivity activity)
	{
		if (PlayerNear(activity.m_vCentre, Math.Max(m_fTownRadius, m_fRetentionDistance)) || PlayerNear(activity.m_vPosition, 350))
			return true;
		if (activity.m_Vehicle && (PlayerNear(activity.m_Vehicle.GetOrigin(), 350) || AROR_ForceSpawner.HasPlayerAboard(activity.m_Vehicle)))
			return true;
		foreach (IEntity member : activity.m_aMembers)
		{
			if (member && PlayerNear(member.GetOrigin(), 350))
				return true;
		}
		return false;
	}

	protected bool UpdateActivity(AROR_CivilianActivity activity)
	{
		activity.m_iAge += UPDATE_SECONDS;
		activity.m_iStateAge += UPDATE_SECONDS;
		AROR_ForceSpawner.CountAlive(activity.m_Group, AROR_EFactionRole.CIVILIANS, activity.m_aAffiliated);
		array<IEntity> living = {};
		AROR_ForceSpawner.CountLiving(activity.m_Group, living);
		array<AIAgent> agents = {};
		if (activity.m_Group)
			activity.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetControlledEntity() && !activity.m_aMembers.Contains(agent.GetControlledEntity()))
				activity.m_aMembers.Insert(agent.GetControlledEntity());
		}
		if (!living.IsEmpty())
			activity.m_vPosition = living[0].GetOrigin();
		if (activity.m_Vehicle && AROR_ForceSpawner.HasPlayerAboard(activity.m_Vehicle))
		{
			activity.m_bClaimedVehicle = true;
			activity.m_bRetiring = true; // Never delete a car a player has taken, even after they get out.
			ClearWaypoint(activity);
		}
		if (VisibleToPlayers(activity))
			activity.m_iUnwatched = 0;
		else
			activity.m_iUnwatched += UPDATE_SECONDS;
		if (activity.m_iUnwatched >= m_iCleanupDelay)
		{
			Remove(activity);
			return false;
		}
		if (activity.m_bRetiring)
			return true;
		if (!activity.m_bReady)
		{
			if (living.Count() >= activity.m_iExpected)
			{
				bool seated = !activity.m_Vehicle || AROR_ForceSpawner.SeatGroup(activity.m_Group, activity.m_Vehicle, activity.m_iExpected, true);
				if (seated)
				{
					activity.m_bReady = true;
					activity.m_iStateAge = 0;
					activity.m_vProgress = activity.m_vPosition;
					IssueRoute(activity);
				}
			}
			if (!activity.m_bReady && activity.m_iStateAge >= m_iSpawnTimeout)
			{
				activity.m_bRetiring = true;
				// Cancel queued spawning; keep existing people until distant cleanup.
				if (activity.m_Group)
					SCR_EntityHelper.DeleteEntityAndChildren(activity.m_Group);
				activity.m_Group = null;
				Print("[AROR CIV] Spawn/seating timed out; retiring activity when players leave.", LogLevel.WARNING);
			}
			return true;
		}
		if (living.IsEmpty() || (activity.m_Vehicle && AROR_ForceSpawner.IsDestroyed(activity.m_Vehicle)))
		{
			activity.m_bRetiring = true;
			ClearWaypoint(activity);
			return true;
		}
		activity.m_iStalled += UPDATE_SECONDS;
		float arrival = 8;
		if (activity.m_Vehicle)
			arrival = 25;
		bool arrived = vector.DistanceXZ(activity.m_vPosition, activity.m_vTarget) <= arrival;
		if (activity.m_iStalled >= 45)
		{
			bool stuck = vector.DistanceXZ(activity.m_vPosition, activity.m_vProgress) < 5;
			activity.m_vProgress = activity.m_vPosition;
			activity.m_iStalled = 0;
			if (stuck)
			{
				activity.m_iStalls++;
				arrived = true;
			}
			else
				activity.m_iStalls = 0;
		}
		if (arrived)
		{
			if (activity.m_iStalls >= MAX_STALLS || !PickDestination(activity.m_vCentre, activity.m_vPosition, activity.m_Vehicle != null, activity.m_vTarget))
			{
				activity.m_bRetiring = true;
				ClearWaypoint(activity);
			}
			else
				IssueRoute(activity);
		}
		return true;
	}

	protected void TryPopulate()
	{
		if (m_iPeopleLimit <= 0)
			return;
		int people, cars;
		foreach (AROR_CivilianActivity activity : m_aActivities)
		{
			people += activity.m_iExpected;
			if (activity.m_Vehicle)
				cars++;
		}
		if (people >= m_iPeopleLimit)
			return;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return;
		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		if (all.IsEmpty())
			return;
		// Round robin makes multiple towns share the population budget without per-player duplication.
		for (int offset = 0; offset < all.Count(); offset++)
		{
			int index = (m_iTownCursor + offset) % all.Count();
			AROR_LocationComponent town = all[index];
			if (!town || (town.GetLocationType() != AROR_ELocationType.TOWN && town.GetLocationType() != AROR_ELocationType.CITY)
				|| !PlayerNear(town.GetCenter(), m_fTownRadius))
				continue;
			int foot, traffic;
			foreach (AROR_CivilianActivity existing : m_aActivities)
			{
				if (existing.m_sLocationId != town.GetId())
					continue;
				if (existing.m_Vehicle)
					traffic++;
				else
					foot += existing.m_iExpected;
			}
			bool motorized = traffic < m_iCarsPerTown && cars < m_iVehicleLimit && (foot >= m_iPedestriansPerTown || Math.RandomInt(0, 100) < 35);
			if (!motorized && foot >= m_iPedestriansPerTown)
				continue;
			m_iTownCursor = (index + 1) % all.Count();
			if (Spawn(town, motorized, m_iPeopleLimit - people))
				return;
		}
	}

	protected bool PickDestination(vector centre, vector from, bool motorized, out vector target)
	{
		for (int attempt = 0; attempt < 8; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float radius = Math.RandomFloat(80, m_fTownRadius * 0.85);
			vector probe = centre + Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
			bool found;
			if (motorized)
				found = AROR_ForceSpawner.FindRoadPosition(probe, from, target);
			else
				found = AROR_ForceSpawner.FindClearPosition(probe, 15, 1, target);
			if (found && vector.DistanceXZ(target, centre) <= m_fTownRadius && vector.DistanceXZ(target, from) >= 60 && DryLand(target))
				return true;
		}
		return false;
	}

	protected bool DryLand(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		return world.GetSurfaceY(position[0], position[2]) > world.GetOceanHeight(position[0], position[2]) + 0.5;
	}

	//! Test the exact road point, avoiding an empty-terrain search that can move cars onto a verge.
	protected bool CarSpawnClear(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		position[1] = world.GetSurfaceY(position[0], position[2]);
		TraceBox trace = new TraceBox();
		trace.Start = position + Vector(0, 0.25, 0);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-3, 0, -3);
		trace.Maxs = Vector(3, 3, 3);
		trace.Flags = TraceFlags.ENTS;
		return world.TraceMove(trace, null) == 1;
	}

	protected bool Spawn(AROR_LocationComponent town, bool motorized, int room)
	{
		ResourceName civilian = AROR_ForceSpawner.UnitPrefab(AROR_EFactionRole.CIVILIANS, AROR_EUnitType.CIVILIAN);
		if (civilian.IsEmpty())
			return false;
		vector start, target;
		bool found;
		for (int attempt = 0; attempt < 8; attempt++)
		{
			if (!PickDestination(town.GetCenter(), town.GetCenter(), motorized, start))
				continue;
			float distance = m_fFootSpawnDistance;
			if (motorized)
				distance = m_fCarSpawnDistance;
			if (PlayerNear(start, distance) || !PickDestination(town.GetCenter(), start, motorized, target))
				continue;
			if (motorized)
			{
				if (!CarSpawnClear(start))
					continue;
			}
			found = true;
			break;
		}
		if (!found)
			return false;
		IEntity vehicle;
		int count = 1;
		if (motorized)
		{
			ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(AROR_EFactionRole.CIVILIANS, AROR_EVehicleType.MOTOR_CAR);
			if (prefab.IsEmpty())
				return false;
			vector heading = target - start;
			float yaw = Math.Atan2(heading[0], heading[2]) * Math.RAD2DEG;
			vehicle = AROR_ForceSpawner.SpawnVehicle(AROR_EFactionRole.CIVILIANS, prefab, start, yaw);
			if (!vehicle)
				return false;
			if (room > 1 && AROR_ForceSpawner.CountCargoSeats(vehicle) > 0 && Math.RandomInt(0, 100) < 50)
				count = 2;
		}
		array<ResourceName> members = {};
		for (int member = 0; member < count; member++)
			members.Insert(civilian);
		vector footStart = start;
		if (vehicle && !AROR_ForceSpawner.FindClearPosition(start, 12, 1, footStart))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return false;
		}
		if (vector.DistanceXZ(footStart, town.GetCenter()) > m_fTownRadius || PlayerNear(footStart, m_fFootSpawnDistance))
		{
			if (vehicle) SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return false;
		}
		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, AROR_EFactionRole.CIVILIANS, members, footStart);
		if (!group)
		{
			if (vehicle)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return false;
		}
		AROR_CivilianActivity activity = new AROR_CivilianActivity();
		activity.m_sLocationId = town.GetId();
		activity.m_vCentre = town.GetCenter();
		activity.m_vPosition = start;
		activity.m_vTarget = target;
		activity.m_Group = group;
		activity.m_Vehicle = vehicle;
		activity.m_iExpected = count;
		m_aActivities.Insert(activity);
		return true;
	}

	protected void ClearWaypoint(AROR_CivilianActivity activity)
	{
		if (!activity.m_Waypoint)
			return;
		if (activity.m_Group)
			activity.m_Group.RemoveWaypoint(activity.m_Waypoint);
		SCR_EntityHelper.DeleteEntityAndChildren(activity.m_Waypoint);
		activity.m_Waypoint = null;
	}

	protected void IssueRoute(AROR_CivilianActivity activity)
	{
		ClearWaypoint(activity);
		float radius = 8;
		if (activity.m_Vehicle)
			radius = 25;
		activity.m_Waypoint = AROR_ForceSpawner.AddWaypoint(activity.m_Group, m_sMovePrefab, activity.m_vTarget, radius);
		activity.m_iStalled = 0;
		activity.m_vProgress = activity.m_vPosition;
		if (!activity.m_Waypoint)
			activity.m_bRetiring = true;
	}

	protected void Remove(AROR_CivilianActivity activity)
	{
		ClearWaypoint(activity);
		AROR_ForceSpawner.DeleteGroupAndMembers(activity.m_Group);
		foreach (IEntity member : activity.m_aMembers)
		{
			if (member)
				SCR_EntityHelper.DeleteEntityAndChildren(member);
		}
		if (activity.m_Vehicle && !activity.m_bClaimedVehicle && !AROR_ForceSpawner.HasPlayerAboard(activity.m_Vehicle))
			SCR_EntityHelper.DeleteEntityAndChildren(activity.m_Vehicle);
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (GetGame().InPlayMode() && Replication.IsServer())
			GetGame().GetCallqueue().CallLater(Update_S, UPDATE_SECONDS * 1000, true);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Update_S);
		super.OnDelete(owner);
	}
}
