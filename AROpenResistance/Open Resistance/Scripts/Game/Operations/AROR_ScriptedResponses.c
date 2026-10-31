//! Scripted responses: a named recipe at frozen Readiness/Crackdown, paid by the scenario and launched from a
//! fixed point instead of a friendly military site. Used by strongholds (see AROR_StrongholdComponent).
//! Trusted server callers only; there is no client RPC. Executor capacity and launch safety still apply.
modded class AROR_OperationManagerComponent
{
	//! Returns the new operation ID, or empty when rejected. Preserve target conflicts with ordinary operations.
	//! Without a fixed origin the response launches from the role's best friendly military site, as usual.
	string RequestScriptedQRF_S(AROR_EFactionRole role, string targetId, string recipeId, int level, bool fixedOrigin, vector origin, bool twoWaves)
	{
		if (!IsReady() || !IsEnemyRole(role) || !HasLocation(targetId) || recipeId.IsEmpty() || level < 1 || level > 5)
			return string.Empty;
		if (role == AROR_EFactionRole.INVADERS && !m_bInvaderOperationsEnabled)
		{
			Print(string.Format("[AROR] Scripted QRF for %1 rejected: Invader operations are disabled in this scenario", targetId), LogLevel.WARNING);
			return string.Empty;
		}
		foreach (AROR_Operation existing : m_aOperations)
		{
			if (!existing.IsTerminal() && existing.GetRole() == role && existing.GetTargetLocationId() == targetId)
				return string.Empty;
		}
		m_iNextId++;
		string id = string.Format("operation_%1", m_iNextId);
		AROR_Operation created = new AROR_Operation(id, role, AROR_EOperationType.QRF, targetId, m_iClock, 0);
		created.m_sRequestedRecipeId = recipeId;
		created.m_iRequestedTier = level;
		created.m_iPoolReadinessLevel = level;
		created.m_bUseResponseMetrics = true;
		created.m_bFundingWaived = true;
		created.m_bHasOriginOverride = fixedOrigin;
		created.m_vOriginOverride = origin;
		created.m_bForceTwoWaves = twoWaves;
		m_aOperations.Insert(created);
		string from = "friendly base";
		if (fixedOrigin)
			from = origin.ToString();
		Print(string.Format("[AROR] Scripted QRF %1 role=%2 target=%3 recipe=%4 level=%5 origin=%6 twoWaves=%7", id, role, targetId, recipeId, level, from, twoWaves), LogLevel.NORMAL);
		NotifyChanged(created);
		return id;
	}
}

modded class AROR_OperationExecutorComponent
{
	override protected vector ResolveQRFOrigin(AROR_Operation operation, AROR_LocationComponent target, vector fallback, bool logOrigin = true)
	{
		if (operation && operation.m_bHasOriginOverride)
		{
			if (logOrigin)
				Print(string.Format("[AROR] QRF %1 fixed origin=%2", operation.GetId(), operation.m_vOriginOverride), LogLevel.NORMAL);
			return operation.m_vOriginOverride;
		}
		return super.ResolveQRFOrigin(operation, target, fallback, logOrigin);
	}

	override protected bool ReserveQRFFunding(AROR_Operation operation, array<AROR_ForceProfile> elements, int tier)
	{
		if (operation.m_bFundingWaived)
			return true;
		return super.ReserveQRFFunding(operation, elements, tier);
	}

	//! A fixed origin has no site to lose; the launch safety check still runs for every package.
	override protected bool TacticalOriginAvailable(AROR_Operation operation)
	{
		if (operation.m_bHasOriginOverride && operation.m_TacticalCommander && operation.m_TacticalCommander.m_sOriginLocationId.IsEmpty())
			return true;
		return super.TacticalOriginAvailable(operation);
	}

	override protected void CreateQRFTacticalCommander(AROR_Operation operation, int packages, vector origin)
	{
		super.CreateQRFTacticalCommander(operation, packages, origin);
		if (operation.m_bHasOriginOverride && operation.m_TacticalCommander)
		{
			// A beach is a launch point, not an owned installation or proof of safe recovery.
			operation.m_TacticalCommander.m_sOriginLocationId = string.Empty;
			operation.m_TacticalCommander.m_bSafeWithdrawalOrigin = false;
		}
		if (operation.m_bForceTwoWaves && operation.m_TacticalCommander)
			operation.m_TacticalCommander.ForceTwoWaves();
	}

	//! Whether a fixed point can launch a ground response at the target right now (land, distance, players).
	bool IsLaunchPointSafe_S(vector position, vector target)
	{
		return Replication.IsServer() && IsLaunchPositionSafe(position, target);
	}

	//! Coastal staging (this search does not spawn sea transport): low dry ground with a road in launch reach, around an
	//! anchor within a bearing arc (0 = north), nearest first. Bounded to a fixed number of terrain samples and
	//! road queries, so it can run once when a response is triggered.
	bool FindCoastalLanding_S(vector anchor, float minDistance, float maxDistance, float bearing, float arc, out vector landing)
	{
		if (!Replication.IsServer() || minDistance < 300 || maxDistance > 8000 || minDistance > maxDistance || arc < 30 || arc > 360)
			return false;
		BaseWorld world = GetGame().GetWorld();
		int roadQueries;
		for (float radius = minDistance; radius <= maxDistance; radius += 150)
		{
			for (int step = 0; step <= 16; step++)
			{
				// Preferred bearing first, then alternating either side out to the edge of the arc
				float offset = ((step + 1) / 2) * arc / 16.0;
				if (Math.Mod(step, 2) == 0)
					offset = -offset;
				float angle = (bearing + offset) * Math.DEG2RAD;
				vector point = anchor + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius;
				if (!IsCoastalLand(world, point))
					continue;
				if (roadQueries >= 48)
					return false;
				roadQueries++;
				vector road;
				if (!TacticalRoadPoint(point, point, road) || vector.DistanceXZ(road, point) > m_fQRFRoadLaunchRadius * 0.6)
					continue;
				if (!IsLaunchPositionSafe(road, anchor))
					continue;
				landing = point;
				landing[1] = world.GetSurfaceY(point[0], point[2]);
				return true;
			}
		}
		return false;
	}

	//! Low dry ground with open water within 100 m
	protected bool IsCoastalLand(BaseWorld world, vector point)
	{
		float height = world.GetSurfaceY(point[0], point[2]);
		float sea = world.GetOceanHeight(point[0], point[2]);
		if (height <= sea + 0.5 || height > sea + 10)
			return false;
		for (int probe = 0; probe < 16; probe++)
		{
			float angle = Math.Mod(probe, 8) * 45 * Math.DEG2RAD;
			float distance = 50;
			if (probe >= 8)
				distance = 100;
			float x = point[0] + Math.Sin(angle) * distance;
			float z = point[2] + Math.Cos(angle) * distance;
			if (world.GetSurfaceY(x, z) <= world.GetOceanHeight(x, z))
				return true;
		}
		return false;
	}
}
