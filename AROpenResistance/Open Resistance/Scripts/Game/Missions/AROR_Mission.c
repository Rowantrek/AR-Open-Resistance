//! One Resistance mission on the server (see ARCHITECTURE.md -> Missions): first an offer on the OPS board, then,
//! once a Resistance Leader accepts it, an active mission with a deadline.
//! Subclasses pick their target (Plan_S), spawn their enemies and props when the Resistance comes near (Deploy_S)
//! and check their objectives (Update_S). AROR_MissionBoardComponent owns the lifecycle, rewards and the snapshot;
//! it tidies spawned entities away after the mission ends (Release_S).
//! Saved for active missions: identity, target, rewards and time left (see AROR_MissionBoardComponentSerializer).
//! Spawned entities and progress are not saved: a loaded mission deploys again from the start.
class AROR_Mission
{
	// Identity and target
	int m_iId;
	AROR_EMissionType m_eType;
	string m_sCodename;
	string m_sLocationId;
	vector m_vPosition;			//!< Objective point (convoys: last known position)
	float m_fAreaRadius;		//!< Radius drawn around the objective (the search area for caches)
	string m_sDestinationId;	//!< Convoy destination, or where freed prisoners are taken
	vector m_vDestination;
	AROR_EFactionRole m_eEnemyRole;
	int m_iFunds;
	int m_iManpower;
	int m_iTimeLimit;			//!< Seconds to complete once accepted
	int m_iRisk;				//!< 0 low, 1 moderate, 2 high, 3 extreme
	int m_iGuards;

	// Offer
	int m_iOfferSeconds;		//!< Seconds left on the board

	// Active
	bool m_bActive;
	int m_iRemaining;			//!< Seconds left to complete
	int m_iLeaderId;
	string m_sLeaderName;
	bool m_bDeployed;
	bool m_bRestored;			//!< Loaded missions wait for Resistance proximity, including convoys
	int m_iDeployAttempts;
	int m_iPhase;
	string m_sProgress;			//!< Status line shown in OPS and on the map
	string m_sOutcome;			//!< Why it ended, set with the result

	// Spawned entities (server only, never saved)
	ref array<SCR_AIGroup> m_aGroups = {};
	ref array<int> m_aGroupRoles = {};	//!< Faction role of each group in m_aGroups
	ref array<IEntity> m_aEntities = {};
	ref array<IEntity> m_aAffiliated = {};
	int m_iDeployedAt;

	static const int RISK_LOW = 0;
	static const int RISK_MODERATE = 1;
	static const int RISK_HIGH = 2;
	static const int RISK_EXTREME = 3;

	//------------------------------------------------------------------------------------------------
	// Overridden by each mission type
	//------------------------------------------------------------------------------------------------
	//! Picks the target and fills in the offer (location, position, enemy, risk); false if no target fits now
	bool Plan_S(AROR_MissionBoardComponent board)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Target remains valid; checked on acceptance and while waiting to deploy
	bool IsStillValid_S(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent location = board.GetLocation(m_sLocationId);
		return location && location.GetControllingRole() == m_eEnemyRole;
	}

	//------------------------------------------------------------------------------------------------
	//! Deploy as soon as the mission is accepted, rather than when the Resistance comes near
	bool DeploysAtOnce()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the mission's enemies and props; false to retry on the next update
	bool Deploy_S(AROR_MissionBoardComponent board)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks the objectives once deployed; sets m_sProgress, and m_sOutcome with a final result
	AROR_EMissionResult Update_S(AROR_MissionBoardComponent board)
	{
		return AROR_EMissionResult.ONGOING;
	}

	//------------------------------------------------------------------------------------------------
	//! Status line from the moment the mission is accepted until it deploys
	string GetStartProgress(AROR_MissionBoardComponent board)
	{
		return "Head to " + GetLocationName(board);
	}

	//------------------------------------------------------------------------------------------------
	string GetTimeoutOutcome()
	{
		return "time ran out";
	}

	//------------------------------------------------------------------------------------------------
	//! Funds paid on success (missions with partial success scale it down)
	int GetRewardFunds()
	{
		return m_iFunds;
	}

