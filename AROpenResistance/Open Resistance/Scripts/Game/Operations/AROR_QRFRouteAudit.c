#ifdef WORKBENCH
//! Explicit, server-only planning sweep. No forces, orders, ownership or money are changed.
//! Run AROR_QRFRouteAudit.Start_S() after setup in an otherwise idle Chicken world.
//! A planning PASS does not prove driving, infantry navigation or flight.
class AROR_QRFRouteAudit
{
	protected static ref array<AROR_LocationComponent> s_aOrigins = {};
	protected static ref array<AROR_LocationComponent> s_aTargets = {};
	protected static AROR_OperationExecutorComponent s_Executor;
	protected static int s_iOrigin;
	protected static int s_iTarget;
	protected static int s_iChecked;
	protected static int s_iPassed;
	protected static int s_iAirTarget;
	protected static int s_iTicks;
	protected static ref AROR_ReaperLandingSearch s_LandingSearch;
	protected static ref map<string, ref array<vector>> s_mLandingPads = new map<string, ref array<vector>>();

	static bool Start_S()
	{
		if (!Replication.IsServer() || s_Executor)
			return false;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!operations || !locations || !IsIdle(operations))
			return false;
		s_Executor = AROR_OperationExecutorComponent.Cast(operations.GetOwner().FindComponent(AROR_OperationExecutorComponent));
		if (!s_Executor)
			return false;
		s_aOrigins.Clear();
		s_aTargets.Clear();
		s_iOrigin = 0;
		s_iTarget = 0;
		s_iChecked = 0;
		s_iPassed = 0;
		s_iAirTarget = 0;
		s_iTicks = 0;
		s_mLandingPads.Clear();
		array<AROR_LocationComponent> locationsSnapshot = {};
		locations.GetLocations(locationsSnapshot);
		foreach (AROR_LocationComponent location : locationsSnapshot)
		{
			if (!location || location.GetLocationType() == AROR_ELocationType.HQ || location.IsControlFixed())
				continue;
			s_aTargets.Insert(location);
			if (AROR_QRFOriginPolicy.IsOriginType(location.GetLocationType()))
				s_aOrigins.Insert(location);
		}
		Print(string.Format("[AROR ROUTE AUDIT] start origins=%1 targets=%2; one native planning pair per second; no troops spawned", s_aOrigins.Count(), s_aTargets.Count()), LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(Tick_S, 1000, true);
		return true;
	}

	protected static bool IsIdle(AROR_OperationManagerComponent operations)
	{
		array<AROR_Operation> outstanding = {};
		operations.GetOperations(outstanding);
		foreach (AROR_Operation operation : outstanding)
		{
			if (!operation.IsTerminal())
				return false;
		}
		return true;
	}

	protected static void Tick_S()
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!Replication.IsServer() || !s_Executor || !operations || !IsIdle(operations))
		{
			Stop_S();
			return;
		}
		s_iTicks++;
		TickLanding_S();
		// Launch escort headings depend on the selected pad, not just the target centre.
		if (s_iAirTarget < s_aTargets.Count())
			return;
		if (s_iOrigin >= s_aOrigins.Count() && s_iAirTarget >= s_aTargets.Count())
		{
			Print(string.Format("[AROR ROUTE AUDIT] complete checked=%1 planningPassed=%2; native driving/flight remain unverified", s_iChecked, s_iPassed), LogLevel.NORMAL);
			Stop_S();
			return;
		}
		if (s_iOrigin >= s_aOrigins.Count())
			return;
		AROR_LocationComponent origin = s_aOrigins[s_iOrigin];
		AROR_LocationComponent target = s_aTargets[s_iTarget];
		s_iTarget++;
		if (s_iTarget >= s_aTargets.Count())
		{
			s_iTarget = 0;
			s_iOrigin++;
		}
		if (!origin || !target || origin == target)
			return;
		vector launch;
		vector approach;
		bool passed = s_Executor.ProbeGroundRoute_S(origin, target, launch, approach);
		s_iChecked++;
		if (passed)
			s_iPassed++;
		array<vector> pads = s_mLandingPads.Get(target.GetId());
		string airResult = "NO_SAFE_LZ";
		string lightAirResult = "NO_SAFE_LZ";
		if (pads && !pads.IsEmpty())
		{
			airResult = "FAIL";
			if (s_Executor.ProbeAirLaunch_S(origin, target, pads))
				airResult = "PASS";
			lightAirResult = "FAIL";
			if (s_Executor.ProbeAirLaunch_S(origin, target, pads, false))
				lightAirResult = "PASS";
		}
		string result = "FAIL";
		if (passed)
			result = "PASS";
		Print(string.Format("[AROR ROUTE AUDIT] pair origin=%1 target=%2 groundPlanning=%3 airLaunchPlanning=%4 lightAirLaunchPlanning=%5 launch=%6 approach=%7", origin.GetId(), target.GetId(), result, airResult, lightAirResult, launch, approach), LogLevel.NORMAL);
	}

	//! Share the production bounded LZ search: at most two radial seeds per audit tick.
	protected static void TickLanding_S()
	{
		if (s_iAirTarget >= s_aTargets.Count())
			return;
		AROR_LocationComponent target = s_aTargets[s_iAirTarget];
		if (!target)
		{
			s_iAirTarget++;
			return;
		}
		if (!s_LandingSearch)
			s_LandingSearch = s_Executor.CreateLandingAudit_S(target);
		if (!s_LandingSearch)
		{
			Print("[AROR ROUTE AUDIT] landing target=" + target.GetId() + " unavailable; check airborne configuration", LogLevel.WARNING);
			s_iAirTarget++;
			return;
		}
		s_LandingSearch.Advance(s_iTicks);
		if (!s_LandingSearch.IsComplete())
			return;
		Print("[AROR ROUTE AUDIT] landing target=" + target.GetId() + " " + s_LandingSearch.GetDiagnostics(), LogLevel.NORMAL);
		array<vector> pads = {};
		for (int index = 0; index < s_LandingSearch.GetResultCount(); index++)
			pads.Insert(s_LandingSearch.GetResult(index));
		s_mLandingPads.Set(target.GetId(), pads);
		s_iAirTarget++;
		s_LandingSearch = null;
	}

	static void Stop_S()
	{
		GetGame().GetCallqueue().Remove(Tick_S);
		s_Executor = null;
		s_LandingSearch = null;
		s_aOrigins.Clear();
		s_aTargets.Clear();
		s_mLandingPads.Clear();
	}
}
#endif
