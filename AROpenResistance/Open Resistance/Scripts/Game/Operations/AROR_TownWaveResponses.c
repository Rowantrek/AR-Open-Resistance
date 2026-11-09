//! Town garrison response waves (see AROR_TownWavesComponent): a named wave recipe at the campaign's current
//! Readiness (equipment) with an element limit from Crackdown (size), paid by the scenario, launched from a road
//! staging point near the town and driven as a convoy. Trusted server callers only; there is no client RPC.
//! Executor capacity, live-ground limits and launch safety still apply.
//! Named after AROR_ScriptedResponses.c on purpose: modded files compile in file order, and these overrides build
//! on its fixed-origin support.
modded class AROR_OperationManagerComponent
{
	[Attribute("0", desc: "Readiness pressure removed each quiet minute (same quiet time as crackdown cooling), never below the starting Readiness level; 0 keeps equipment progress. Town fights and captures push it back up.", params: "0 100 1")]
	protected int m_iReadinessDecay;
	protected int m_iNextReadinessDecay;

	//! Town fighting (AROR_TownWavesComponent): crackdown and readiness pressure for an army's losses in a town or a
	//! taken garrison post. Counts as fresh evidence, so neither scale cools while the fight goes on.
	//! capture: also apply the scenario's capture escalation (as for a captured location).
	void RecordTownEscalation_S(AROR_EFactionRole role, int crackdown, int readiness, string townId, string reason, bool capture = false)
	{
		if (!Replication.IsServer() || !IsEnemyRole(role))
			return;
		int crackdownBefore = GetCrackdown(role);
		int readinessBefore = GetReadinessPressure(role);
		if (capture)
			RecordCaptureEscalation_S(role);
		SetCrackdown_S(role, GetCrackdown(role) + crackdown);
		SetReadinessPressure_S(role, GetReadinessPressure(role) + readiness);
		if (role == AROR_EFactionRole.OCCUPIERS)
			m_iOccupierLastThreat = m_iClock;
		else
			m_iInvaderLastThreat = m_iClock;
		Print(string.Format("[AROR BALANCE] town_escalation town=%1 role=%2 reason=%3 crackdown=%4->%5 readiness=%6->%7 crackdownLevel=%8 readinessLevel=%9", townId, role, reason, crackdownBefore, GetCrackdown(role), readinessBefore, GetReadinessPressure(role), GetCrackdownLevel(role), GetReadinessLevel(role)), LogLevel.NORMAL);
	}

	//! Readiness cools like crackdown once an army has been quiet, down to the scenario's starting level
	override protected void UpdateCrackdown_S()
	{
		super.UpdateCrackdown_S();
		if (!Replication.IsServer() || !m_bQRFStrategicEconomy || m_iReadinessDecay <= 0 || m_iClock < m_iNextReadinessDecay)
			return;
		m_iNextReadinessDecay = m_iClock + 60;
		int floor = AROR_QRFStrategicPolicy.PressureForLevel(m_iStartingReadinessLevel);
		if (m_iClock - m_iOccupierLastThreat >= m_iCrackdownQuietTime && GetReadinessPressure(AROR_EFactionRole.OCCUPIERS) > floor)
			SetReadinessPressure_S(AROR_EFactionRole.OCCUPIERS, Math.Max(floor, GetReadinessPressure(AROR_EFactionRole.OCCUPIERS) - m_iReadinessDecay));
		if (m_iClock - m_iInvaderLastThreat >= m_iCrackdownQuietTime && GetReadinessPressure(AROR_EFactionRole.INVADERS) > floor)
			SetReadinessPressure_S(AROR_EFactionRole.INVADERS, Math.Max(floor, GetReadinessPressure(AROR_EFactionRole.INVADERS) - m_iReadinessDecay));
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the new operation ID, or empty when rejected. Several waves may target the same town at once.
	//! origin vector.Zero launches from the role's best friendly military site, as an ordinary QRF does (the
	//! helicopter assault). elementOffset skips the recipe's first elements (the second column of a split wave).
	string RequestTownWave_S(AROR_EFactionRole role, string targetId, string recipeId, int readiness, int crackdownLevel, vector origin, int elementLimit, float dismount, int elementOffset = 0)
	{
		if (!IsReady() || !IsEnemyRole(role) || !HasLocation(targetId) || recipeId.IsEmpty())
			return string.Empty;
		if (readiness < 1 || readiness > 5 || crackdownLevel < 1 || crackdownLevel > 5)
			return string.Empty;
		if (role == AROR_EFactionRole.INVADERS && !m_bInvaderOperationsEnabled)
		{
			Print(string.Format("[AROR] Town wave for %1 rejected: Invader operations are disabled in this scenario", targetId), LogLevel.WARNING);
			return string.Empty;
		}
		m_iNextId++;
		string id = string.Format("operation_%1", m_iNextId);
		AROR_Operation created = new AROR_Operation(id, role, AROR_EOperationType.QRF, targetId, m_iClock, 0);
		created.m_sRequestedRecipeId = recipeId;
		created.m_iRequestedTier = crackdownLevel;
		created.m_iPoolReadinessLevel = readiness;
		created.m_bUseResponseMetrics = true;
		created.m_bFundingWaived = true;
		created.m_bHasOriginOverride = origin != vector.Zero;
		created.m_vOriginOverride = origin;
		created.m_bAssaultWave = true;
		created.m_bConvoy = dismount > 0;
		created.m_fConvoyDismount = dismount;
		created.m_iElementLimit = Math.Max(elementLimit, 0);
		created.m_iElementOffset = Math.Max(elementOffset, 0);
		m_aOperations.Insert(created);
		Print(string.Format("[AROR] Town wave %1 role=%2 target=%3 recipe=%4 readiness=%5 crackdown=%6 elements=%7+%8 origin=%9", id, role, targetId, recipeId, readiness, crackdownLevel, elementOffset, elementLimit, origin), LogLevel.NORMAL);
		NotifyChanged(created);
		return id;
	}

	//! Seconds until a town wave's first package reaches the town, or -1 before dispatch or when unknown
	int GetTownWaveEta(string operationId)
	{
		AROR_Operation operation = GetOperation(operationId);
		if (!operation)
			return -1;
		return operation.GetEtaRemaining(m_iClock);
	}
}

modded class AROR_OperationExecutorComponent
{
	protected static const float CONVOY_STOP_SPACING = 25;	//!< Metres between the vehicles' stops in a column
	protected static const int STAGING_ROAD_QUERIES = 48;

	//! The role's wave recipe for this wave number that the Readiness window allows, or empty
	string FindTownWaveRecipeId_S(AROR_EFactionRole role, int wave, int readiness)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!Replication.IsServer() || !roles || wave < 1)
			return string.Empty;
		string faction = roles.GetFactionKey(role);
		foreach (AROR_GroundQRFRecipe recipe : m_aGroundQRFRecipes)
		{
			if (recipe && recipe.m_iAssaultWave == wave && recipe.m_eRole == role && recipe.m_sFactionKey == faction && recipe.AllowsReadinessLevel(readiness))
				return recipe.m_sId;
		}
		return string.Empty;
	}

	//! A road point between min and max distance from the target, nearest the preferred direction first, that a
	//! ground response can launch from right now (dry land, away from players) and that is at least avoidDistance
	//! from every point in avoid (roads other columns already use). Bounded number of road queries, so it can run
	//! once per wave.
	bool FindWaveStaging_S(vector target, vector towards, float minDistance, float maxDistance, out vector staging, array<vector> avoid = null, float avoidDistance = 0)
	{
		if (!Replication.IsServer() || minDistance < 300 || minDistance > maxDistance)
			return false;
		vector heading = towards - target;
		heading[1] = 0;
		float bearing = Math.RandomFloat(0, Math.PI2);
		if (heading.Length() > 1)
			bearing = Math.Atan2(heading[0], heading[2]);
		BaseWorld world = GetGame().GetWorld();
		int queries;
		float step = Math.Max((maxDistance - minDistance) / 3, 100);
		for (float radius = minDistance; radius <= maxDistance; radius += step)
		{
			for (int side = 0; side < 9; side++)
			{
				// Preferred bearing first, then alternating either side up to 120 degrees off it
				float offset = ((side + 1) / 2) * 0.5235988;
				if (Math.Mod(side, 2) == 1)
					offset = -offset;
				float angle = bearing + offset;
				vector desired = target + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius;
				if (queries >= STAGING_ROAD_QUERIES)
					return false;
				queries++;
				vector road;
				if (!TacticalRoadPoint(desired, desired, road, 150))
					continue;
				float distance = vector.DistanceXZ(road, target);
				if (distance < minDistance * 0.8 || distance > maxDistance * 1.2)
					continue;
				if (world.GetSurfaceY(road[0], road[2]) <= world.GetOceanHeight(road[0], road[2]) + 0.5)
					continue;
				if (IsNearAny(road, avoid, avoidDistance))
					continue;
				if (!IsLaunchPositionSafe(road, target))
					continue;
				staging = road;
				staging[1] = world.GetSurfaceY(road[0], road[2]);
				return true;
			}
		}
		return false;
	}

	protected static bool IsNearAny(vector point, array<vector> others, float distance)
	{
		if (!others || distance <= 0)
			return false;
		foreach (vector other : others)
		{
			if (vector.DistanceXZ(point, other) < distance)
				return true;
		}
		return false;
	}

	//! The soldiers an operation fielded (infantry and vehicle crews), so a town's lockdown counts only the
	//! reinforcements that arrived and never the defenders who were already there
	void GetOperationMembers_S(string operationId, notnull array<IEntity> members)
	{
		if (!Replication.IsServer() || operationId.IsEmpty())
			return;
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (!force || force.m_sOperationId != operationId)
				continue;
			AddGroupMembers(force.m_Group, members);
			AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
			if (ground)
				AddGroupMembers(ground.m_CrewGroup, members);
		}
	}

	protected static void AddGroupMembers(SCR_AIGroup group, notnull array<IEntity> members)
	{
		if (!group)
			return;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity soldier = agent.GetControlledEntity();
			if (soldier && !members.Contains(soldier))
				members.Insert(soldier);
		}
	}

	//! Whether a staging point found earlier can still launch the next wave
	bool IsWaveStagingSafe_S(vector staging, vector target)
	{
		return Replication.IsServer() && IsLaunchPositionSafe(staging, target);
	}

	//! How many of an operation's live packages (group leader or vehicle) are within radius of the target: a wave
	//! "pressing" the objective, which commits the helicopter assault
	int CountPressingPackages_S(string operationId, vector target, float radius)
	{
		if (!Replication.IsServer() || operationId.IsEmpty())
			return 0;
		int pressing;
		foreach (AROR_OperationForce force : m_aForces)
		{
			if (!force || force.m_sOperationId != operationId)
				continue;
			IEntity anchor;
			if (force.m_Group)
				anchor = force.m_Group.GetLeaderEntity();
			AROR_GroundOperationForce ground = AROR_GroundOperationForce.Cast(force);
			if (!anchor && ground)
				anchor = ground.m_Vehicle;
			if (anchor && vector.DistanceXZ(anchor.GetOrigin(), target) <= radius)
				pressing++;
		}
		return pressing;
	}

	override protected void CreateQRFTacticalCommander(AROR_Operation operation, int packages, vector origin)
	{
		super.CreateQRFTacticalCommander(operation, packages, origin);
		if (!operation || !operation.m_bAssaultWave || !operation.m_TacticalCommander)
			return;
		// A wave arrives as one column; its follow-up is the next wave, not a held-back reserve
		operation.m_TacticalCommander.ForceSingleWave();
		operation.m_TacticalCommander.m_bConvoy = operation.m_bConvoy;
		operation.m_TacticalCommander.m_fConvoyDismount = operation.m_fConvoyDismount;
	}

	//! Convoy packages line up behind the lead along the launch heading instead of side by side
	override protected vector TacticalLaunchPoint(AROR_QRFTacticalCommander commander, int index)
	{
		if (!commander || !commander.m_bConvoy)
			return super.TacticalLaunchPoint(commander, index);
		vector heading = commander.m_vTown - commander.m_vOrigin;
		heading[1] = 0;
		if (heading.Length() < 1)
			return super.TacticalLaunchPoint(commander, index);
		heading.Normalize();
		return commander.m_vOrigin - heading * (index * m_fPackageSpacing);
	}

	//! Convoy packages stop one behind another on the road leading in from the launch side, short of the objective;
	//! infantry walk the rest. Falls back to the normal approach search when no such road stop exists.
	override protected bool FindTacticalApproach(AROR_GroundOperationForce force, AROR_QRFTacticalCommander commander, vector origin, float standoff, out vector approach, int firstAttempt = 0, bool preferFoot = false)
	{
		if (!force || !commander || !commander.m_bConvoy || commander.m_fConvoyDismount <= 0)
			return super.FindTacticalApproach(force, commander, origin, standoff, approach, firstAttempt, preferFoot);
		vector heading = commander.m_vOrigin - commander.m_vTown;
		heading[1] = 0;
		if (heading.Length() < 1)
			return super.FindTacticalApproach(force, commander, origin, standoff, approach, firstAttempt, preferFoot);
		heading.Normalize();
		float limit = Math.Max(m_fQRFMaximumRoadDismount - 10, m_fQRFObjectiveDeliveryRadius);
		float distance = Math.Min(commander.m_fConvoyDismount + force.m_iPackageIndex * CONVOY_STOP_SPACING, limit);
		vector desired = commander.m_vTown + heading * distance;
		vector reachable;
		if (TacticalRoadPoint(origin, desired, reachable, 120))
		{
			float townDistance = vector.DistanceXZ(reachable, commander.m_vTown);
			if (townDistance >= 15 && townDistance <= m_fQRFMaximumRoadDismount && vector.DistanceXZ(reachable, origin) >= 40 && CanDismountOnFoot(reachable))
			{
				approach = reachable;
				force.m_fTacticalDeliveryRadius = Math.Min(m_fQRFMaximumRoadDismount, townDistance + m_fQRFVehicleArrivalTolerance);
				Print(string.Format("[AROR TACTICAL] %1 package=%2 convoy stop=%3 townDistance=%4; infantry walk in", force.m_sOperationId, force.m_iPackageIndex + 1, approach, townDistance), LogLevel.NORMAL);
				return true;
			}
		}
		return super.FindTacticalApproach(force, commander, origin, standoff, approach, firstAttempt, preferFoot);
	}
}
