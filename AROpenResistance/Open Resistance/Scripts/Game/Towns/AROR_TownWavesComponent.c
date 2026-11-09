[ComponentEditorProps(category: "OpenResistance/Locations", description: "Town garrison response: player casualties or a taken post trigger finite ground waves and a transport insertion with gunship support. Role doctrines control the assault. Separate from ordinary QRFs. Put on a town or city with a settlement garrison post.")]
class AROR_TownWavesComponentClass : ScriptComponentClass {}

//! Server logic for one town's wave response, in phases:
//! - ALERT: the trigger kill; the first wave forms up after a short random delay.
//! - WAVES: each wave is the holding army's wave recipe at its current Readiness (equipment), sized by its Crackdown
//!   (element count), paid by the scenario, driven as a convoy from a road 1-2 km out that dismounts short of the post.
//!   Waves use different roads; the role doctrine controls tempo, splitting, dismount and infantry tasks.
//! - A transport/gunship insertion (joint recipe) commits with the first ground wave by default; timing is configurable.
//! - RESOLVING: once every wave and the assault have ended, the town holds: a support reward and an HQ headline.
//! - LOCKDOWN, throughout: the army's fighters and the Resistance's are counted around the town. Outnumbered for the
//!   lockdown time in total and the army retakes the town: the post is restored and the Resistance support gained
//!   since the contact is lost. The same happens if the Resistance leaves. Clients see it on AROR_TownContestHud.
//! Only one town response runs on the map at a time; a second town under attack gets no reinforcements until it ends.
//! Every enemy killed in the town (garrison, checkpoints, patrol, wave troops) raises the army's Crackdown and
//! Readiness through Operations (RecordTownEscalation_S). "[AROR BALANCE]" log lines record each response.
//! Waves go through Operations (RequestTownWave_S). Session-only: kill progress and a running response are not saved.
class AROR_TownWavesComponent : ScriptComponent
{
	[Attribute("1", desc: "Role doctrines for this town's waves and attack orders: Invaders use fast split shock assaults; Occupiers use deliberate convoy assaults. Disable for the original shared tactics.", category: "Doctrine")]
	protected bool m_bFactionDoctrine;
	[Attribute("0.65", desc: "Invader alert and wave interval multiplier (same finite force budget)", params: "0.4 1 0.05", category: "Doctrine")]
	protected float m_fShockTempo;
	[Attribute("1", desc: "Invader waves split from this wave onwards, when the split element threshold is met", params: "1 5 1", category: "Doctrine")]
	protected int m_iShockSplitFromWave;
	[Attribute("160", desc: "Invader convoy dismount distance, metres; still subject to safe road and foot-access checks", params: "100 290 10", category: "Doctrine")]
	protected float m_fShockDismountDistance;
	[Attribute("10", desc: "Minimum seconds between Invader tactical retasks", params: "5 60 5", category: "Doctrine")]
	protected int m_iShockRetaskSeconds;
	[Attribute("25", desc: "Minimum seconds between Occupier tactical retasks", params: "5 60 5", category: "Doctrine")]
	protected int m_iDeliberateRetaskSeconds;
	[Attribute("45", desc: "Town doctrine contact reports expire after this many seconds; no live enemy tracking", params: "15 120 5", category: "Doctrine")]
	protected int m_iDoctrineContactMaxAge;
	[Attribute("70", desc: "Infantry flank offset from a reported contact, metres (bounded to the objective vicinity)", params: "30 120 10", category: "Doctrine")]
	protected float m_fDoctrineFlankDistance;
	[Attribute("100", desc: "Occupier covering element and vehicle support standoff from a reported contact, metres", params: "40 180 10", category: "Doctrine")]
	protected float m_fDeliberateSupportDistance;
	[Attribute("0.25", desc: "Invader withdrawal threshold: surviving share without reserves", params: "0.1 0.6 0.05", category: "Doctrine")]
	protected float m_fShockWithdrawFraction;
	[Attribute("0.45", desc: "Occupier withdrawal threshold: surviving share without reserves", params: "0.1 0.6 0.05", category: "Doctrine")]
	protected float m_fDeliberateWithdrawFraction;

	[Attribute("8", desc: "Army crackdown points per enemy killed in the town (0-1000 scale, 200 per level)", params: "0 100 1", category: "Escalation")]
	protected int m_iKillCrackdown;
	[Attribute("4", desc: "Army readiness pressure per enemy killed in the town (levels at 100, 250, 450, 700)", params: "0 100 1", category: "Escalation")]
	protected int m_iKillReadiness;
	[Attribute("0", desc: "Radius counted for the lockdown, metres; 0 uses the town's radius (kept within 120-300)", params: "0 600 10", category: "Lockdown")]
	protected float m_fContestRadius;
	[Attribute("600", desc: "Seconds the army must outnumber the Resistance in the town (in total; it counts back down while it does not) to retake it", params: "60 3600 30", category: "Lockdown")]
	protected int m_iLockdownSeconds;
	[Attribute("700", desc: "Players this close to the town see the lockdown indicator, metres", params: "100 2000 50", category: "Lockdown")]
	protected float m_fIndicatorRadius;
	[Attribute("1", desc: "How many enemy fighters one Resistance fighter (player or recruit) is worth in the lockdown count; above 1 lets small squads hold against larger AI forces", params: "0.5 5 0.25", category: "Lockdown")]
	protected float m_fResistanceWeight;

	[Attribute("0.25", desc: "Share of the town's defenders (post, checkpoints and patrol) players must kill to bring the response", params: "0.05 1 0.05")]
	protected float m_fTriggerShare;
	[Attribute("3", desc: "Fewest player kills that bring the response", params: "1 30 1")]
	protected int m_iMinimumKills;
	[Attribute("3", desc: "Waves in one response", params: "1 5 1")]
	protected int m_iWaves;
	[Attribute("30", desc: "Shortest delay from the trigger to the first wave, seconds", params: "0 600 5")]
	protected int m_iAlertDelayMin;
	[Attribute("90", desc: "Longest delay from the trigger to the first wave, seconds", params: "0 600 5")]
	protected int m_iAlertDelayMax;
	[Attribute("150", desc: "Average seconds between one wave being sent and the next", params: "30 900 10")]
	protected int m_iWaveInterval;
	[Attribute("30", desc: "Random spread either side of the wave interval, seconds", params: "0 300 5")]
	protected int m_iWaveIntervalJitter;
	[Attribute("1000", desc: "Nearest road staging distance from the post, metres", params: "300 3000 50")]
	protected float m_fStagingMinDistance;
	[Attribute("2000", desc: "Furthest road staging distance from the post, metres", params: "500 4000 50")]
	protected float m_fStagingMaxDistance;
	[Attribute("500", desc: "A wave's staging road keeps at least this far from the previous wave's and from its other column, so attacks come from different directions, metres (0 = any road)", params: "0 2000 50")]
	protected float m_fAxisSeparation;
	[Attribute("220", desc: "Where the convoy's lead vehicle stops, metres short of the post; later vehicles stop behind it. Infantry walk in.", params: "100 290 10")]
	protected float m_fDismountDistance;
	[Attribute("0", desc: "Extra wave elements on top of the Crackdown size; cities always add one", params: "0 4 1")]
	protected int m_iExtraElements;
	[Attribute("3", desc: "A split-eligible wave needs at least this many elements to use two columns (0 = never). Final wave for Occupiers/shared tactics; shock split setting for Invaders.", params: "0 8 1")]
	protected int m_iSplitMinElements;
	[Attribute("1", desc: "Lowest Readiness at which a transport insertion with gunship support joins the response (0 = never). Uses the army's wave 9 recipe.", params: "0 5 1")]
	protected int m_iAirAssaultReadiness;
	[Attribute("1", desc: "Request the helicopter insertion when the first ground wave is committed, so every town response plans a troop drop. Disable to wait until ground forces press the post.")]
	protected bool m_bAirOnFirstWave;
	[Attribute("350", desc: "A wave package this close to the post commits the helicopter insertion when first-wave timing is disabled, metres", params: "100 1000 25")]
	protected float m_fPressingDistance;
	[Attribute("20", desc: "Resistance support gained in the town when the whole response is beaten off, percent", params: "0 50 1")]
	protected float m_fRepelledSupportSwing;
	[Attribute("900", desc: "Seconds after the last wave is sent before an unresolved response ends without an outcome", params: "120 3600 30")]
	protected int m_iResolveTimeout;
	[Attribute("1", desc: "If the Resistance leaves while the response runs, the army retakes a taken garrison post at once instead of waiting for it to re-form")]
	protected bool m_bRetakeWhenAbandoned;
	[Attribute("1800", desc: "Seconds after a response ends before kills count towards a new one", params: "300 7200 60")]
	protected int m_iRearmSeconds;
	[Attribute("1500", desc: "The response is abandoned once no Resistance player has been this close for the abandon time, metres", params: "300 4000 50")]
	protected float m_fAbandonDistance;
	[Attribute("180", desc: "Seconds without a Resistance player near before the response is abandoned", params: "30 900 10")]
	protected int m_iAbandonSeconds;
	[Attribute("1", desc: "The waves are this town's whole response: a change of control here never draws an ordinary capture QRF")]
	protected bool m_bReplacesCaptureQRF;

