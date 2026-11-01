//! Prisoner Rescue: three detained sympathisers held under guard at an enemy-held location. Clear the guards and
//! reach the prisoners to free them; they then walk to the nearest friendly location. Detained, they count as
//! civilians (the guards leave them be); once freed they are Resistance and can be shot. Manpower is paid for
//! each prisoner who arrives.
class AROR_PrisonerRescueMission : AROR_Mission
{
	protected static const ref array<AROR_ELocationType> SITE_TYPES = {AROR_ELocationType.TOWN, AROR_ELocationType.CITY, AROR_ELocationType.OUTPOST, AROR_ELocationType.MILITARY_BASE, AROR_ELocationType.FACTORY, AROR_ELocationType.RESOURCES, AROR_ELocationType.PORT, AROR_ELocationType.HELIPORT};

	protected static const int PRISONERS = 3;
	protected static const float FREE_DISTANCE = 10;	//!< A Resistance player this close frees them...
	protected static const float GUARD_CLEAR = 40;		//!< ...once no living guard is this close

	protected SCR_AIGroup m_Prisoners;
	protected AIWaypoint m_HoldWaypoint;
	protected vector m_vHoldSpot;
	protected int m_iPrisonersSeen;
	protected int m_iRescued;
	protected ref array<IEntity> m_aObservedPrisoners = {};

