class AROR_GroundOperationForce : AROR_OperationForce
{
	ref AROR_ForceProfile m_Profile;
	SCR_AIGroup m_CrewGroup;
	IEntity m_Vehicle;
	AIWaypoint m_CrewWaypoint;
	bool m_bMounted;
	bool m_bMountRequested;
	bool m_bDismountRequested;
	bool m_bDismounted;
	bool m_bTacticalDropStarted;
	int m_iTacticalExitDeadline;
	int m_iTacticalExitStarted;
	int m_iTacticalNextTravelLog;
	int m_iVehicleFactionMismatchSince = -1;
	bool m_bEmptyVehicleFactionLogged;
	bool m_bSawInfantry;
	bool m_bSawCrew;
	bool m_bTownSearchAttempted;
	bool m_bTownSearchFinished;
	int m_iExitAttempts;
	int m_iTacticalNextRouteCheck;
	int m_iTacticalRouteRetries;
	vector m_vTacticalProgressPosition;
	ref AROR_VehicleTownSearch m_TownSearch;
	vector m_vApproachPosition;
	protected ref array<IEntity> m_aPassengers = {};

	int CountInfantry()
	{
		int living = super.CountLivingMembers();
		if (living > 0)
			m_bSawInfantry = true;
		return living;
	}

	int CountCrew()
	{
		if (!m_CrewGroup)
			return 0;
		array<AIAgent> agents = {};
		m_CrewGroup.GetAgents(agents);
		int living;
		foreach (AIAgent agent : agents)
		{
			OnAgentAdded(agent);
			if (!agent)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (character && character.GetCharacterController() && !character.GetCharacterController().IsDead())
				living++;
		}
		if (living > 0)
			m_bSawCrew = true;
		return living;
	}

	override int CountLivingMembers()
	{
		int living = super.CountLivingMembers();
		if (m_CrewGroup != m_Group)
			living += CountCrew();
		return living;
	}

