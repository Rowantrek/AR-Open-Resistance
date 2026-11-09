//! What a Resistance town garrison can hold
enum AROR_EMilitiaKind
{
	INFANTRY,	//!< Bought as a team of up to four soldiers
	APC,		//!< Needs one held seaport
	ARMOUR		//!< Needs two held seaports
}

[ComponentEditorProps(category: "OpenResistance/Towns", description: "Resistance town garrisons bought in HICOMM: a one-time cost per unit, saved with the campaign, raised in the town while players or enemies are near. Attach once to the game mode.")]
class AROR_TownMilitiaComponentClass : ScriptComponentClass {}

//! Server-only runtime of one town's garrison while it is raised
class AROR_MilitiaPost : Managed
{
	string m_sTownId;
	ref array<SCR_AIGroup> m_aGroups = {};
	ref array<IEntity> m_aVehicles = {};
	ref array<int> m_aVehicleKinds = {};
	ref array<SCR_AIGroup> m_aCrews = {};
	ref array<bool> m_aSeated = {};
	int m_iRaisedAt;
	int m_iLastBusy;
}

//! Resistance town garrisons (see ARCHITECTURE.md -> Towns). A Resistance Leader buys infantry, APCs and armour for a
//! held town in HICOMM with faction funds and manpower; what is bought stays in the town (saved) until it dies or the
//! town is lost. Seaports bring the heavy kit: one held seaport unlocks APCs, two unlock armour. The units are only
//! physical while Resistance players or enemy soldiers are near the town; losses are counted when they despawn.
class AROR_TownMilitiaComponent : ScriptComponent
{
	[Attribute("300", desc: "Faction funds per infantry soldier (a team is up to four)", params: "0 100000 10", category: "Prices")]
	protected int m_iSoldierCost;
	[Attribute("1", desc: "Manpower per infantry soldier", params: "0 20 1", category: "Prices")]
	protected int m_iSoldierManpower;
	[Attribute("12000", desc: "Faction funds per APC, crew included", params: "0 1000000 100", category: "Prices")]
	protected int m_iApcCost;
	[Attribute("25000", desc: "Faction funds per armoured vehicle, crew included", params: "0 1000000 100", category: "Prices")]
	protected int m_iArmourCost;
	[Attribute("3", desc: "Manpower per vehicle (its crew)", params: "0 20 1", category: "Prices")]
	protected int m_iVehicleManpower;

	[Attribute("12", desc: "Most infantry a town holds", params: "0 60 1", category: "Size")]
	protected int m_iTownInfantry;
	[Attribute("20", desc: "Most infantry a city holds", params: "0 60 1", category: "Size")]
	protected int m_iCityInfantry;
	[Attribute("2", desc: "Most vehicles (APCs and armour together) a town holds", params: "0 10 1", category: "Size")]
	protected int m_iTownVehicles;
	[Attribute("3", desc: "Most vehicles a city holds", params: "0 10 1", category: "Size")]
	protected int m_iCityVehicles;
	[Attribute("1", desc: "Held seaports needed to buy APCs", params: "0 10 1", category: "Size")]
	protected int m_iApcPorts;
	[Attribute("2", desc: "Held seaports needed to buy armour", params: "0 10 1", category: "Size")]
	protected int m_iArmourPorts;

	[Attribute("1200", desc: "The garrison is raised while a Resistance player is this close to the town, metres", params: "300 3000 50", category: "Activation")]
	protected float m_fPlayerDistance;
	[Attribute("700", desc: "...or while an enemy soldier is this close (an attack on the town), metres", params: "100 2000 50", category: "Activation")]
	protected float m_fEnemyDistance;
	[Attribute("90", desc: "Seconds with nobody near before the garrison is stood down (survivors are kept)", params: "30 900 10", category: "Activation")]
	protected int m_iStandDownSeconds;
	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty AI group prefab", params: "et", category: "Activation")]
	protected ResourceName m_sGroupPrefab;
	[Attribute("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", UIWidgets.ResourceNamePicker, desc: "Defend waypoint the garrison holds the town with", params: "et", category: "Activation")]
	protected ResourceName m_sDefendWaypoint;