	//------------------------------------------------------------------------------------------------
	int GetRewardManpower()
	{
		return m_iManpower;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands spawned entities to the board for removal once players have left; a mission keeps what it awards
	//! (e.g. a stolen truck) by leaving it out
	void Release_S(AROR_MissionBoardComponent board, bool succeeded)
	{
		board.QueueCleanup_S(m_aGroups, m_aEntities);
	}

	//------------------------------------------------------------------------------------------------
	// Shared helpers
	//------------------------------------------------------------------------------------------------
	//! Spawns a group of the role and tracks it for affiliation and cleanup
	protected SCR_AIGroup AddGroup_S(AROR_MissionBoardComponent board, AROR_EFactionRole role, notnull array<ResourceName> members, vector position)
	{
		SCR_AIGroup group = board.SpawnGroup_S(role, members, position);
		if (group)
		{
			m_aGroups.Insert(group);
			m_aGroupRoles.Insert(role);
		}

		return group;
	}

	//------------------------------------------------------------------------------------------------
	//! An enemy guard team holding the area around centre (none if count is 0)
	protected SCR_AIGroup SpawnGuards_S(AROR_MissionBoardComponent board, vector centre, int count, float holdRadius)
	{
		if (count <= 0)
			return null;

		array<ResourceName> members = {};
		board.BuildGuardTeam_S(m_eEnemyRole, count, members);

		vector spot = centre;
		vector found;
		if (board.FindClearPosition_S(centre, 20, 1, found))
			spot = found;

		SCR_AIGroup group = AddGroup_S(board, m_eEnemyRole, members, spot);
		if (group)
			board.AddWaypoint_S(group, true, centre, holdRadius, m_aEntities);

		return group;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps newly spawned members on their campaign faction; returns the living members of the role's groups
	protected int UpdateGroups_S(AROR_MissionBoardComponent board, AROR_EFactionRole role)
	{
		int alive;
		foreach (int i, SCR_AIGroup group : m_aGroups)
		{
			int count = board.CountAlive_S(group, m_aGroupRoles[i], m_aAffiliated);
			if (m_aGroupRoles[i] == role)
				alive += count;
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the map marker to a new position once it has moved far enough (keeps the snapshot quiet)
	protected void TrackMarker(vector position, float threshold)
	{
		if (vector.DistanceXZ(position, m_vPosition) >= threshold)
			m_vPosition = Vector(position[0], 0, position[2]);
	}

	//! Rewards, time limit and guard count from the type settings, raised by the threat level
	void ApplySettings(AROR_MissionType settings)
	{
		float bonus = 1 + 0.25 * m_iRisk;
		m_iFunds = Math.Round(settings.m_iFunds * bonus / 50) * 50;
		m_iManpower = settings.m_iManpower;
		if (m_iRisk >= RISK_HIGH)
			m_iManpower++;

		m_iTimeLimit = settings.m_iTimeLimit * 60;
		m_iGuards = Math.Min(settings.m_iGuards + m_iRisk, 10);
	}

	//------------------------------------------------------------------------------------------------
	string GetLocationName(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent location = board.GetLocation(m_sLocationId);
		if (!location)
			return "the area";

		return location.GetName();
	}

	//------------------------------------------------------------------------------------------------
	string GetDestinationName(AROR_MissionBoardComponent board)
	{
		AROR_LocationComponent location = board.GetLocation(m_sDestinationId);
		if (!location)
			return "a friendly location";

		return location.GetName();
	}

	//------------------------------------------------------------------------------------------------
	//! Threat level of an enemy site by its type
	static int RiskOf(AROR_LocationComponent location)
	{
		if (!location)
			return RISK_MODERATE;

		switch (location.GetLocationType())
		{
			case AROR_ELocationType.TOWN:
				return RISK_LOW;
			case AROR_ELocationType.PORT:
			case AROR_ELocationType.HELIPORT:
				return RISK_HIGH;
			case AROR_ELocationType.MILITARY_BASE:
				return RISK_HIGH;
			case AROR_ELocationType.AIRPORT:
				return RISK_EXTREME;
		}

		return RISK_MODERATE;
	}
}
