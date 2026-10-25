//! Server-owned, bounded mounted search of reported contact and shared town sectors.
class AROR_VehicleTownSearch
{
	protected SCR_AIGroup m_Group;
	protected IEntity m_Vehicle;
	protected AROR_LocationComponent m_Location;
	protected AIWaypoint m_Waypoint;
	protected ResourceName m_sWaypointPrefab;
	protected string m_sOperationId;
	protected bool m_bActive;
	protected int m_iEndTime;
	protected int m_iLegEndTime;
	protected int m_iDwellEndTime;
	protected int m_iDwellSeconds;
	protected int m_iLegSeconds;
	protected int m_iMaxStops;
	protected int m_iIssuedStops;
	protected int m_iPointOrdinal;
	protected ref AROR_QRFSearchPlan m_Plan;
	protected int m_iPackageIndex;
	protected int m_iPackages;
	protected float m_fVicinityMargin;
	protected float m_fStopSpacing;
	protected bool m_bContactPhaseFinished;

	void Configure_S(AROR_QRFSearchPlan plan, int packageIndex, int packages, float vicinityMargin, float stopSpacing)
	{
		if (!Replication.IsServer() || m_bActive)
			return;
		m_Plan = plan;
		m_iPackageIndex = packageIndex;
		m_iPackages = packages;
		m_fVicinityMargin = vicinityMargin;
		m_fStopSpacing = stopSpacing;
	}

	bool Start_S(SCR_AIGroup group, IEntity vehicle, AROR_LocationComponent location, AIWaypoint previousWaypoint, ResourceName waypointPrefab, string operationId, int now, int duration, int maxStops, int dwellSeconds, int legSeconds)
	{
		if (!Replication.IsServer() || !group || !vehicle || !location || !m_Plan || duration < 1 || maxStops < 1 || dwellSeconds < 0 || legSeconds < 1 || m_iPackages < 1 || m_iPackageIndex < 0 || m_iPackageIndex >= m_iPackages || m_fVicinityMargin < 0 || m_fStopSpacing < 1)
			return false;
		m_Group = group;
		m_Vehicle = vehicle;
		m_Location = location;
		m_sWaypointPrefab = waypointPrefab;
		m_sOperationId = operationId;
		m_iEndTime = now + duration;
		m_iDwellSeconds = dwellSeconds;
		m_iLegSeconds = legSeconds;
		m_iMaxStops = maxStops;
		// Find a usable stop before disturbing the original deployment order.
		if (!CreateNextStop_S(now))
			return false;
		if (previousWaypoint)
			m_Group.RemoveWaypoint(previousWaypoint);
		m_Group.AddWaypoint(m_Waypoint);
		m_bActive = true;
		Print(string.Format("[AROR] Mounted town search started: %1 location=%2 package=%3/%4 duration=%5s", m_sOperationId, location.GetId(), m_iPackageIndex + 1, m_iPackages, duration), LogLevel.NORMAL);
		return true;
	}

	//! True while searching. False hands the vehicle back to its original deployment policy.
	bool Tick_S(int now)
	{
		if (!Replication.IsServer() || !m_bActive)
			return false;
		if (!m_Group || !m_Vehicle || !m_Location || now >= m_iEndTime)
		{
			Finish_S("Search duration elapsed or search entity unavailable");
			return false;
		}
		if (m_Waypoint)
		{
			if (vector.DistanceXZ(m_Vehicle.GetOrigin(), m_Waypoint.GetOrigin()) <= 25)
			{
				Print(string.Format("[AROR] Mounted search %1 reached stop %2", m_sOperationId, m_iIssuedStops), LogLevel.NORMAL);
				ClearWaypoint_S();
				m_iDwellEndTime = now + m_iDwellSeconds;
			}
			else if (now >= m_iLegEndTime)
			{
				Print(string.Format("[AROR] Mounted search %1 skipped stalled stop %2", m_sOperationId, m_iIssuedStops), LogLevel.NORMAL);
				ClearWaypoint_S();
				m_iDwellEndTime = now;
			}
			else
				return true;
		}
		if (now < m_iDwellEndTime)
			return true;
		if (m_iIssuedStops >= m_iMaxStops || !CreateNextStop_S(now))
		{
			Finish_S("Stop limit reached or no further reachable town road point");
			return false;
		}
		m_Group.AddWaypoint(m_Waypoint);
		return true;
	}