	protected static const int UPDATE_SECONDS = 5;
	protected static const int FORM_UP_SECONDS = 30;	//!< Losses are only read once the groups have spawned
	protected static const int TEAM_SIZE = 4;

	// Replicated garrison book: one row per town with a garrison
	[RplProp(onRplName: "OnBookReplicated")]
	protected ref array<string> m_aTownIds = {};
	[RplProp()]
	protected ref array<int> m_aInfantry = {};
	[RplProp()]
	protected ref array<int> m_aApcs = {};
	[RplProp()]
	protected ref array<int> m_aArmour = {};

	protected ref array<ref AROR_MilitiaPost> m_aPosts = {};	//!< Server: raised garrisons
	protected int m_iClock;
	protected int m_iRevision;

	protected static AROR_TownMilitiaComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	static AROR_TownMilitiaComponent GetInstance()
	{
		return s_Instance;
	}

	int GetRevision() { return m_iRevision; }
	int GetSoldierCost() { return m_iSoldierCost; }
	int GetSoldierManpower() { return m_iSoldierManpower; }
	int GetVehicleManpower() { return m_iVehicleManpower; }

	//------------------------------------------------------------------------------------------------
	//! Every machine: the town's garrison
	void GetGarrison(string townId, out int infantry, out int apcs, out int armour)
	{
		int row = m_aTownIds.Find(townId);
		if (row < 0)
			return;
		infantry = m_aInfantry[row];
		apcs = m_aApcs[row];
		armour = m_aArmour[row];
	}

	int GetMaxInfantry(AROR_LocationComponent town)
	{
		if (town && town.GetLocationType() == AROR_ELocationType.CITY)
			return m_iCityInfantry;
		return m_iTownInfantry;
	}

	int GetMaxVehicles(AROR_LocationComponent town)
	{
		if (town && town.GetLocationType() == AROR_ELocationType.CITY)
			return m_iCityVehicles;
		return m_iTownVehicles;
	}

	int GetVehicleCost(AROR_EMilitiaKind kind)
	{
		if (kind == AROR_EMilitiaKind.ARMOUR)
			return m_iArmourCost;
		return m_iApcCost;
	}

