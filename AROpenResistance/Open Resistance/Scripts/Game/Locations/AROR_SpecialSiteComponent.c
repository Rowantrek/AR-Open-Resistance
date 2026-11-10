[ComponentEditorProps(category: "OpenResistance/Locations", description: "Special site: a high-value installation that pays its holder extra income every payment, has helipads with parked helicopters while an AI army holds it, and foot patrols walking its outer walls. Put on a location next to its garrison.")]
class AROR_SpecialSiteComponentClass : ScriptComponentClass {}

//! A high-value site (Chicken: Stone Ridge Base). Server-only runtime; nothing is saved except through the income it
//! pays. Helipads are raised once per session on clear, level ground around the site. While an AI army holds it and
//! players are near, a parked helicopter of that army stands on each pad (players can steal them) and fire teams
//! walk a loop outside the walls. Lost helicopters and patrols return after the respawn time.
class AROR_SpecialSiteComponent : ScriptComponent
{
	[Attribute("1500", desc: "Extra money paid to the holder every income payment", params: "0 100000 50", category: "Income")]
	protected int m_iMoneyPerPayment;
	[Attribute("3", desc: "Extra Resistance manpower every income payment while the Resistance holds it", params: "0 50 1", category: "Income")]
	protected int m_iManpowerPerPayment;

	[Attribute("2", desc: "Helipads raised around the site at play start", params: "0 4 1", category: "Helipads")]
	protected int m_iHelipads;
	[Attribute("{A13F6A7F82DF031C}Prefabs/Compositions/Slotted/SlotFlatLarge/Helipad_L_Conflict_US_01.et", UIWidgets.ResourceNamePicker, desc: "Helipad composition", params: "et", category: "Helipads")]
	protected ResourceName m_sHelipadPrefab;
	[Attribute("55", desc: "Nearest a helipad stands to the site centre, metres", params: "10 300 5", category: "Helipads")]
	protected float m_fPadMinDistance;
	[Attribute("130", desc: "Furthest a helipad stands from the site centre, metres", params: "20 400 5", category: "Helipads")]
	protected float m_fPadMaxDistance;
	[Attribute("12", desc: "Clear radius a helipad and its helicopter need (no trees, walls or buildings), metres", params: "5 25 0.5", category: "Helipads")]
	protected float m_fPadClearance;
	[Attribute("1.5", desc: "Most terrain height difference across a helipad, metres", params: "0.2 4 0.1", category: "Helipads")]
	protected float m_fPadMaxRelief;
	[Attribute("", UIWidgets.ComboBox, desc: "Helicopter parked on each pad while an AI army holds the site, in pad order (empty: transport helicopters). Taken from the holder's faction preset.", enums: ParamEnumArray.FromEnum(AROR_EVehicleType), category: "Helipads")]
	protected ref array<AROR_EVehicleType> m_aHelicopterTypes;

	[Attribute("2", desc: "Fire teams walking a loop outside the walls while an AI army holds the site", params: "0 6 1", category: "Patrols")]
	protected int m_iFootPatrols;
	[Attribute("75", desc: "Radius of the patrol loop around the centre, metres", params: "20 400 5", category: "Patrols")]
	protected float m_fPatrolRadius;