	protected bool CreateNextStop_S(int now)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return false;
		RoadNetworkManager roads = aiWorld.GetRoadNetworkManager();
		if (!roads)
			return false;
		vector center = m_Location.GetCenter();
		vector contactCenter;
		bool hasContact = m_Plan.GetContactCenter(center, m_Location.GetRadius(), m_fVicinityMargin, contactCenter);
		if (!hasContact || m_iIssuedStops >= 2)
			m_bContactPhaseFinished = true;
		// First two issued stops investigate the report. Failed contact queries fall back
		// to town patrol, with at most sixteen total road queries per stop.
		for (int attempt = 0; attempt < 16; attempt++)
		{
			if (attempt == 8)
				m_bContactPhaseFinished = true;
			bool investigate = !m_bContactPhaseFinished;
			vector searchCenter = center;
			float radius = m_Location.GetRadius() * Math.RandomFloat(0.35, 0.8);
			if (investigate)
			{
				searchCenter = contactCenter;
				radius = Math.RandomFloat(35, 80);
			}
			vector desired = m_Plan.GetSectorPoint(searchCenter, radius, m_iPackageIndex, m_iPackages, m_iPointOrdinal);
			m_iPointOrdinal++;
			vector reachable;
			if (!roads.GetReachableWaypointInRoad(m_Vehicle.GetOrigin(), desired, 75, reachable))
				continue;
			float allowedRadius = m_Location.GetRadius();
			if (investigate)
				allowedRadius += m_fVicinityMargin;
			if (vector.DistanceXZ(center, reachable) > allowedRadius || vector.DistanceXZ(reachable, desired) > 75 || vector.DistanceXZ(m_Vehicle.GetOrigin(), reachable) < 40 || !m_Plan.CanClaimStop(reachable, m_fStopSpacing))
				continue;
			Resource resource = Resource.Load(m_sWaypointPrefab);
			if (!resource || !resource.IsValid())
				return false;
			EntitySpawnParams spawnParams = new EntitySpawnParams();
			spawnParams.TransformMode = ETransformMode.WORLD;
			reachable[1] = GetGame().GetWorld().GetSurfaceY(reachable[0], reachable[2]);
			spawnParams.Transform[3] = reachable;
			IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
			m_Waypoint = AIWaypoint.Cast(entity);
			if (!m_Waypoint)
			{
				if (entity)
					SCR_EntityHelper.DeleteEntityAndChildren(entity);
				return false;
			}
			m_Waypoint.SetCompletionRadius(20);
			if (!m_Plan.ClaimStop_S(reachable, m_fStopSpacing))
			{
				ClearWaypoint_S();
				return false;
			}
			m_iLegEndTime = now + m_iLegSeconds;
			m_iIssuedStops++;
			string phase = "town patrol";
			if (investigate)
				phase = "reported contact";
			Print(string.Format("[AROR] Mounted search %1 package=%2 stop=%3 phase=%4 road point=%5", m_sOperationId, m_iPackageIndex + 1, m_iIssuedStops, phase, reachable), LogLevel.NORMAL);
			return true;
		}
		return false;
	}

	protected void ClearWaypoint_S()
	{
		if (m_Waypoint)
		{
			if (m_Group)
				m_Group.RemoveWaypoint(m_Waypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		}
		m_Waypoint = null;
	}

	protected void Finish_S(string reason)
	{
		Cleanup_S();
		Print(string.Format("[AROR] Mounted town search finished: %1: %2; resuming deployment", m_sOperationId, reason), LogLevel.NORMAL);
	}

	void Cleanup_S()
	{
		if (!Replication.IsServer())
			return;
		ClearWaypoint_S();
		m_bActive = false;
		m_Plan = null;
	}
}
