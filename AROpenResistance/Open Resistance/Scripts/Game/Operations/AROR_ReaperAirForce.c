//! One experimental QRF package. The normal executor owns its reservation and lifetime.
class AROR_ReaperAirForce : AROR_GroundOperationForce
{
	ref AROR_GroundOperationForce m_Escort;
	int m_iPackageCount = 1;
	vector m_vLaunch;
	vector m_vEscortLaunch;
	vector m_vLanding;
	vector m_vAirApproach;
	vector m_vCover;
	int m_iFlightStage;
	int m_iFlightDeadline;
	int m_iSettleTicks;
	int m_iEscortStage;
	int m_iEscortDeadline;
	int m_iPassOrdinal;
	vector m_vPassPoint;
	bool m_bEscortLost;
	bool m_bTransportLost;
	bool m_bTransportRetired;
	bool m_bCleanupQueued;
	bool m_bImmediateCleanup;
	bool m_bDepartureWarning;
	bool m_bAircraftHeld;
	bool m_bConstructionPending;
	ResourceName m_sPilotPrefab;
	bool m_bWaitingForLanding;
	ref AROR_ReaperLandingSearch m_AlternateLandingSearch;
	int m_iNextFormationUpdate;
	int m_iSupportEndTime;
	int m_iDepartureTime;
	int m_iPatrolStep;
	int m_iPatrolDeadline;
	int m_iRetainSeconds;
	float m_fCleanupDistance;
	float m_fTownRadius;
	vector m_vTown;
	vector m_vHeading;
	vector m_vEgress;
	vector m_vInvestigation;
	vector m_vGunshipSearch;
	int m_iGunshipContactSequence;
	int m_iNextContactCheck;
	int m_iTrafficYieldUntil;
	bool m_bTrafficYielding;
	ref AROR_QRFSearchPlan m_SearchPlan;

	//! Native COLLISION state keeps boarding/collision support without letting
	//! uncrewed aircraft fall while the asynchronous member queue is running.
	void HoldAircraft_S()
	{
		if (!Replication.IsServer() || !m_bAircraftHeld)
			return;
		SetAircraftSimulation(m_Vehicle, SimulationState.COLLISION);
		if (m_Escort)
			SetAircraftSimulation(m_Escort.m_Vehicle, SimulationState.COLLISION);
	}

	void ReleaseAircraft_S()
	{
		if (!Replication.IsServer() || !m_bAircraftHeld)
			return;
		SetAircraftSimulation(m_Vehicle, SimulationState.SIMULATION);
		if (m_Escort)
			SetAircraftSimulation(m_Escort.m_Vehicle, SimulationState.SIMULATION);
		m_bAircraftHeld = false;
	}

	protected static void SetAircraftSimulation(IEntity vehicle, SimulationState state)
	{
		if (!vehicle)
			return;
		Physics physics = vehicle.GetPhysics();
		if (!physics)
			return;
		physics.ChangeSimulationState(state);
		physics.SetVelocity(vector.Zero);
	}

	static REAPER_AiHelicopterControllerComponent Controller(IEntity vehicle)
	{
		if (!vehicle)
			return null;
		SCR_HelicopterControllerComponent controller = SCR_HelicopterControllerComponent.Cast(vehicle.FindComponent(SCR_HelicopterControllerComponent));
		if (!controller)
			return null;
		return controller.REAPER_GetAiControllerComponent();
	}

	static bool SetFlightOrder_S(AROR_GroundOperationForce aircraft, ResourceName prefab, vector point, float speed, float height)
	{
		if (!Replication.IsServer() || !aircraft || !aircraft.m_CrewGroup)
			return false;
		EntitySpawnParams parameters = new EntitySpawnParams();
		parameters.TransformMode = ETransformMode.WORLD;
		point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		parameters.Transform[3] = point;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return false;
		IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
		REAPER_AiHelicopterBaseWaypoint next = REAPER_AiHelicopterBaseWaypoint.Cast(entity);
		if (!next)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return false;
		}
		next.REAPER_SetMaxSpeed(speed);
		next.REAPER_SetHeightAboveTerrain(height);
		// All removal is ours, after checks against every player. Never backend auto-delete.
		next.REAPER_SetDeleteHelicopterAndCrew(false);
		REAPER_AiHelicopterLandWaypoint land = REAPER_AiHelicopterLandWaypoint.Cast(next);
		if (land)
		{
			// Hold until our ground-contact and animated-exit checks release it.
			land.REAPER_SetWaitPlayersGetIn(true);
			land.REAPER_SetWaitPlayersMinimumCount(9999);
			land.REAPER_SetWaitPlayersGetInPercent(0);
			land.REAPER_SetWaitPlayersGetOut(false);
			land.REAPER_SetWaitAiGroupGetOut(false);
			land.REAPER_SetCheckForCloseUnts(false);
		}
		aircraft.m_CrewGroup.AddWaypoint(next);
		if (aircraft.m_CrewWaypoint)
		{
			aircraft.m_CrewGroup.RemoveWaypoint(aircraft.m_CrewWaypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(aircraft.m_CrewWaypoint);
		}
		aircraft.m_CrewWaypoint = next;
		return true;
	}

