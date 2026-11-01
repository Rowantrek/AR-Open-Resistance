[ComponentEditorProps(category: "OpenResistance/Missions", description: "Resistance missions offered on the OPS phone app: the board, active missions, rewards and markers. Attach to the game mode entity.")]
class AROR_MissionBoardComponentClass : SCR_BaseGameModeComponentClass
{
}

//! A spawned mission's leftovers, removed once no Resistance player is near (or after a timeout)
class AROR_MissionCleanup
{
	ref array<IEntity> m_aEntities = {};
	int m_iAge;		//!< Seconds since the mission ended
}

//! Resistance missions (see ARCHITECTURE.md -> Missions), used through the OPS phone app.
//! Server: keeps a board of mission offers (rerolled as they expire or are taken), the active missions (accepted by
//! Resistance Leaders, capped), their deployment near the Resistance, objectives, rewards (Resistance funds and
//! manpower), notifications and a short network log. Requests arrive through the AROR_RequestMission* RPCs on
//! SCR_PlayerController.
//! Every machine: parses a compact replicated snapshot (missions and log) for OPS, the game map and HICOMM.
//! Saved: active missions (AROR_MissionBoardComponentSerializer); the board rerolls on load.
class AROR_MissionBoardComponent : SCR_BaseGameModeComponent
{
	[Attribute("", desc: "Per-type settings (rewards, time limit, guards, enabled); types not listed use built-in defaults")]
	protected ref array<ref AROR_MissionType> m_aMissionTypes;

	[Attribute("6", desc: "Missions offered on the board at once", params: "1 12 1")]
	protected int m_iBoardSize;

	[Attribute("3", desc: "Most missions active at once", params: "1 6 1")]
	protected int m_iMaxActive;

	[Attribute("30", desc: "Minutes an offer stays on the board before it is replaced", params: "5 240 1")]
	protected int m_iOfferLifetime;

	[Attribute("90", desc: "Seconds before a taken or expired offer is replaced", params: "0 1800 1")]
	protected int m_iRefillDelay;

	[Attribute("1200", desc: "A mission's enemies and props spawn when a Resistance player comes this close (m)", params: "200 5000 10")]
	protected float m_fDeployRange;

	[Attribute("400", desc: "A finished mission's leftovers are removed once no Resistance player is this close (m)", params: "100 3000 10")]
	protected float m_fCleanupRange;

	[Attribute("900", desc: "Seconds after which a finished mission's leftovers are removed even with players near", params: "60 7200 10")]
	protected int m_iCleanupTimeout;

	[Attribute("2", desc: "Seconds between mission updates", params: "1 10 1")]
	protected int m_iUpdateInterval;

	[Attribute("30", desc: "Seconds between snapshot refreshes while only timers and convoy positions change", params: "5 120 1")]
	protected int m_iResyncInterval;

	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty AI group prefab mission units spawn in", params: "et", category: "Prefabs")]
	protected ResourceName m_sGroupPrefab;