	//------------------------------------------------------------------------------------------------
	override bool Plan_S(AROR_MissionBoardComponent board)
	{
		if (!board.HasUnit_S(AROR_EFactionRole.CIVILIANS, AROR_EUnitType.CIVILIAN))
			return false;

		AROR_LocationComponent site = board.PickEnemyLocation_S(SITE_TYPES);
		if (!site)
			return false;

		m_eEnemyRole = site.GetControllingRole();
		m_sLocationId = site.GetId();
		m_vPosition = site.GetCenter();
		m_fAreaRadius = 50;
		m_iRisk = RiskOf(site);
		ApplySettings(board.GetSettings(m_eType));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool Deploy_S(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent site = board.GetLocation(m_sLocationId);
		if (!site)
			return false;

		if (!board.FindClearPosition_S(site.GetCenter(), 35, 2, m_vHoldSpot))
			return false;

		array<AROR_EUnitType> members = {};
		for (int i = 0; i < PRISONERS; i++)
		{
			members.Insert(AROR_EUnitType.CIVILIAN);
		}

		m_Prisoners = AddGroup_S(board, AROR_EFactionRole.CIVILIANS, members, m_vHoldSpot);
		if (!m_Prisoners)
			return false;

		m_HoldWaypoint = board.AddWaypoint_S(m_Prisoners, true, m_vHoldSpot, 4, m_aEntities);
		SpawnGuards_S(board, m_vHoldSpot, m_iGuards, 25);

		// A mission loaded after the prisoners were freed starts over: drop the old destination
		m_sDestinationId = string.Empty;
		m_vDestination = vector.Zero;
		m_vPosition = Vector(m_vHoldSpot[0], 0, m_vHoldSpot[2]);
		m_fAreaRadius = 30;
		m_sProgress = "Clear the guards and free the prisoners at " + GetLocationName(board);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override AROR_EMissionResult Update_S(AROR_MissionBoardComponent board)
	{
		UpdateGroups_S(board, m_eEnemyRole);

		array<IEntity> prisoners = {};
		int alive = AROR_MissionBoardComponent.CountLiving(m_Prisoners, prisoners);
		array<AIAgent> agents = {};
		if (m_Prisoners)
			m_Prisoners.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity member = agent.GetControlledEntity();
			if (member && !m_aObservedPrisoners.Contains(member))
				m_aObservedPrisoners.Insert(member);
		}

		// Do not free or reward a partially spawned prisoner group.
		if (m_aObservedPrisoners.Count() < PRISONERS)
			return AROR_EMissionResult.ONGOING;

		m_iPrisonersSeen = Math.Max(m_iPrisonersSeen, alive);
		if (m_iPrisonersSeen > 0 && alive == 0)
		{
			m_sOutcome = "the prisoners were killed";
			return AROR_EMissionResult.FAILED;
		}

		if (m_iPhase == 0)
		{
			foreach (IEntity prisoner : prisoners)
			{
				if (CanBeFreed(board, prisoner))
				{
					Free_S(board, prisoners);
					break;
				}
			}

			return AROR_EMissionResult.ONGOING;
		}

		if (prisoners.IsEmpty())
			return AROR_EMissionResult.ONGOING;

		// Wait for every survivor; only prisoners physically home count toward rewards.
		TrackMarker(prisoners[0].GetOrigin(), 150);
		float arrival = 60;
		AROR_LocationComponent destination = board.GetLocation(m_sDestinationId);
		if (destination)
			arrival = Math.Max(destination.GetRadius(), arrival);

		m_iRescued = 0;
		foreach (IEntity walker : prisoners)
		{
			if (vector.DistanceXZ(walker.GetOrigin(), m_vDestination) <= arrival)
				m_iRescued++;
		}

		m_sProgress = string.Format("Prisoners at destination: %1/%2 survivors", m_iRescued, alive);
		if (m_iRescued == alive)
		{
			m_sOutcome = string.Format("%1 prisoners rescued", m_iRescued);
			return AROR_EMissionResult.SUCCEEDED;
		}

		return AROR_EMissionResult.ONGOING;
	}

	//------------------------------------------------------------------------------------------------
	//! A Resistance player beside the prisoner and no living guard near
	protected bool CanBeFreed(AROR_MissionBoardComponent board, IEntity prisoner)
	{
		vector position = prisoner.GetOrigin();
		if (!board.IsResistanceNear_S(position, FREE_DISTANCE))
			return false;

		foreach (int i, SCR_AIGroup group : m_aGroups)
		{
			if (m_aGroupRoles[i] != m_eEnemyRole)
				continue;

			array<IEntity> guards = {};
			AROR_MissionBoardComponent.CountLiving(group, guards);
			foreach (IEntity guard : guards)
			{
				if (vector.DistanceXZ(guard.GetOrigin(), position) <= GUARD_CLEAR)
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The prisoners join the Resistance and head for the nearest friendly location
	protected void Free_S(AROR_MissionBoardComponent board, array<IEntity> prisoners)
	{
		string resistanceKey = board.GetFactionKey(AROR_EFactionRole.RESISTANCE);
		Faction resistance = GetGame().GetFactionManager().GetFactionByKey(resistanceKey);
		if (resistance)
			m_Prisoners.SetFaction(resistance);

		foreach (IEntity prisoner : prisoners)
		{
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(prisoner.FindComponent(FactionAffiliationComponent));
			if (affiliation)
				affiliation.SetAffiliatedFactionByKey(resistanceKey);
		}

		// Later members must not be switched back by the affiliation pass
		int index = m_aGroups.Find(m_Prisoners);
		if (index >= 0)
			m_aGroupRoles[index] = AROR_EFactionRole.RESISTANCE;

		if (m_HoldWaypoint)
		{
			m_Prisoners.RemoveWaypoint(m_HoldWaypoint);
			m_aEntities.RemoveItem(m_HoldWaypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(m_HoldWaypoint);
			m_HoldWaypoint = null;
		}

		string destinationId;
		vector destination;
		if (!board.FindFriendlyDestination_S(m_vHoldSpot, destinationId, destination))
			destination = m_vHoldSpot;

		m_sDestinationId = destinationId;
		m_vDestination = destination;
		board.AddWaypoint_S(m_Prisoners, false, destination, 25, m_aEntities);

		m_iPhase = 1;
		m_fAreaRadius = 0;
		m_sProgress = "Prisoners freed: escort them to " + GetDestinationName(board);
		board.AddLog(m_sCodename + ": prisoners freed");
		board.ReplyLeader(this, "prisoners freed, escort them to " + GetDestinationName(board));
	}

	//------------------------------------------------------------------------------------------------
	override int GetRewardManpower()
	{
		float share = m_iRescued / (PRISONERS * 1.0);
		return Math.Max(Math.Round(m_iManpower * share), 1);
	}

	//------------------------------------------------------------------------------------------------
	override string GetStartProgress(AROR_MissionBoardComponent board)
	{
		return "Head to " + GetLocationName(board) + " and find the prisoners";
	}

	//------------------------------------------------------------------------------------------------
	override string GetTimeoutOutcome()
	{
		return "the prisoners were transferred";
	}
}