	static const int AIR_ASSAULT_WAVE = 9;			//!< Recipe m_iAssaultWave of a response's helicopter assault
	protected static const int KILL_CREDIT_SECONDS = 30;	//!< A death counts for the player who hit the defender this recently
	protected static const int STAGING_RETRY_SECONDS = 20;
	protected static const int STAGING_ATTEMPTS = 6;
	protected static const int PRESSING_CHECK_SECONDS = 5;
	protected static const int CONTEST_CHECK_SECONDS = 5;
	protected static const int CONTEST_LOG_SECONDS = 30;
	protected static const int MAX_WATCHES = 250;
	protected static const float AXIS_PREFERENCE_DISTANCE = 1000;

	protected static AROR_TownWavesComponent s_ActiveResponse;	//!< Server: the one town response running on the map
	protected static ref array<AROR_TownWavesComponent> s_aTowns = {};	//!< Every machine: all towns, for the HUD

	//! Replicated lockdown state for the HUD (see AROR_TownContestHud)
	[RplProp()]
	protected bool m_bContestActive;
	[RplProp()]
	protected int m_iContestSeconds;
	[RplProp()]
	protected int m_iContestEnemies;
	[RplProp()]
	protected int m_iContestFriendlies;
	[RplProp()]
	protected bool m_bContestLosing;	//!< The army outnumbers the Resistance (weighted) right now
	[RplProp()]
	protected int m_iContestLimit;		//!< Seconds at which the town is lost (the lockdown, or an attack order's contest)

	protected static const int PHASE_IDLE = 0;
	protected static const int PHASE_ALERT = 1;		//!< Triggered; the first wave is forming up
	protected static const int PHASE_WAVES = 2;		//!< Sending waves
	protected static const int PHASE_RESOLVING = 3;	//!< Every wave sent; waiting for them (and the assault) to end

	protected AROR_LocationComponent m_Location;
	protected AROR_GarrisonComponent m_Garrison;
	protected AROR_TownLifeComponent m_TownLife;
	protected ref array<ref AROR_TownCasualtyWatch> m_aWatches = {};
	protected ref array<IEntity> m_aSeen = {};	//!< Every defender watched since the response last armed
	protected ref array<string> m_aOperationIds = {};	//!< Every column and the assault of the running response
	protected ref array<int> m_aOperationWaves = {};	//!< Parallel: wave number (AIR_ASSAULT_WAVE for the assault)
	protected ref array<string> m_aOperationDirections = {};	//!< Parallel: compass side it comes from, or empty
	protected ref array<string> m_aFielded = {};	//!< Operations of the running response that reached the field
	protected ref array<int> m_aAnnouncedWaves = {};	//!< Waves whose response report went out
	protected ref array<int> m_aContactPlayers = {};	//!< Players whose kills count towards (or brought) the response
	protected ref array<vector> m_aLastAxes = {};	//!< Staging roads of the previous wave
	protected AROR_EFactionRole m_eRole;
	protected int m_iPhase;
	protected int m_iClock;
	protected int m_iKills;
	protected int m_iWavesSent;		//!< Waves sent or skipped in the running response
	protected int m_iAxisIndex;		//!< Columns sent so far; picks the next preferred bearing
	protected int m_iAxisSide;		//!< +1 or -1: which way the axes rotate in this response
	protected int m_iNextWaveAt;
	protected int m_iResolveSince;
	protected int m_iRearmAt;
	protected int m_iLastResistanceNear;
	protected int m_iStagingFailures;
	protected bool m_bAirRequested;
	protected int m_iNextAirAttempt;
	protected bool m_bDeferredReported;	//!< The "no reinforcements available" contact report went out this arming
	protected bool m_bPostTakenTrigger;	//!< Preserve a relief request even if taking the flag flips support immediately
	protected float m_fPostSupportBefore;
	protected string m_sOutcome;		//!< Set before a response ends, for the balance log
	// Balance log snapshot of the running response
	protected int m_iStartedAt;
	protected int m_iResponseKills;
	protected int m_iCrackdownAtStart;
	protected int m_iReadinessAtStart;
	protected float m_fSupportAtContact;	//!< Resistance support when the response began; lost gains go back to it
	protected int m_iPeakEnemies;
	protected int m_iPeakFriendlies;
	protected int m_iNextContestCheck;
	protected int m_iNextContestLog;
	// Lockdown scan scratch
	protected int m_iScanEnemies;
	protected int m_iScanFriendlies;
	protected AROR_FactionRolesComponent m_ScanRoles;

	//------------------------------------------------------------------------------------------------
	//! Every machine: lockdown state for the HUD
	bool IsContestActive() { return m_bContestActive; }
	int GetContestSeconds() { return m_iContestSeconds; }
	int GetLockdownSeconds()
	{
		if (m_iContestLimit > 0)
			return m_iContestLimit;
		return m_iLockdownSeconds;
	}

	//! Server: show an AI attack order's occupation contest on this Resistance-held town (AROR_RecaptureComponent) on
	//! the lockdown indicator, so defenders see the count and the time left. A running garrison response owns the
	//! indicator instead.
	void ShowAttackContest_S(int attackers, int defenders, int seconds, int limit, bool losing)
	{
		if (!Replication.IsServer() || m_iPhase != PHASE_IDLE)
			return;
		if (m_bContestActive && m_iContestEnemies == attackers && m_iContestFriendlies == defenders && m_iContestSeconds == seconds && m_bContestLosing == losing && m_iContestLimit == limit)
			return;
		m_bContestActive = true;
		m_iContestEnemies = attackers;
		m_iContestFriendlies = defenders;
		m_iContestSeconds = seconds;
		m_iContestLimit = limit;
		m_bContestLosing = losing;
		Replication.BumpMe();
	}

