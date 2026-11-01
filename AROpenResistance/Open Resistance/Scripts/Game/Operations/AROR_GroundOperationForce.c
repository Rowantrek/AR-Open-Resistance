class AROR_GroundOperationForce : AROR_OperationForce
{
	ref AROR_ForceProfile m_Profile;
	//! Infantry unit types in slot order (faction preset units; see AROR_FactionPresetsComponent.FillGroup_S)
	ref array<AROR_EUnitType> m_aInfantryTypes = {};
	SCR_AIGroup m_CrewGroup;
	IEntity m_Vehicle;
	AIWaypoint m_CrewWaypoint;
	bool m_bMounted;
	bool m_bMountRequested;
	int m_iMountWaitTicks;
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
			m_sSeatRejection = string.Format("canEnter=%1 entering=%2 exiting=%3 inside=%4 paused=%5", access.CanGetInVehicle(m_Vehicle), access.IsGettingIn(), access.IsGettingOut(), access.IsInCompartment(), initialDeployment);
		return accepted;
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

	//! Armed cargo-style seats (including mounted weapons) also require dedicated crew.
	//! Native ExtBaseCompartmentSlot.GetAttachedTurret is inherited by BaseCompartmentSlot.
	protected bool IsRequiredGunnerSeat(BaseCompartmentSlot slot)
	{
		return slot && (TurretCompartmentSlot.Cast(slot) || slot.GetAttachedTurret());
	}

	//! Freeze the native hull's passenger roster before either group requests spawning.
	//! Repeat the authored infantry mix; never turn spare crew into dismount infantry.
	bool PreparePassengerRoster_S()
	{
		if (!Replication.IsServer() || !m_Profile || !m_Vehicle || !m_Group || !m_CrewGroup)
			return false;
		if (!m_Profile.m_bFillPassengerSeats)
			return true;
		if (m_Group == m_CrewGroup || m_aInfantryTypes.IsEmpty())
			return false;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage || !usage.GetPilotCompartmentSlot())
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		bool airborne = m_Profile.m_eType == AROR_EForceType.AIRBORNE;
		int transportCrew = m_Profile.m_iCrewCount;
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
		if (passengers < 1 || passengers + transportCrew + escortCrew > m_iReservedMembers)
			return false;
		array<AROR_EUnitType> infantryMix = {};
		infantryMix.Copy(m_aInfantryTypes);
		m_aInfantryTypes.Clear();
		for (int i = 0; i < passengers; i++)
			m_aInfantryTypes.Insert(infantryMix[i % infantryMix.Count()]);
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		return presets && presets.FillGroup_S(m_Group, m_Profile.m_eRole, m_aInfantryTypes) == passengers;
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
			else if (slot.IsCompartmentAccessible() && !slot.IsOccupied() && CargoCompartmentSlot.Cast(slot))
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
		if (!m_bMountRequested || !m_Vehicle || !m_CrewGroup || m_CrewGroup.GetPlayerCount() > 0)
			return false;
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!usage || !usage.GetPilotCompartmentSlot() || !usage.GetPilotCompartmentSlot().GetOccupant())
			return false;
		IEntity driver = usage.GetPilotCompartmentSlot().GetOccupant();
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(driver) > 0)
			return false;
		ChimeraCharacter driverCharacter = ChimeraCharacter.Cast(driver);
		if (!driverCharacter || !driverCharacter.GetCharacterController() || driverCharacter.GetCharacterController().IsDead())
			return false;
		bool ownDriver;
		array<AIAgent> crew = {};
		m_CrewGroup.GetAgents(crew);
		foreach (AIAgent agent : crew)
		{
			if (!agent || !IsSeatedInOwnVehicle(agent.GetControlledEntity()))
				return false;
			if (agent.GetControlledEntity() == driver)
				ownDriver = true;
		}
		foreach (IEntity passenger : m_aPassengers)
		{
			if (!IsSeatedInOwnVehicle(passenger))
				return false;
		}
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!IsRequiredGunnerSeat(slot))
				continue;
			IEntity gunner = slot.GetOccupant();
			ChimeraCharacter gunnerCharacter = ChimeraCharacter.Cast(gunner);
			if (!slot.IsCompartmentAccessible() || !gunnerCharacter || !gunnerCharacter.GetCharacterController() || gunnerCharacter.GetCharacterController().IsDead())
				return false;
			if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(gunner) > 0 || !IsSeatedInOwnVehicle(gunner))
				return false;
			CompartmentAccessComponent gunnerAccess = CompartmentAccessComponent.Cast(gunner.FindComponent(CompartmentAccessComponent));
			if (!gunnerAccess || gunnerAccess.GetCompartment() != slot)
				return false;
			bool ownGunner = false;
			foreach (AIAgent crewAgent : crew)
			{
				if (crewAgent && crewAgent.GetControlledEntity() == gunner)
					ownGunner = true;
			}
			if (!ownGunner)
				return false;
		}
		return ownDriver;
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
		return string.Format("driver %1, unseated crew=%2 passengers=%3, accepted=%4 attempted=%5 queueResets=%6 busy=%7, occupied gunner seats=%8/%9", pilot, missingCrew, missingPassengers, boarding, m_iSeatAttempts, m_iSeatQueueResets, m_iSeatBusy, seatedGunners, requiredGunners) + "; native rejection=" + m_sSeatRejection;
	}

	//! Last resort for the first seating. Moving a character into a seat is refused for AI the engine has not woken
	//! (packages spawned about a kilometre or more from every player), so each member still standing beside the
	//! hull is replaced by a new character of the same unit type spawned straight into that seat
	//! (BaseCompartmentSlot.SpawnCharacterInCompartment) and dressed from its faction preset loadout, in the same
	//! group so orders, faction and loss tracking carry on. Seats are assigned as in ReseatMissing_S.
	//! Returns how many were replaced.
	int ReplaceUnseated_S()
	{
		if (!Replication.IsServer() || !m_Vehicle || !m_CrewGroup || m_bMounted || m_bDismounted)
			return 0;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(BaseCompartmentManagerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(m_Vehicle.FindComponent(SCR_AIVehicleUsageComponent));
		if (!compartments || !usage || !usage.GetPilotCompartmentSlot())
			return 0;
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
		int replaced;
		array<AIAgent> crew = {};
		m_CrewGroup.GetAgents(crew);
		foreach (int crewIndex, AIAgent agent : crew)
		{
			if (!agent)
				continue;
			BaseCompartmentSlot assigned;
			if (crewIndex < crewSeats.Count())
				assigned = crewSeats[crewIndex];
			else if (crewIndex - crewSeats.Count() < cargoSeats.Count())
				assigned = cargoSeats[crewIndex - crewSeats.Count()];
			if (ReplaceIntoSeat(agent.GetControlledEntity(), assigned, m_CrewGroup, m_Profile.m_eCrewType))
				replaced++;
		}
		int passengerIndex = Math.Max(0, crew.Count() - crewSeats.Count());
		for (int p = 0; p < m_aPassengers.Count(); p++)
		{
			BaseCompartmentSlot passengerSeat;
			if (passengerIndex < cargoSeats.Count())
				passengerSeat = cargoSeats[passengerIndex];
			passengerIndex++;
			AROR_EUnitType passengerType = AROR_EUnitType.RIFLEMAN;
			if (!m_aInfantryTypes.IsEmpty())
				passengerType = m_aInfantryTypes[p % m_aInfantryTypes.Count()];
			IEntity replacement = ReplaceIntoSeatEntity(m_aPassengers[p], passengerSeat, m_Group, passengerType);
			if (replacement)
			{
				m_aPassengers[p] = replacement;
				replaced++;
			}
		}
		return replaced;
	}

	protected bool ReplaceIntoSeat(IEntity member, BaseCompartmentSlot seat, SCR_AIGroup group, AROR_EUnitType type)
	{
		return ReplaceIntoSeatEntity(member, seat, group, type) != null;
	}

	//! The seated replacement, or null when the member is already seated or the seat cannot be filled this way
	protected IEntity ReplaceIntoSeatEntity(IEntity member, BaseCompartmentSlot seat, SCR_AIGroup group, AROR_EUnitType type)
	{
		if (!member || !seat || !group || IsSeatedInOwnVehicle(member))
			return null;
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(member) > 0)
			return null;
		ChimeraCharacter character = ChimeraCharacter.Cast(member);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
			return null;
		if (seat.IsOccupied() || seat.GetOccupant() || !seat.IsCompartmentAccessible())
			return null;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets || !m_Profile)
			return null;
		ResourceName prefab = presets.GetSpawnPrefab(m_Profile.m_eRole, type);
		if (prefab.IsEmpty())
			return null;
		AIGroup target = group;
		IEntity spawned = seat.SpawnCharacterInCompartment(prefab, target);
		if (!spawned)
			return null;
		presets.OutfitLater_S(spawned, m_Profile.m_eRole, type);
		// Keep the replacement in this package's group whatever group the engine put it in
		AIControlComponent control = AIControlComponent.Cast(spawned.FindComponent(AIControlComponent));
		if (control && control.GetControlAIAgent())
		{
			AIAgent spawnedAgent = control.GetControlAIAgent();
			AIGroup parent = spawnedAgent.GetParentGroup();
			if (parent != group)
			{
				if (parent)
					parent.RemoveAgent(spawnedAgent);
				group.AddAgent(spawnedAgent);
			}
		}
		SCR_EntityHelper.DeleteEntityAndChildren(member);
		return spawned;
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
