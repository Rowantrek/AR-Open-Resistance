[ComponentEditorProps(category: "OpenResistance/HighCommand", description: "Resistance high command (HICOMM): leaders, AI squads and their orders. Attach to the game mode entity.")]
class AROR_HighCommandComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Resistance high command (see ARCHITECTURE.md -> High Command), used through the HICOMM phone app.
//! Server: owns the Resistance leaders, the resistance name, raised AI squads and their orders. Every request arrives
//! through the AROR_RequestHiComm* RPCs on SCR_PlayerController and is validated here.
//! Clients: read a compact replicated snapshot (leaders, name, squad summaries) to draw HICOMM.
//! Leaders: the host (listen server/Workbench player; admins on a dedicated server) is always a leader and can
//! delegate other players. Each leader commands only their own squads; everyone sees every squad.
//! Only the resistance name is saved (AROR_HighCommandComponentSerializer): squads are session-only and the Economy
//! serializer saves funds/manpower as if every living squad had been refunded, so a loaded campaign never loses their cost.
class AROR_HighCommandComponent : SCR_BaseGameModeComponent
{
	[Attribute("", desc: "Squads leaders can raise in Unit Management")]
	protected ref array<ref AROR_HighCommandSquadType> m_aSquadTypes;

	[Attribute("4", desc: "Most squads one leader can command at a time", params: "1 20 1")]
	protected int m_iMaxSquadsPerLeader;

	[Attribute("Resistance", desc: "Name of the resistance until campaign setup sets it")]
	protected string m_sDefaultResistanceName;

	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty AI group prefab squads are built in", params: "et")]
	protected ResourceName m_sGroupPrefab;

	[Attribute("{1B0E3436C30FA211}Prefabs/AI/Waypoints/AIWaypoint_Attack.et", UIWidgets.ResourceNamePicker, desc: "Waypoint for Attack orders", params: "et")]
	protected ResourceName m_sAttackWaypointPrefab;

