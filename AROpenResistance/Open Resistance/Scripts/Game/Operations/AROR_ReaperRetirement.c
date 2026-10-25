//! Session-only retention. Retired forces still reserve capacity until actually removed.
class AROR_ReaperRetirement
{
	protected static ref array<ref AROR_ReaperRetiredForce> s_aForces = {};

	static bool NearPlayers(IEntity entity, float distance)
	{
		if (!entity)
			return false;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int id : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (player && vector.DistanceXZ(player.GetOrigin(), entity.GetOrigin()) < distance)
				return true;
		}
		return false;
	}

	static bool ForceNearPlayers(AROR_ReaperAirForce force)
	{
		float distance = force.m_fCleanupDistance;
		if (NearPlayers(force.m_Vehicle, distance) || (force.m_Escort && NearPlayers(force.m_Escort.m_Vehicle, distance)))
			return true;
		array<IEntity> members = {};
		force.GetOwnedMembers(members);
		foreach (IEntity member : members)
		{
			if (NearPlayers(member, distance))
				return true;
		}
		if (force.m_Escort)
		{
			force.m_Escort.GetOwnedMembers(members);
			foreach (IEntity escort : members)
			{
				if (NearPlayers(escort, distance))
					return true;
			}
		}
		return false;
	}

	static bool MembersNearPlayers(AROR_OperationForce force, float distance)
	{
		array<IEntity> members = {};
		force.GetOwnedMembers(members);
		foreach (IEntity member : members)
		{
			if (NearPlayers(member, distance))
				return true;
		}
		return false;
	}

	static bool GroupNearPlayers(SCR_AIGroup group, float distance)
	{
		if (!group)
			return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (agent && NearPlayers(agent.GetControlledEntity(), distance))
				return true;
		}
		return false;
	}

	static void Enqueue_S(AROR_ReaperAirForce force)
	{
		if (!Replication.IsServer())
			return;
		AROR_ReaperRetiredForce retired = new AROR_ReaperRetiredForce();
		retired.m_Force = force;
		s_aForces.Insert(retired);
		// Stop offensive support on terminal operations, while keeping survivors/wrecks.
		REAPER_AiHelicopterControllerComponent controller;
		if (force.m_Escort)
		{
			controller = AROR_ReaperAirForce.Controller(force.m_Escort.m_Vehicle);
			if (controller)
				controller.AROR_ConfigureQRF(force.m_sFactionKey, force.m_vTown, force.m_fTownRadius, 100, false);
			if (!force.m_bEscortLost && force.m_Escort.m_CrewGroup && force.m_Escort.m_CrewGroup.GetPlayerCount() == 0)
				AROR_ReaperAirForce.SetFlightOrder_S(force.m_Escort, MovePrefab(), force.m_vEgress, 140, 120 + force.m_iPackageIndex * 15);
		}
		if (force.m_bDismounted && !force.m_bTransportLost && !force.m_bTransportRetired && force.m_CrewGroup && force.m_CrewGroup.GetPlayerCount() == 0)
			AROR_ReaperAirForce.SetFlightOrder_S(force, MovePrefab(), force.m_vEgress, 140, 120 + force.m_iPackageIndex * 15);
		if (s_aForces.Count() == 1)
			GetGame().GetCallqueue().CallLater(Tick_S, 5000, true);
		Print(string.Format("[AROR REAPER] %1 retained after terminal state: minimum %2s AND all players beyond %3m before cleanup", force.m_sOperationId, force.m_iRetainSeconds, force.m_fCleanupDistance), LogLevel.NORMAL);
	}

	static ResourceName MovePrefab()
	{
		return "{F6FB686B76E77E7D}Prefabs/AI/Waypoints/REAPER_AiHelicopterMoveWaypoint.et";
	}

	static int Reserved(int resourceType)
	{
		int total;
		foreach (AROR_ReaperRetiredForce entry : s_aForces)
		{
			if (resourceType == 0)
				total += entry.m_Force.m_iReservedGroups;
			else if (resourceType == 1)
				total += entry.m_Force.m_iReservedMembers;
			else
				total += entry.m_Force.m_iReservedVehicles;
		}
		return total;
	}

	//! Healthy retained aircraft still exist and must not make room for extra live frames.
	static int LiveAircraftCount()
	{
		int count = 0;
		foreach (AROR_ReaperRetiredForce entry : s_aForces)
		{
			AROR_ReaperAirForce force = entry.m_Force;
			if (AROR_ReaperAirForce.AircraftAlive(force.m_Vehicle))
				count++;
			if (force.m_Escort && AROR_ReaperAirForce.AircraftAlive(force.m_Escort.m_Vehicle))
				count++;
		}
		return count;
	}

	protected static void Tick_S()
	{
		if (!Replication.IsServer())
			return;
		for (int index = s_aForces.Count() - 1; index >= 0; index--)
		{
			AROR_ReaperRetiredForce entry = s_aForces[index];
			entry.m_iAge += 5;
			if (entry.m_iAge < entry.m_Force.m_iRetainSeconds || ForceNearPlayers(entry.m_Force))
				continue;
			entry.m_Force.m_bImmediateCleanup = true;
			entry.m_Force.Cleanup_S();
			s_aForces.Remove(index);
		}
		if (s_aForces.Count() == 0)
			GetGame().GetCallqueue().Remove(Tick_S);
	}

	//! World unload must cancel timers and release ownership even when players were nearby.
	static void Flush_S()
	{
		GetGame().GetCallqueue().Remove(Tick_S);
		foreach (AROR_ReaperRetiredForce entry : s_aForces)
		{
			entry.m_Force.m_bImmediateCleanup = true;
			entry.m_Force.Cleanup_S();
		}
		s_aForces.Clear();
	}
}

class AROR_ReaperRetiredForce
{
	ref AROR_ReaperAirForce m_Force;
	int m_iAge;
}
