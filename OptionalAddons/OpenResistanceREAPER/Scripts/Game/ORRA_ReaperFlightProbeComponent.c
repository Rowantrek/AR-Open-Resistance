[ComponentEditorProps(category: "OpenResistanceREAPER/Tests", description: "Opt-in server transport flight/landing probe. Place only in a disposable test world with a clear departure and LZ.")]
class ORRA_ReaperFlightProbeComponentClass : ScriptComponentClass
{
}

//! First backend proof only: no passenger delivery, gunship orders or QRF dispatch claims.
class ORRA_ReaperFlightProbeComponent : ScriptComponent
{
	[Attribute("0", desc: "Enable this isolated probe. Configure and inspect all route points first.")]
	protected bool m_bEnabled;
	[Attribute("0 0 0", UIWidgets.Coords, desc: "Clear transport departure point; terrain height is applied")]
	protected vector m_vDeparture;
	[Attribute("0 0 0", UIWidgets.Coords, desc: "Approach point at least 100m from departure and landing zone; REAPER owns flight altitude")]
	protected vector m_vApproach;
	[Attribute("0 0 0", UIWidgets.Coords, desc: "Clear inspected landing zone at least 200m from departure")]
	protected vector m_vLanding;
	[Attribute("180", desc: "Deadline in server ticks for each flight stage", params: "30 600 1")]
	protected int m_iStageTimeout;
	[Attribute("{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et", UIWidgets.ResourceNamePicker, desc: "Transport", params: "et")]
	protected ResourceName m_sTransport;
	[Attribute("{2B5FFD713CEF5848}Prefabs/Characters/REAPER_US_Pilot.et.et", UIWidgets.ResourceNamePicker, desc: "Installed REAPER pilot (verified double .et extension)", params: "et")]
	protected ResourceName m_sPilot;
	[Attribute("{F6FB686B76E77E7D}Prefabs/AI/Waypoints/REAPER_AiHelicopterMoveWaypoint.et", UIWidgets.ResourceNamePicker, desc: "REAPER Move waypoint", params: "et")]
	protected ResourceName m_sMoveWaypoint;
	[Attribute("{6D8ADA4DF1A482C6}Prefabs/AI/Waypoints/REAPER_AiHelicopterLandWaypoint.et", UIWidgets.ResourceNamePicker, desc: "REAPER Land waypoint", params: "et")]
	protected ResourceName m_sLandWaypoint;

	protected ref AROR_GroundOperationForce m_Crew;
	protected IEntity m_Transport;
	protected AIWaypoint m_Order;
	protected vector m_vPreviousPosition;
	protected int m_iStage;
	protected int m_iStageTicks;
	protected int m_iSettledTicks;
	protected bool m_bSawAirborne;
	protected bool m_bHavePreviousPosition;
	protected string m_sFactionKey;

	protected IEntity SpawnResource(ResourceName resourceName, vector position)
	{
		Resource resource = Resource.Load(resourceName);
		if (!resource || !resource.IsValid())
			return null;
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		spawnParams.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
	}

