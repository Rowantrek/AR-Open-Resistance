//! Saves active missions (AROR_MissionBoardComponent on the game mode, which has a Persistence component): their
//! identity, target, rewards and time left. Offers are not saved (the board rerolls on load) and spawned entities
//! are not saved (a loaded mission deploys again from the start). Nothing is written while no mission is active.
class AROR_MissionBoardComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return AROR_MissionBoardComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		AROR_MissionBoardComponent board = AROR_MissionBoardComponent.Cast(component);
		array<AROR_Mission> missions = {};
		if (board.GetActiveMissions_S(missions) == 0)
			return ESerializeResult.DEFAULT;

		array<int> ids = {};
		array<int> types = {};
		array<string> codenames = {};
		array<string> locations = {};
		array<string> destinations = {};
		array<int> positionsX = {};
		array<int> positionsZ = {};
		array<int> destinationsX = {};
		array<int> destinationsZ = {};
		array<int> radii = {};
		array<int> enemies = {};
		array<int> funds = {};
		array<int> manpower = {};
		array<int> limits = {};
		array<int> remaining = {};
		array<int> risks = {};
		array<int> guards = {};
		array<string> leaders = {};
		foreach (AROR_Mission mission : missions)
		{
			ids.Insert(mission.m_iId);
			types.Insert(mission.m_eType);
			codenames.Insert(mission.m_sCodename);
			locations.Insert(mission.m_sLocationId);
			destinations.Insert(mission.m_sDestinationId);
			positionsX.Insert(RoundToInt(mission.m_vPosition[0]));
			positionsZ.Insert(RoundToInt(mission.m_vPosition[2]));
			destinationsX.Insert(RoundToInt(mission.m_vDestination[0]));
			destinationsZ.Insert(RoundToInt(mission.m_vDestination[2]));
			radii.Insert(RoundToInt(mission.m_fAreaRadius));
			enemies.Insert(mission.m_eEnemyRole);
			funds.Insert(mission.m_iFunds);
			manpower.Insert(mission.m_iManpower);
			limits.Insert(mission.m_iTimeLimit);
			remaining.Insert(Math.Max(mission.m_iRemaining, 60));
			risks.Insert(mission.m_iRisk);
			guards.Insert(mission.m_iGuards);
			leaders.Insert(mission.m_sLeaderName);
		}

		context.WriteValue("version", VERSION);
		context.WriteValue("nextId", board.GetNextId());
		context.WriteValue("ids", ids);
		context.WriteValue("types", types);
		context.WriteValue("codenames", codenames);
		context.WriteValue("locations", locations);
		context.WriteValue("destinations", destinations);
		context.WriteValue("positionsX", positionsX);
		context.WriteValue("positionsZ", positionsZ);
		context.WriteValue("destinationsX", destinationsX);
		context.WriteValue("destinationsZ", destinationsZ);
		context.WriteValue("radii", radii);
		context.WriteValue("enemies", enemies);
		context.WriteValue("funds", funds);
		context.WriteValue("manpower", manpower);
		context.WriteValue("limits", limits);
		context.WriteValue("remaining", remaining);
		context.WriteValue("risks", risks);
		context.WriteValue("guards", guards);
		context.WriteValue("leaders", leaders);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected static int RoundToInt(float value)
	{
		int rounded = Math.Round(value);
		return rounded;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_MissionBoardComponent board = AROR_MissionBoardComponent.Cast(component);

		int version;
		if (!context.ReadValue("version", version) || version < 1)
			return true;

		int nextId;
		if (context.ReadValue("nextId", nextId))
			board.RestoreNextId_S(nextId);

		array<int> ids = {};
		array<int> types = {};
		array<string> codenames = {};
		array<string> locations = {};
		array<string> destinations = {};
		array<int> positionsX = {};
		array<int> positionsZ = {};
		array<int> destinationsX = {};
		array<int> destinationsZ = {};
		array<int> radii = {};
		array<int> enemies = {};
		array<int> funds = {};
		array<int> manpower = {};
		array<int> limits = {};
		array<int> remaining = {};
		array<int> risks = {};
		array<int> guards = {};
		array<string> leaders = {};

		// Read in groups: one long condition exceeds the script compiler's expression limit
		bool ok = context.ReadValue("ids", ids) && context.ReadValue("types", types) && context.ReadValue("codenames", codenames);
		ok = ok && context.ReadValue("locations", locations) && context.ReadValue("destinations", destinations);
		ok = ok && context.ReadValue("positionsX", positionsX) && context.ReadValue("positionsZ", positionsZ);
		ok = ok && context.ReadValue("destinationsX", destinationsX) && context.ReadValue("destinationsZ", destinationsZ);
		ok = ok && context.ReadValue("radii", radii) && context.ReadValue("enemies", enemies);
		ok = ok && context.ReadValue("funds", funds) && context.ReadValue("manpower", manpower);
		ok = ok && context.ReadValue("limits", limits) && context.ReadValue("remaining", remaining);
		ok = ok && context.ReadValue("risks", risks) && context.ReadValue("guards", guards) && context.ReadValue("leaders", leaders);
		if (!ok)
			return true;

		int count = ids.Count();
		bool sizes = types.Count() == count && codenames.Count() == count && locations.Count() == count && destinations.Count() == count;
		sizes = sizes && positionsX.Count() == count && positionsZ.Count() == count && destinationsX.Count() == count && destinationsZ.Count() == count;
		sizes = sizes && radii.Count() == count && enemies.Count() == count && funds.Count() == count && manpower.Count() == count;
		sizes = sizes && limits.Count() == count && remaining.Count() == count && risks.Count() == count && guards.Count() == count && leaders.Count() == count;
		if (!sizes)
			return true;

		for (int i = 0; i < count; i++)
		{
			// A type removed since the save (e.g. an extender mod's) is dropped
			AROR_Mission mission = board.CreateMission(types[i]);
			if (!mission)
				continue;

			mission.m_iId = ids[i];
			mission.m_eType = types[i];
			mission.m_sCodename = codenames[i];
			mission.m_sLocationId = locations[i];
			mission.m_sDestinationId = destinations[i];
			mission.m_vPosition = Vector(positionsX[i], 0, positionsZ[i]);
			mission.m_vDestination = Vector(destinationsX[i], 0, destinationsZ[i]);
			mission.m_fAreaRadius = radii[i];
			mission.m_eEnemyRole = enemies[i];
			mission.m_iFunds = funds[i];
			mission.m_iManpower = manpower[i];
			mission.m_iTimeLimit = limits[i];
			mission.m_iRemaining = remaining[i];
			mission.m_iRisk = risks[i];
			mission.m_iGuards = guards[i];
			mission.m_sLeaderName = leaders[i];
			board.RestoreMission_S(mission);
		}

		return true;
	}
}