	//! Conservative occupancy check: an unavailable seat manager is not proof of an empty vehicle.
	bool HasVehicleOccupants()
	{
		if (!m_Vehicle)
			return true;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return true;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		if (slots.Count() == 0)
			return true;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && (slot.IsOccupied() || slot.GetOccupant()))
				return true;
		}
		return false;
	}

	bool Seat(IEntity entity, BaseCompartmentSlot slot)
	{
		if (!entity || !slot || slot.IsOccupied())
			return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
		// forceTeleport=true: initial deployment never asks AI to walk to or animate into a seat.
		return access && access.GetInVehicle(m_Vehicle, slot, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
	}

	//! Blank native vehicle affiliation is not evidence of hostile capture. Require
	//! a complete seat audit: only this package's own AI may still occupy the hull.
	bool HasOnlyOwnedVehicleOccupants()
	{
		if (!m_Vehicle)
			return false;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		if (slots.Count() == 0)
			return false;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				return false;
			IEntity occupant = slot.GetOccupant();
			if (!occupant)
			{
				if (slot.IsOccupied())
					return false;
				continue;
			}
			if (!m_aMembers.Contains(occupant) || GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(occupant) > 0)
				return false;
			FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(occupant.FindComponent(FactionAffiliationComponent));
			if (!faction || faction.GetAffiliatedFactionKey() != m_sFactionKey)
				return false;
		}
		return true;
	}

	//! Spawn into known seats only after every requested member exists; no crew/infantry index mixing.
	bool Mount_S()
	{
		if (!Replication.IsServer() || !m_Vehicle || !m_CrewGroup)
			return false;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage || !usage.CanBePiloted() || !usage.GetPilotCompartmentSlot())
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		array<BaseCompartmentSlot> crewSeats = {};
		array<BaseCompartmentSlot> cargoSeats = {};
		crewSeats.Insert(usage.GetPilotCompartmentSlot());
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot || !slot.IsCompartmentAccessible() || slot.IsOccupied() || crewSeats.Contains(slot))
				continue;
			if (TurretCompartmentSlot.Cast(slot))
				crewSeats.Insert(slot);
			else if (CargoCompartmentSlot.Cast(slot))
				cargoSeats.Insert(slot);
		}
		array<AIAgent> crew = {};
		m_CrewGroup.GetAgents(crew);
		if (crew.Count() < crewSeats.Count())
			return false;
		// Extra crew may occupy spare cargo seats, but never displace passenger infantry.
		array<AIAgent> infantry = {};
		if (m_Group != m_CrewGroup)
			m_Group.GetAgents(infantry);
		int extraCrew = crew.Count() - crewSeats.Count();
		if (cargoSeats.Count() < infantry.Count() + extraCrew)
			return false;
		for (int i = 0; i < crew.Count(); i++)
		{
			BaseCompartmentSlot crewSeat;
			if (i < crewSeats.Count())
				crewSeat = crewSeats[i];
			else
				crewSeat = cargoSeats[i - crewSeats.Count()];
			if (!crew[i] || !Seat(crew[i].GetControlledEntity(), crewSeat))
				return false;
		}
		for (int p = 0; p < infantry.Count(); p++)
		{
			IEntity passenger;
			if (infantry[p])
				passenger = infantry[p].GetControlledEntity();
			if (!Seat(passenger, cargoSeats[extraCrew + p]))
				return false;
			m_aPassengers.Insert(passenger);
		}
		SCR_AIGroupUtilityComponent utility = m_CrewGroup.GetGroupUtilityComponent();
		if (!utility)
			return false;
		utility.AddUsableVehicle(usage);
		if (m_Group != m_CrewGroup)
		{
			SCR_AIGroupUtilityComponent infantryUtility = m_Group.GetGroupUtilityComponent();
			if (!infantryUtility)
				return false;
			infantryUtility.AddUsableVehicle(usage);
		}
		m_bMountRequested = true;
		return true;
	}

	//! A native teleport request may complete after this tick. Verify the actual seats,
	//! including the driver, before activating or giving either group movement orders.
	bool IsMountComplete()
	{
		if (!m_bMountRequested || !m_Vehicle || !m_CrewGroup)
			return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || !usage.GetPilotCompartmentSlot() || !usage.GetPilotCompartmentSlot().GetOccupant())
			return false;
		array<AIAgent> crew = {};
		m_CrewGroup.GetAgents(crew);
		foreach (AIAgent agent : crew)
		{
			if (!agent || !IsSeatedInOwnVehicle(agent.GetControlledEntity()))
				return false;
		}
		foreach (IEntity passenger : m_aPassengers)
		{
			if (!IsSeatedInOwnVehicle(passenger))
				return false;
		}
		return true;
	}

	protected bool IsSeatedInOwnVehicle(IEntity entity)
	{
		if (!entity)
			return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
		if (!access || !access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut())
			return false;
		BaseCompartmentSlot seat = access.GetCompartment();
		return seat && seat.GetVehicle() == m_Vehicle && seat.GetOccupant() == entity;
	}

	//! Recover only a stuck, living AI passenger at a stationary ground delivery vehicle.
	//! Native clearance supplies the exit transform; never fabricate a position beside the hull.
	bool RecoverPassengerExit_S(IEntity passenger, CompartmentAccessComponent access)
	{
		if (!Replication.IsServer() || !passenger || !access || !m_Vehicle || !m_Profile || !m_aPassengers.Contains(passenger))
			return false;
		if (m_Profile.m_eType != AROR_EForceType.MOTORIZED && m_Profile.m_eType != AROR_EForceType.MECHANIZED)
			return false;
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(passenger) > 0)
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(passenger);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
			return false;
		if (!access.IsInCompartment() || access.IsGettingOut() || access.IsSwitchingSeatsAnim())
			return false;
		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot || slot.GetVehicle() != m_Vehicle)
			return false;
		Physics physics = m_Vehicle.GetPhysics();
		if (!physics || physics.GetVelocity().Length() > 0.5)
			return false;
		vector exitTransform[4];
		passenger.GetTransform(exitTransform);
		if (!access.FindSuitableTeleportLocation(exitTransform))
		{
			if (m_iExitAttempts % 10 == 0)
				Print(string.Format("[AROR] %1 package=%2 recovery waiting: native exit clearance failed for seat %3", m_sOperationId, m_iPackageIndex + 1, slot.GetCompartmentSlotID()), LogLevel.NORMAL);
			return false;
		}
		vector position = exitTransform[3];
		if (vector.DistanceXZ(position, m_Vehicle.GetOrigin()) > 12)
		{
			if (m_iExitAttempts % 10 == 0)
				Print(string.Format("[AROR] %1 package=%2 recovery waiting: native exit too far from hull at %3", m_sOperationId, m_iPackageIndex + 1, position), LogLevel.NORMAL);
			return false;
		}
		float surface = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		if (Math.AbsFloat(position[1] - surface) > 2)
		{
			if (m_iExitAttempts % 10 == 0)
				Print(string.Format("[AROR] %1 package=%2 recovery waiting: native exit height unsuitable at %3 surface=%4", m_sOperationId, m_iPackageIndex + 1, position, surface), LogLevel.NORMAL);
			return false;
		}
		if (!access.GetOutVehicle_NoDoor(exitTransform, false, false, true))
			return false;
		Print(string.Format("[AROR] %1 package=%2 recovered stuck passenger through native cleared exit at %3", m_sOperationId, m_iPackageIndex + 1, position), LogLevel.NORMAL);
		return true;
	}

	//! One accepted exit per call; caller verifies physical completion and retries.
	bool RequestDismount_S(bool allowGroundRecovery = false)
	{
		if (!Replication.IsServer())
			return false;
		bool accepted = true;
		int blocked;
		int pending;
		bool issued;
		m_iExitAttempts++;
		foreach (IEntity passenger : m_aPassengers)
		{
			if (!passenger)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(passenger);
			if (character && character.GetCharacterController() && character.GetCharacterController().IsDead())
				continue;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(passenger.FindComponent(CompartmentAccessComponent));
			if (!access)
			{
				accepted = false;
				blocked++;
				continue;
			}
			if (access.IsInCompartment() || access.IsGettingOut())
				pending++;
			if (access.IsInCompartment() && !access.IsGettingOut() && !access.IsSwitchingSeatsAnim() && !issued)
			{
				BaseCompartmentSlot slot = access.GetCompartment();
				array<int> doors = {};
				if (slot)
					slot.GetAvailableDoorIndices(doors);
				bool exiting;
				foreach (int candidate : doors)
				{
					if (access.CanGetOutVehicleViaDoor(candidate) && access.GetOutVehicle(EGetOutType.ANIMATED, candidate, ECloseDoorAfterActions.LEAVE_OPEN, false))
					{
						exiting = true;
						break;
					}
				}
				if (!exiting && allowGroundRecovery)
					exiting = RecoverPassengerExit_S(passenger, access);
				if (exiting)
					issued = true;
				else
				{
					// One obstructed exit must not prevent the rest of the squad unloading.
					accepted = false;
					blocked++;
				}
			}
		}
		m_bDismountRequested = accepted;
		if (pending > 0 && (m_iExitAttempts == 1 || m_iExitAttempts % 10 == 0))
			Print(string.Format("[AROR] %1 package=%2 passenger exit: pending=%3 blocked/rejected=%4 attempt=%5", m_sOperationId, m_iPackageIndex + 1, pending, blocked, m_iExitAttempts), LogLevel.NORMAL);
		return accepted;
	}

	//! Resume an abandoned driver seat during bounded ingress recovery, using nearby AI crew only.
	bool RestoreNearbyDriver_S()
	{
		if (!Replication.IsServer() || !m_Vehicle || !m_CrewGroup || m_CrewGroup.GetPlayerCount() > 0)
			return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || !usage.CanBePiloted())
			return false;
		BaseCompartmentSlot driver = usage.GetPilotCompartmentSlot();
		if (!driver || driver.IsOccupied())
			return false;
		Physics physics = m_Vehicle.GetPhysics();
		if (!physics || physics.GetVelocity().Length() > 0.5)
			return false;
		array<AIAgent> agents = {};
		m_CrewGroup.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			ChimeraCharacter crew = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!crew || !crew.GetCharacterController() || crew.GetCharacterController().IsDead())
				continue;
			if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(crew) > 0 || vector.DistanceXZ(crew.GetOrigin(), m_Vehicle.GetOrigin()) > 15)
				continue;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(crew.FindComponent(CompartmentAccessComponent));
			if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut())
				continue;
			if (Seat(crew, driver))
				return true;
		}
		return false;
	}

	bool HasDismountedInfantry()
	{
		foreach (IEntity passenger : m_aPassengers)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(passenger);
			if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
				continue;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(passenger.FindComponent(CompartmentAccessComponent));
			if (access && !access.IsInCompartment() && !access.IsGettingOut())
				return true;
		}
		return false;
	}

	bool ArePassengersOut()
	{
		foreach (IEntity passenger : m_aPassengers)
		{
			if (!passenger)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(passenger);
			if (character && character.GetCharacterController() && character.GetCharacterController().IsDead())
				continue;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(passenger.FindComponent(CompartmentAccessComponent));
			if (access && (access.IsInCompartment() || access.IsGettingOut()))
				return false;
		}
		return true;
	}

	override void Cleanup_S()
	{
		if (!Replication.IsServer())
			return;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
			operations.GetQRFFinance().Observe_S(m_Funding);
		if (m_TownSearch)
			m_TownSearch.Cleanup_S();
		m_TownSearch = null;
		CountLivingMembers();
		bool preserveVehicle;
		if (m_Vehicle)
		{
			BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
			array<BaseCompartmentSlot> slots = {};
			if (compartments)
				compartments.GetCompartments(slots);
			foreach (BaseCompartmentSlot slot : slots)
			{
				if (slot && slot.GetOccupant() && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(slot.GetOccupant()) > 0)
					preserveVehicle = true;
			}
		}
		if (m_CrewGroup && m_CrewGroup != m_Group)
		{
			m_CrewGroup.GetOnAgentAdded().Remove(OnAgentAdded);
			if (m_CrewWaypoint)
				m_CrewGroup.RemoveWaypoint(m_CrewWaypoint);
			if (m_CrewGroup.GetPlayerCount() == 0)
				SCR_EntityHelper.DeleteEntityAndChildren(m_CrewGroup);
		}
		if (m_CrewWaypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_CrewWaypoint);
		super.Cleanup_S();
		if (m_Vehicle && !preserveVehicle)
		{
			if (m_Funding)
				m_Funding.Retire_S(m_Vehicle);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
		}
		m_Vehicle = null;
		m_CrewGroup = null;
		m_CrewWaypoint = null;
		m_aPassengers.Clear();
	}
}