	protected bool GiveOrder(ResourceName prefab, vector position)
	{
		IEntity entity = SpawnResource(prefab, position);
		AIWaypoint next = AIWaypoint.Cast(entity);
		if (!next)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return false;
		}
		ClearOrder();
		m_Order = next;
		m_Crew.m_Group.AddWaypoint(m_Order);
		m_iStageTicks = 0;
		return true;
	}

	protected void ClearOrder()
	{
		if (!m_Order)
			return;
		if (m_Crew && m_Crew.m_Group)
			m_Crew.m_Group.RemoveWaypoint(m_Order);
		SCR_EntityHelper.DeleteEntityAndChildren(m_Order);
		m_Order = null;
	}

	protected void StopProbe(string reason, bool failed)
	{
		GetGame().GetCallqueue().Remove(Tick_S);
		if (failed)
			Print(string.Format("[ORRA] Flight probe stopped: %1; inspect aircraft before ending Play", reason), LogLevel.ERROR);
		else
			Print(string.Format("[ORRA] Flight probe observation: %1", reason), LogLevel.NORMAL);
		// Leave aircraft and its current flight order intact for inspection. Deletion happens
		// only when the probe is removed/world ends; never report an operation as delivered.
	}

	protected bool Prepare_S()
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles || !roles.GetFaction(AROR_EFactionRole.OCCUPIERS))
			return false;
		m_sFactionKey = roles.GetFactionKey(AROR_EFactionRole.OCCUPIERS);
		m_Crew = new AROR_GroundOperationForce();
		m_Crew.m_sFactionKey = m_sFactionKey;
		m_Crew.m_sAssetFactionKey = "US";
		m_Transport = SpawnResource(m_sTransport, m_vDeparture);
		m_Crew.m_Vehicle = m_Transport;
		if (!m_Transport || !m_Crew.InitializeSpawnFaction_S(m_Transport))
		{
			StopProbe("Transport load or campaign-faction assignment failed", true);
			return false;
		}
		SCR_AIGroup.IgnoreSpawning(true);
		vector pilotPosition = m_vDeparture;
		pilotPosition[0] = pilotPosition[0] + 8;
		IEntity groupEntity = SpawnResource("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", pilotPosition);
		SCR_AIGroup.IgnoreSpawning(false);
		m_Crew.m_Group = SCR_AIGroup.Cast(groupEntity);
		m_Crew.m_CrewGroup = m_Crew.m_Group;
		if (!m_Crew.m_Group)
		{
			if (groupEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(groupEntity);
			StopProbe("Empty pilot group could not be created", true);
			return false;
		}
		if (!m_Crew.InitializeSpawnGroupFaction_S(m_Crew.m_Group))
		{
			StopProbe("Pilot group cannot use the Occupier campaign faction", true);
			return false;
		}
		m_Crew.m_Group.GetOnAgentAdded().Insert(m_Crew.OnAgentAdded);
		m_Crew.m_Group.SetCanDeleteIfNoPlayer(false);
		m_Crew.m_Group.SetPrivate(true);
		m_Crew.m_Group.SetPrivacyChangeable(false);
		m_Crew.m_Group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		m_Crew.m_Group.SetImportance(SCR_EAISpawnImportance.HIGH);
		if (!m_Crew.m_Group.m_aUnitPrefabSlots)
			m_Crew.m_Group.m_aUnitPrefabSlots = {};
		m_Crew.m_Group.m_aUnitPrefabSlots.Clear();
		m_Crew.m_Group.m_aUnitPrefabSlots.Insert(m_sPilot);
		m_Crew.m_Group.RequestSpawn(1);
		m_iStage = 1;
		m_iStageTicks = 0;
		Print(string.Format("[ORRA] Flight probe prepared: faction=%1 departure=%2 approach=%3 LZ=%4", m_sFactionKey, m_vDeparture, m_vApproach, m_vLanding), LogLevel.NORMAL);
		return true;
	}

	protected bool BoardPilot_S()
	{
		array<AIAgent> agents = {};
		m_Crew.m_Group.GetAgents(agents);
		if (agents.Count() != 1 || !agents[0])
			return false;
		IEntity pilot = agents[0].GetControlledEntity();
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(m_Transport.FindComponent(BaseCompartmentManagerComponent));
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
				return access.GetInVehicle(m_Transport, slot, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
		}
		return false;
	}

	protected void Tick_S()
	{
		if (!Replication.IsServer())
			return;
		m_iStageTicks++;
		if (m_iStageTicks > m_iStageTimeout)
		{
			StopProbe(string.Format("Stage %1 timed out (a waypoint is not proof of flight)", m_iStage), true);
			return;
		}
		if (m_iStage == 0)
		{
			Prepare_S();
			return;
		}
		if (!m_Transport || !m_Crew || !m_Crew.m_Group)
		{
			StopProbe("Transport or crew group was deleted", true);
			return;
		}
		DamageManagerComponent damage = DamageManagerComponent.Cast(m_Transport.FindComponent(DamageManagerComponent));
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(m_Transport.FindComponent(FactionAffiliationComponent));
		int living = m_Crew.CountLivingMembers();
		if ((damage && damage.IsDestroyed()) || !affiliation || affiliation.GetAffiliatedFactionKey() != m_sFactionKey || !m_Crew.HasCorrectMemberFactions())
		{
			StopProbe("Aircraft destroyed or aircraft/personnel faction mismatch", true);
			return;
		}
		if (m_iStage == 1)
		{
			if (living < 1 || !m_Crew.m_Group.IsExpandComplete())
				return;
			if (!BoardPilot_S() || !GiveOrder(m_sMoveWaypoint, m_vApproach))
			{
				StopProbe("Pilot seating or REAPER Move waypoint failed", true);
				return;
			}
			m_iStage = 2;
			Print("[ORRA] Pilot boarded; REAPER Move waypoint issued. Waiting for physical flight.", LogLevel.NORMAL);
			return;
		}
		if (living < 1)
		{
			StopProbe("Pilot is no longer alive", true);
			return;
		}
		vector position = m_Transport.GetOrigin();
		float height = position[1] - GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		float displacement = 0;
		if (m_bHavePreviousPosition)
			displacement = vector.Distance(position, m_vPreviousPosition);
		m_vPreviousPosition = position;
		m_bHavePreviousPosition = true;
		if (height > 10)
			m_bSawAirborne = true;
		if (m_iStage == 2 && m_bSawAirborne && vector.DistanceXZ(position, m_vApproach) <= 75)
		{
			if (!GiveOrder(m_sLandWaypoint, m_vLanding))
			{
				StopProbe("REAPER Land waypoint could not be issued", true);
				return;
			}
			m_iStage = 3;
			Print("[ORRA] Airborne approach observed; REAPER Land waypoint issued.", LogLevel.NORMAL);
		}
		if (m_iStage == 3)
		{
			if (height >= 0 && height <= 5 && displacement < 0.5 && vector.DistanceXZ(position, m_vLanding) <= 20)
				m_iSettledTicks++;
			else
				m_iSettledTicks = 0;
			if (m_iSettledTicks >= 5)
				StopProbe("Low, stable aircraft observed at LZ. Verify actual ground contact visually; no passenger delivery or QRF success claimed.", false);
		}
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode() || !Replication.IsServer() || !m_bEnabled)
			return;
		if (m_iStageTimeout < 30 || m_iStageTimeout > 600 || vector.DistanceXZ(m_vDeparture, m_vLanding) < 200 || vector.DistanceXZ(m_vApproach, m_vLanding) < 100 || vector.DistanceXZ(m_vDeparture, m_vApproach) < 100)
		{
			Print("[ORRA] Configure distinct, inspected departure/approach/LZ points and a 30-600 tick deadline before enabling the flight probe", LogLevel.ERROR);
			return;
		}
		GetGame().GetCallqueue().CallLater(Tick_S, 1000, true);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Tick_S);
		if (Replication.IsServer())
		{
			ClearOrder();
			// Reuse player-preserving vehicle/crew ownership cleanup, not ground driving logic.
			if (m_Crew)
				m_Crew.Cleanup_S();
		}
		super.OnDelete(owner);
	}
}
