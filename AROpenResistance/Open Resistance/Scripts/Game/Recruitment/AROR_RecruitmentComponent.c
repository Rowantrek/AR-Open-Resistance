[ComponentEditorProps(category: "OpenResistance/Recruitment", description: "Personal Resistance recruits, individual orders and vehicle roles. Server authoritative.")]
class AROR_RecruitmentComponentClass : SCR_BaseGameModeComponentClass {}

class AROR_RecruitmentComponent : SCR_BaseGameModeComponent
{
	[Attribute("8", desc: "Maximum personal recruits per player", params: "1 16 1")]
	protected int m_iPlayerLimit;
	[Attribute("32", desc: "Maximum personal recruits on the server", params: "1 128 1")]
	protected int m_iServerLimit;
	[Attribute("200", desc: "Resistance funds per recruit", params: "0 inf")]
	protected int m_iFundsCost;
	[Attribute("1", desc: "Resistance manpower per recruit", params: "0 inf")]
	protected int m_iManpowerCost;
	[Attribute("75", desc: "Recruit/dismiss range from Resistance HQ in metres", params: "10 300 1")]
	protected float m_fHQRange;
	[Attribute("90", desc: "Spawn timeout before full refund in seconds", params: "10 300 1")]
	protected int m_iSpawnTimeout;
	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, params: "et")]
	protected ResourceName m_sGroupPrefab;
	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, params: "et")]
	protected ResourceName m_sMovePrefab;
	[Attribute("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", UIWidgets.ResourceNamePicker, params: "et")]
	protected ResourceName m_sHoldPrefab;
	protected static AROR_RecruitmentComponent s_Instance;
	protected ref array<ref AROR_Recruit> m_aRecruits = {};
	protected int m_iNextId = 1;

	static AROR_RecruitmentComponent GetInstance() { return s_Instance; }
	int GetFundsCost() { return m_iFundsCost; }
	int GetManpowerCost() { return m_iManpowerCost; }
	int GetPlayerLimit() { return m_iPlayerLimit; }

	protected int Owned(int playerId)
	{
		int count;
		foreach (AROR_Recruit recruit : m_aRecruits)
		{
			if (recruit.m_iOwnerId == playerId)
				count++;
		}
		return count;
	}

	protected bool NearHQ(vector position)
	{
		AROR_LocationComponent hq = AROR_HighCommandComponent.FindHQ();
		return hq && vector.DistanceXZ(position, hq.GetCenter()) <= m_fHQRange;
	}

	void Request_S(int playerId, int action, int recruitId, int targetPlayer, RplId vehicleId)
	{
		if (!Replication.IsServer())
			return;
		IEntity player = AROR_FieldAccess.Player(playerId);
		if (!player || recruitId < 0 || action < AROR_ERecruitAction.RECRUIT || action > AROR_ERecruitAction.DISMISS)
			return;
		if (action == AROR_ERecruitAction.RECRUIT)
		{
			Recruit_S(playerId, player);
			return;
		}
		IEntity target;
		if (action == AROR_ERecruitAction.TRANSFER)
		{
			target = AROR_FieldAccess.Player(targetPlayer);
			if (!target || targetPlayer == playerId || vector.Distance(player.GetOrigin(), target.GetOrigin()) > 30)
			{
				Reply(playerId, "Transfers need another living Resistance player within 30 m.");
				return;
			}
		}
		IEntity vehicle;
		if (IsBoardOrder(action))
		{
			vehicle = SCR_EntityHelper.RplIdToEntity(vehicleId);
			if (!vehicle || !AROR_GarageSnapshotHelper.IsVehicle(vehicle) || AROR_ForceSpawner.IsDestroyed(vehicle)
				|| vector.Distance(player.GetOrigin(), vehicle.GetOrigin()) > 25 || !FriendlyOrCivilian(vehicle))
			{
				Reply(playerId, "Use a nearby friendly or civilian vehicle (within 25 m).");
				return;
			}
		}
		int changed;
		for (int i = m_aRecruits.Count() - 1; i >= 0; i--)
		{
			AROR_Recruit recruit = m_aRecruits[i];
			if (recruit.m_iOwnerId != playerId || recruit.m_sOwnerKey != AROR_FieldAccess.PlayerKey(playerId)
				|| (recruitId > 0 && recruit.m_iId != recruitId))
				continue;
			if (!recruit.m_bFormed || !AROR_ForceSpawner.IsAlive(recruit.m_Character))
				continue;
			if (action == AROR_ERecruitAction.TRANSFER)
			{
				if (Owned(targetPlayer) >= m_iPlayerLimit || vector.Distance(recruit.m_Character.GetOrigin(), target.GetOrigin()) > 50)
					continue;
				recruit.m_iOwnerId = targetPlayer;
				recruit.m_sOwnerKey = AROR_FieldAccess.PlayerKey(targetPlayer);
				SetOrder(recruit, AROR_ERecruitAction.FOLLOW);
				Reply(targetPlayer, "A recruit has joined your squad.");
			}
			else if (action == AROR_ERecruitAction.DISMISS)
			{
				if (!NearHQ(player.GetOrigin()) || !NearHQ(recruit.m_Character.GetOrigin()) || IsSeated(recruit.m_Character))
					continue;
				Refund(recruit);
				Remove_S(i, true);
			}
			else if (IsBoardOrder(action))
			{
				if (vector.Distance(recruit.m_Character.GetOrigin(), vehicle.GetOrigin()) > 30 || IsSeated(recruit.m_Character))
					continue;
				SetOrder(recruit, action);
				recruit.m_Vehicle = vehicle;
				recruit.m_iBoardDeadline = GetGame().GetWorld().GetWorldTime() + 20000;
			}
			else if (action == AROR_ERecruitAction.DISMOUNT)
			{
				if (!Dismount(recruit))
					continue;
				SetOrder(recruit, AROR_ERecruitAction.FOLLOW);
			}
			else
			{
				if (IsSeated(recruit.m_Character))
					continue;
				if (action == AROR_ERecruitAction.HOLD)
				{
					SetOrder(recruit, action);
					Waypoint(recruit, m_sHoldPrefab, recruit.m_Character.GetOrigin(), 10);
				}
				else
					SetOrder(recruit, AROR_ERecruitAction.FOLLOW);
			}
			changed++;
		}
		if (changed == 0)
			Reply(playerId, "No recruits could take that order. Check range, seats and HQ requirements.");
		else
			Reply(playerId, string.Format("Order sent to %1 recruit(s).", changed));
	}

	protected void Recruit_S(int playerId, IEntity player)
	{
		if (!NearHQ(player.GetOrigin()))
		{
			Reply(playerId, "Recruit at the Resistance HQ.");
			return;
		}
		if (Owned(playerId) >= m_iPlayerLimit || m_aRecruits.Count() >= m_iServerLimit)
		{
			Reply(playerId, "Recruit limit reached.");
			return;
		}
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy || economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE) < m_iFundsCost || economy.GetManpower() < m_iManpowerCost)
		{
			Reply(playerId, "The Resistance needs more funds or manpower.");
			return;
		}
		ResourceName prefab = AROR_ForceSpawner.UnitPrefab(AROR_EFactionRole.RESISTANCE, AROR_EUnitType.RIFLEMAN);
		vector position;
		if (prefab.IsEmpty() || !AROR_ForceSpawner.FindClearPosition(player.GetOrigin(), 20, 1, position))
		{
			Reply(playerId, "No recruit prefab or clear spawn position available.");
			return;
		}
		if (!economy.SpendFactionFunds_S(AROR_EFactionRole.RESISTANCE, m_iFundsCost))
			return;
		if (!economy.SpendManpower_S(m_iManpowerCost))
		{
			economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, m_iFundsCost);
			return;
		}
		array<ResourceName> members = {prefab};
		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, AROR_EFactionRole.RESISTANCE, members, position);
		if (!group)
		{
			economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, m_iFundsCost);
			economy.AddManpower_S(m_iManpowerCost);
			Reply(playerId, "Recruit could not spawn; cost refunded.");
			return;
		}
		AROR_Recruit recruit = new AROR_Recruit();
		recruit.m_iId = m_iNextId++;
		recruit.m_iOwnerId = playerId;
		recruit.m_sOwnerKey = AROR_FieldAccess.PlayerKey(playerId);
		recruit.m_Group = group;
		recruit.m_iRaisedAt = GetGame().GetWorld().GetWorldTime();
		recruit.m_iFundsCost = m_iFundsCost;
		recruit.m_iManpowerCost = m_iManpowerCost;
		m_aRecruits.Insert(recruit);
		Reply(playerId, "Recruit forming up. Open FIELD to issue orders.");
	}

	protected static bool IsBoardOrder(int order)
	{
		return order >= AROR_ERecruitAction.BOARD_PASSENGER && order <= AROR_ERecruitAction.BOARD_GUNNER;
	}

	protected static bool IsSeated(IEntity entity)
	{
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
		return access && (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut());
	}

	protected static bool FriendlyOrCivilian(IEntity vehicle)
	{
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;
		string key = affiliation.GetAffiliatedFactionKey();
		return key == AROR_ForceSpawner.FactionKey(AROR_EFactionRole.RESISTANCE) || key == AROR_ForceSpawner.FactionKey(AROR_EFactionRole.CIVILIANS) || key == "CIV";
	}

	protected bool Dismount(AROR_Recruit recruit)
	{
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(recruit.m_Character.FindComponent(CompartmentAccessComponent));
		if (!access || !access.IsInCompartment())
			return true;
		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot || !slot.GetVehicle() || access.IsGettingOut())
			return false;
		Physics physics = slot.GetVehicle().GetPhysics();
		if (!physics || physics.GetVelocity().Length() > 0.5)
			return false;
		vector transform[4];
		recruit.m_Character.GetTransform(transform);
		if (!access.FindSuitableTeleportLocation(transform))
			return false;
		vector exit = transform[3];
		float surface = GetGame().GetWorld().GetSurfaceY(exit[0], exit[2]);
		if (vector.DistanceXZ(exit, slot.GetVehicle().GetOrigin()) > 12 || Math.AbsFloat(exit[1] - surface) > 2)
			return false;
		return access.GetOutVehicle_NoDoor(transform, false, false, true);
	}

	protected bool Board(AROR_Recruit recruit)
	{
		IEntity vehicle = recruit.m_Vehicle;
		if (!vehicle || AROR_ForceSpawner.IsDestroyed(vehicle) || !FriendlyOrCivilian(vehicle)
			|| vector.Distance(vehicle.GetOrigin(), recruit.m_Character.GetOrigin()) > 30)
			return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(recruit.m_Character.FindComponent(CompartmentAccessComponent));
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!access || !compartments || !usage || access.IsGettingIn() || access.IsGettingOut())
			return false;
		if (access.IsInCompartment())
		{
			BaseCompartmentSlot current = access.GetCompartment();
			if (!current || current.GetVehicle() != vehicle)
				return false;
			if (recruit.m_iOrder == AROR_ERecruitAction.BOARD_DRIVER)
				return current == usage.GetPilotCompartmentSlot();
			if (recruit.m_iOrder == AROR_ERecruitAction.BOARD_GUNNER)
				return TurretCompartmentSlot.Cast(current) != null;
			return CargoCompartmentSlot.Cast(current) != null;
		}
		// A correctly boarded recruit remains boarded when the vehicle starts moving.
		Physics physics = vehicle.GetPhysics();
		if (!physics || physics.GetVelocity().Length() > 0.5)
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot seat : slots)
		{
			if (!seat || seat.IsOccupied() || seat.GetOccupant() || !seat.IsCompartmentAccessible())
				continue;
			bool matches = recruit.m_iOrder == AROR_ERecruitAction.BOARD_PASSENGER && CargoCompartmentSlot.Cast(seat) != null;
			if (recruit.m_iOrder == AROR_ERecruitAction.BOARD_DRIVER)
				matches = seat == usage.GetPilotCompartmentSlot();
			if (recruit.m_iOrder == AROR_ERecruitAction.BOARD_GUNNER)
				matches = TurretCompartmentSlot.Cast(seat) != null;
			if (!matches)
				continue;
			access.GetInVehicle(vehicle, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
			SCR_AIGroupUtilityComponent utility = recruit.m_Group.GetGroupUtilityComponent();
			if (utility)
				utility.AddUsableVehicle(usage);
			return false; // Observe actual occupancy on the next tick.
		}
		return false;
	}

	protected void ClearWaypoint(AROR_Recruit recruit)
	{
		if (!recruit.m_Waypoint)
			return;
		if (recruit.m_Group)
			recruit.m_Group.RemoveWaypoint(recruit.m_Waypoint);
		SCR_EntityHelper.DeleteEntityAndChildren(recruit.m_Waypoint);
		recruit.m_Waypoint = null;
	}

	protected void SetOrder(AROR_Recruit recruit, int order)
	{
		ClearWaypoint(recruit);
		recruit.m_iOrder = order;
		recruit.m_Vehicle = null;
		recruit.m_iBoardDeadline = 0;
	}

	protected void Waypoint(AROR_Recruit recruit, ResourceName prefab, vector target, float radius)
	{
		ClearWaypoint(recruit);
		recruit.m_vTarget = target;
		recruit.m_Waypoint = AROR_ForceSpawner.AddWaypoint(recruit.m_Group, prefab, target, radius);
	}

	protected void Update_S()
	{
		int now = GetGame().GetWorld().GetWorldTime();
		for (int i = m_aRecruits.Count() - 1; i >= 0; i--)
		{
			AROR_Recruit recruit = m_aRecruits[i];
			int living = AROR_ForceSpawner.CountAlive(recruit.m_Group, AROR_EFactionRole.RESISTANCE, recruit.m_aAffiliated);
			array<IEntity> members = {};
			AROR_ForceSpawner.CountLiving(recruit.m_Group, members);
			if (living > 0)
			{
				recruit.m_bFormed = true;
				recruit.m_Character = members[0];
			}
			if (!recruit.m_bFormed)
			{
				if (!recruit.m_Group || now - recruit.m_iRaisedAt >= m_iSpawnTimeout * 1000)
				{
					Refund(recruit);
					Reply(recruit.m_iOwnerId, "Recruit failed to form; cost refunded.");
					Remove_S(i, true);
				}
				continue;
			}
			if (living == 0)
			{
				Remove_S(i, false);
				continue;
			}
			if (IsBoardOrder(recruit.m_iOrder))
			{
				if (Board(recruit))
					recruit.m_iBoardDeadline = 0;
				else if (recruit.m_iBoardDeadline == 0 || now >= recruit.m_iBoardDeadline)
				{
					SetOrder(recruit, AROR_ERecruitAction.HOLD);
					if (!IsSeated(recruit.m_Character))
						Waypoint(recruit, m_sHoldPrefab, recruit.m_Character.GetOrigin(), 10);
					Reply(recruit.m_iOwnerId, "Recruit boarding ended; no safe seat available.");
				}
				continue;
			}
			IEntity player = AROR_FieldAccess.Player(recruit.m_iOwnerId);
			if (recruit.m_iOrder != AROR_ERecruitAction.FOLLOW || !player || IsSeated(recruit.m_Character))
				continue;
			vector target = player.GetOrigin();
			float distance = vector.DistanceXZ(recruit.m_Character.GetOrigin(), target);
			if (distance > 15 && (!recruit.m_Waypoint || vector.DistanceXZ(recruit.m_vTarget, target) > 12 || recruit.m_Group.GetCurrentWaypoint() != recruit.m_Waypoint))
				Waypoint(recruit, m_sMovePrefab, target, 8);
		}
	}

	string Snapshot_S(int playerId)
	{
		string result;
		foreach (AROR_Recruit recruit : m_aRecruits)
		{
			if (recruit.m_iOwnerId != playerId)
				continue;
			vector position;
			if (recruit.m_Character)
				position = recruit.m_Character.GetOrigin();
			int formed;
			if (recruit.m_bFormed)
				formed = 1;
			result += string.Format("%1|%2|%3|%4|%5;", recruit.m_iId, formed, recruit.m_iOrder, Math.Round(position[0]), Math.Round(position[2]));
		}
		return result;
	}

	//! Session-only recruits follow HICOMM's save policy: return only living/pending personnel value.
	void GetRefundValue_S(out int funds, out int manpower)
	{
		funds = 0;
		manpower = 0;
		foreach (AROR_Recruit recruit : m_aRecruits)
		{
			array<IEntity> living = {};
			if (!recruit.m_bFormed || AROR_ForceSpawner.CountLiving(recruit.m_Group, living) > 0)
			{
				funds += recruit.m_iFundsCost;
				manpower += recruit.m_iManpowerCost;
			}
		}
	}

	protected void Refund(AROR_Recruit recruit)
	{
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy)
			return;
		economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, recruit.m_iFundsCost);
		economy.AddManpower_S(recruit.m_iManpowerCost);
	}

	protected void Remove_S(int index, bool deleteMembers)
	{
		AROR_Recruit recruit = m_aRecruits[index];
		ClearWaypoint(recruit);
		if (deleteMembers)
			AROR_ForceSpawner.DeleteGroupAndMembers(recruit.m_Group);
		else if (recruit.m_Group)
			SCR_EntityHelper.DeleteEntityAndChildren(recruit.m_Group);
		m_aRecruits.Remove(index);
	}

	protected void Reply(int playerId, string text) { AROR_NotificationManagerComponent.SendHint_S(playerId, text); }

	//! Return surviving recruits to the pool on disconnect; never leave ownership attached to a reusable player ID.
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);
		if (!Replication.IsServer())
			return;
		for (int i = m_aRecruits.Count() - 1; i >= 0; i--)
		{
			AROR_Recruit recruit = m_aRecruits[i];
			if (recruit.m_iOwnerId != playerId)
				continue;
			array<IEntity> living = {};
			if (!recruit.m_bFormed || AROR_ForceSpawner.CountLiving(recruit.m_Group, living) > 0)
				Refund(recruit);
			Remove_S(i, true);
		}
	}

	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);
		if (!Replication.IsServer())
			return;
		for (int i = m_aRecruits.Count() - 1; i >= 0; i--)
		{
			array<IEntity> living = {};
			if (!m_aRecruits[i].m_bFormed || AROR_ForceSpawner.CountLiving(m_aRecruits[i].m_Group, living) > 0)
				Refund(m_aRecruits[i]);
			Remove_S(i, true);
		}
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		s_Instance = this;
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(Update_S, 2000, true);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Update_S);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}
}