	[Attribute("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", UIWidgets.ResourceNamePicker, desc: "Waypoint for guards holding an objective", params: "et", category: "Prefabs")]
	protected ResourceName m_sDefendWaypointPrefab;

	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, desc: "Waypoint for convoys and freed prisoners", params: "et", category: "Prefabs")]
	protected ResourceName m_sMoveWaypointPrefab;

	[Attribute("{5C9A089F424D8001}Prefabs/Props/Military/AmmoBoxes/US/EquipmentBoxWooden_Weapon_01_US.et", UIWidgets.ResourceNamePicker, desc: "Crate spawned for weapons cache missions", params: "et", category: "Prefabs")]
	protected ResourceName m_sCachePrefab;

	protected static AROR_MissionBoardComponent s_Instance;

	protected static const string ROW_SEPARATOR = ";";
	protected static const string FIELD_SEPARATOR = "|";
	protected static const int SNAPSHOT_FIELDS = 22;
	protected static const int LOG_LINES = 8;

	protected static const ref array<string> CODE_ADJECTIVES = {"GREY", "BROKEN", "QUIET", "DEAD", "BRIGHT", "OPEN", "SILENT", "IRON", "PALE", "HOLLOW", "COLD", "BLACK", "RED", "LONG", "LAST", "FALLEN", "WILD", "DEEP", "STILL", "BURNT"};
	protected static const ref array<string> CODE_NOUNS = {"LARDER", "ROTOR", "ROAD", "HAND", "AIR", "NIGHT", "DOOR", "VAULT", "TRUTH", "VOICE", "HARBOUR", "LANTERN", "ORCHARD", "ANVIL", "SPARROW", "WINTER", "TIDE", "FURROW", "BELL", "CINDER"};

	//! Replicated snapshot: one row per mission (see BuildSnapshot) and the network log
	[RplProp(onRplName: "OnSnapshotReplicated")]
	protected string m_sMissions;

	[RplProp(onRplName: "OnSnapshotReplicated")]
	protected string m_sLog;

	// Server
	protected ref array<ref AROR_Mission> m_aOffers = {};
	protected ref array<ref AROR_Mission> m_aActive = {};
	protected ref array<ref AROR_MissionCleanup> m_aCleanups = {};
	protected ref array<string> m_aLog = {};
	protected ref array<vector> m_aPlayerPositions = {};
	protected int m_iNextId = 1;
	protected int m_iRefillTimer;
	protected int m_iResyncTimer;
	protected string m_sLastStructure;

	// Every machine: parsed snapshot
	protected ref array<ref AROR_MissionInfo> m_aInfos = {};
	protected ref array<string> m_aLogLines = {};
	protected int m_iSnapshotRevision;
	protected int m_iReceivedAt;	//!< System tick when the snapshot arrived (timers count down from here)

	//------------------------------------------------------------------------------------------------
	//! Exists on every machine (the snapshot is replicated)
	static AROR_MissionBoardComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// Every machine: read access for OPS, the map and HICOMM
	//------------------------------------------------------------------------------------------------
	int GetMissions(notnull array<AROR_MissionInfo> outMissions)
	{
		outMissions.Clear();
		foreach (AROR_MissionInfo info : m_aInfos)
		{
			outMissions.Insert(info);
		}

		return outMissions.Count();
	}

	//------------------------------------------------------------------------------------------------
	AROR_MissionInfo GetMission(int missionId)
	{
		foreach (AROR_MissionInfo info : m_aInfos)
		{
			if (info.m_iId == missionId)
				return info;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int CountActive()
	{
		int count;
		foreach (AROR_MissionInfo info : m_aInfos)
		{
			if (info.m_bActive)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	int GetMaxActive()
	{
		return m_iMaxActive;
	}

	//------------------------------------------------------------------------------------------------
	//! Newest first
	int GetLogLines(notnull array<string> outLines)
	{
		outLines.Clear();
		foreach (string line : m_aLogLines)
		{
			outLines.Insert(line);
		}

		return outLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Increases whenever the snapshot changes
	int GetSnapshotRevision()
	{
		return m_iSnapshotRevision;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds left now (offers: on the board; active: to complete), counted down since the snapshot arrived
	int GetSecondsLeft(notnull AROR_MissionInfo info)
	{
		int elapsed = (System.GetTickCount() - m_iReceivedAt) / 1000;
		return Math.Max(info.m_iSeconds - elapsed, 0);
	}

	//------------------------------------------------------------------------------------------------
	// Server: requests (arrive through SCR_PlayerController RPCs)
	//------------------------------------------------------------------------------------------------
	void RequestAccept_S(int playerId, int missionId)
	{
		if (!Replication.IsServer())
			return;

		if (!IsLeader(playerId))
		{
			Reply(playerId, "Only a Resistance Leader can accept operations");
			return;
		}

		AROR_Mission mission = FindOffer(missionId);
		if (!mission)
		{
			Reply(playerId, "That operation is no longer on the board");
			return;
		}

		if (m_aActive.Count() >= m_iMaxActive)
		{
			Reply(playerId, string.Format("The Resistance already runs %1 operations. Finish or abort one first", m_iMaxActive));
			return;
		}

		if (!mission.IsStillValid_S(this))
		{
			m_aOffers.RemoveItem(mission);
			Reply(playerId, "That intel is out of date; the board has been updated");
			PublishSnapshot_S(true);
			return;
		}

		// Kept alive by a local ref while it moves between the lists
		AROR_Mission accepted = mission;
		m_aOffers.RemoveItem(mission);
		accepted.m_bActive = true;
		accepted.m_iRemaining = accepted.m_iTimeLimit;
		accepted.m_iLeaderId = playerId;
		accepted.m_sLeaderName = Clean(GetGame().GetPlayerManager().GetPlayerName(playerId));
		accepted.m_sProgress = accepted.GetStartProgress(this);
		m_aActive.Insert(accepted);

		if (accepted.DeploysAtOnce())
			TryDeploy_S(accepted);

		string typeName = AROR_MissionTexts.TypeName(accepted.m_eType);
		Print(string.Format("[AROR MISSIONS] %1 accepted %2 (%3) at %4", accepted.m_sLeaderName, accepted.m_sCodename, typeName, accepted.GetLocationName(this)), LogLevel.NORMAL);
		AddLog(string.Format("%1 accepted %2", accepted.m_sLeaderName, accepted.m_sCodename));
		Notify(AROR_ENotificationType.NOTIFICATION, string.Format("%1: %2 at %3 is under way", accepted.m_sCodename, typeName, accepted.GetLocationName(this)), "Operation started", Color.FromRGBA(230, 168, 58, 255));
		m_iRefillTimer = Math.Max(m_iRefillTimer, m_iRefillDelay);
		PublishSnapshot_S(true);
	}

	//------------------------------------------------------------------------------------------------
	//! The accepting leader or host can abort; after load any current leader can abort.
	void RequestAbort_S(int playerId, int missionId)
	{
		if (!Replication.IsServer())
			return;

		AROR_Mission mission = FindActive(missionId);
		if (!mission)
		{
			Reply(playerId, "That operation is not running");
			return;
		}

		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		bool allowed = IsLeader(playerId) && (mission.m_iLeaderId == 0 || mission.m_iLeaderId == playerId || (highCommand && highCommand.IsHost(playerId)));
		if (!allowed)
		{
			Reply(playerId, "Only the leader who accepted it (or the host) can abort " + mission.m_sCodename);
			return;
		}

		mission.m_sOutcome = "aborted by " + Clean(GetGame().GetPlayerManager().GetPlayerName(playerId));
		Finish_S(mission, AROR_EMissionResult.FAILED);
	}

	//------------------------------------------------------------------------------------------------
	// Server: helpers for missions
	//------------------------------------------------------------------------------------------------
	AROR_LocationComponent GetLocation(string locationId)
	{
		if (locationId.IsEmpty())
			return null;

		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return null;

		return locations.GetLocationById(locationId);
	}

	//------------------------------------------------------------------------------------------------
	//! A random location of one of the types, held by the Occupiers or Invaders and not used by another mission
	AROR_LocationComponent PickEnemyLocation_S(notnull array<AROR_ELocationType> types)
	{
		array<AROR_LocationComponent> candidates = {};
		CollectLocations(types, candidates);
		for (int i = candidates.Count() - 1; i >= 0; i--)
		{
			if (!IsEnemyRole(candidates[i].GetControllingRole()))
				candidates.Remove(i);
		}

		return PickRandom(candidates);
	}

	//------------------------------------------------------------------------------------------------
	//! A random location of one of the types (any controller) not used by another mission
	AROR_LocationComponent PickAnyLocation_S(notnull array<AROR_ELocationType> types)
	{
		array<AROR_LocationComponent> candidates = {};
		CollectLocations(types, candidates);
		return PickRandom(candidates);
	}

	//------------------------------------------------------------------------------------------------
	//! Locations held by a role, excluding one, between the distances (m) from a point
	int FindLocationsHeldBy_S(AROR_EFactionRole role, vector from, float minDistance, float maxDistance, string excludeId, notnull array<AROR_LocationComponent> outLocations)
	{
		outLocations.Clear();
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return 0;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		foreach (AROR_LocationComponent location : all)
		{
			if (!location || location.GetId() == excludeId || location.GetControllingRole() != role)
				continue;

			if (location.GetLocationType() == AROR_ELocationType.HQ || location.GetLocationType() == AROR_ELocationType.SUPPORT_CORRIDOR)
				continue;

			float distance = vector.DistanceXZ(from, location.GetCenter());
			if (distance >= minDistance && distance <= maxDistance)
				outLocations.Insert(location);
		}

		return outLocations.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest Resistance-held location (or the HQ) to a point; false if there is none
	bool FindFriendlyDestination_S(vector from, out string locationId, out vector position)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return false;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		float nearest = float.MAX;
		AROR_LocationComponent best;
		foreach (AROR_LocationComponent location : all)
		{
			if (!location || location.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
				continue;

			float distance = vector.DistanceXZ(from, location.GetCenter());
			if (distance < nearest)
			{
				nearest = distance;
				best = location;
			}
		}

		if (!best)
			return false;

		locationId = best.GetId();
		position = best.GetCenter();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Inside the HQ or a Resistance-held location
	bool IsInFriendlyLocation_S(vector position)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return false;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		foreach (AROR_LocationComponent location : all)
		{
			if (location && location.GetControllingRole() == AROR_EFactionRole.RESISTANCE && vector.DistanceXZ(position, location.GetCenter()) <= Math.Max(location.GetRadius(), 60))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	string GetFactionKey(AROR_EFactionRole role)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return string.Empty;

		return roles.GetFactionKey(role);
	}

	//------------------------------------------------------------------------------------------------
	//! A unit of the type from the role's faction preset, falling back to its rifleman
	ResourceName GetUnitPrefab_S(AROR_EFactionRole role, AROR_EUnitType type)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return string.Empty;

		AROR_FactionPreset preset = presets.GetPreset(role);
		if (!preset)
			return string.Empty;

		AROR_FactionPresetUnit unit = preset.GetUnit(type);
		if (!unit)
			unit = preset.GetUnit(AROR_EUnitType.RIFLEMAN);

		if (!unit)
			return string.Empty;

		return unit.GetPrefab();
	}

	//------------------------------------------------------------------------------------------------
	//! A random vehicle of the type from the role's faction preset that its readiness allows (any of the type if none
	//! is allowed yet), or empty if it has none
	ResourceName GetVehiclePrefab_S(AROR_EFactionRole role, AROR_EVehicleType type)
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

		array<ResourceName> allowed = {};
		foreach (AROR_FactionPresetEntry candidate : entries)
		{
			if (AROR_ContentPacksComponent.IsAvailableAtReadiness(candidate.GetPrefab(), role))
				allowed.Insert(candidate.GetPrefab());
		}

		if (!allowed.IsEmpty())
			return allowed.GetRandomElement();

		AROR_FactionPresetEntry entry = entries.GetRandomElement();
		return entry.GetPrefab();
	}

	//------------------------------------------------------------------------------------------------
	//! A guard team for the role: a leader and riflemen with an automatic rifleman and a machine gunner mixed in
	void BuildGuardTeam_S(AROR_EFactionRole role, int count, notnull array<ResourceName> outMembers)
	{
		outMembers.Clear();
		for (int i = 0; i < count; i++)
		{
			AROR_EUnitType type = AROR_EUnitType.RIFLEMAN;
			if (i == 0)
				type = AROR_EUnitType.TEAMLEADER;
			else if (i == 2)
				type = AROR_EUnitType.AUTORIFLEMAN;
			else if (i == 4)
				type = AROR_EUnitType.MACHINEGUNNER;

			ResourceName prefab = GetUnitPrefab_S(role, type);
			if (!prefab.IsEmpty())
				outMembers.Insert(prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! An AI group of the role with these members (spawned through the engine's queue, not all this frame)
	SCR_AIGroup SpawnGroup_S(AROR_EFactionRole role, notnull array<ResourceName> members, vector position)
	{
		if (members.IsEmpty())
			return null;

		string factionKey = GetFactionKey(role);
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
		if (!faction)
			return null;

		SCR_AIGroup.IgnoreSpawning(true);
		IEntity entity = SpawnPrefab_S(m_sGroupPrefab, position, 0);
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
	IEntity SpawnVehicle_S(AROR_EFactionRole role, ResourceName prefab, vector position, float yaw)
	{
		IEntity vehicle = SpawnPrefab_S(prefab, position, yaw);
		if (!vehicle)
			return null;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		string factionKey = GetFactionKey(role);
		if (affiliation && !factionKey.IsEmpty())
			affiliation.SetAffiliatedFactionByKey(factionKey);

		return vehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a prefab on the terrain surface (yaw in degrees)
	IEntity SpawnPrefab_S(ResourceName prefab, vector position, float yaw)
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
	//! Gives the group a defend or move waypoint; the waypoint is added to entities for cleanup
	AIWaypoint AddWaypoint_S(SCR_AIGroup group, bool defend, vector position, float radius, notnull array<IEntity> entities)
	{
		if (!group)
			return null;

		ResourceName prefab = m_sMoveWaypointPrefab;
		if (defend)
			prefab = m_sDefendWaypointPrefab;

		AIWaypoint waypoint = AIWaypoint.Cast(SpawnPrefab_S(prefab, position, 0));
		if (!waypoint)
			return null;

		waypoint.SetCompletionRadius(radius);
		group.AddWaypoint(waypoint);
		entities.Insert(waypoint);
		return waypoint;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetCachePrefab()
	{
		return m_sCachePrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! A clear spot on dry land within radius of centre with room for an object of the clearance radius
	bool FindClearPosition_S(vector centre, float radius, float clearance, out vector position)
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
	bool FindRoadPosition_S(vector near, vector goal, out vector position)
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
	//! A living Resistance player within range (positions are refreshed each update)
	bool IsResistanceNear_S(vector position, float range)
	{
		foreach (vector player : m_aPlayerPositions)
		{
			if (vector.DistanceXZ(player, position) <= range)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	float GetDeployRange()
	{
		return m_fDeployRange;
	}

	//------------------------------------------------------------------------------------------------
	int GetUpdateInterval()
	{
		return m_iUpdateInterval;
	}

	//------------------------------------------------------------------------------------------------
	//! Living members of a group; new members are given the role's campaign faction once (presets spawn with their
	//! native faction)
	int CountAlive_S(SCR_AIGroup group, AROR_EFactionRole role, notnull array<IEntity> affiliated)
	{
		if (!group)
			return 0;

		string factionKey = GetFactionKey(role);
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
	//! Seats a group's members in a vehicle (driver first, then turrets, then cargo), teleporting them in. Called
	//! every update until it returns true: seat requests can be dropped, and members spawn over several frames.
	bool SeatGroup_S(SCR_AIGroup group, IEntity vehicle, int expectedMembers)
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

	//------------------------------------------------------------------------------------------------
	//! Removes these entities once no Resistance player is near; characters inside groups go with their group.
	//! Vehicles with a player aboard are left alone.
	void QueueCleanup_S(notnull array<SCR_AIGroup> groups, notnull array<IEntity> entities)
	{
		AROR_MissionCleanup cleanup = new AROR_MissionCleanup();
		foreach (SCR_AIGroup group : groups)
		{
			if (!group)
				continue;

			array<IEntity> members = {};
			CountLiving(group, members);
			foreach (IEntity member : members)
			{
				cleanup.m_aEntities.Insert(member);
			}

			cleanup.m_aEntities.Insert(group);
		}

		foreach (IEntity entity : entities)
		{
			if (entity)
				cleanup.m_aEntities.Insert(entity);
		}

		if (!cleanup.m_aEntities.IsEmpty())
			m_aCleanups.Insert(cleanup);
	}

	//------------------------------------------------------------------------------------------------
	//! Private hint to one player
	void Reply(int playerId, string text)
	{
		if (playerId > 0)
			AROR_NotificationManagerComponent.SendHint_S(playerId, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Hint to the leader running a mission (progress the whole cell should hear goes to the log instead)
	void ReplyLeader(AROR_Mission mission, string text)
	{
		if (mission.m_iLeaderId > 0)
		{
			Reply(mission.m_iLeaderId, mission.m_sCodename + ": " + text);
			return;
		}

		// Loaded missions have no session owner; current leaders can manage them.
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			if (IsLeader(playerId))
				Reply(playerId, mission.m_sCodename + ": " + text);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Server: board
	//------------------------------------------------------------------------------------------------
	//! Creates a mission of the type (extender mods add types by overriding this in a modded class)
	AROR_Mission CreateMission(AROR_EMissionType type)
	{
		switch (type)
		{
			case AROR_EMissionType.SUPPLY_THEFT:
				return new AROR_SupplyTheftMission();
			case AROR_EMissionType.CONVOY_AMBUSH:
				return new AROR_ConvoyAmbushMission();
			case AROR_EMissionType.PRISONER_RESCUE:
				return new AROR_PrisonerRescueMission();
			case AROR_EMissionType.SABOTAGE:
				return new AROR_SabotageMission();
			case AROR_EMissionType.ASSASSINATION:
				return new AROR_AssassinationMission();
			case AROR_EMissionType.CACHE_RECOVERY:
				return new AROR_CacheRecoveryMission();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Settings for a type: the board's entry if it lists one, else the built-in defaults
	AROR_MissionType GetSettings(AROR_EMissionType type)
	{
		if (m_aMissionTypes)
		{
			foreach (AROR_MissionType settings : m_aMissionTypes)
			{
				if (settings && settings.m_eType == type)
					return settings;
			}
		}

		return AROR_MissionType.CreateDefault(type);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds one offer, preferring the type least represented on the board and among active missions
	protected bool AddOffer_S()
	{
		array<int> types = {};
		array<int> counts = {};
		array<int> values = GetTypeValues();
		foreach (int value : values)
		{
			AROR_MissionType settings = GetSettings(value);
			if (!settings || !settings.m_bEnabled)
				continue;

			types.Insert(value);
			counts.Insert(CountOfType(value));
		}

		// Try types from least to most represented, shuffled within equal counts
		for (int attempt = 0; attempt < types.Count(); attempt++)
		{
			int best = -1;
			int bestCount = int.MAX;
			foreach (int i, int type : types)
			{
				if (type < 0)
					continue;

				int count = counts[i] * 10 + Math.RandomInt(0, 10);
				if (count < bestCount)
				{
					bestCount = count;
					best = i;
				}
			}

			if (best < 0)
				return false;

			AROR_EMissionType chosen = types[best];
			types[best] = -1;

			AROR_Mission mission = CreateMission(chosen);
			if (!mission)
				continue;

			mission.m_eType = chosen;
			if (!mission.Plan_S(this))
				continue;

			mission.m_iId = m_iNextId++;
			mission.m_sCodename = NewCodename();
			mission.m_iOfferSeconds = m_iOfferLifetime * 60;
			m_aOffers.Insert(mission);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every value of AROR_EMissionType (including any added by modded enums)
	protected static array<int> GetTypeValues()
	{
		array<int> values = {};
		typename type = AROR_EMissionType;
		int count = type.GetVariableCount();
		for (int i = 0; i < count; i++)
		{
			int value;
			if (type.GetVariableValue(null, i, value))
				values.Insert(value);
		}

		return values;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountOfType(AROR_EMissionType type)
	{
		int count;
		foreach (AROR_Mission offer : m_aOffers)
		{
			if (offer.m_eType == type)
				count++;
		}

		foreach (AROR_Mission active : m_aActive)
		{
			if (active.m_eType == type)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected string NewCodename()
	{
		for (int attempt = 0; attempt < 20; attempt++)
		{
			string codename = "OP " + CODE_ADJECTIVES.GetRandomElement() + " " + CODE_NOUNS.GetRandomElement();
			if (!IsCodenameUsed(codename))
				return codename;
		}

		return string.Format("OP %1 %2", CODE_NOUNS.GetRandomElement(), m_iNextId);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsCodenameUsed(string codename)
	{
		foreach (AROR_Mission offer : m_aOffers)
		{
			if (offer.m_sCodename == codename)
				return true;
		}

		foreach (AROR_Mission active : m_aActive)
		{
			if (active.m_sCodename == codename)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectLocations(notnull array<AROR_ELocationType> types, notnull array<AROR_LocationComponent> outLocations)
	{
		outLocations.Clear();
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		foreach (AROR_LocationComponent location : all)
		{
			if (location && types.Contains(location.GetLocationType()) && !IsLocationInUse(location.GetId()))
				outLocations.Insert(location);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Used by an offer or active mission (as its site or destination)
	bool IsLocationInUse(string locationId)
	{
		foreach (AROR_Mission offer : m_aOffers)
		{
			if (offer.m_sLocationId == locationId || offer.m_sDestinationId == locationId)
				return true;
		}

		foreach (AROR_Mission active : m_aActive)
		{
			if (active.m_sLocationId == locationId || active.m_sDestinationId == locationId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static AROR_LocationComponent PickRandom(notnull array<AROR_LocationComponent> candidates)
	{
		if (candidates.IsEmpty())
			return null;

		return candidates.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEnemyRole(AROR_EFactionRole role)
	{
		return role == AROR_EFactionRole.OCCUPIERS || role == AROR_EFactionRole.INVADERS;
	}

	//------------------------------------------------------------------------------------------------
	protected AROR_Mission FindOffer(int missionId)
	{
		foreach (AROR_Mission offer : m_aOffers)
		{
			if (offer.m_iId == missionId)
				return offer;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected AROR_Mission FindActive(int missionId)
	{
		foreach (AROR_Mission active : m_aActive)
		{
			if (active.m_iId == missionId)
				return active;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLeader(int playerId)
	{
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		return highCommand && highCommand.IsLeader(playerId);
	}

	//------------------------------------------------------------------------------------------------
	// Server: update
	//------------------------------------------------------------------------------------------------
	protected void Update_S()
	{
		AROR_GameFlowComponent gameFlow = AROR_GameFlowComponent.GetInstance();
		if (gameFlow && !gameFlow.IsPlaythroughReady())
			return;

		RefreshPlayerPositions_S();
		int seconds = m_iUpdateInterval;

		// Offers age off the board
		for (int i = m_aOffers.Count() - 1; i >= 0; i--)
		{
			m_aOffers[i].m_iOfferSeconds = m_aOffers[i].m_iOfferSeconds - seconds;
			if (m_aOffers[i].m_iOfferSeconds <= 0)
				m_aOffers.Remove(i);
		}

		// Refill: the first board fills at once, later gaps after a delay
		if (m_aOffers.Count() < m_iBoardSize)
		{
			m_iRefillTimer -= seconds;
			bool firstFill = m_aOffers.IsEmpty() && m_aActive.IsEmpty();
			if (firstFill)
			{
				for (int fill = 0; fill < m_iBoardSize; fill++)
				{
					if (!AddOffer_S())
						break;
				}
			}
			else if (m_iRefillTimer <= 0)
			{
				AddOffer_S();
				m_iRefillTimer = m_iRefillDelay;
			}
		}

		// Active missions
		for (int a = m_aActive.Count() - 1; a >= 0; a--)
		{
			if (!m_aActive.IsIndexValid(a))
				continue;

			AROR_Mission mission = m_aActive[a];
			mission.m_iRemaining = mission.m_iRemaining - seconds;

			if (!mission.m_bDeployed && !mission.IsStillValid_S(this))
			{
				mission.m_sOutcome = "the target is no longer valid";
				Finish_S(mission, AROR_EMissionResult.FAILED);
				continue;
			}

			if (!mission.m_bDeployed && ((!mission.m_bRestored && mission.DeploysAtOnce()) || IsResistanceNear_S(mission.m_vPosition, m_fDeployRange)))
				TryDeploy_S(mission);

			AROR_EMissionResult result = AROR_EMissionResult.ONGOING;
			if (mission.m_bDeployed)
				result = mission.Update_S(this);
			else if (mission.m_iDeployAttempts >= 5)
			{
				mission.m_sOutcome = "the operation could not be set up";
				result = AROR_EMissionResult.FAILED;
			}

			if (result == AROR_EMissionResult.ONGOING && mission.m_iRemaining <= 0)
			{
				mission.m_sOutcome = mission.GetTimeoutOutcome();
				result = AROR_EMissionResult.FAILED;
			}

			if (result != AROR_EMissionResult.ONGOING)
				Finish_S(mission, result);
		}

		UpdateCleanups_S(seconds);

		m_iResyncTimer -= seconds;
		PublishSnapshot_S(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TryDeploy_S(AROR_Mission mission)
	{
		mission.m_bDeployed = mission.Deploy_S(this);
		if (mission.m_bDeployed)
		{
			mission.m_iDeployedAt = GetGame().GetWorld().GetWorldTime();
			Print(string.Format("[AROR MISSIONS] %1 deployed at %2", mission.m_sCodename, mission.GetLocationName(this)), LogLevel.NORMAL);
			return;
		}

		// Roll back partial spawns now, even beside players. Deleting the group also
		// cancels outstanding native spawn requests; members need explicit removal.
		foreach (SCR_AIGroup group : mission.m_aGroups)
		{
			if (!group)
				continue;

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

		foreach (IEntity entity : mission.m_aEntities)
		{
			if (entity && !HasPlayerAboard(entity))
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		mission.m_aGroups.Clear();
		mission.m_aGroupRoles.Clear();
		mission.m_aAffiliated.Clear();
		mission.m_aEntities.Clear();
		mission.m_iDeployAttempts++;
		Print(string.Format("[AROR MISSIONS] %1 could not deploy (attempt %2)", mission.m_sCodename, mission.m_iDeployAttempts), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish_S(AROR_Mission mission, AROR_EMissionResult result)
	{
		bool succeeded = result == AROR_EMissionResult.SUCCEEDED;
		string typeName = AROR_MissionTexts.TypeName(mission.m_eType);
		if (succeeded)
		{
			int funds = mission.GetRewardFunds();
			int manpower = mission.GetRewardManpower();
			AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
			if (economy)
			{
				economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, funds);
				economy.AddManpower_S(manpower);
			}

			string reward = string.Format("+%1  +%2 MP", AROR_EconomyComponent.FormatMoney(funds), manpower);
			Notify(AROR_ENotificationType.HEADLINE, string.Format("%1 complete   %2", mission.m_sCodename, reward), string.Empty, Color.FromRGBA(159, 211, 107, 255));
			AddLog(string.Format("%1 complete (%2)", mission.m_sCodename, reward));
			Print(string.Format("[AROR MISSIONS] %1 (%2) succeeded: %3", mission.m_sCodename, typeName, reward), LogLevel.NORMAL);
		}
		else
		{
			string outcome = mission.m_sOutcome;
			if (outcome.IsEmpty())
				outcome = "the operation failed";

			Notify(AROR_ENotificationType.NOTIFICATION, string.Format("%1 failed: %2", mission.m_sCodename, outcome), "Operation failed", Color.FromRGBA(224, 87, 63, 255));
			AddLog(string.Format("%1 failed: %2", mission.m_sCodename, outcome));
			Print(string.Format("[AROR MISSIONS] %1 (%2) failed: %3", mission.m_sCodename, typeName, outcome), LogLevel.NORMAL);
		}

		mission.Release_S(this, succeeded);
		m_aActive.RemoveItem(mission);
		m_iRefillTimer = Math.Max(m_iRefillTimer, m_iRefillDelay);
		PublishSnapshot_S(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCleanups_S(int seconds)
	{
		for (int i = m_aCleanups.Count() - 1; i >= 0; i--)
		{
			AROR_MissionCleanup cleanup = m_aCleanups[i];
			cleanup.m_iAge = cleanup.m_iAge + seconds;

			bool watched;
			if (cleanup.m_iAge < m_iCleanupTimeout)
			{
				foreach (IEntity entity : cleanup.m_aEntities)
				{
					if (entity && IsResistanceNear_S(entity.GetOrigin(), m_fCleanupRange))
					{
						watched = true;
						break;
					}
				}
			}

			if (watched)
				continue;

			foreach (IEntity leftover : cleanup.m_aEntities)
			{
				if (leftover && !HasPlayerAboard(leftover))
					SCR_EntityHelper.DeleteEntityAndChildren(leftover);
			}

			m_aCleanups.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle with a player in one of its seats
	protected static bool HasPlayerAboard(IEntity entity)
	{
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
	protected void RefreshPlayerPositions_S()
	{
		m_aPlayerPositions.Clear();
		array<int> playerIds = {};
		PlayerManager playerManager = GetGame().GetPlayerManager();
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
			if (IsAlive(controlled))
				m_aPlayerPositions.Insert(controlled.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Notify(AROR_ENotificationType type, string text, string kicker, Color color)
	{
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(type, text, string.Empty, color, kicker));
	}

	//------------------------------------------------------------------------------------------------
	//! Network log line, stamped with the in-game time
	void AddLog(string text)
	{
		m_aLog.InsertAt(GetClockText() + "  " + Clean(text), 0);
		while (m_aLog.Count() > LOG_LINES)
		{
			m_aLog.Remove(m_aLog.Count() - 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetClockText()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world || !world.GetTimeAndWeatherManager())
			return "--:--";

		int hours, minutes, seconds;
		world.GetTimeAndWeatherManager().GetHoursMinutesSeconds(hours, minutes, seconds);
		return hours.ToString(2) + ":" + minutes.ToString(2);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the snapshot separators from free text
	static string Clean(string text)
	{
		text.Replace(FIELD_SEPARATOR, "/");
		text.Replace(ROW_SEPARATOR, ",");
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Saving (see AROR_MissionBoardComponentSerializer)
	//------------------------------------------------------------------------------------------------
	int GetActiveMissions_S(notnull array<AROR_Mission> outMissions)
	{
		outMissions.Clear();
		foreach (AROR_Mission mission : m_aActive)
		{
			outMissions.Insert(mission);
		}

		return outMissions.Count();
	}

	//------------------------------------------------------------------------------------------------
	int GetNextId()
	{
		return m_iNextId;
	}

	//------------------------------------------------------------------------------------------------
	//! A saved active mission; it deploys again from the start when the Resistance comes near
	void RestoreMission_S(AROR_Mission mission)
	{
		if (!mission || m_aActive.Count() >= m_iMaxActive)
			return;

		mission.m_bActive = true;
		mission.m_bDeployed = false;
		mission.m_bRestored = true;
		mission.m_iLeaderId = 0;
		mission.m_iPhase = 0;
		// Convoys restart at their origin, not at the saved last-known marker position.
		if (mission.m_eType == AROR_EMissionType.CONVOY_AMBUSH)
		{
			AROR_LocationComponent origin = GetLocation(mission.m_sLocationId);
			if (origin)
				mission.m_vPosition = origin.GetCenter();
		}

		mission.m_sProgress = mission.GetStartProgress(this);
		m_iNextId = Math.Max(m_iNextId, mission.m_iId + 1);
		m_aActive.Insert(mission);
		PublishSnapshot_S(true);
	}

	//------------------------------------------------------------------------------------------------
	void RestoreNextId_S(int nextId)
	{
		m_iNextId = Math.Max(m_iNextId, nextId);
	}

	//------------------------------------------------------------------------------------------------
	// Snapshot (server writes, every machine parses)
	//------------------------------------------------------------------------------------------------
	//! force: publish now; otherwise only when something besides timers changed, or every resync interval
	protected void PublishSnapshot_S(bool force)
	{
		if (!Replication.IsServer())
			return;

		string structure;
		string snapshot = BuildSnapshot(structure);
		string log = JoinLog();
		structure += ROW_SEPARATOR + log;

		bool changed = structure != m_sLastStructure;
		if (!force && !changed && m_iResyncTimer > 0)
			return;

		m_sLastStructure = structure;
		m_iResyncTimer = m_iResyncInterval;
		m_sMissions = snapshot;
		m_sLog = log;
		Replication.BumpMe();
		OnSnapshotReplicated();
	}

	//------------------------------------------------------------------------------------------------
	//! Row per mission: id|type|active|codename|location|x|z|radius|destination|dx|dz|enemy|funds|manpower|limit|
	//! seconds|risk|guards|leader|phase|progress. structure: the same without the timers, for change detection.
	protected string BuildSnapshot(out string structure)
	{
		string snapshot;
		structure = string.Empty;
		array<AROR_Mission> missions = {};
		foreach (AROR_Mission offer : m_aOffers)
		{
			missions.Insert(offer);
		}

		foreach (AROR_Mission active : m_aActive)
		{
			missions.Insert(active);
		}

		foreach (int i, AROR_Mission mission : missions)
		{
			int activeValue;
			int seconds = mission.m_iOfferSeconds;
			if (mission.m_bActive)
			{
				activeValue = 1;
				seconds = mission.m_iRemaining;
			}

			int type = mission.m_eType;
			int enemy = mission.m_eEnemyRole;
			string head = string.Format("%1|%2|%3|%4|%5|%6|%7|%8|%9", mission.m_iId, type, activeValue, mission.m_sCodename, mission.m_sLocationId, Math.Round(mission.m_vPosition[0]), Math.Round(mission.m_vPosition[2]), Math.Round(mission.m_fAreaRadius), mission.m_sDestinationId);
			string middle = string.Format("|%1|%2|%3|%4|%5|%6", Math.Round(mission.m_vDestination[0]), Math.Round(mission.m_vDestination[2]), enemy, mission.m_iFunds, mission.m_iManpower, mission.m_iTimeLimit);
			string tail = string.Format("|%1|%2|%3|%4|%5", mission.m_iRisk, mission.m_iGuards, mission.m_sLeaderName, mission.m_iPhase, Clean(mission.m_sProgress));
			tail += "|" + mission.m_iLeaderId.ToString();

			if (i > 0)
			{
				snapshot += ROW_SEPARATOR;
				structure += ROW_SEPARATOR;
			}

			snapshot += head + middle + "|" + seconds.ToString() + tail;
			structure += head + middle + tail;
		}

		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	protected string JoinLog()
	{
		string joined;
		foreach (int i, string line : m_aLog)
		{
			if (i > 0)
				joined += ROW_SEPARATOR;

			joined += line;
		}

		return joined;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSnapshotReplicated()
	{
		m_iReceivedAt = System.GetTickCount();
		ParseMissions();

		m_aLogLines.Clear();
		if (!m_sLog.IsEmpty())
			m_sLog.Split(ROW_SEPARATOR, m_aLogLines, true);

		m_iSnapshotRevision++;
	}

	//------------------------------------------------------------------------------------------------
	protected void ParseMissions()
	{
		m_aInfos.Clear();
		if (m_sMissions.IsEmpty())
			return;

		array<string> rows = {};
		m_sMissions.Split(ROW_SEPARATOR, rows, true);
		foreach (string row : rows)
		{
			array<string> fields = {};
			row.Split(FIELD_SEPARATOR, fields, false);
			if (fields.Count() < SNAPSHOT_FIELDS)
				continue;

			AROR_MissionInfo info = new AROR_MissionInfo();
			info.m_iId = fields[0].ToInt();
			info.m_eType = fields[1].ToInt();
			info.m_bActive = fields[2].ToInt() != 0;
			info.m_sCodename = fields[3];
			info.m_sLocationId = fields[4];
			info.m_vPosition = Vector(fields[5].ToFloat(), 0, fields[6].ToFloat());
			info.m_fAreaRadius = fields[7].ToFloat();
			info.m_sDestinationId = fields[8];
			info.m_vDestination = Vector(fields[9].ToFloat(), 0, fields[10].ToFloat());
			info.m_bHasDestination = info.m_vDestination[0] != 0 || info.m_vDestination[2] != 0;
			info.m_eEnemyRole = fields[11].ToInt();
			info.m_iFunds = fields[12].ToInt();
			info.m_iManpower = fields[13].ToInt();
			info.m_iTimeLimit = fields[14].ToInt();
			info.m_iSeconds = fields[15].ToInt();
			info.m_iRisk = fields[16].ToInt();
			info.m_iGuards = fields[17].ToInt();
			info.m_sLeaderName = fields[18];
			info.m_iPhase = fields[19].ToInt();
			info.m_sProgress = fields[20];
			info.m_iLeaderId = fields[21].ToInt();
			m_aInfos.Insert(info);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Lifecycle
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(Update_S, m_iUpdateInterval * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Update_S);

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