	[Attribute("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", UIWidgets.ResourceNamePicker, desc: "Waypoint for Defend and Hold orders (and squads that reach a Move point)", params: "et")]
	protected ResourceName m_sDefendWaypointPrefab;

	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, desc: "Waypoint for Move orders", params: "et")]
	protected ResourceName m_sMoveWaypointPrefab;

	[Attribute("25", desc: "A squad within this distance (m) of its Move point has arrived and stands by there", params: "5 100 1")]
	protected float m_fMoveArrivalRadius;

	[Attribute("30", desc: "Radius (m) a standing-by squad defends around its position", params: "10 150 1")]
	protected float m_fHoldRadius;

	[Attribute("20", desc: "Squads form up within this distance (m) of the Resistance HQ", params: "5 100 1")]
	protected float m_fFormUpRadius;

	[Attribute("90", desc: "A squad whose members have not spawned after this many seconds is refunded and removed", params: "10 600 1")]
	protected int m_iFormTimeout;

	[Attribute("2", desc: "Seconds between squad status updates (alive count, position)", params: "1 10 1")]
	protected int m_iUpdateInterval;

	[Attribute("0 0 0", UIWidgets.Coords, desc: "South-west corner of the area HICOMM's map shows", category: "Map")]
	protected vector m_vMapMin;

	[Attribute("12800 0 12800", UIWidgets.Coords, desc: "North-east corner of the area HICOMM's map shows (Everon: 12800 x 12800)", category: "Map")]
	protected vector m_vMapMax;

	protected static AROR_HighCommandComponent s_Instance;

	protected static const string LIST_SEPARATOR = ",";
	protected static const string ROW_SEPARATOR = ";";
	protected static const string FIELD_SEPARATOR = "|";

	//! Replicated snapshot: comma-separated player IDs
	[RplProp(onRplName: "OnSnapshotReplicated")]
	protected string m_sHostIds;

	[RplProp(onRplName: "OnSnapshotReplicated")]
	protected string m_sDelegateIds;

	[RplProp(onRplName: "OnSnapshotReplicated")]
	protected string m_sResistanceName;

	//! Replicated snapshot: one row per squad (see BuildSquadSnapshot)
	[RplProp(onRplName: "OnSnapshotReplicated")]
	protected string m_sSquads;

	// Server
	protected ref array<ref AROR_HighCommandSquad> m_aSquads = {};
	protected ref array<int> m_aDelegates = {};
	protected ref map<int, int> m_mCallsignCounters = new map<int, int>();
	protected int m_iNextSquadId = 1;
	protected string m_sLastPublished;

	// Every machine: parsed snapshot
	protected ref array<int> m_aHostIdsCache = {};
	protected ref array<int> m_aDelegateIdsCache = {};
	protected ref array<ref AROR_HighCommandSquadInfo> m_aSquadInfos = {};
	protected int m_iSnapshotRevision;

	//------------------------------------------------------------------------------------------------
	//! Exists on every machine (the snapshot is replicated)
	static AROR_HighCommandComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// Every machine: read access for HICOMM
	//------------------------------------------------------------------------------------------------
	string GetResistanceName()
	{
		if (m_sResistanceName.IsEmpty())
			return m_sDefaultResistanceName;

		return m_sResistanceName;
	}

	//------------------------------------------------------------------------------------------------
	//! The name a new campaign suggests on the setup screen
	string GetDefaultResistanceName()
	{
		return m_sDefaultResistanceName;
	}

	//------------------------------------------------------------------------------------------------
	int GetMaxSquadsPerLeader()
	{
		return m_iMaxSquadsPerLeader;
	}

	//------------------------------------------------------------------------------------------------
	//! Increases whenever the snapshot changes, so the app can redraw only then
	int GetSnapshotRevision()
	{
		return m_iSnapshotRevision;
	}

	//------------------------------------------------------------------------------------------------
	bool IsHost(int playerId)
	{
		return playerId > 0 && m_aHostIdsCache.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	bool IsDelegate(int playerId)
	{
		return playerId > 0 && m_aDelegateIdsCache.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Resistance Leader: the host or a delegated leader
	bool IsLeader(int playerId)
	{
		return IsHost(playerId) || IsDelegate(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! A leader may command their own squads; the host also commands unassigned ones
	bool CanCommand(int playerId, int ownerId)
	{
		if (!IsLeader(playerId))
			return false;

		return ownerId == playerId || (ownerId == 0 && IsHost(playerId));
	}

	//------------------------------------------------------------------------------------------------
	int GetSquadTypes(notnull array<AROR_HighCommandSquadType> outTypes)
	{
		outTypes.Clear();
		if (!m_aSquadTypes)
			return 0;

		foreach (AROR_HighCommandSquadType type : m_aSquadTypes)
		{
			outTypes.Insert(type);
		}

		return outTypes.Count();
	}

	//------------------------------------------------------------------------------------------------
	AROR_HighCommandSquadType GetSquadType(int index)
	{
		if (!m_aSquadTypes || !m_aSquadTypes.IsIndexValid(index))
			return null;

		return m_aSquadTypes[index];
	}

	//------------------------------------------------------------------------------------------------
	int GetSquads(notnull array<AROR_HighCommandSquadInfo> outSquads)
	{
		outSquads.Clear();
		foreach (AROR_HighCommandSquadInfo info : m_aSquadInfos)
		{
			outSquads.Insert(info);
		}

		return outSquads.Count();
	}

	//------------------------------------------------------------------------------------------------
	AROR_HighCommandSquadInfo GetSquadInfo(int squadId)
	{
		foreach (AROR_HighCommandSquadInfo info : m_aSquadInfos)
		{
			if (info.m_iId == squadId)
				return info;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int CountSquadsOwnedBy(int playerId)
	{
		int count;
		foreach (AROR_HighCommandSquadInfo info : m_aSquadInfos)
		{
			if (info.m_iOwnerId == playerId)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	void GetMapBounds(out vector min, out vector max)
	{
		min = m_vMapMin;
		max = m_vMapMax;
	}

	//------------------------------------------------------------------------------------------------
	//! The Resistance HQ location, or null before a playthrough exists
	static AROR_LocationComponent FindHQ()
	{
		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (!locationManager)
			return null;

		array<AROR_LocationComponent> hqs = {};
		locationManager.GetLocationsOfType(AROR_ELocationType.HQ, hqs);
		foreach (AROR_LocationComponent hq : hqs)
		{
			if (hq)
				return hq;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the order cannot be given to this target, or empty if it can (also checked on clients for feedback)
	static string GetOrderTargetProblem(AROR_EHighCommandOrder order, AROR_LocationComponent target)
	{
		if (!target)
			return "Select a target location";

		bool resistanceHeld = target.GetControllingRole() == AROR_EFactionRole.RESISTANCE;
		if (order == AROR_EHighCommandOrder.ATTACK && resistanceHeld)
			return target.GetName() + " is already held by the Resistance";

		if (order == AROR_EHighCommandOrder.DEFEND && !resistanceHeld)
			return "Only Resistance-held locations can be defended";

		if (order != AROR_EHighCommandOrder.ATTACK && order != AROR_EHighCommandOrder.DEFEND)
			return "Select an order";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	// Server: requests (arrive through SCR_PlayerController RPCs)
	//------------------------------------------------------------------------------------------------
	void RequestRaise_S(int playerId, int typeIndex)
	{
		if (!Replication.IsServer())
			return;

		RefreshLeaders_S();
		if (!IsLeader(playerId))
		{
			Reply(playerId, "Only a Resistance Leader can raise squads");
			return;
		}

		AROR_HighCommandSquadType type = GetSquadType(typeIndex);
		if (!type || type.GetSize() == 0)
		{
			Reply(playerId, "Unknown squad type");
			return;
		}

		if (CountServerSquadsOwnedBy(playerId) >= m_iMaxSquadsPerLeader)
		{
			Reply(playerId, string.Format("You already command %1 squads (the maximum)", m_iMaxSquadsPerLeader));
			return;
		}

		AROR_LocationComponent hq = FindHQ();
		if (!hq)
		{
			Reply(playerId, "No Resistance HQ to raise squads at");
			return;
		}

		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy)
			return;

		if (economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE) < type.m_iFundsCost || economy.GetManpower() < type.m_iManpowerCost)
		{
			Reply(playerId, string.Format("Not enough resources: %1 needs %2 and %3 MP", type.m_sName, AROR_EconomyComponent.FormatMoney(type.m_iFundsCost), type.m_iManpowerCost));
			return;
		}

		if (!economy.SpendFactionFunds_S(AROR_EFactionRole.RESISTANCE, type.m_iFundsCost))
			return;

		if (!economy.SpendManpower_S(type.m_iManpowerCost))
		{
			economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, type.m_iFundsCost);
			return;
		}

		SCR_AIGroup group = CreateGroup_S(hq.GetCenter(), type);
		if (!group)
		{
			economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, type.m_iFundsCost);
			economy.AddManpower_S(type.m_iManpowerCost);
			Reply(playerId, "The squad could not be raised (group setup failed)");
			return;
		}

		AROR_HighCommandSquad squad = new AROR_HighCommandSquad();
		squad.m_iId = m_iNextSquadId++;
		squad.m_iOwnerId = playerId;
		squad.m_iType = typeIndex;
		squad.m_sCallsign = NextCallsign(typeIndex, type);
		squad.m_iFundsCost = type.m_iFundsCost;
		squad.m_iManpowerCost = type.m_iManpowerCost;
		squad.m_iSize = type.GetSize();
		squad.m_iRaisedAt = GetGame().GetWorld().GetWorldTime();
		squad.m_Group = group;
		squad.m_vPosition = group.GetOrigin();
		m_aSquads.Insert(squad);

		Print(string.Format("[AROR HICOMM] Player %1 raised %2 (%3 men) at the HQ", playerId, squad.m_sCallsign, squad.m_iSize), LogLevel.NORMAL);
		Reply(playerId, squad.m_sCallsign + " is forming up at the HQ");
		PublishSnapshot_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Attack/defend take a location ID; move takes a map position; hold uses the squad's current position
	void RequestOrder_S(int playerId, int squadId, AROR_EHighCommandOrder order, string locationId, vector position)
	{
		if (!Replication.IsServer())
			return;

		RefreshLeaders_S();
		AROR_HighCommandSquad squad = FindServerSquad(squadId);
		if (!squad)
		{
			Reply(playerId, "That squad no longer exists");
			return;
		}

		if (!CanCommand(playerId, squad.m_iOwnerId))
		{
			Reply(playerId, squad.m_sCallsign + " is not under your command");
			return;
		}

		string problem;
		string description;
		if (order == AROR_EHighCommandOrder.MOVE)
		{
			problem = GetMoveProblem(position);
			if (problem.IsEmpty() && !IssueWaypoint_S(squad, m_sMoveWaypointPrefab, position, m_fMoveArrivalRadius))
				problem = "Order failed: the move waypoint prefab is not set or invalid";

			description = "moving to grid " + GridReference(position);
			squad.m_sTargetId = string.Empty;
		}
		else if (order == AROR_EHighCommandOrder.HOLD)
		{
			position = squad.m_vPosition;
			if (!IssueWaypoint_S(squad, m_sDefendWaypointPrefab, position, m_fHoldRadius))
				problem = "Order failed: the defend waypoint prefab is not set or invalid";

			description = "holding at grid " + GridReference(position);
			squad.m_sTargetId = string.Empty;
		}
		else
		{
			AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
			AROR_LocationComponent target;
			if (locationManager)
				target = locationManager.GetLocationById(locationId);

			problem = GetOrderTargetProblem(order, target);
			if (problem.IsEmpty())
			{
				// Attack: move to the site first. The attack waypoint completes at once when no enemy is in sight
				// yet, which left far-away squads standing still; the assault starts on arrival (see Update_S).
				ResourceName prefab = m_sMoveWaypointPrefab;
				float radius = Math.Max(target.GetRadius() * 0.5, 25);
				if (order == AROR_EHighCommandOrder.DEFEND)
					prefab = m_sDefendWaypointPrefab;

				position = target.GetCenter();
				squad.m_bAssaulting = false;
				if (!IssueWaypoint_S(squad, prefab, position, radius))
					problem = "Order failed: the waypoint prefab is not set or invalid";

				description = OrderVerb(order) + " " + target.GetName();
				squad.m_sTargetId = target.GetId();
			}
		}

		if (!problem.IsEmpty())
		{
			Reply(playerId, problem);
			return;
		}

		squad.m_eOrder = order;
		squad.m_vTarget = position;

		// The squad's leader may have handed command to the host; ordering it takes it over
		if (squad.m_iOwnerId == 0)
			squad.m_iOwnerId = playerId;

		Print(string.Format("[AROR HICOMM] Player %1 ordered %2: %3 (%4)", playerId, squad.m_sCallsign, typename.EnumToString(AROR_EHighCommandOrder, order), description), LogLevel.NORMAL);
		Reply(playerId, squad.m_sCallsign + ": " + description);
		PublishSnapshot_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the squad's waypoint; false if the prefab is not a waypoint
	protected bool IssueWaypoint_S(AROR_HighCommandSquad squad, ResourceName prefab, vector position, float radius)
	{
		AIWaypoint waypoint = AIWaypoint.Cast(SpawnPrefab(prefab, position));
		if (!waypoint)
			return false;

		waypoint.SetCompletionRadius(radius);
		ClearWaypoint_S(squad);
		squad.m_Waypoint = waypoint;
		if (squad.m_Group)
			squad.m_Group.AddWaypoint(waypoint);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Why a squad cannot be moved to this point, or empty if it can (also checked on clients for feedback)
	string GetMoveProblem(vector position)
	{
		if (position[0] < m_vMapMin[0] || position[0] > m_vMapMax[0] || position[2] < m_vMapMin[2] || position[2] > m_vMapMax[2])
			return "That point is off the map";

		BaseWorld world = GetGame().GetWorld();
		if (world && world.GetSurfaceY(position[0], position[2]) <= 0)
			return "Squads can only move over land";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Six-figure grid reference (100 m), as on the HICOMM map
	static string GridReference(vector position)
	{
		int x = Math.Floor(Math.Max(position[0], 0) / 100);
		int z = Math.Floor(Math.Max(position[2], 0) / 100);
		return x.ToString(3) + " " + z.ToString(3);
	}

	//------------------------------------------------------------------------------------------------
	//! Host only: make a player a Resistance Leader, or revoke it (their squads become unassigned)
	void RequestSetLeader_S(int requesterId, int targetId, bool leader)
	{
		if (!Replication.IsServer())
			return;

		RefreshLeaders_S();
		if (!IsHost(requesterId))
		{
			Reply(requesterId, "Only the host can appoint Resistance Leaders");
			return;
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager.GetPlayerController(targetId))
		{
			Reply(requesterId, "That player is not connected");
			return;
		}

		if (IsHost(targetId))
		{
			Reply(requesterId, "The host is always a Resistance Leader");
			return;
		}

		string name = playerManager.GetPlayerName(targetId);
		if (leader)
		{
			if (m_aDelegates.Contains(targetId))
				return;

			m_aDelegates.Insert(targetId);
			Reply(requesterId, name + " is now a Resistance Leader");
			Reply(targetId, "You are now a Resistance Leader: open HICOMM on your phone");
		}
		else
		{
			if (!m_aDelegates.Contains(targetId))
				return;

			m_aDelegates.RemoveItem(targetId);
			ReleaseSquads_S(targetId);
			Reply(requesterId, name + " is no longer a Resistance Leader");
			Reply(targetId, "You are no longer a Resistance Leader");
		}

		PublishSnapshot_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Sets the name without checks: campaign setup (already validated) and save restore.
	void SetResistanceName_S(string name)
	{
		if (!Replication.IsServer())
			return;

		m_sResistanceName = name;
		PublishSnapshot_S();
	}

	//------------------------------------------------------------------------------------------------
	// Server: refunds
	//------------------------------------------------------------------------------------------------
	//! Funds and manpower that living squads would return if the campaign ended now (used by the Economy serializer)
	void GetRefundValue_S(out int funds, out int manpower)
	{
		funds = 0;
		manpower = 0;
		foreach (AROR_HighCommandSquad squad : m_aSquads)
		{
			funds += squad.m_iFundsCost;
			manpower += squad.m_iManpowerCost;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Returns every living squad's full cost to the Resistance and stands the squads down
	void RefundAllSquads_S(string reason)
	{
		if (!Replication.IsServer() || m_aSquads.IsEmpty())
			return;

		int funds, manpower;
		GetRefundValue_S(funds, manpower);

		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (economy)
		{
			economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, funds);
			economy.AddManpower_S(manpower);
		}

		Print(string.Format("[AROR HICOMM] %1: refunded %2 squads (%3, %4 MP)", reason, m_aSquads.Count(), AROR_EconomyComponent.FormatMoney(funds), manpower), LogLevel.NORMAL);

		for (int i = m_aSquads.Count() - 1; i >= 0; i--)
		{
			RemoveSquad_S(i, false);
		}

		PublishSnapshot_S();
	}

	//------------------------------------------------------------------------------------------------
	// Server internals
	//------------------------------------------------------------------------------------------------
	protected void Reply(int playerId, string text)
	{
		AROR_NotificationManagerComponent.SendHint_S(playerId, text);
	}

	//------------------------------------------------------------------------------------------------
	protected static string OrderVerb(AROR_EHighCommandOrder order)
	{
		if (order == AROR_EHighCommandOrder.ATTACK)
			return "attacking";

		if (order == AROR_EHighCommandOrder.DEFEND)
			return "defending";

		if (order == AROR_EHighCommandOrder.MOVE)
			return "moving";

		return "holding";
	}

	//------------------------------------------------------------------------------------------------
	protected string NextCallsign(int typeIndex, AROR_HighCommandSquadType type)
	{
		int number = 1;
		if (m_mCallsignCounters.Contains(typeIndex))
			number = m_mCallsignCounters.Get(typeIndex) + 1;

		m_mCallsignCounters.Set(typeIndex, number);
		return string.Format("%1-%2", type.m_sCallsign, number);
	}

	//------------------------------------------------------------------------------------------------
	protected AROR_HighCommandSquad FindServerSquad(int squadId)
	{
		foreach (AROR_HighCommandSquad squad : m_aSquads)
		{
			if (squad.m_iId == squadId)
				return squad;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountServerSquadsOwnedBy(int playerId)
	{
		int count;
		foreach (AROR_HighCommandSquad squad : m_aSquads)
		{
			if (squad.m_iOwnerId == playerId)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! A leader's squads keep their orders but become unassigned, so the host can take them over
	protected void ReleaseSquads_S(int playerId)
	{
		foreach (AROR_HighCommandSquad squad : m_aSquads)
		{
			if (squad.m_iOwnerId == playerId)
				squad.m_iOwnerId = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Host: the listen-server/Workbench player; on a dedicated server, every connected admin
	protected bool IsHostPlayer_S(int playerId)
	{
		if (playerId <= 0)
			return false;

		if (RplSession.Mode() != RplMode.Dedicated)
		{
			PlayerController localController = GetGame().GetPlayerController();
			return localController && localController.GetPlayerId() == playerId;
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		return playerManager.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR) || playerManager.HasPlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR);
	}

	//------------------------------------------------------------------------------------------------
	//! Recomputes the hosts from the connected players (the host's controller may not exist yet when they register)
	protected void RefreshLeaders_S()
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		array<int> hosts = {};
		foreach (int playerId : playerIds)
		{
			if (IsHostPlayer_S(playerId))
				hosts.Insert(playerId);
		}

		m_sHostIds = JoinIds(hosts);
		ParseLeaders();
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_AIGroup CreateGroup_S(vector hqPosition, AROR_HighCommandSquadType type)
	{
		vector position = hqPosition;
		float angle = Math.RandomFloat(0, Math.PI2);
		float distance = Math.RandomFloatInclusive(m_fFormUpRadius * 0.5, m_fFormUpRadius);
		position[0] = position[0] + Math.Cos(angle) * distance;
		position[2] = position[2] + Math.Sin(angle) * distance;

		SCR_AIGroup.IgnoreSpawning(true);
		IEntity entity = SpawnPrefab(m_sGroupPrefab, position);
		SCR_AIGroup.IgnoreSpawning(false);

		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);

			return null;
		}

		string factionKey = GetResistanceFactionKey();
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
		bool assigned;
		if (faction)
		{
			if (!group.GetFaction())
				assigned = group.InitFactionKey(factionKey);
			else
				assigned = group.SetFaction(faction);
		}

		if (!assigned)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return null;
		}

		group.SetCanDeleteIfNoPlayer(false);
		group.SetPrivate(true);
		group.SetPrivacyChangeable(false);
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);

		// Each member spawns as the Resistance base character and is dressed from its preset unit loadout
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		int slots;
		if (presets)
			slots = presets.FillGroup_S(group, AROR_EFactionRole.RESISTANCE, type.m_aUnits);

		if (slots < 1)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return null;
		}

		// Members spawn through the engine's shared AI queue, not all in this frame
		group.RequestSpawn(slots);
		return group;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnPrefab(ResourceName prefab, vector position)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		BaseWorld world = GetGame().GetWorld();
		position[1] = world.GetSurfaceY(position[0], position[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(resource, world, params);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearWaypoint_S(AROR_HighCommandSquad squad)
	{
		if (!squad.m_Waypoint)
			return;

		if (squad.m_Group)
			squad.m_Group.RemoveWaypoint(squad.m_Waypoint);

		SCR_EntityHelper.DeleteEntityAndChildren(squad.m_Waypoint);
		squad.m_Waypoint = null;
	}

	//------------------------------------------------------------------------------------------------
	//! deleteGroup: remove the (empty) group entity too; a refund at campaign end leaves the soldiers in the world
	protected void RemoveSquad_S(int index, bool deleteGroup)
	{
		AROR_HighCommandSquad squad = m_aSquads[index];
		ClearWaypoint_S(squad);
		if (deleteGroup && squad.m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(squad.m_Group);

		m_aSquads.RemoveOrdered(index);
	}

	//------------------------------------------------------------------------------------------------
	protected string GetResistanceFactionKey()
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.Cast(GetOwner().FindComponent(AROR_FactionRolesComponent));
		if (!roles)
			return string.Empty;

		return roles.GetFactionKey(AROR_EFactionRole.RESISTANCE);
	}

	//------------------------------------------------------------------------------------------------
	//! Periodic: hosts, squad strength/position, formation timeouts and losses
	protected void Update_S()
	{
		RefreshLeaders_S();

		int now = GetGame().GetWorld().GetWorldTime();
		string factionKey = GetResistanceFactionKey();

		for (int i = m_aSquads.Count() - 1; i >= 0; i--)
		{
			AROR_HighCommandSquad squad = m_aSquads[i];
			squad.m_iAlive = CountAlive(squad, factionKey);

			if (squad.m_iAlive > 0)
				squad.m_bFormed = true;

			if (!squad.m_bFormed && now - squad.m_iRaisedAt > m_iFormTimeout * 1000)
			{
				// Never spawned: return the full cost rather than charge for nothing
				AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
				if (economy)
				{
					economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, squad.m_iFundsCost);
					economy.AddManpower_S(squad.m_iManpowerCost);
				}

				Print(string.Format("[AROR HICOMM] %1 did not spawn within %2 s; refunded", squad.m_sCallsign, m_iFormTimeout), LogLevel.WARNING);
				if (squad.m_iOwnerId > 0)
					Reply(squad.m_iOwnerId, squad.m_sCallsign + " could not form up and was refunded");

				RemoveSquad_S(i, true);
				continue;
			}

			if (squad.m_bFormed && (squad.m_iAlive == 0 || !squad.m_Group))
			{
				Print(string.Format("[AROR HICOMM] %1 was wiped out", squad.m_sCallsign), LogLevel.NORMAL);
				if (squad.m_iOwnerId > 0)
					Reply(squad.m_iOwnerId, squad.m_sCallsign + " has been wiped out");

				RemoveSquad_S(i, true);
				continue;
			}

			if (squad.m_Group)
			{
				IEntity leader = squad.m_Group.GetLeaderEntity();
				if (leader)
					squad.m_vPosition = leader.GetOrigin();
				else
					squad.m_vPosition = squad.m_Group.GetOrigin();
			}

			// Arrived at a Move point: stand by there, defending it, until the next order
			if (squad.m_eOrder == AROR_EHighCommandOrder.MOVE && vector.DistanceXZ(squad.m_vPosition, squad.m_vTarget) <= m_fMoveArrivalRadius)
			{
				if (IssueWaypoint_S(squad, m_sDefendWaypointPrefab, squad.m_vTarget, m_fHoldRadius))
				{
					squad.m_eOrder = AROR_EHighCommandOrder.HOLD;
					if (squad.m_iOwnerId > 0)
						Reply(squad.m_iOwnerId, squad.m_sCallsign + " in position at grid " + GridReference(squad.m_vTarget) + ", standing by");
				}
			}

			// Reached the attack target: assault it (attack waypoint now that enemies are close), then hold the
			// site with a defend waypoint once the attack waypoint is done
			if (squad.m_eOrder == AROR_EHighCommandOrder.ATTACK && !squad.m_bAssaulting)
			{
				AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
				AROR_LocationComponent target;
				if (locationManager)
					target = locationManager.GetLocationById(squad.m_sTargetId);

				if (target && vector.DistanceXZ(squad.m_vPosition, target.GetCenter()) <= Math.Max(target.GetRadius(), m_fMoveArrivalRadius * 2))
				{
					squad.m_bAssaulting = true;
					IssueWaypoint_S(squad, m_sAttackWaypointPrefab, target.GetCenter(), Math.Max(target.GetRadius() * 0.5, 25));
					if (squad.m_iOwnerId > 0)
						Reply(squad.m_iOwnerId, squad.m_sCallsign + " assaulting " + target.GetName());
				}
			}
			else if (squad.m_eOrder == AROR_EHighCommandOrder.ATTACK && squad.m_bAssaulting && squad.m_Group && squad.m_Waypoint && squad.m_Group.GetCurrentWaypoint() != squad.m_Waypoint)
			{
				// The attack waypoint finished: dig in at the site rather than idling
				IssueWaypoint_S(squad, m_sDefendWaypointPrefab, squad.m_vTarget, m_fHoldRadius * 2);
			}
		}

		PublishSnapshot_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Living members; newly spawned ones are given the Resistance faction once
	protected int CountAlive(AROR_HighCommandSquad squad, string factionKey)
	{
		if (!squad.m_Group)
			return 0;

		array<AIAgent> agents = {};
		squad.m_Group.GetAgents(agents);

		int alive;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			ChimeraCharacter character = ChimeraCharacter.Cast(entity);
			if (!character)
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (controller && controller.IsDead())
				continue;

			alive++;

			if (!squad.m_aAffiliated.Contains(entity))
			{
				squad.m_aAffiliated.Insert(entity);
				FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
				if (affiliation && !factionKey.IsEmpty() && affiliation.GetAffiliatedFactionKey() != factionKey)
					affiliation.SetAffiliatedFactionByKey(factionKey);
			}
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	// Snapshot (server writes, every machine parses)
	//------------------------------------------------------------------------------------------------
	protected void PublishSnapshot_S()
	{
		if (!Replication.IsServer())
			return;

		m_sDelegateIds = JoinIds(m_aDelegates);
		m_sSquads = BuildSquadSnapshot();

		// Only replicate (and redraw) when something actually changed
		string published = m_sHostIds + ROW_SEPARATOR + m_sDelegateIds + ROW_SEPARATOR + m_sResistanceName + ROW_SEPARATOR + m_sSquads;
		if (published == m_sLastPublished)
			return;

		m_sLastPublished = published;
		Replication.BumpMe();
		OnSnapshotReplicated();
	}

	//------------------------------------------------------------------------------------------------
	//! Row per squad: id|owner|type|callsign|order|target|alive|size|formed|x|z|targetX|targetZ
	protected string BuildSquadSnapshot()
	{
		string snapshot;
		foreach (int i, AROR_HighCommandSquad squad : m_aSquads)
		{
			if (i > 0)
				snapshot += ROW_SEPARATOR;

			int formed;
			if (squad.m_bFormed)
				formed = 1;

			int order = squad.m_eOrder;
			snapshot += string.Format("%1|%2|%3|%4|%5|%6|%7|%8|%9", squad.m_iId, squad.m_iOwnerId, squad.m_iType, squad.m_sCallsign, order, squad.m_sTargetId, squad.m_iAlive, squad.m_iSize, formed);
			snapshot += string.Format("|%1|%2|%3|%4", Math.Round(squad.m_vPosition[0]), Math.Round(squad.m_vPosition[2]), Math.Round(squad.m_vTarget[0]), Math.Round(squad.m_vTarget[2]));
		}

		return snapshot;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSnapshotReplicated()
	{
		ParseLeaders();
		ParseSquads();
		m_iSnapshotRevision++;
	}

	//------------------------------------------------------------------------------------------------
	protected void ParseLeaders()
	{
		ParseIds(m_sHostIds, m_aHostIdsCache);
		ParseIds(m_sDelegateIds, m_aDelegateIdsCache);
	}

	//------------------------------------------------------------------------------------------------
	protected void ParseSquads()
	{
		m_aSquadInfos.Clear();
		if (m_sSquads.IsEmpty())
			return;

		array<string> rows = {};
		m_sSquads.Split(ROW_SEPARATOR, rows, true);
		foreach (string row : rows)
		{
			array<string> fields = {};
			row.Split(FIELD_SEPARATOR, fields, false);
			if (fields.Count() < 13)
				continue;

			AROR_HighCommandSquadInfo info = new AROR_HighCommandSquadInfo();
			info.m_iId = fields[0].ToInt();
			info.m_iOwnerId = fields[1].ToInt();
			info.m_iType = fields[2].ToInt();
			info.m_sCallsign = fields[3];
			info.m_eOrder = fields[4].ToInt();
			info.m_sTargetId = fields[5];
			info.m_iAlive = fields[6].ToInt();
			info.m_iSize = fields[7].ToInt();
			info.m_bFormed = fields[8].ToInt() != 0;
			info.m_vPosition = Vector(fields[9].ToFloat(), 0, fields[10].ToFloat());
			info.m_vTarget = Vector(fields[11].ToFloat(), 0, fields[12].ToFloat());
			m_aSquadInfos.Insert(info);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string JoinIds(notnull array<int> ids)
	{
		string joined;
		foreach (int i, int id : ids)
		{
			if (i > 0)
				joined += LIST_SEPARATOR;

			joined += id.ToString();
		}

		return joined;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ParseIds(string joined, notnull array<int> outIds)
	{
		outIds.Clear();
		if (joined.IsEmpty())
			return;

		array<string> parts = {};
		joined.Split(LIST_SEPARATOR, parts, true);
		foreach (string part : parts)
		{
			outIds.Insert(part.ToInt());
		}
	}

	//------------------------------------------------------------------------------------------------
	// Lifecycle
	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);

		if (!Replication.IsServer())
			return;

		m_aDelegates.RemoveItem(playerId);
		ReleaseSquads_S(playerId);
		RefreshLeaders_S();
		PublishSnapshot_S();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerRoleChange(int playerId, EPlayerRole roleFlags)
	{
		super.OnPlayerRoleChange(playerId, roleFlags);

		if (!Replication.IsServer())
			return;

		RefreshLeaders_S();
		PublishSnapshot_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Campaign over: every living squad's cost goes back to the Resistance
	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);

		if (Replication.IsServer())
			RefundAllSquads_S("Campaign ended");
	}

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
