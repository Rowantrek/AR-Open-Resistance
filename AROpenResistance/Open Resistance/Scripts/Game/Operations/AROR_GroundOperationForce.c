class AROR_GroundOperationForce : AROR_OperationForce
{
	ref AROR_ForceProfile m_Profile;
	SCR_AIGroup m_CrewGroup;
	IEntity m_Vehicle;
	//! The hull was picked from the faction preset rather than authored on the profile
	bool m_bPresetVehicle;
	//! Crew for this dispatch: the profile's count, raised to the hull's driver and turret seats when the vehicle was
	//! picked from the faction preset (-1: use the profile's count)
	int m_iCrewCount = -1;
	AIWaypoint m_CrewWaypoint;
	bool m_bMounted;
	bool m_bMountRequested;
	int m_iMountWaitTicks;
	protected bool m_bBoardingWakeRequested;
	protected int m_iSeatAttempts;
	protected int m_iSeatQueueResets;
	protected int m_iSeatBusy;
	protected string m_sSeatRejection;
	bool m_bDismountRequested;
	bool m_bDismounted;
	bool m_bTacticalDropStarted;
	int m_iTacticalExitDeadline;
	int m_iTacticalExitStarted;
	int m_iTacticalExitRetries;
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
	float m_fTacticalBestDistance = -1;
	int m_iTacticalLastAdvance;
	ref AROR_VehicleTownSearch m_TownSearch;
	vector m_vApproachPosition;
	// Attack staging: the package waits at a road rally point until the assault is released
	bool m_bTacticalStaging;
	bool m_bTacticalStaged;
	vector m_vStagingPosition;
	//! A planned road dismount may be outside the security area; infantry must still walk in.
	float m_fTacticalDeliveryRadius;
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
		if (!Replication.IsServer() || !entity || !slot || slot.IsOccupied() || slot.GetOccupant())
			return false;
		if (!slot.IsCompartmentAccessible() || (slot.IsReserved() && !slot.IsReservedBy(entity)))
			return false;
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity) > 0)
			return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
		if (!access)
			return false;
		m_iSeatAttempts++;
		// Spawn origins can be outside the player simulation bubble. Execute the
		// initial teleport even when the actor is paused, rather than queueing forever.
		bool initialDeployment = !m_bMounted && !m_bDismounted;
		bool accepted;
		if (initialDeployment)
		{
			// As the base game's own move-in (SCR_CompartmentAccessComponent.MoveInVehicle): the slot's own entity,
			// since turret and gunner seats live on child entities, and no door action on a teleport. Placing into the
			// vehicle with a door close was refused for every seat of vehicles spawned away from players.
			IEntity seatEntity = slot.GetOwner();
			if (!seatEntity)
				seatEntity = m_Vehicle;
			accepted = access.GetInVehicle(seatEntity, slot, true, -1, ECloseDoorAfterActions.INVALID, true);
			// Still refused: the base game's own call, which passes performWhenPaused only while game time is paused
			if (!accepted)
				accepted = access.GetInVehicle(seatEntity, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
		}
		else
		{
			accepted = access.GetInVehicle(m_Vehicle, slot, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
		}
		// Report the first native rejection through the existing paced retry log,
		// without resetting actions or relaxing occupancy/clearance requirements.
		if (!accepted && m_sSeatRejection.IsEmpty())
			m_sSeatRejection = string.Format("canEnter=%1 entering=%2 exiting=%3 inside=%4 initialDeployment=%5", access.CanGetInVehicle(m_Vehicle), access.IsGettingIn(), access.IsGettingOut(), access.IsInCompartment(), initialDeployment);
		return accepted;
	}

	//! Wake existing members without changing their roster, faction or asset accounts.
	//! Activation observes the engine's world limit (1.8 API); the short LOD lease expires naturally.
	static void WakeBoardingGroup_S(SCR_AIGroup group)
	{
		if (!Replication.IsServer() || !group || group.GetPlayerCount() > 0)
			return;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || !AROR_ForceSpawner.IsAlive(agent.GetControlledEntity()))
				continue;
			if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(agent.GetControlledEntity()) > 0)
				continue;
			agent.PreventMaxLOD(10);
			agent.ActivateAI(true);
		}
	}

	//! Give activation a simulation tick before the first compartment request.
	bool PrepareBoarding_S(bool refresh = false)
	{
		if (!Replication.IsServer() || !m_CrewGroup || !m_Group || m_CrewGroup.GetPlayerCount() > 0 || m_Group.GetPlayerCount() > 0)
			return false;
		if (m_bBoardingWakeRequested && !refresh)
			return true;
		WakeBoardingGroup_S(m_CrewGroup);
		if (m_Group != m_CrewGroup)
			WakeBoardingGroup_S(m_Group);
		m_bBoardingWakeRequested = true;
		return false;
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
			// Retained own casualties cannot capture their hull when native affiliation clears.
			if (!AROR_ForceSpawner.IsAlive(occupant))
				continue;
			FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(occupant.FindComponent(FactionAffiliationComponent));
			if (!faction || faction.GetAffiliatedFactionKey() != m_sFactionKey)
				return false;
		}
		return true;
	}

	//! Armed cargo-style seats (including mounted weapons) also require dedicated crew.
	//! Native ExtBaseCompartmentSlot.GetAttachedTurret is inherited by BaseCompartmentSlot.
	protected bool IsRequiredGunnerSeat(BaseCompartmentSlot slot)
	{
		return slot && (TurretCompartmentSlot.Cast(slot) || slot.GetAttachedTurret());
	}

	//! Crew used for this dispatch (see m_iCrewCount)
	int GetCrewCount()
	{
		if (m_iCrewCount > 0)
			return m_iCrewCount;
		return m_Profile.m_iCrewCount;
	}

	//! Driver plus required gunner seats of the spawned hull, or -1 if the hull cannot be piloted
	int CountRequiredCrewSeats_S()
	{
		if (!m_Vehicle)
			return -1;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage || !usage.GetPilotCompartmentSlot())
			return -1;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		int seats = 1;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && slot != usage.GetPilotCompartmentSlot() && IsRequiredGunnerSeat(slot))
				seats++;
		}
		return seats;
	}

	//! Accessible passenger seats of the spawned hull (crew seats excluded)
	int CountCargoSeats_S()
	{
		if (!m_Vehicle)
			return 0;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage)
			return 0;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		int seats;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && slot != usage.GetPilotCompartmentSlot() && !IsRequiredGunnerSeat(slot) && slot.IsCompartmentAccessible() && CargoCompartmentSlot.Cast(slot))
				seats++;
		}
		return seats;
	}

	//! Freeze the native hull's passenger roster before either group requests spawning.
	//! Repeat the authored infantry mix; never turn spare crew into dismount infantry.
	bool PreparePassengerRoster_S()
	{
		if (!Replication.IsServer() || !m_Profile || !m_Vehicle || !m_Group || !m_CrewGroup)
			return false;
		if (!m_Profile.m_bFillPassengerSeats)
			return true;
		if (m_Group == m_CrewGroup || !m_Group.m_aUnitPrefabSlots || m_Group.m_aUnitPrefabSlots.IsEmpty())
			return false;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage || !usage.GetPilotCompartmentSlot())
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		bool airborne = m_Profile.m_eType == AROR_EForceType.AIRBORNE;
		int transportCrew = GetCrewCount();
		int escortCrew;
		if (airborne)
		{
			transportCrew = 1;
			escortCrew = m_Profile.m_iEscortCount;
		}
		int crewSeats = 1;
		int cargoSeats;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot || slot == usage.GetPilotCompartmentSlot())
				continue;
			if (IsRequiredGunnerSeat(slot))
			{
				if (!airborne)
					crewSeats++;
			}
			else if (slot.IsCompartmentAccessible() && CargoCompartmentSlot.Cast(slot))
				cargoSeats++;
		}
		if (transportCrew < crewSeats)
			return false;
		int passengers = cargoSeats - (transportCrew - crewSeats);
		// A preset-picked hull can seat more than the authored one: fill only within the reserved personnel budget
		if (m_bPresetVehicle)
			passengers = Math.Min(passengers, m_iReservedMembers - transportCrew - escortCrew);
		if (passengers < 1 || passengers + transportCrew + escortCrew > m_iReservedMembers)
			return false;
		array<ResourceName> infantryMix = {};
		infantryMix.Copy(m_Group.m_aUnitPrefabSlots);
		m_Group.m_aUnitPrefabSlots.Clear();
		for (int i = 0; i < passengers; i++)
			m_Group.m_aUnitPrefabSlots.Insert(infantryMix[i % infantryMix.Count()]);
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
			if (!slot || crewSeats.Contains(slot))
				continue;
			// Keep required seats even when blocked/occupied: Seat must reject an unsafe
			// assignment, rather than silently deploying their crew as passengers.
			if (IsRequiredGunnerSeat(slot))
				crewSeats.Insert(slot);
			else if (slot.IsCompartmentAccessible() && CargoCompartmentSlot.Cast(slot))
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
			if (!crew[i] || !crew[i].GetControlledEntity())
				return false;
			if (!IsSeatedInOwnVehicle(crew[i].GetControlledEntity()))
				Seat(crew[i].GetControlledEntity(), crewSeat);
		}
		for (int p = 0; p < infantry.Count(); p++)
		{
			IEntity passenger;
			if (infantry[p])
				passenger = infantry[p].GetControlledEntity();
			if (!passenger)
				return false;
			if (!m_aPassengers.Contains(passenger))
				m_aPassengers.Insert(passenger);
			if (!IsSeatedInOwnVehicle(passenger))
				Seat(passenger, cargoSeats[extraCrew + p]);
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
		return GetMountRejection().IsEmpty();
	}

	//! One audit supplies both dispatch readiness and paced retry diagnostics.
	string GetMountRejection()
	{
		if (!m_bMountRequested || !m_Vehicle || !m_CrewGroup || m_CrewGroup.GetPlayerCount() > 0)
			return "mount not requested, vehicle/crew missing, or player-controlled crew group";
		if (m_Group && m_Group.GetPlayerCount() > 0)
			return "player-controlled infantry group";
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || !usage.GetPilotCompartmentSlot() || !usage.GetPilotCompartmentSlot().GetOccupant())
			return "driver seat unavailable or empty";
		IEntity driver = usage.GetPilotCompartmentSlot().GetOccupant();
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(driver) > 0)
			return "driver controlled by a player";
		ChimeraCharacter driverCharacter = ChimeraCharacter.Cast(driver);
		if (!driverCharacter || !driverCharacter.GetCharacterController() || driverCharacter.GetCharacterController().IsDead())
			return "driver missing controller or dead";
		bool ownDriver;
		array<AIAgent> crew = {};
		m_CrewGroup.GetAgents(crew);
		foreach (AIAgent agent : crew)
		{
			if (!agent || !AROR_ForceSpawner.IsAlive(agent.GetControlledEntity()))
				return "assigned crew missing or dead";
			if (!agent || !IsSeatedInOwnVehicle(agent.GetControlledEntity()))
				return "assigned crew not settled in this vehicle";
			if (agent.GetControlledEntity() == driver)
				ownDriver = true;
		}
		foreach (IEntity passenger : m_aPassengers)
		{
			if (!AROR_ForceSpawner.IsAlive(passenger))
				return "assigned passenger missing or dead";
			if (!IsSeatedInOwnVehicle(passenger))
				return "assigned passenger not settled in this vehicle";
		}
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return "compartment manager missing";
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!IsRequiredGunnerSeat(slot))
				continue;
			IEntity gunner = slot.GetOccupant();
			ChimeraCharacter gunnerCharacter = ChimeraCharacter.Cast(gunner);
			if (!slot.IsCompartmentAccessible() || !gunnerCharacter || !gunnerCharacter.GetCharacterController() || gunnerCharacter.GetCharacterController().IsDead())
				return string.Format("gunner seat %1 inaccessible, empty or occupant dead", slot.GetCompartmentSlotID());
			if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(gunner) > 0 || !IsSeatedInOwnVehicle(gunner))
				return string.Format("gunner seat %1 player-controlled or occupant not settled", slot.GetCompartmentSlotID());
			CompartmentAccessComponent gunnerAccess = CompartmentAccessComponent.Cast(gunner.FindComponent(CompartmentAccessComponent));
			if (!gunnerAccess || gunnerAccess.GetCompartment() != slot)
				return string.Format("gunner seat %1 compartment identity mismatch", slot.GetCompartmentSlotID());
			bool ownGunner = false;
			foreach (AIAgent crewAgent : crew)
			{
				if (crewAgent && crewAgent.GetControlledEntity() == gunner)
					ownGunner = true;
			}
			if (!ownGunner)
				return string.Format("gunner seat %1 occupant outside assigned crew", slot.GetCompartmentSlotID());
		}
		if (!ownDriver)
			return "driver outside assigned crew";
		return string.Empty;
	}

	//! A native seat teleport can be dropped without failing the request. Re-request a free seat for every
	//! living member not yet seated in this vehicle; returns what was still missing, for the log.
	string ReseatMissing_S(bool recoverStale = false)
	{
		m_iSeatAttempts = 0;
		m_iSeatQueueResets = 0;
		m_iSeatBusy = 0;
		m_sSeatRejection = "";
		if (!Replication.IsServer() || !m_Vehicle || !m_CrewGroup)
			return "no vehicle or crew group";
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage || !usage.GetPilotCompartmentSlot())
			return "no seat manager";
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		array<BaseCompartmentSlot> crewSeats = {usage.GetPilotCompartmentSlot()};
		array<BaseCompartmentSlot> cargoSeats = {};
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot || !slot.IsCompartmentAccessible() || crewSeats.Contains(slot))
				continue;
			if (IsRequiredGunnerSeat(slot))
				crewSeats.Insert(slot);
			else if (CargoCompartmentSlot.Cast(slot))
				cargoSeats.Insert(slot);
		}
		int missingCrew, missingPassengers, boarding;
		array<AIAgent> crew = {};
		m_CrewGroup.GetAgents(crew);
		int crewIndex;
		foreach (AIAgent agent : crew)
		{
			if (!agent)
				continue;
			IEntity member = agent.GetControlledEntity();
			BaseCompartmentSlot assigned;
			if (crewIndex < crewSeats.Count())
				assigned = crewSeats[crewIndex];
			else if (crewIndex - crewSeats.Count() < cargoSeats.Count())
				assigned = cargoSeats[crewIndex - crewSeats.Count()];
			crewIndex++;
			if (IsSeatedInOwnVehicle(member))
				continue;
			missingCrew++;
			array<BaseCompartmentSlot> assignedSeats = {assigned};
			if (ReseatMember(member, assignedSeats, null, recoverStale))
				boarding++;
		}
		int passengerIndex = Math.Max(0, crew.Count() - crewSeats.Count());
		foreach (IEntity passenger : m_aPassengers)
		{
			BaseCompartmentSlot passengerSeat;
			if (passengerIndex < cargoSeats.Count())
				passengerSeat = cargoSeats[passengerIndex];
			passengerIndex++;
			if (IsSeatedInOwnVehicle(passenger))
				continue;
			missingPassengers++;
			array<BaseCompartmentSlot> passengerSeats = {passengerSeat};
			if (ReseatMember(passenger, passengerSeats, null, recoverStale))
				boarding++;
		}
		int requiredGunners, seatedGunners;
		foreach (BaseCompartmentSlot gunnerSeat : crewSeats)
		{
			if (!IsRequiredGunnerSeat(gunnerSeat))
				continue;
			requiredGunners++;
			if (gunnerSeat.GetOccupant())
				seatedGunners++;
		}
		string pilot = "seated";
		if (!usage.GetPilotCompartmentSlot().GetOccupant())
			pilot = "empty";
		return string.Format("driver %1, unseated crew=%2 passengers=%3, accepted=%4 attempted=%5 queueResets=%6 busy=%7, occupied gunner seats=%8/%9", pilot, missingCrew, missingPassengers, boarding, m_iSeatAttempts, m_iSeatQueueResets, m_iSeatBusy, seatedGunners, requiredGunners) + "; native rejection=" + m_sSeatRejection + "; readiness=" + GetMountRejection();
	}

	//! Compatibility entry point: never replace an owned soldier to work around native boarding rejection.
	//! Bounded activation/reseating now preserves the selected units and their finance accounts.
	int ReplaceUnseated_S()
	{
		return 0;
	}

	//! Seats a living, idle member in the first free seat of the preferred list, then of the fallback list
	protected bool ReseatMember(IEntity member, array<BaseCompartmentSlot> preferred, array<BaseCompartmentSlot> fallback, bool recoverStale = false)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
			return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(member.FindComponent(CompartmentAccessComponent));
		if (recoverStale && access && access.IsGettingIn())
			RecoverStaleBoarding_S(member, access);
		if (!access || access.IsGettingIn() || access.IsGettingOut() || access.IsInCompartment())
		{
			m_iSeatBusy++;
			return false;
		}
		foreach (BaseCompartmentSlot seat : preferred)
		{
			if (seat && seat.IsCompartmentAccessible() && !seat.IsOccupied() && !seat.GetOccupant() && (!seat.IsReserved() || seat.IsReservedBy(member)))
				return Seat(member, seat);
		}
		if (!fallback)
			return false;
		foreach (BaseCompartmentSlot seat : fallback)
		{
			if (seat && seat.IsCompartmentAccessible() && !seat.IsOccupied() && !seat.GetOccupant() && (!seat.IsReserved() || seat.IsReservedBy(member)))
				return Seat(member, seat);
		}
		return false;
	}

	//! Reset only a pre-deployment, stuck boarding action belonging to this package.
	//! Native API: CompartmentAccessComponent.InterruptVehicleActionQueue(true, true, true).
	//! https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceCompartmentAccessComponent.html
	protected bool RecoverStaleBoarding_S(IEntity member, CompartmentAccessComponent access)
	{
		if (!Replication.IsServer() || !m_bMountRequested || m_bMounted || m_bDismountRequested || m_bDismounted)
			return false;
		if (!member || !access || !m_Vehicle || !m_Profile || !m_aMembers.Contains(member))
			return false;
		if (m_Profile.m_eType == AROR_EForceType.AIRBORNE || GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(member) > 0)
			return false;
		if (!m_CrewGroup || m_CrewGroup.GetPlayerCount() > 0 || (m_Group && m_Group.GetPlayerCount() > 0))
			return false;
		if (!access.IsGettingIn() || access.IsInCompartment() || access.IsGettingOut())
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
			return false;
		Physics physics = m_Vehicle.GetPhysics();
		if (!physics || physics.GetVelocity().Length() > 0.5)
			return false;
		BaseCompartmentSlot current = access.GetCompartment();
		if (current && current.GetVehicle() != m_Vehicle)
			return false;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return false;
		// Only this member's empty reservation may be released; never eject occupants
		// or take another member's claim. The next request still verifies actual seats.
		access.InterruptVehicleActionQueue(true, true, true);
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && !slot.GetOccupant() && slot.IsReservedBy(member))
				slot.SetReserved(null);
		}
		m_iSeatQueueResets++;
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
		if (m_Profile.m_eType != AROR_EForceType.MOTORIZED && m_Profile.m_eType != AROR_EForceType.MECHANIZED && !m_Profile.m_bFillPassengerSeats)
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
			else
				m_bPackageQueuesPreserved = true;
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