	int GetPortsNeeded(AROR_EMilitiaKind kind)
	{
		if (kind == AROR_EMilitiaKind.ARMOUR)
			return m_iArmourPorts;
		if (kind == AROR_EMilitiaKind.APC)
			return m_iApcPorts;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: the vehicle the Resistance would field for a kind (its preset first, then any enabled pack
	//! vehicle the Resistance may buy); empty when none exists (armour in the base game)
	static ResourceName VehicleFor(AROR_EMilitiaKind kind)
	{
		array<AROR_EVehicleType> types = {};
		if (kind == AROR_EMilitiaKind.ARMOUR)
		{
			types.Insert(AROR_EVehicleType.TANK);
			types.Insert(AROR_EVehicleType.IFV);
		}
		else if (kind == AROR_EMilitiaKind.APC)
		{
			types.Insert(AROR_EVehicleType.APC);
		}
		foreach (AROR_EVehicleType type : types)
		{
			ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(AROR_EFactionRole.RESISTANCE, type);
			if (!prefab.IsEmpty())
				return prefab;
		}
		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
		if (!packs)
			return string.Empty;
		array<AROR_ContentPackEntry> entries = {};
		packs.GetEnabledEntries(AROR_EFactionPresetCategory.VEHICLES, entries);
		foreach (AROR_EVehicleType wanted : types)
		{
			foreach (AROR_ContentPackEntry entry : entries)
			{
				if (entry && entry.CanResistancePurchase() && entry.GetTypeValue() == wanted)
					return entry.GetPrefab();
			}
		}
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: why the Resistance cannot buy one of this kind for the town now; empty when it can
	string WhyNot(AROR_LocationComponent town, AROR_EMilitiaKind kind)
	{
		if (!town || town.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
			return "Town not held";
		int infantry, apcs, armour;
		GetGarrison(town.GetId(), infantry, apcs, armour);
		if (kind == AROR_EMilitiaKind.INFANTRY)
		{
			if (infantry >= GetMaxInfantry(town))
				return "Full";
			return string.Empty;
		}
		if (apcs + armour >= GetMaxVehicles(town))
			return "Full";
		int ports = GetPortsNeeded(kind);
		if (AROR_ImportPoints.CountPorts(AROR_EFactionRole.RESISTANCE) < ports)
			return string.Format("Needs %1 seaport(s)", ports);
		if (VehicleFor(kind).IsEmpty())
			return "None available";
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldiers one infantry purchase adds: a team, or what is left of the town's room
	int InfantryToBuy(AROR_LocationComponent town)
	{
		int infantry, apcs, armour;
		if (town)
			GetGarrison(town.GetId(), infantry, apcs, armour);
		int room = GetMaxInfantry(town) - infantry;
		if (room > TEAM_SIZE)
			room = TEAM_SIZE;
		if (room < 0)
			room = 0;
		return room;
	}

	//------------------------------------------------------------------------------------------------
	// Server: purchases (arrive through the player controller)
	//------------------------------------------------------------------------------------------------
	void RequestBuy_S(int playerId, string townId, AROR_EMilitiaKind kind)
	{
		if (!Replication.IsServer())
			return;
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (!highCommand || !highCommand.IsLeader(playerId))
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Only a Resistance Leader can garrison towns");
			return;
		}
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		AROR_LocationComponent town;
		if (locations)
			town = locations.GetLocationById(townId);
		if (!town || !economy || (town.GetLocationType() != AROR_ELocationType.TOWN && town.GetLocationType() != AROR_ELocationType.CITY))
			return;
		string reason = WhyNot(town, kind);
		if (!reason.IsEmpty())
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("%1: %2", town.GetName(), reason));
			return;
		}

		int count = 1;
		int funds = GetVehicleCost(kind);
		int manpower = m_iVehicleManpower;
		if (kind == AROR_EMilitiaKind.INFANTRY)
		{
			count = InfantryToBuy(town);
			funds = count * m_iSoldierCost;
			manpower = count * m_iSoldierManpower;
		}
		if (economy.GetFactionFunds(AROR_EFactionRole.RESISTANCE) < funds || economy.GetManpower() < manpower)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("Not enough resources: needs %1 and %2 MP", AROR_EconomyComponent.FormatMoney(funds), manpower));
			return;
		}
		if (!economy.SpendFactionFunds_S(AROR_EFactionRole.RESISTANCE, funds))
			return;
		if (!economy.SpendManpower_S(manpower))
		{
			economy.AddFactionFunds_S(AROR_EFactionRole.RESISTANCE, funds);
			return;
		}

		int row = Row_S(townId);
		if (kind == AROR_EMilitiaKind.INFANTRY)
			m_aInfantry[row] = m_aInfantry[row] + count;
		else if (kind == AROR_EMilitiaKind.APC)
			m_aApcs[row] = m_aApcs[row] + 1;
		else
			m_aArmour[row] = m_aArmour[row] + 1;
		Changed_S();

		// A raised garrison gets the new unit straight away
		AROR_MilitiaPost post = FindPost(townId);
		if (post)
		{
			if (kind == AROR_EMilitiaKind.INFANTRY)
				RaiseInfantry_S(post, town, count);
			else
				RaiseVehicle_S(post, town, kind);
		}

		string what = "an APC";
		if (kind == AROR_EMilitiaKind.ARMOUR)
			what = "armour";
		else if (kind == AROR_EMilitiaKind.INFANTRY)
			what = string.Format("%1 soldiers", count);
		Print(string.Format("[AROR MILITIA] player %1 garrisoned %2 with %3 for %4 and %5 MP", playerId, townId, what, funds, manpower), LogLevel.NORMAL);
		AROR_NotificationManagerComponent.SendHint_S(playerId, string.Format("%1 garrisoned with %2", town.GetName(), what));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the town's row, created on first use
	protected int Row_S(string townId)
	{
		int row = m_aTownIds.Find(townId);
		if (row >= 0)
			return row;
		m_aTownIds.Insert(townId);
		m_aInfantry.Insert(0);
		m_aApcs.Insert(0);
		m_aArmour.Insert(0);
		return m_aTownIds.Count() - 1;
	}

	protected void Changed_S()
	{
		// Empty rows go so the book only lists garrisoned towns
		for (int row = m_aTownIds.Count() - 1; row >= 0; row--)
		{
			if (m_aInfantry[row] <= 0 && m_aApcs[row] <= 0 && m_aArmour[row] <= 0 && !FindPost(m_aTownIds[row]))
			{
				m_aTownIds.Remove(row);
				m_aInfantry.Remove(row);
				m_aApcs.Remove(row);
				m_aArmour.Remove(row);
			}
		}
		m_iRevision++;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	// Server: raising and standing down
	//------------------------------------------------------------------------------------------------
	protected void Update_S()
	{
		m_iClock += UPDATE_SECONDS;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return;
		bool changed;
		for (int row = m_aTownIds.Count() - 1; row >= 0; row--)
		{
			string townId = m_aTownIds[row];
			AROR_LocationComponent town = locations.GetLocationById(townId);
			AROR_MilitiaPost post = FindPost(townId);
			bool busy;
			if (!town || town.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
			{
				// The town is lost and its garrison with it; anyone still standing fights on unsaved until it is quiet
				if (m_aInfantry[row] > 0 || m_aApcs[row] > 0 || m_aArmour[row] > 0)
				{
					Print(string.Format("[AROR MILITIA] %1 lost: garrison of %2 infantry, %3 APC, %4 armour gone", townId, m_aInfantry[row], m_aApcs[row], m_aArmour[row]), LogLevel.NORMAL);
					m_aInfantry[row] = 0;
					m_aApcs[row] = 0;
					m_aArmour[row] = 0;
					changed = true;
				}
				if (!post)
					continue;
				if (town)
					busy = IsBusy(town);
				if (busy)
					post.m_iLastBusy = m_iClock;
				else if (m_iClock - post.m_iLastBusy >= m_iStandDownSeconds)
				{
					StandDown_S(post);
					changed = true;
				}
				continue;
			}
			busy = IsBusy(town);
			if (!post)
			{
				if (busy)
					Raise_S(town, row);
				continue;
			}
			if (busy)
				post.m_iLastBusy = m_iClock;
			SeatCrews_S(post);
			if (m_iClock - post.m_iRaisedAt >= FORM_UP_SECONDS && CountLosses_S(post, row))
				changed = true;
			if (m_iClock - post.m_iLastBusy >= m_iStandDownSeconds)
			{
				StandDown_S(post);
				changed = true;
			}
		}
		if (changed)
			Changed_S();
	}

	//! A Resistance player, or an enemy soldier, is near the town
	protected bool IsBusy(AROR_LocationComponent town)
	{
		vector center = town.GetCenter();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (player && vector.DistanceXZ(player.GetOrigin(), center) <= m_fPlayerDistance)
				return true;
		}
		Faction resistance = GetGame().GetFactionManager().GetFactionByKey(AROR_ForceSpawner.FactionKey(AROR_EFactionRole.RESISTANCE));
		return resistance && AROR_HostilesQuery.HasHostilesInRadius(resistance, center, m_fEnemyDistance);
	}

	protected AROR_MilitiaPost FindPost(string townId)
	{
		foreach (AROR_MilitiaPost post : m_aPosts)
		{
			if (post.m_sTownId == townId)
				return post;
		}
		return null;
	}

	protected void Raise_S(AROR_LocationComponent town, int row)
	{
		AROR_MilitiaPost post = new AROR_MilitiaPost();
		post.m_sTownId = town.GetId();
		post.m_iRaisedAt = m_iClock;
		post.m_iLastBusy = m_iClock;
		m_aPosts.Insert(post);
		RaiseInfantry_S(post, town, m_aInfantry[row]);
		for (int apc = 0; apc < m_aApcs[row]; apc++)
			RaiseVehicle_S(post, town, AROR_EMilitiaKind.APC);
		for (int armour = 0; armour < m_aArmour[row]; armour++)
			RaiseVehicle_S(post, town, AROR_EMilitiaKind.ARMOUR);
		Print(string.Format("[AROR MILITIA] %1 garrison raised: %2 infantry, %3 APC, %4 armour", town.GetId(), m_aInfantry[row], m_aApcs[row], m_aArmour[row]), LogLevel.NORMAL);
	}

	//! Teams of up to four, each holding the town around its centre
	protected void RaiseInfantry_S(AROR_MilitiaPost post, AROR_LocationComponent town, int soldiers)
	{
		vector center = town.GetCenter();
		while (soldiers > 0)
		{
			array<AROR_EUnitType> units = {AROR_EUnitType.TEAMLEADER, AROR_EUnitType.RIFLEMAN, AROR_EUnitType.AUTORIFLEMAN, AROR_EUnitType.LAT};
			int size = TEAM_SIZE;
			if (soldiers < size)
				size = soldiers;
			units.Resize(size);
			soldiers -= size;
			vector position;
			if (!AROR_ForceSpawner.FindClearPosition(center, 60, 1.5, position))
				position = center;
			SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, AROR_EFactionRole.RESISTANCE, units, position);
			if (!group)
				continue;
			AROR_ForceSpawner.AddWaypoint(group, m_sDefendWaypoint, center, Math.Min(town.GetRadius() * 0.5, 120));
			post.m_aGroups.Insert(group);
		}
		post.m_iRaisedAt = m_iClock;
	}

	//! A vehicle parked at the roadside with a crew of two that boards it and holds the town
	protected void RaiseVehicle_S(AROR_MilitiaPost post, AROR_LocationComponent town, AROR_EMilitiaKind kind)
	{
		ResourceName prefab = VehicleFor(kind);
		if (prefab.IsEmpty())
			return;
		vector position;
		float yaw;
		if (!AROR_ForceSpawner.FindRoadsidePosition(town.GetCenter(), 150, true, position, yaw))
		{
			if (!AROR_ForceSpawner.FindClearPosition(town.GetCenter(), 120, 4, position))
				return;
		}
		IEntity vehicle = AROR_ForceSpawner.SpawnVehicle(AROR_EFactionRole.RESISTANCE, prefab, position, yaw);
		if (!vehicle)
			return;
		array<AROR_EUnitType> crew = {AROR_EUnitType.CREW, AROR_EUnitType.CREW};
		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, AROR_EFactionRole.RESISTANCE, crew, position);
		post.m_aVehicles.Insert(vehicle);
		post.m_aVehicleKinds.Insert(kind);
		post.m_aCrews.Insert(group);
		post.m_aSeated.Insert(false);
		post.m_iRaisedAt = m_iClock;
	}