	//! Server: the attack order is over (or the town changed hands); hide its contest again
	void ClearAttackContest_S()
	{
		if (!Replication.IsServer() || m_iPhase != PHASE_IDLE || !m_bContestActive)
			return;
		m_bContestActive = false;
		m_bContestLosing = false;
		m_iContestSeconds = 0;
		m_iContestLimit = 0;
		Replication.BumpMe();
	}
	int GetContestEnemies() { return m_iContestEnemies; }
	int GetContestFriendlies() { return m_iContestFriendlies; }
	bool IsContestLosing() { return m_bContestLosing; }
	AROR_LocationComponent GetLocation() { return m_Location; }

	//! Every machine: the town whose lockdown indicator a player at this position should see, or null
	static AROR_TownWavesComponent FindContest(vector position)
	{
		foreach (AROR_TownWavesComponent town : s_aTowns)
		{
			if (town && town.m_bContestActive && town.m_Location && vector.DistanceXZ(position, town.m_Location.GetCenter()) <= town.m_fIndicatorRadius)
				return town;
		}
		return null;
	}

	//! Server: a taken garrison post escalates the holding army like a lost location (called by Location Capture)
	static void RecordPostTaken_S(AROR_LocationComponent location, AROR_EFactionRole holder, float supportBefore = -1)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!Replication.IsServer() || !operations || !location)
			return;
		operations.RecordTownEscalation_S(holder, 0, 0, location.GetId(), "post_taken", true);
		AROR_TownWavesComponent town = AROR_TownWavesComponent.Cast(location.GetOwner().FindComponent(AROR_TownWavesComponent));
		if (town && town.m_iPhase == PHASE_IDLE && town.m_iClock >= town.m_iRearmAt)
		{
			town.m_eRole = holder;
			town.m_bPostTakenTrigger = true;
			town.m_fPostSupportBefore = supportBefore;
			if (supportBefore < 0)
				town.m_fPostSupportBefore = town.ResistanceSupport();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: true if this town's waves stand in for the capture QRF
	static bool ReplacesCaptureQRF(AROR_LocationComponent location)
	{
		if (!location)
			return false;
		AROR_TownWavesComponent waves = AROR_TownWavesComponent.Cast(location.GetOwner().FindComponent(AROR_TownWavesComponent));
		return waves && waves.m_bReplacesCaptureQRF;
	}

	bool IsResponding() { return m_iPhase != PHASE_IDLE; }

	//! Applied only to this town's responses and attack orders, never to ordinary QRFs or player groups.
	void ConfigureCommander_S(AROR_QRFTacticalCommander commander)
	{
		if (!Replication.IsServer() || !m_bFactionDoctrine || !commander)
			return;
		commander.m_bTownDoctrine = true;
		commander.m_bShockAssault = commander.m_eRole == AROR_EFactionRole.INVADERS;
		commander.m_iContactMaxAge = Math.Clamp(m_iDoctrineContactMaxAge, 15, 120);
		commander.m_fFlankDistance = Math.Clamp(m_fDoctrineFlankDistance, 30, 120);
		commander.m_fCoverDistance = Math.Clamp(m_fDeliberateSupportDistance, 40, 180);
		if (commander.m_bShockAssault)
		{
			commander.m_iRetaskSeconds = Math.Clamp(m_iShockRetaskSeconds, 5, 60);
			commander.m_fWithdrawFraction = Math.Clamp(m_fShockWithdrawFraction, 0.1, 0.6);
		}
		else
		{
			commander.m_iRetaskSeconds = Math.Clamp(m_iDeliberateRetaskSeconds, 5, 60);
			commander.m_fWithdrawFraction = Math.Clamp(m_fDeliberateWithdrawFraction, 0.1, 0.6);
		}
	}

	protected bool UsesShockDoctrine()
	{
		return m_bFactionDoctrine && m_eRole == AROR_EFactionRole.INVADERS;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!Replication.IsServer() || !m_Location)
			return;
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!commanders || !commanders.IsCampaignActive())
			return;
		m_iClock++;

		AROR_EFactionRole role = m_Location.GetControllingRole();
		if (m_iPhase == PHASE_IDLE && m_bPostTakenTrigger && role == AROR_EFactionRole.RESISTANCE)
			role = m_eRole;
		// A running response stays with its army even when the town's support flips it to the Resistance mid-fight:
		// the lockdown decides who keeps it. Only another army taking the town ends it.
		if (m_iPhase != PHASE_IDLE)
		{
			if (AROR_GarrisonPolicy.IsEnemyRole(role) && role != m_eRole)
			{
				m_sOutcome = "HANDOVER";
				EndResponse(false);
				m_eRole = role;
			}
			else
			{
				RefreshWatches();
				UpdateResponse();
				return;
			}
		}
		if (!AROR_GarrisonPolicy.IsEnemyRole(role))
		{
			if (m_iKills > 0 || !m_aWatches.IsEmpty())
				EndResponse(false);
			return;
		}
		if (role != m_eRole)
		{
			// A new holder starts its own count
			EndResponse(false);
			m_eRole = role;
		}

		RefreshWatches();
		if (m_iClock < m_iRearmAt)
			return;
		if (!m_bPostTakenTrigger && m_iKills < KillThreshold())
			return;
		// One town response on the map at a time: the garrison here fights on alone until that one ends
		if (s_ActiveResponse && s_ActiveResponse != this && s_ActiveResponse.IsResponding())
		{
			if (!m_bDeferredReported)
			{
				m_bDeferredReported = true;
				Print(string.Format("[AROR BALANCE] town_response_deferred town=%1 role=%2 kills=%3 busy=%4", m_Location.GetId(), m_eRole, m_iKills, s_ActiveResponse.GetLocation().GetId()), LogLevel.NORMAL);
				Headline("CONTACT REPORT  ·  %1 / Garrison engaged by " + ContactNames() + ". No reinforcements available", AROR_UIColors.WARN);
			}
			return;
		}
		// A deferred town whose attackers have since left does not send waves at nobody
		if (!IsResistanceNear())
			return;
		StartResponse();
	}

	//------------------------------------------------------------------------------------------------
	//! The kills that bring the response, from every defender seen since it armed
	int KillThreshold()
	{
		return Math.Max(m_iMinimumKills, Math.Ceil(m_aSeen.Count() * m_fTriggerShare));
	}

	//------------------------------------------------------------------------------------------------
	protected void StartResponse()
	{
		ResetResponse();
		m_iPhase = PHASE_ALERT;
		m_iAxisSide = 1;
		if (Math.RandomInt(0, 2) == 0)
			m_iAxisSide = -1;
		int delayMin = Math.Min(m_iAlertDelayMin, m_iAlertDelayMax);
		int delayMax = Math.Max(m_iAlertDelayMin, m_iAlertDelayMax);
		m_iNextWaveAt = m_iClock + Math.RandomInt(delayMin, delayMax + 1);
		if (UsesShockDoctrine())
		{
			float shockDelay = (m_iNextWaveAt - m_iClock) * Math.Clamp(m_fShockTempo, 0.4, 1);
			int delay = Math.Round(shockDelay);
			m_iNextWaveAt = m_iClock + Math.Max(5, delay);
		}
		m_iLastResistanceNear = m_iClock;
		s_ActiveResponse = this;

		// Snapshot for the lockdown (support to fall back to) and the balance log
		m_iStartedAt = m_iClock;
		m_iResponseKills = 0;
		m_iPeakEnemies = 0;
		m_iPeakFriendlies = 0;
		m_iNextContestCheck = m_iClock;
		m_iNextContestLog = m_iClock + CONTEST_LOG_SECONDS;
		m_fSupportAtContact = ResistanceSupport();
		if (m_bPostTakenTrigger)
			m_fSupportAtContact = m_fPostSupportBefore;
		m_bPostTakenTrigger = false;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
		{
			m_iCrackdownAtStart = operations.GetCrackdown(m_eRole);
			m_iReadinessAtStart = operations.GetReadinessPressure(m_eRole);
			Print(string.Format("[AROR BALANCE] town_response_start town=%1 role=%2 readinessLevel=%3 crackdownLevel=%4 readiness=%5 crackdown=%6 players=%7 defenders=%8 kills=%9", m_Location.GetId(), m_eRole, operations.GetReadinessLevel(m_eRole), operations.GetCrackdownLevel(m_eRole), m_iReadinessAtStart, m_iCrackdownAtStart, ResistancePlayersNear(), m_aSeen.Count(), m_iKills), LogLevel.NORMAL);
		}
		m_bContestActive = true;
		m_iContestSeconds = 0;
		m_iContestLimit = m_iLockdownSeconds;
		m_iContestEnemies = 0;
		m_iContestFriendlies = 0;
		Replication.BumpMe();
		Print(string.Format("[AROR TOWN] %1 response triggered: %2 player kills of %3 defenders seen; first wave in %4s", m_Location.GetId(), m_iKills, m_aSeen.Count(), m_iNextWaveAt - m_iClock), LogLevel.NORMAL);
		// The contact report: the garrison names who hit it before the response is on its way
		Headline("CONTACT REPORT  ·  %1 / Garrison engaged by " + ContactNames() + ". Reinforcements requested", AROR_UIColors.DANGER);
	}

	//! "Jasper", "Jasper and Rowan", "Jasper, Rowan and 2 others", or "Resistance fighters" when nobody is known
	protected string ContactNames()
	{
		array<string> names = {};
		foreach (int playerId : m_aContactPlayers)
		{
			string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
			if (!name.IsEmpty())
				names.Insert(name);
		}
		if (names.IsEmpty())
			return "Resistance fighters";
		if (names.Count() == 1)
			return names[0];
		if (names.Count() == 2)
			return names[0] + " and " + names[1];
		if (names.Count() == 3)
			return names[0] + ", " + names[1] + " and " + names[2];
		return names[0] + ", " + names[1] + " and " + (names.Count() - 2).ToString() + " others";
	}

	//! Compass side of a point as seen from the post: "north", "south-west"...
	protected static string CompassSide(vector from, vector to)
	{
		// World X runs east and Z north, so this is a bearing clockwise from north
		float bearing = Math.Atan2(to[0] - from[0], to[2] - from[2]) * Math.RAD2DEG;
		if (bearing < 0)
			bearing += 360;
		array<string> sides = {"north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west"};
		int sector = Math.Round(bearing / 45);
		return sides[sector - (sector / 8) * 8];
	}

	//! The response report for one wave, once its first column is on the road: where it comes from and when it
	//! arrives. "RESPONSE · MEAUX / Convoy 2 of 3 inbound from the south-west · ETA ~3 MIN"
	protected void AnnounceWave(int wave, string operationId)
	{
		if (m_aAnnouncedWaves.Contains(wave))
			return;
		m_aAnnouncedWaves.Insert(wave);
		array<string> sides = {};
		foreach (int index, string id : m_aOperationIds)
		{
			string side = m_aOperationDirections[index];
			if (m_aOperationWaves[index] == wave && !side.IsEmpty() && !sides.Contains(side))
				sides.Insert(side);
		}
		string text;
		if (wave == AIR_ASSAULT_WAVE)
			text = "Helicopters inbound";
		else if (sides.Count() > 1)
			text = "Two columns inbound from the " + sides[0] + " and " + sides[1] + ", wave " + wave.ToString() + " of " + m_iWaves.ToString();
		else
		{
			text = "Convoy " + wave.ToString() + " of " + m_iWaves.ToString() + " inbound";
			if (!sides.IsEmpty())
				text += " from the " + sides[0];
		}
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
		{
			string eta = AROR_OperationEta.Describe(operations.GetTownWaveEta(operationId));
			if (!eta.IsEmpty())
				text += "  ·  " + eta;
		}
		Headline("RESPONSE  ·  %1 / " + text, AROR_UIColors.WARN);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateResponse()
	{
		if (IsResistanceNear())
			m_iLastResistanceNear = m_iClock;
		else if (m_iClock - m_iLastResistanceNear >= m_iAbandonSeconds)
		{
			Abandon();
			return;
		}
		TrackFielded();
		if (m_iClock >= m_iNextContestCheck)
		{
			m_iNextContestCheck = m_iClock + CONTEST_CHECK_SECONDS;
			if (UpdateContest())
				return;
		}

		if (m_iPhase == PHASE_ALERT || m_iPhase == PHASE_WAVES)
		{
			if (m_iClock >= m_iNextWaveAt)
			{
				if (m_iWavesSent < m_iWaves)
					LaunchWave(m_iWavesSent + 1);
				if (m_iWavesSent >= m_iWaves)
				{
					m_iPhase = PHASE_RESOLVING;
					m_iResolveSince = m_iClock;
				}
			}
		}
		TryAirAssault();
		if (m_iPhase == PHASE_RESOLVING)
			TryResolve();
	}

	//------------------------------------------------------------------------------------------------
	//! Remember which operations reached the field, so the outcome knows whether anything was actually fought
	protected void TrackFielded()
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return;
		foreach (int index, string id : m_aOperationIds)
		{
			if (m_aFielded.Contains(id))
				continue;
			AROR_Operation operation = operations.GetOperation(id);
			if (operation && (operation.GetState() == AROR_EOperationState.ACTIVE || operation.GetState() == AROR_EOperationState.SUCCEEDED))
			{
				m_aFielded.Insert(id);
				// On the road: the response report goes out once per wave
				AnnounceWave(m_aOperationWaves[index], id);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every wave sent: the response is over once each of its operations has ended (destroyed, withdrawn or failed).
	//! Nothing is declared while any column or the assault is still out there, armour included.
	protected void TryResolve()
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return;
		foreach (string id : m_aOperationIds)
		{
			AROR_Operation operation = operations.GetOperation(id);
			if (operation && !operation.IsTerminal())
			{
				if (m_iClock - m_iResolveSince < m_iResolveTimeout)
					return;
				Print(string.Format("[AROR TOWN] %1 response timed out %2s after its last wave; ended without an outcome", m_Location.GetId(), m_iResolveTimeout), LogLevel.WARNING);
				m_sOutcome = "TIMEOUT";
				EndResponse(true);
				return;
			}
		}
		if (m_aFielded.IsEmpty())
		{
			Print(string.Format("[AROR TOWN] %1 response ended: no wave reached the field", m_Location.GetId()), LogLevel.WARNING);
			m_sOutcome = "NO_WAVE";
			EndResponse(true);
			return;
		}
		Repelled();
	}

	//------------------------------------------------------------------------------------------------
	//! The lockdown, every few seconds: count both sides around the town. While the army outnumbers the Resistance
	//! the clock runs up, otherwise it runs back down. True when the army has retaken the town (response ended).
	protected bool UpdateContest()
	{
		CountContest();
		m_bContestLosing = m_iScanEnemies > m_iScanFriendlies * m_fResistanceWeight;
		if (m_bContestLosing)
			m_iContestSeconds = Math.Min(m_iContestSeconds + CONTEST_CHECK_SECONDS, m_iLockdownSeconds);
		else
			m_iContestSeconds = Math.Max(m_iContestSeconds - CONTEST_CHECK_SECONDS, 0);
		m_iPeakEnemies = Math.Max(m_iPeakEnemies, m_iScanEnemies);
		m_iPeakFriendlies = Math.Max(m_iPeakFriendlies, m_iScanFriendlies);
		m_iContestEnemies = m_iScanEnemies;
		m_iContestFriendlies = m_iScanFriendlies;
		Replication.BumpMe();
		if (m_iClock >= m_iNextContestLog)
		{
			m_iNextContestLog = m_iClock + CONTEST_LOG_SECONDS;
			Print(string.Format("[AROR BALANCE] town_contest town=%1 role=%2 enemies=%3 resistance=%4 lockdown=%5/%6 phase=%7 waves=%8", m_Location.GetId(), m_eRole, m_iScanEnemies, m_iScanFriendlies, m_iContestSeconds, m_iLockdownSeconds, m_iPhase, m_iWavesSent), LogLevel.NORMAL);
		}
		if (m_iContestSeconds < m_iLockdownSeconds)
			return false;
		Retaken("%1 RETAKEN / Enemy forces held the town. Resistance support gains lost", "RETAKEN");
		return true;
	}

	//! Living fighters of the responding army and of the Resistance within the lockdown radius. The army's fighters
	//! (wave troops included) are watched too, so the players' kills among them count towards escalation.
	protected void CountContest()
	{
		m_iScanEnemies = 0;
		m_iScanFriendlies = 0;
		m_ScanRoles = AROR_FactionRolesComponent.GetInstance();
		if (!m_ScanRoles)
			return;
		GetGame().GetWorld().QueryEntitiesBySphere(m_Location.GetCenter(), ContestRadius(), OnContestEntity, null, EQueryEntitiesFlags.DYNAMIC);
	}

	//! Sphere query callback (public, as engine callbacks are in the base game); true keeps the query going
	bool OnContestEntity(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
			return true;
		if (AROR_LocationForces.IsInAircraft(character))
			return true;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return true;
		AROR_EFactionRole role;
		if (!m_ScanRoles.GetRoleOfFaction(affiliation.GetAffiliatedFaction(), role))
			return true;
		if (role == AROR_EFactionRole.RESISTANCE)
			m_iScanFriendlies++;
		else if (role == m_eRole)
		{
			m_iScanEnemies++;
			Watch(character);
		}
		return true;
	}

	protected float ContestRadius()
	{
		if (m_fContestRadius > 0)
			return m_fContestRadius;
		return Math.Clamp(m_Location.GetRadius(), 120, 300);
	}

	//! The army has the town back: the post is restored and the Resistance loses the support it gained since the
	//! contact (which can flip the town back through the normal support rule)
	protected void Retaken(string headline, string outcome)
	{
		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(m_Location.GetOwner().FindComponent(AROR_SettlementComponent));
		float support = ResistanceSupport();
		if (settlement && support > m_fSupportAtContact)
			settlement.SetSupport_S(AROR_EFactionRole.RESISTANCE, m_fSupportAtContact);
		if (m_Garrison && m_Garrison.IsPostBroken())
			m_Garrison.ReformSoon_S();
		Print(string.Format("[AROR TOWN] %1 retaken by role %2: Resistance support %3 -> %4", m_Location.GetId(), m_eRole, support, ResistanceSupport()), LogLevel.NORMAL);
		Headline(headline, AROR_UIColors.DANGER);
		m_sOutcome = outcome;
		EndResponse(true);
	}

	protected float ResistanceSupport()
	{
		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(m_Location.GetOwner().FindComponent(AROR_SettlementComponent));
		if (!settlement)
			return 0;
		return settlement.GetSupport(AROR_EFactionRole.RESISTANCE);
	}

	protected int ResistancePlayersNear()
	{
		int count;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (AROR_QRFSpotting.IsResistance(player) && vector.DistanceXZ(player.GetOrigin(), m_Location.GetCenter()) <= m_fAbandonDistance)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected void Repelled()
	{
		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(m_Location.GetOwner().FindComponent(AROR_SettlementComponent));
		if (settlement && m_fRepelledSupportSwing > 0)
			settlement.AddSupport_S(AROR_EFactionRole.RESISTANCE, m_fRepelledSupportSwing);
		Print(string.Format("[AROR TOWN] %1 response repelled: %2 of %3 operations fielded; Resistance support +%4", m_Location.GetId(), m_aFielded.Count(), m_aOperationIds.Count(), m_fRepelledSupportSwing), LogLevel.NORMAL);
		Headline(RepelledLine(), AROR_UIColors.REBEL);
		m_sOutcome = "REPELLED";
		EndResponse(true);
	}

	//! A line from the HQ pool; %1 is the town
	protected string RepelledLine()
	{
		array<string> lines = {
			"%1 HOLDS / The enemy assault is broken",
			"%1 HOLDS / Enemy columns pull back from the town",
			"%1 HOLDS / The counterattack has been beaten off",
			"%1 HOLDS / Enemy reinforcements destroyed",
			"%1 HOLDS / The garrison's relief has failed",
			"%1 HOLDS / The enemy falls back, support for the Resistance rises"
		};
		return lines[Math.RandomInt(0, lines.Count())];
	}

	//------------------------------------------------------------------------------------------------
	//! No Resistance near for the abandon time: end outstanding waves through Operations retention. With retake on,
	//! the town is retaken as by the lockdown (post restored, support gains lost).
	protected void Abandon()
	{
		Print(string.Format("[AROR TOWN] %1 response abandoned after %2 of %3 waves: no Resistance near", m_Location.GetId(), m_iWavesSent, m_iWaves), LogLevel.NORMAL);
		bool gained = ResistanceSupport() > m_fSupportAtContact || (m_Garrison && m_Garrison.IsPostBroken());
		if (m_bRetakeWhenAbandoned && gained)
		{
			Retaken("%1 RETAKEN / The Resistance pulled out. Support gains lost", "ABANDONED");
			return;
		}
		m_sOutcome = "ABANDONED";
		EndResponse(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one wave (one or two columns). Retried shortly when no staging road or capacity is free yet; skipped
	//! after repeated failures or when the army has no recipe for it.
	protected void LaunchWave(int wave)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		AROR_OperationExecutorComponent executor = Executor();
		m_iPhase = PHASE_WAVES;
		m_iNextWaveAt = m_iClock + STAGING_RETRY_SECONDS;
		// Bound all rejection paths, including missing services and rejected requests, not just road searches.
		m_iStagingFailures++;
		if (m_iStagingFailures > STAGING_ATTEMPTS)
		{
			Print(string.Format("[AROR TOWN] %1 wave %2 exhausted its launch attempts; skipped", m_Location.GetId(), wave), LogLevel.WARNING);
			SkipWave();
			return;
		}
		if (!operations || !executor)
			return;

		int readiness = Math.Clamp(operations.GetReadinessLevel(m_eRole), 1, 5);
		int crackdown = Math.Clamp(operations.GetCrackdownLevel(m_eRole), 1, 5);
		string recipe = executor.FindTownWaveRecipeId_S(m_eRole, wave, readiness);
		if (recipe.IsEmpty())
		{
			Print(string.Format("[AROR TOWN] %1 has no wave %2 recipe for role %3 at readiness %4; wave skipped", m_Location.GetId(), wave, m_eRole, readiness), LogLevel.WARNING);
			SkipWave();
			return;
		}

		int elements = WaveElements(wave, crackdown);
		int columns = 1;
		int splitFrom = m_iWaves;
		if (UsesShockDoctrine())
			splitFrom = Math.Clamp(m_iShockSplitFromWave, 1, m_iWaves);
		if (wave >= splitFrom && m_iSplitMinElements > 0 && elements >= m_iSplitMinElements)
			columns = 2;

		// One staging road per column, each on its own axis away from the previous wave's roads
		vector post = PostPosition();
		array<vector> stagings = {};
		array<vector> avoid = {};
		avoid.Copy(m_aLastAxes);
		for (int column = 0; column < columns; column++)
		{
			vector staging;
			if (!FindStaging(executor, post, m_iAxisIndex + column, avoid, staging))
				break;
			stagings.Insert(staging);
			avoid.Insert(staging);
		}
		if (stagings.IsEmpty())
		{
			Print(string.Format("[AROR TOWN] %1 wave %2 has no safe road staging %3-%4m out (attempt %5)", m_Location.GetId(), wave, m_fStagingMinDistance, m_fStagingMaxDistance, m_iStagingFailures), LogLevel.WARNING);
			return;
		}
		columns = stagings.Count();

		// The lead column takes the recipe's first elements (its heaviest), the second the rest
		int leadElements = elements;
		if (columns == 2)
			leadElements = (elements + 1) / 2;
		array<string> sent = {};
		array<string> sides = {};
		for (int index = 0; index < columns; index++)
		{
			int offset = 0;
			int limit = leadElements;
			if (index == 1)
			{
				offset = leadElements;
				limit = elements - leadElements;
			}
			float dismount = m_fDismountDistance;
			if (UsesShockDoctrine())
				dismount = Math.Clamp(m_fShockDismountDistance, 100, 290);
			string id = operations.RequestTownWave_S(m_eRole, m_Location.GetId(), recipe, readiness, crackdown, stagings[index], limit, dismount, offset);
			if (id.IsEmpty())
				continue;
			sent.Insert(id);
			sides.Insert(CompassSide(post, stagings[index]));
			Print(string.Format("[AROR TOWN] %1 wave %2/%3 column %4/%5 requested: %6 recipe=%7 elements=%8+%9", m_Location.GetId(), wave, m_iWaves, index + 1, columns, id, recipe, offset, limit), LogLevel.NORMAL);
		}
		if (sent.IsEmpty())
			return;

		Print(string.Format("[AROR BALANCE] town_wave town=%1 role=%2 wave=%3 columns=%4 elements=%5 recipe=%6 readinessLevel=%7 crackdownLevel=%8 seconds=%9", m_Location.GetId(), m_eRole, wave, sent.Count(), elements, recipe, readiness, crackdown, m_iClock - m_iStartedAt), LogLevel.NORMAL);
		// Reported once the first column is on the road (AnnounceWave), with its side and arrival estimate
		foreach (int sentIndex, string sentId : sent)
			TrackOperation(sentId, wave, sides[sentIndex]);
		m_aLastAxes.Copy(stagings);
		m_iAxisIndex += columns;
		m_iStagingFailures = 0;
		m_iWavesSent++;
		m_iNextWaveAt = m_iClock + NextInterval();
	}

	protected void TrackOperation(string operationId, int wave, string side)
	{
		m_aOperationIds.Insert(operationId);
		m_aOperationWaves.Insert(wave);
		m_aOperationDirections.Insert(side);
	}

	protected void SkipWave()
	{
		m_iWavesSent++;
		m_iStagingFailures = 0;
		m_iNextWaveAt = m_iClock + NextInterval();
	}

	protected int NextInterval()
	{
		int jitter = Math.Max(m_iWaveIntervalJitter, 0);
		int interval = m_iWaveInterval + Math.RandomInt(-jitter, jitter + 1);
		if (UsesShockDoctrine())
		{
			// Scale in float, then round back explicitly: no int-by-float compound assignment
			float scaled = interval * Math.Clamp(m_fShockTempo, 0.4, 1);
			interval = Math.Round(scaled);
		}
		return Math.Max(30, interval);
	}

	//------------------------------------------------------------------------------------------------
	//! A staging road for the axis-th column of this response. Axes rotate around the bearing to the army's nearest
	//! base (0, then 90 degrees to one side, 90 to the other, then the diagonals). Falls back to any road when every
	//! road on a new axis is taken.
	protected bool FindStaging(AROR_OperationExecutorComponent executor, vector post, int axis, array<vector> avoid, out vector staging)
	{
		vector base = NearestFriendlyBase(post);
		vector heading = base - post;
		heading[1] = 0;
		float bearing = Math.RandomFloat(0, Math.PI2);
		if (heading.Length() > 1)
			bearing = Math.Atan2(heading[0], heading[2]);
		array<float> offsets = {0.0, 90.0, -90.0, 45.0, -45.0, 135.0};
		int slot = axis - (axis / offsets.Count()) * offsets.Count();
		float offset = offsets[slot] * m_iAxisSide * Math.DEG2RAD;
		vector towards = post + Vector(Math.Sin(bearing + offset), 0, Math.Cos(bearing + offset)) * AXIS_PREFERENCE_DISTANCE;
		float maxDistance = Math.Max(m_fStagingMinDistance, m_fStagingMaxDistance);
		if (executor.FindWaveStaging_S(post, towards, m_fStagingMinDistance, maxDistance, staging, avoid, m_fAxisSeparation))
			return true;
		// A repeated road beats no wave, unless this is a second column (the lead column already has that road) or
		// nothing was avoided in the first place
		if (avoid.IsEmpty() || avoid.Count() > m_aLastAxes.Count())
			return false;
		return executor.FindWaveStaging_S(post, towards, m_fStagingMinDistance, maxDistance, staging);
	}

	//------------------------------------------------------------------------------------------------
	//! Commit one transport/gunship insertion from owned bases once a ground wave is sent (or presses the post when
	//! configured). Physical launch, occupancy and landing safety remain owned by the airborne executor.
	protected void TryAirAssault()
	{
		if (m_bAirRequested || m_iAirAssaultReadiness <= 0 || m_iPhase == PHASE_ALERT || m_iClock < m_iNextAirAttempt)
			return;
		m_iNextAirAttempt = m_iClock + PRESSING_CHECK_SECONDS;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		AROR_OperationExecutorComponent executor = Executor();
		if (!operations || !executor)
			return;
		int readiness = Math.Clamp(operations.GetReadinessLevel(m_eRole), 1, 5);
		if (readiness < m_iAirAssaultReadiness)
			return;
		vector post = PostPosition();
		bool pressing = m_bAirOnFirstWave && m_iWavesSent > 0;
		if (!pressing)
		{
			foreach (string id : m_aOperationIds)
			{
				if (executor.CountPressingPackages_S(id, post, m_fPressingDistance) > 0)
				{
					pressing = true;
					break;
				}
			}
		}
		if (!pressing)
			return;

		string recipe = executor.FindTownWaveRecipeId_S(m_eRole, AIR_ASSAULT_WAVE, readiness);
		if (recipe.IsEmpty())
		{
			m_bAirRequested = true;
			Print(string.Format("[AROR TOWN] %1 has no helicopter assault recipe for role %2 at readiness %3", m_Location.GetId(), m_eRole, readiness), LogLevel.NORMAL);
			return;
		}
		int crackdown = Math.Clamp(operations.GetCrackdownLevel(m_eRole), 1, 5);
		string assault = operations.RequestTownWave_S(m_eRole, m_Location.GetId(), recipe, readiness, crackdown, vector.Zero, 0, 0);
		if (assault.IsEmpty())
			return;
		m_bAirRequested = true;
		TrackOperation(assault, AIR_ASSAULT_WAVE, string.Empty);
		Print(string.Format("[AROR TOWN] %1 helicopter assault requested: %2 recipe=%3", m_Location.GetId(), assault, recipe), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Crackdown sizes each wave: 2 elements at levels 1-2, 3 at 3-4, 4 at 5; the last wave and cities add one
	protected int WaveElements(int wave, int crackdown)
	{
		int elements = 1 + (crackdown + 1) / 2 + m_iExtraElements;
		if (wave >= m_iWaves)
			elements++;
		if (m_Location.GetLocationType() == AROR_ELocationType.CITY)
			elements++;
		return Math.Clamp(elements, 1, 8);
	}

	//------------------------------------------------------------------------------------------------
	//! A headline to everyone; %1 in text is the town's name
	protected void Headline(string text, int colorKey)
	{
		string name = m_Location.GetName();
		name.ToUpper();
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.HEADLINE, string.Format(text, name), m_Location.GetMapIcon(), AROR_UIColors.Get(colorKey), "INTEL"));
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetResponse()
	{
		m_iPhase = PHASE_IDLE;
		m_iWavesSent = 0;
		m_iAxisIndex = 0;
		m_iStagingFailures = 0;
		m_bAirRequested = false;
		m_iNextAirAttempt = 0;
		m_aOperationIds.Clear();
		m_aOperationWaves.Clear();
		m_aOperationDirections.Clear();
		m_aFielded.Clear();
		m_aAnnouncedWaves.Clear();
		m_aLastAxes.Clear();
	}

	//! End the response and cancel its outstanding children through normal Operations retention; kills count after the
	//! rearm time when it ran, at once when the town changed hands
	protected void EndResponse(bool rearmDelay)
	{
		if (m_iPhase != PHASE_IDLE)
		{
			Print(string.Format("[AROR TOWN] %1 response ended after %2 waves", m_Location.GetId(), m_iWavesSent), LogLevel.NORMAL);
			LogResponseEnd();
		}
		// Abandonment, handover and timeout must not leave queued reinforcements launching after the town slot frees.
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
		{
			foreach (string id : m_aOperationIds)
			{
				AROR_Operation operation = operations.GetOperation(id);
				if (operation && !operation.IsTerminal())
					operations.ResolveOperation_S(id, AROR_EOperationState.CANCELLED, "Town response ended: " + m_sOutcome);
			}
		}
		if (s_ActiveResponse == this)
			s_ActiveResponse = null;
		if (m_bContestActive)
		{
			m_bContestActive = false;
			m_bContestLosing = false;
			m_iContestSeconds = 0;
			m_iContestLimit = 0;
			Replication.BumpMe();
		}
		m_sOutcome = string.Empty;
		m_bDeferredReported = false;
		m_bPostTakenTrigger = false;
		ResetResponse();
		m_iKills = 0;
		m_aSeen.Clear();
		m_aContactPlayers.Clear();
		ClearWatches();
		if (rearmDelay)
			m_iRearmAt = m_iClock + m_iRearmSeconds;
		else
			m_iRearmAt = m_iClock;
	}

	//! One line per response for tuning: how it went and what it did to the army and the town
	protected void LogResponseEnd()
	{
		string outcome = m_sOutcome;
		if (outcome.IsEmpty())
			outcome = "ENDED";
		int crackdown = -1;
		int readiness = -1;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
		{
			crackdown = operations.GetCrackdown(m_eRole);
			readiness = operations.GetReadinessPressure(m_eRole);
		}
		Print(string.Format("[AROR BALANCE] town_response_end town=%1 role=%2 outcome=%3 seconds=%4 waves=%5 fielded=%6 kills=%7 peakEnemies=%8 peakResistance=%9", m_Location.GetId(), m_eRole, outcome, m_iClock - m_iStartedAt, m_iWavesSent, m_aFielded.Count(), m_iResponseKills, m_iPeakEnemies, m_iPeakFriendlies), LogLevel.NORMAL);
		Print(string.Format("[AROR BALANCE] town_response_effect town=%1 role=%2 outcome=%3 support=%4->%5 crackdown=%6->%7 readiness=%8->%9", m_Location.GetId(), m_eRole, outcome, m_fSupportAtContact, ResistanceSupport(), m_iCrackdownAtStart, crackdown, m_iReadinessAtStart, readiness), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected AROR_OperationExecutorComponent Executor()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;
		return AROR_OperationExecutorComponent.Cast(gameMode.FindComponent(AROR_OperationExecutorComponent));
	}

	protected vector PostPosition()
	{
		if (m_Garrison && m_Garrison.IsEligible())
			return m_Garrison.GetDefenceCenter();
		return m_Location.GetCenter();
	}

	//------------------------------------------------------------------------------------------------
	//! Centre of the holding army's nearest military site, or the post itself when it has none (any direction)
	protected vector NearestFriendlyBase(vector post)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return post;
		array<AROR_LocationComponent> sites = {};
		locations.GetLocations(sites);
		vector best = post;
		float bestDistance = float.MAX;
		foreach (AROR_LocationComponent site : sites)
		{
			if (!site || site == m_Location || site.GetControllingRole() != m_eRole || !AROR_QRFOriginPolicy.IsOriginType(site.GetLocationType()))
				continue;
			float distance = vector.DistanceXZ(site.GetCenter(), post);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = site.GetCenter();
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsResistanceNear()
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (AROR_QRFSpotting.IsResistance(player) && vector.DistanceXZ(player.GetOrigin(), m_Location.GetCenter()) <= m_fAbandonDistance)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Watch every living defender (garrison infantry and crews, checkpoint guards, patrol crew) and count the deaths
	//! that a player caused
	protected void RefreshWatches()
	{
		for (int index = m_aWatches.Count() - 1; index >= 0; index--)
		{
			AROR_TownCasualtyWatch watch = m_aWatches[index];
			if (!watch.m_Entity)
			{
				// Retirement can delete a living defender after a player hit; that is not a kill
				watch.Detach_S();
				m_aWatches.Remove(index);
				continue;
			}
			if (!watch.IsDead())
				continue;
			if (watch.WasPlayerKill(KILL_CREDIT_SECONDS))
			{
				if (m_iPhase != PHASE_IDLE || m_iClock >= m_iRearmAt)
					m_iKills++;
				if (m_iPhase != PHASE_IDLE)
					m_iResponseKills++;
				if (!m_aContactPlayers.Contains(watch.m_iPlayerId))
					m_aContactPlayers.Insert(watch.m_iPlayerId);
				Print(string.Format("[AROR TOWN] %1 defender killed by player %2 (%3/%4)", m_Location.GetId(), watch.m_iPlayerId, m_iKills, KillThreshold()), LogLevel.NORMAL);
				// Every enemy defeated in the town escalates its army, at once
				AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
				if (operations && (m_iKillCrackdown > 0 || m_iKillReadiness > 0))
					operations.RecordTownEscalation_S(m_eRole, m_iKillCrackdown, m_iKillReadiness, m_Location.GetId(), "kill");
			}
			watch.Detach_S();
			m_aWatches.Remove(index);
		}

		array<IEntity> defenders = {};
		if (m_Garrison)
		{
			array<ref AROR_GarrisonSlot> slots = {};
			m_Garrison.GetSlots(slots);
			foreach (AROR_GarrisonSlot slot : slots)
			{
				// Infantry and crews: the people a player can kill
				if ((slot.m_iKind == 0 || slot.m_iKind == 4) && slot.m_Entity)
					defenders.Insert(slot.m_Entity);
			}
		}
		if (m_TownLife)
			m_TownLife.GetMilitaryMembers_S(defenders);

		foreach (IEntity defender : defenders)
		{
			if (Watch(defender) && m_iClock >= m_iRearmAt && !m_aSeen.Contains(defender))
				m_aSeen.Insert(defender);
		}
	}

	//! Start counting a character's death if a player causes it; true once it is watched. Wave troops met by the
	//! lockdown scan are watched too, but only the town's own defenders count towards the trigger (m_aSeen).
	protected bool Watch(IEntity character)
	{
		if (!character)
			return false;
		// A support flip may replace the post with Resistance troops while the original army's response continues.
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		AROR_EFactionRole role;
		if (!roles || !affiliation || !roles.GetRoleOfFaction(affiliation.GetAffiliatedFaction(), role) || role != m_eRole)
			return false;
		if (IsWatched(character))
			return true;
		if (m_aWatches.Count() >= MAX_WATCHES)
			return false;
		AROR_TownCasualtyWatch added = new AROR_TownCasualtyWatch();
		if (!added.Attach_S(character))
			return false;
		added.BindContact_S(m_eRole, m_Location.GetId());
		m_aWatches.Insert(added);
		return true;
	}

	protected bool IsWatched(IEntity entity)
	{
		foreach (AROR_TownCasualtyWatch watch : m_aWatches)
		{
			if (watch.m_Entity == entity)
				return true;
		}
		return false;
	}

	protected void ClearWatches()
	{
		foreach (AROR_TownCasualtyWatch watch : m_aWatches)
			watch.Detach_S();
		m_aWatches.Clear();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Location = AROR_LocationComponent.Cast(owner.FindComponent(AROR_LocationComponent));
		m_Garrison = AROR_GarrisonComponent.Cast(owner.FindComponent(AROR_GarrisonComponent));
		m_TownLife = AROR_TownLifeComponent.Cast(owner.FindComponent(AROR_TownLifeComponent));
		if (!GetGame().InPlayMode())
			return;
		if (!s_aTowns.Contains(this))
			s_aTowns.Insert(this);
		if (!Replication.IsServer())
			return;
		if (!m_Location)
		{
			Print("[AROR TOWN] Town waves need a location component; disabled", LogLevel.ERROR);
			return;
		}
		m_eRole = m_Location.GetControllingRole();
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && Replication.IsServer() && m_iPhase != PHASE_IDLE)
		{
			m_sOutcome = "REMOVED";
			EndResponse(false);
		}
		if (GetGame())
			GetGame().GetCallqueue().Remove(Tick);
		ClearWatches();
		s_aTowns.RemoveItem(this);
		if (s_ActiveResponse == this)
			s_ActiveResponse = null;
		super.OnDelete(owner);
	}
}

//! One character's damage subscription. Remembers the last player who hurt it and when (world seconds); the owner
//! checks for death. Shared by town waves (defenders) and town life (civilians).
class AROR_TownCasualtyWatch
{
	IEntity m_Entity;
	int m_iPlayerId;
	int m_iHitAt;
	protected SCR_DamageManagerComponent m_DamageManager;
	protected AROR_EFactionRole m_eReportingRole;
	protected string m_sReportingTownId;	//!< Empty for civilian watches: they never send combat reports

	void BindContact_S(AROR_EFactionRole role, string townId)
	{
		if (!Replication.IsServer())
			return;
		m_eReportingRole = role;
		m_sReportingTownId = townId;
	}

	static int NowSeconds()
	{
		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//! True if a player hurt the character within the last seconds
	bool WasPlayerKill(int seconds)
	{
		return m_iPlayerId > 0 && NowSeconds() - m_iHitAt <= seconds;
	}

	bool Attach_S(IEntity entity)
	{
		if (!Replication.IsServer() || !entity)
			return false;
		m_DamageManager = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if (!m_DamageManager || m_DamageManager.IsDestroyed())
			return false;
		m_Entity = entity;
		m_DamageManager.GetOnDamage().Insert(OnDamage);
		return true;
	}

	//! Only observed deaths count. A missing reference alone may be ordinary retirement.
	bool IsDead()
	{
		if (!m_Entity || !m_DamageManager)
			return false;
		if (m_DamageManager.IsDestroyed())
			return true;
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Entity);
		return character && character.GetCharacterController() && character.GetCharacterController().IsDead();
	}

	protected void OnDamage(BaseDamageContext context)
	{
		if (!Replication.IsServer() || !context || !context.instigator || context.damageValue <= 0)
			return;
		if (context.damageType == EDamageType.HEALING || context.damageType == EDamageType.REGENERATION)
			return;
		if (context.instigator.GetInstigatorType() == InstigatorType.INSTIGATOR_GM)
			return;
		// The contact report is best effort (it needs both sides' factions, which some weapons and vehicles do not
		// carry); a player's hit always counts towards the kill, so the trigger and escalation never miss it
		if (!m_sReportingTownId.IsEmpty())
			ReportContact_S(context);
		int playerId = context.instigator.GetInstigatorPlayerID();
		if (playerId <= 0)
			return;
		m_iPlayerId = playerId;
		m_iHitAt = NowSeconds();
	}

	//! Damage-source snapshot, not a visual sighting. Shared with every friendly column through Operations.
	//! Validate affiliation/diplomacy like QRFDefense; AI-on-AI contacts may report too, but never award player kills.
	protected bool ReportContact_S(BaseDamageContext context)
	{
		if (m_sReportingTownId.IsEmpty() || !m_Entity)
			return false;
		IEntity attacker = context.instigator.GetInstigatorEntity();
		if (!attacker || attacker == m_Entity)
			return false;
		FactionAffiliationComponent victimAffiliation = FactionAffiliationComponent.Cast(m_Entity.FindComponent(FactionAffiliationComponent));
		FactionAffiliationComponent attackerAffiliation = FactionAffiliationComponent.Cast(attacker.FindComponent(FactionAffiliationComponent));
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles || !victimAffiliation || !attackerAffiliation)
			return false;
		Faction victimFaction = victimAffiliation.GetAffiliatedFaction();
		Faction attackerFaction = attackerAffiliation.GetAffiliatedFaction();
		AROR_EFactionRole victimRole;
		if (!victimFaction || !attackerFaction || !roles.GetRoleOfFaction(victimFaction, victimRole) || victimRole != m_eReportingRole || !victimFaction.IsFactionEnemy(attackerFaction))
			return false;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (operations)
			operations.ReportCombatContact_S(m_eReportingRole, m_sReportingTownId, attacker.GetOrigin(), false);
		return true;
	}

	void Detach_S()
	{
		if (m_DamageManager)
			m_DamageManager.GetOnDamage().Remove(OnDamage);
		m_DamageManager = null;
		m_Entity = null;
	}
}