	[Attribute("1300", desc: "Helicopters and patrols turn out while a player is this close, metres", params: "300 3000 50", category: "Activation")]
	protected float m_fActivationDistance;
	[Attribute("1600", desc: "...and are removed once no player has been this close for half a minute, metres", params: "400 4000 50", category: "Activation")]
	protected float m_fRetireDistance;
	[Attribute("1800", desc: "Seconds before a lost helicopter or patrol is replaced", params: "120 14400 60", category: "Activation")]
	protected int m_iRespawnSeconds;
	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty AI group prefab", params: "et", category: "Activation")]
	protected ResourceName m_sGroupPrefab;
	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, params: "et", category: "Activation")]
	protected ResourceName m_sMoveWaypointPrefab;

	protected static const int TICK_SECONDS = 2;
	protected static const int RETIRE_DELAY = 30;
	protected static const int LOOP_POINTS = 8;
	protected static const int PAD_ATTEMPTS = 40;
	protected static const float PAD_MOVED = 30;	//!< A helicopter this far off its pad has been taken

	protected AROR_LocationComponent m_Location;
	protected bool m_bPadsPlaced;
	protected bool m_bActive;
	protected int m_iClock;
	protected int m_iFarSince = -1;
	protected AROR_EFactionRole m_eRole;
	protected ref array<vector> m_aPads = {};
	protected ref array<float> m_aPadYaws = {};
	protected ref array<IEntity> m_aPadEntities = {};
	protected ref array<IEntity> m_aHelicopters = {};	//!< Parallel to the pads; null when none stands there
	protected ref array<int> m_aHelicopterLostAt = {};	//!< Parallel: clock when its helicopter was lost, -1 none
	protected ref array<ref AROR_SpecialSitePatrol> m_aPatrols = {};
	protected ref array<int> m_aPatrolLostAt = {};

	//------------------------------------------------------------------------------------------------
	int GetMoneyPerPayment() { return m_iMoneyPerPayment; }
	int GetManpowerPerPayment() { return m_iManpowerPerPayment; }

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!Replication.IsServer() || !m_Location)
			return;

		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!commanders || !commanders.IsCampaignActive())
			return;

		m_iClock += TICK_SECONDS;
		if (!m_bPadsPlaced)
			PlacePads();

		vector centre = m_Location.GetCenter();
		if (!m_bActive)
		{
			if (PlayerWithin(centre, m_fActivationDistance))
			{
				m_bActive = true;
				m_iFarSince = -1;
				Print(string.Format("[AROR SITE] %1 active", m_Location.GetId()), LogLevel.NORMAL);
			}
			return;
		}

		if (PlayerWithin(centre, m_fRetireDistance))
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
			// A new holder: the old army's helicopters and patrols are left where they are, no longer this site's
			Forget();
			m_eRole = role;
		}

		if (!AROR_GarrisonPolicy.IsEnemyRole(role))
			return;

		UpdateHelicopters(role);
		UpdatePatrols(role);
	}

	//------------------------------------------------------------------------------------------------
	//! Helipads on level ground clear of trees and walls, spread around the site, raised once per session
	protected void PlacePads()
	{
		m_bPadsPlaced = true;
		if (m_iHelipads <= 0 || m_sHelipadPrefab.IsEmpty())
			return;

		vector centre = m_Location.GetCenter();
		float startBearing = Math.RandomFloat(0, 360);
		for (int attempt = 0; attempt < PAD_ATTEMPTS && m_aPads.Count() < m_iHelipads; attempt++)
		{
			// Walk round the site so pads end up on different sides
			float bearing = (startBearing + attempt * 137.5) * Math.DEG2RAD;
			float distance = Math.RandomFloat(m_fPadMinDistance, m_fPadMaxDistance);
			vector spot = centre + Vector(Math.Sin(bearing), 0, Math.Cos(bearing)) * distance;
			if (!IsPadSpot(spot))
				continue;

			// Facing the site, so the helicopters line up with it
			vector toCentre = centre - spot;
			float yaw = Math.Atan2(toCentre[0], toCentre[2]) * Math.RAD2DEG;
			IEntity pad = AROR_ForceSpawner.SpawnPrefab(m_sHelipadPrefab, spot, yaw);
			if (!pad)
				break;

			m_aPads.Insert(pad.GetOrigin());
			m_aPadYaws.Insert(yaw);
			m_aPadEntities.Insert(pad);
			m_aHelicopters.Insert(null);
			m_aHelicopterLostAt.Insert(-1);
		}

		Print(string.Format("[AROR SITE] %1 helipads raised: %2 of %3", m_Location.GetId(), m_aPads.Count(), m_iHelipads), LogLevel.NORMAL);
	}

	protected bool IsPadSpot(vector spot)
	{
		BaseWorld world = GetGame().GetWorld();
		float low = world.GetSurfaceY(spot[0], spot[2]);
		if (low <= world.GetOceanHeight(spot[0], spot[2]) + 0.5)
			return false;

		float high = low;
		for (int corner = 0; corner < 8; corner++)
		{
			float angle = corner * Math.PI2 / 8;
			float y = world.GetSurfaceY(spot[0] + Math.Sin(angle) * m_fPadClearance, spot[2] + Math.Cos(angle) * m_fPadClearance);
			low = Math.Min(low, y);
			high = Math.Max(high, y);
		}

		if (high - low > m_fPadMaxRelief)
			return false;

		foreach (vector other : m_aPads)
		{
			if (vector.DistanceXZ(spot, other) < m_fPadClearance * 2.2)
				return false;
		}

		if (AROR_ForceSpawner.IsNearRoad(spot, m_fPadClearance))
			return false;

		return AROR_ForceSpawner.IsSpotClear(spot, m_fPadClearance, 8);
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

			if (PlayerWithin(pad, 60))
				continue;

			ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(role, HelicopterType(index));
			if (prefab.IsEmpty())
				prefab = AROR_ForceSpawner.VehiclePrefab(role, AROR_EVehicleType.HELI_TRANSPORT);
			if (prefab.IsEmpty())
				continue;

			IEntity spawned = AROR_ForceSpawner.SpawnVehicle(role, prefab, pad, m_aPadYaws[index]);
			if (!spawned)
			{
				// Keep it from retrying every tick
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

	//! Spawned on the terrain; lifted onto the pad's deck when the pad stands proud of it
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
	protected void UpdatePatrols(AROR_EFactionRole role)
	{
		for (int index = m_aPatrols.Count() - 1; index >= 0; index--)
		{
			AROR_SpecialSitePatrol patrol = m_aPatrols[index];
			array<IEntity> living = {};
			int alive = AROR_ForceSpawner.CountLiving(patrol.m_Group, living);
			if (!patrol.m_Group || (alive == 0 && m_iClock - patrol.m_iRaisedAt > 60))
			{
				// Also after a failed spawn, so a broken preset never retries every tick
				m_aPatrolLostAt.Insert(m_iClock);
				if (patrol.m_bSeen)
					Print(string.Format("[AROR SITE] %1 foot patrol wiped out", m_Location.GetId()), LogLevel.NORMAL);
				else
					Print(string.Format("[AROR SITE] %1 foot patrol never formed", m_Location.GetId()), LogLevel.WARNING);
				patrol.Remove_S();
				m_aPatrols.Remove(index);
				continue;
			}

			if (alive > 0)
				patrol.m_bSeen = true;

			bool arrived = alive > 0 && vector.DistanceXZ(living[0].GetOrigin(), patrol.m_vGoal) < 15;
			if (m_iClock >= patrol.m_iNextOrderAt || arrived)
			{
				patrol.m_iPoint = (patrol.m_iPoint + patrol.m_iDirection + LOOP_POINTS) % LOOP_POINTS;
				patrol.m_vGoal = LoopPoint(patrol.m_iPoint);
				patrol.Order_S(m_sMoveWaypointPrefab, patrol.m_vGoal);
				patrol.m_iNextOrderAt = m_iClock + 120;
			}
		}

		for (int lost = m_aPatrolLostAt.Count() - 1; lost >= 0; lost--)
		{
			if (m_iClock - m_aPatrolLostAt[lost] >= m_iRespawnSeconds)
				m_aPatrolLostAt.Remove(lost);
		}

		if (m_aPatrols.Count() + m_aPatrolLostAt.Count() >= m_iFootPatrols)
			return;

		// One new patrol per tick, starting on the loop away from any player, the second going the other way round
		int start = Math.RandomInt(0, LOOP_POINTS);
		vector spawn = LoopPoint(start);
		if (PlayerWithin(spawn, 120))
			return;

		vector clear;
		if (AROR_ForceSpawner.FindClearPosition(spawn, 15, 1, clear))
			spawn = clear;

		array<AROR_EUnitType> units = {};
		AROR_Formations.GetUnits(AROR_EFormation.RIFLE_FIRE_TEAM, units);
		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, role, units, spawn);
		if (!group)
			return;

		AROR_SpecialSitePatrol added = new AROR_SpecialSitePatrol();
		added.m_Group = group;
		added.m_iRaisedAt = m_iClock;
		added.m_iPoint = start;
		added.m_iDirection = 1;
		if (m_aPatrols.Count() % 2 == 1)
			added.m_iDirection = -1;
		added.m_iNextOrderAt = m_iClock + 4;
		m_aPatrols.Insert(added);
		Print(string.Format("[AROR SITE] %1 foot patrol raised for role %2", m_Location.GetId(), role), LogLevel.NORMAL);
	}

	protected vector LoopPoint(int point)
	{
		float bearing = point * Math.PI2 / LOOP_POINTS;
		vector spot = m_Location.GetCenter() + Vector(Math.Sin(bearing), 0, Math.Cos(bearing)) * m_fPatrolRadius;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		return spot;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlayerWithin(vector position, float distance)
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (player && vector.DistanceXZ(player.GetOrigin(), position) <= distance)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Nobody near: patrols go, and helicopters still parked on their pads (unclaimed) are put away until next time
	protected void Retire()
	{
		m_bActive = false;
		foreach (AROR_SpecialSitePatrol patrol : m_aPatrols)
		{
			patrol.Remove_S();
		}
		m_aPatrols.Clear();

		foreach (int index, IEntity helicopter : m_aHelicopters)
		{
			if (helicopter && !AROR_ForceSpawner.IsDestroyed(helicopter) && !AROR_ResistanceFleet.IsClaimed(helicopter) && vector.DistanceXZ(helicopter.GetOrigin(), m_aPads[index]) <= PAD_MOVED)
				SCR_EntityHelper.DeleteEntityAndChildren(helicopter);
			m_aHelicopters[index] = null;
		}

		Print(string.Format("[AROR SITE] %1 retired", m_Location.GetId()), LogLevel.NORMAL);
	}

	//! Control changed while players are here: stop managing the old holder's helicopters and patrols
	protected void Forget()
	{
		m_aPatrols.Clear();
		m_aPatrolLostAt.Clear();
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
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		if (!m_Location)
		{
			Print("[AROR SITE] Special site needs a location component; disabled", LogLevel.ERROR);
			return;
		}

		m_eRole = m_Location.GetControllingRole();
		GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Tick);
		if (Replication.IsServer() && m_bActive)
			Retire();
		super.OnDelete(owner);
	}
}

//! A fire team walking the special site's outer loop
class AROR_SpecialSitePatrol
{
	SCR_AIGroup m_Group;
	AIWaypoint m_Waypoint;
	int m_iRaisedAt;
	int m_iPoint;
	int m_iDirection = 1;
	int m_iNextOrderAt;
	bool m_bSeen;	//!< Its members have spawned; from then on none living means wiped out
	vector m_vGoal;

	void Order_S(ResourceName prefab, vector goal)
	{
		if (!m_Group)
			return;

		if (m_Waypoint)
		{
			m_Group.RemoveWaypoint(m_Waypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		}

		m_Waypoint = AROR_ForceSpawner.AddWaypoint(m_Group, prefab, goal, 8);
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