	protected void SeatCrews_S(AROR_MilitiaPost post)
	{
		foreach (int index, IEntity vehicle : post.m_aVehicles)
		{
			if (post.m_aSeated[index] || !vehicle || !post.m_aCrews[index])
				continue;
			if (AROR_ForceSpawner.SeatGroup(post.m_aCrews[index], vehicle, 2))
			{
				post.m_aSeated[index] = true;
				AROR_ForceSpawner.AddWaypoint(post.m_aCrews[index], m_sDefendWaypoint, vehicle.GetOrigin(), 60);
			}
		}
	}

	//! Survivors replace the book's numbers (never raise them); destroyed vehicles are gone. True if anything changed.
	protected bool CountLosses_S(AROR_MilitiaPost post, int row)
	{
		bool changed;
		int alive;
		foreach (SCR_AIGroup group : post.m_aGroups)
		{
			array<IEntity> members = {};
			if (group)
				alive += AROR_ForceSpawner.CountLiving(group, members);
		}
		if (alive < m_aInfantry[row])
		{
			m_aInfantry[row] = alive;
			changed = true;
		}
		for (int index = post.m_aVehicles.Count() - 1; index >= 0; index--)
		{
			IEntity vehicle = post.m_aVehicles[index];
			if (vehicle && !AROR_ForceSpawner.IsDestroyed(vehicle))
				continue;
			if (post.m_aVehicleKinds[index] == AROR_EMilitiaKind.ARMOUR)
			{
				if (m_aArmour[row] > 0)
					m_aArmour[row] = m_aArmour[row] - 1;
			}
			else if (m_aApcs[row] > 0)
				m_aApcs[row] = m_aApcs[row] - 1;
			post.m_aVehicles.Remove(index);
			post.m_aVehicleKinds.Remove(index);
			post.m_aCrews.Remove(index);
			post.m_aSeated.Remove(index);
			changed = true;
		}
		return changed;
	}

