[ComponentEditorProps(category: "OpenResistance/Locations", description: "Town garrison response: once players kill a share of a town's defenders (post, checkpoints, patrol), the holding army sends convoy waves from roads nearby, a helicopter assault once they press the post, and ends repelled or holding. Separate from QRFs. Put on a town or city location with a settlement garrison post.")]
class AROR_TownWavesComponentClass : ScriptComponentClass {}

//! Server logic for one town's wave response, in phases:
//! - ALERT: the trigger kill; the first wave forms up after a short random delay.
//! - WAVES: each wave is the holding army's wave recipe at its current Readiness (equipment), sized by its Crackdown
//!   (element count), paid by the scenario, driven as a convoy from a road 1-2 km out that dismounts short of the post.
//!   Waves come in on different roads (axes); the last one splits into two columns on two roads.
//! - A helicopter assault (joint recipe) commits once a wave is pressing the post, at high Readiness.
//! - RESOLVING: once every wave and the assault have ended, the town holds: a support reward and an HQ headline.
//!   If the Resistance leaves instead, the army retakes a broken post at once.
//! Waves go through Operations (RequestTownWave_S). Session-only: kill progress and a running response are not saved.
class AROR_TownWavesComponent : ScriptComponent
{
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
	[Attribute("3", desc: "The last wave splits into two columns on two roads when it has at least this many elements (0 = never)", params: "0 8 1")]
	protected int m_iSplitMinElements;
	[Attribute("4", desc: "Lowest Readiness at which a helicopter assault joins the response (0 = never). Uses the army's wave 9 recipe.", params: "0 5 1")]
	protected int m_iAirAssaultReadiness;
	[Attribute("350", desc: "A wave package this close to the post is pressing it; that commits the helicopter assault, metres", params: "100 1000 25")]
	protected float m_fPressingDistance;
	[Attribute("10", desc: "Resistance support gained in the town when the whole response is beaten off, percent", params: "0 50 1")]
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
	protected static const float AXIS_PREFERENCE_DISTANCE = 1000;

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
	protected ref array<string> m_aFielded = {};	//!< Operations of the running response that reached the field
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
		if (!AROR_GarrisonPolicy.IsEnemyRole(role))
		{
			if (m_iPhase != PHASE_IDLE || m_iKills > 0 || !m_aWatches.IsEmpty())
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
		if (m_iPhase != PHASE_IDLE)
		{
			UpdateResponse();
			return;
		}
		if (m_iClock < m_iRearmAt)
			return;
		if (m_iKills >= KillThreshold())
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
		m_iLastResistanceNear = m_iClock;
		Print(string.Format("[AROR TOWN] %1 response triggered: %2 player kills of %3 defenders seen; first wave in %4s", m_Location.GetId(), m_iKills, m_aSeen.Count(), m_iNextWaveAt - m_iClock), LogLevel.NORMAL);
		Headline("%1 GARRISON CALLS FOR HELP / Enemy reinforcements are forming up", AROR_UIColors.DANGER);
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
		if (m_iClock % PRESSING_CHECK_SECONDS == 0)
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
		foreach (string id : m_aOperationIds)
		{
			if (m_aFielded.Contains(id))
				continue;
			AROR_Operation operation = operations.GetOperation(id);
			if (operation && (operation.GetState() == AROR_EOperationState.ACTIVE || operation.GetState() == AROR_EOperationState.SUCCEEDED))
				m_aFielded.Insert(id);
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
				EndResponse(true);
				return;
			}
		}
		if (m_aFielded.IsEmpty())
		{
			Print(string.Format("[AROR TOWN] %1 response ended: no wave reached the field", m_Location.GetId()), LogLevel.WARNING);
			EndResponse(true);
			return;
		}
		Repelled();
	}

	//------------------------------------------------------------------------------------------------
	protected void Repelled()
	{
		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(m_Location.GetOwner().FindComponent(AROR_SettlementComponent));
		if (settlement && m_fRepelledSupportSwing > 0)
			settlement.AddSupport_S(AROR_EFactionRole.RESISTANCE, m_fRepelledSupportSwing);
		Print(string.Format("[AROR TOWN] %1 response repelled: %2 of %3 operations fielded; Resistance support +%4", m_Location.GetId(), m_aFielded.Count(), m_aOperationIds.Count(), m_fRepelledSupportSwing), LogLevel.NORMAL);
		Headline(RepelledLine(), AROR_UIColors.REBEL);
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
	//! No Resistance near for the abandon time: waves already on the road carry on under Operations, and the army
	//! takes back a broken garrison post at once
	protected void Abandon()
	{
		Print(string.Format("[AROR TOWN] %1 response abandoned after %2 of %3 waves: no Resistance near", m_Location.GetId(), m_iWavesSent, m_iWaves), LogLevel.NORMAL);
		if (m_bRetakeWhenAbandoned && m_Garrison && m_Garrison.IsPostBroken())
		{
			m_Garrison.ReformSoon_S();
			Headline("%1 GARRISON RESTORED / The army has retaken the post", AROR_UIColors.DANGER);
		}
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
		if (wave >= m_iWaves && m_iSplitMinElements > 0 && elements >= m_iSplitMinElements)
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
			m_iStagingFailures++;
			Print(string.Format("[AROR TOWN] %1 wave %2 has no safe road staging %3-%4m out (attempt %5)", m_Location.GetId(), wave, m_fStagingMinDistance, m_fStagingMaxDistance, m_iStagingFailures), LogLevel.WARNING);
			if (m_iStagingFailures >= STAGING_ATTEMPTS)
				SkipWave();
			return;
		}
		columns = stagings.Count();

		// The lead column takes the recipe's first elements (its heaviest), the second the rest
		int leadElements = elements;
		if (columns == 2)
			leadElements = (elements + 1) / 2;
		array<string> sent = {};
		for (int index = 0; index < columns; index++)
		{
			int offset = 0;
			int limit = leadElements;
			if (index == 1)
			{
				offset = leadElements;
				limit = elements - leadElements;
			}
			string id = operations.RequestTownWave_S(m_eRole, m_Location.GetId(), recipe, readiness, crackdown, stagings[index], limit, m_fDismountDistance, offset);
			if (id.IsEmpty())
				continue;
			sent.Insert(id);
			Print(string.Format("[AROR TOWN] %1 wave %2/%3 column %4/%5 requested: %6 recipe=%7 elements=%8+%9", m_Location.GetId(), wave, m_iWaves, index + 1, columns, id, recipe, offset, limit), LogLevel.NORMAL);
		}
		if (sent.IsEmpty())
			return;

		foreach (string sentId : sent)
			m_aOperationIds.Insert(sentId);
		m_aLastAxes.Copy(stagings);
		m_iAxisIndex += columns;
		m_iStagingFailures = 0;
		m_iWavesSent++;
		m_iNextWaveAt = m_iClock + NextInterval();
		string waveText = " wave " + wave.ToString() + " of " + m_iWaves.ToString();
		if (sent.Count() > 1)
			Headline("%1 UNDER ATTACK / Two enemy columns inbound," + waveText, AROR_UIColors.DANGER);
		else
			Headline("%1 UNDER ATTACK / Enemy convoy inbound," + waveText, AROR_UIColors.DANGER);
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
		return Math.Max(30, m_iWaveInterval + Math.RandomInt(-jitter, jitter + 1));
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
		float offset = offsets[axis % offsets.Count()] * m_iAxisSide * Math.DEG2RAD;
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
	//! At high Readiness, once a wave is pressing the post, the army commits one helicopter assault (its wave 9 joint
	//! recipe) from its own bases, as an ordinary airborne response would launch
	protected void TryAirAssault()
	{
		if (m_bAirRequested || m_iAirAssaultReadiness <= 0 || m_iPhase == PHASE_ALERT)
			return;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		AROR_OperationExecutorComponent executor = Executor();
		if (!operations || !executor)
			return;
		int readiness = Math.Clamp(operations.GetReadinessLevel(m_eRole), 1, 5);
		if (readiness < m_iAirAssaultReadiness)
			return;
		vector post = PostPosition();
		bool pressing;
		foreach (string id : m_aOperationIds)
		{
			if (executor.CountPressingPackages_S(id, post, m_fPressingDistance) > 0)
			{
				pressing = true;
				break;
			}
		}
		if (!pressing)
			return;

		m_bAirRequested = true;
		string recipe = executor.FindTownWaveRecipeId_S(m_eRole, AIR_ASSAULT_WAVE, readiness);
		if (recipe.IsEmpty())
		{
			Print(string.Format("[AROR TOWN] %1 has no helicopter assault recipe for role %2 at readiness %3", m_Location.GetId(), m_eRole, readiness), LogLevel.NORMAL);
			return;
		}
		int crackdown = Math.Clamp(operations.GetCrackdownLevel(m_eRole), 1, 5);
		string assault = operations.RequestTownWave_S(m_eRole, m_Location.GetId(), recipe, readiness, crackdown, vector.Zero, 0, 0);
		if (assault.IsEmpty())
			return;
		m_aOperationIds.Insert(assault);
		Print(string.Format("[AROR TOWN] %1 helicopter assault requested: %2 recipe=%3", m_Location.GetId(), assault, recipe), LogLevel.NORMAL);
		Headline("%1 UNDER ATTACK / Enemy helicopters inbound", AROR_UIColors.DANGER);
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
		m_aOperationIds.Clear();
		m_aFielded.Clear();
		m_aLastAxes.Clear();
	}

	//! End the response (operations already on the road carry on under Operations); kills count again after the
	//! rearm time when it ran, at once when the town changed hands
	protected void EndResponse(bool rearmDelay)
	{
		if (m_iPhase != PHASE_IDLE)
			Print(string.Format("[AROR TOWN] %1 response ended after %2 waves", m_Location.GetId(), m_iWavesSent), LogLevel.NORMAL);
		ResetResponse();
		m_iKills = 0;
		m_aSeen.Clear();
		ClearWatches();
		if (rearmDelay)
			m_iRearmAt = m_iClock + m_iRearmSeconds;
		else
			m_iRearmAt = m_iClock;
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
				m_iKills++;
				Print(string.Format("[AROR TOWN] %1 defender killed by player %2 (%3/%4)", m_Location.GetId(), watch.m_iPlayerId, m_iKills, KillThreshold()), LogLevel.NORMAL);
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
			if (!defender || IsWatched(defender))
				continue;
			AROR_TownCasualtyWatch added = new AROR_TownCasualtyWatch();
			if (!added.Attach_S(defender))
				continue;
			m_aWatches.Insert(added);
			if (!m_aSeen.Contains(defender))
				m_aSeen.Insert(defender);
		}
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
		if (!GetGame().InPlayMode() || !Replication.IsServer())
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
		GetGame().GetCallqueue().Remove(Tick);
		ClearWatches();
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
		if (!context || !context.instigator || context.damageValue <= 0)
			return;
		if (context.damageType == EDamageType.HEALING || context.damageType == EDamageType.REGENERATION)
			return;
		int playerId = context.instigator.GetInstigatorPlayerID();
		if (playerId <= 0)
			return;
		m_iPlayerId = playerId;
		m_iHitAt = NowSeconds();
	}

	void Detach_S()
	{
		if (m_DamageManager)
			m_DamageManager.GetOnDamage().Remove(OnDamage);
		m_DamageManager = null;
		m_Entity = null;
	}
}