	//! Remove only a successfully departing transport; delivered infantry retain ownership/orders.
	void RemoveDepartedTransport_S()
	{
		if (!Replication.IsServer() || m_bTransportRetired || !m_bDismounted)
			return;
		if ((m_CrewGroup && m_CrewGroup.GetPlayerCount() > 0) || AROR_ReaperRetirement.NearPlayers(m_Vehicle, m_fCleanupDistance) || AROR_ReaperRetirement.GroupNearPlayers(m_CrewGroup, m_fCleanupDistance))
			return;
		array<AIAgent> agents = {};
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
			operations.GetQRFFinance().Observe_S(m_Funding);
		if (m_CrewGroup)
		{
			m_CrewGroup.GetAgents(agents);
			m_CrewGroup.GetOnAgentAdded().Remove(OnAgentAdded);
			if (m_CrewWaypoint)
				m_CrewGroup.RemoveWaypoint(m_CrewWaypoint);
		}
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.GetControlledEntity())
			{
				if (m_Funding)
					m_Funding.Retire_S(agent.GetControlledEntity());
				SCR_EntityHelper.DeleteEntityAndChildren(agent.GetControlledEntity());
			}
		}
		if (m_CrewWaypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_CrewWaypoint);
		if (m_CrewGroup)
			SCR_EntityHelper.DeleteEntityAndChildren(m_CrewGroup);
		if (m_Vehicle)
		{
			if (m_Funding)
				m_Funding.Retire_S(m_Vehicle);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
		}
		m_CrewWaypoint = null;
		m_CrewGroup = null;
		m_Vehicle = null;
		m_bTransportRetired = true;
		m_iReservedGroups--;
		m_iReservedVehicles--;
	}

	static bool BoardPilot_S(SCR_AIGroup group, IEntity vehicle)
	{
		if (!Replication.IsServer() || !group || !vehicle)
			return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.Count() != 1 || !agents[0])
			return false;
		IEntity pilot = agents[0].GetControlledEntity();
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!pilot || !compartments)
			return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(pilot.FindComponent(CompartmentAccessComponent));
		if (!access)
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (PilotCompartmentSlot.Cast(slot) && slot.IsCompartmentAccessible() && !slot.IsOccupied())
				return access.GetInVehicle(vehicle, slot, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
		}
		return false;
	}

	bool BoardPackage_S()
	{
		if (!m_Escort || !BoardPilot_S(m_CrewGroup, m_Vehicle) || !BoardPilot_S(m_Escort.m_CrewGroup, m_Escort.m_Vehicle))
			return false;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return false;
		array<BaseCompartmentSlot> slots = {};
		array<BaseCompartmentSlot> cargo = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (CargoCompartmentSlot.Cast(slot) && slot.IsCompartmentAccessible() && !slot.IsOccupied())
				cargo.Insert(slot);
		}
		array<AIAgent> agents = {};
		m_Group.GetAgents(agents);
		if (agents.Count() < 1 || cargo.Count() < agents.Count())
			return false;
		for (int index = 0; index < agents.Count(); index++)
		{
			if (!agents[index] || !Seat(agents[index].GetControlledEntity(), cargo[index]))
				return false;
			m_aPassengers.Insert(agents[index].GetControlledEntity());
		}
		m_bMounted = true;
		return true;
	}

	//! Recovery after pilot loss still requires surviving passengers physically on the ground.
	bool PassengersSafeOnGround()
	{
		if (!ArePassengersOut())
			return false;
		bool found;
		foreach (IEntity passenger : m_aPassengers)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(passenger);
			if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
				continue;
			found = true;
			vector point = passenger.GetOrigin();
			if (point[1] - GetGame().GetWorld().GetSurfaceY(point[0], point[2]) > 3)
				return false;
		}
		return found;
	}

	static bool AircraftAlive(IEntity vehicle)
	{
		if (!vehicle)
			return false;
		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		return !damage || !damage.IsDestroyed();
	}

	//! Ground contact plus low speed at the selected pad, not a timer-only landing claim.
	static bool SettledAt(IEntity vehicle, vector pad)
	{
		if (!vehicle || vector.DistanceXZ(vehicle.GetOrigin(), pad) > 25)
			return false;
		HelicopterControllerComponent controller = HelicopterControllerComponent.Cast(vehicle.FindComponent(HelicopterControllerComponent));
		Physics physics = vehicle.GetPhysics();
		if (!controller || !controller.GetSimulation() || !physics || !controller.GetSimulation().HasAnyGroundContact())
			return false;
		vector velocity = physics.GetVelocity();
		vector position = vehicle.GetOrigin();
		float height = position[1] - GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		return velocity.Length() < 1 && height >= -1 && height <= 5;
	}

	override void Cleanup_S()
	{
		if (!Replication.IsServer())
			return;
		// Also unfreeze preserved/player-occupied aircraft on abort and world unload.
		ReleaseAircraft_S();
		if (m_bOrdersIssued && !m_bImmediateCleanup)
		{
			if (!m_bCleanupQueued)
			{
				m_bCleanupQueued = true;
				AROR_ReaperRetirement.Enqueue_S(this);
			}
			return;
		}
		if (m_Escort)
			m_Escort.Cleanup_S();
		m_Escort = null;
		super.Cleanup_S();
	}
}