	//! Survivors are kept in the book; the bodies and vehicles leave the world
	protected void StandDown_S(AROR_MilitiaPost post)
	{
		int row = m_aTownIds.Find(post.m_sTownId);
		if (row >= 0 && m_iClock - post.m_iRaisedAt >= FORM_UP_SECONDS)
			CountLosses_S(post, row);
		foreach (SCR_AIGroup group : post.m_aGroups)
		{
			if (group)
				SCR_EntityHelper.DeleteEntityAndChildren(group);
		}
		foreach (SCR_AIGroup crew : post.m_aCrews)
		{
			if (crew)
				SCR_EntityHelper.DeleteEntityAndChildren(crew);
		}
		foreach (IEntity vehicle : post.m_aVehicles)
		{
			if (vehicle)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		}
		m_aPosts.RemoveItem(post);
		Print(string.Format("[AROR MILITIA] %1 garrison stood down", post.m_sTownId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Saving
	//------------------------------------------------------------------------------------------------
	//! Server: the book as it stands, survivors of raised garrisons counted
	void GetBook_S(notnull array<string> townIds, notnull array<int> infantry, notnull array<int> apcs, notnull array<int> armour)
	{
		foreach (AROR_MilitiaPost post : m_aPosts)
		{
			int row = m_aTownIds.Find(post.m_sTownId);
			if (row >= 0 && m_iClock - post.m_iRaisedAt >= FORM_UP_SECONDS)
				CountLosses_S(post, row);
		}
		townIds.Copy(m_aTownIds);
		infantry.Copy(m_aInfantry);
		apcs.Copy(m_aApcs);
		armour.Copy(m_aArmour);
	}

	void RestoreBook_S(array<string> townIds, array<int> infantry, array<int> apcs, array<int> armour)
	{
		if (!Replication.IsServer() || !townIds || !infantry || !apcs || !armour)
			return;
		int count = townIds.Count();
		if (infantry.Count() < count)
			count = infantry.Count();
		if (apcs.Count() < count)
			count = apcs.Count();
		if (armour.Count() < count)
			count = armour.Count();
		m_aTownIds.Clear();
		m_aInfantry.Clear();
		m_aApcs.Clear();
		m_aArmour.Clear();
		for (int row = 0; row < count; row++)
		{
			m_aTownIds.Insert(townIds[row]);
			m_aInfantry.Insert(infantry[row]);
			m_aApcs.Insert(apcs[row]);
			m_aArmour.Insert(armour[row]);
		}
		// Sent as a change later, never at init (see the RplProp note in AROR_TownWavesComponent's notes)
		GetGame().GetCallqueue().CallLater(Changed_S, 1000, false);
	}

	bool IsEmpty()
	{
		return m_aTownIds.IsEmpty();
	}

	protected void OnBookReplicated()
	{
		m_iRevision++;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		s_Instance = this;
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(Update_S, UPDATE_SECONDS * 1000, true);
	}

	override void OnDelete(IEntity owner)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(Update_S);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}
}
