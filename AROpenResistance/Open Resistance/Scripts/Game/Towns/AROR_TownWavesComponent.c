[ComponentEditorProps(category: "OpenResistance/Locations", description: "Town garrison response: once players kill a share of a town's defenders (post, checkpoints, patrol), the holding army sends convoy waves from a road staging point nearby. Separate from QRFs. Put on a town or city location with a settlement garrison post.")]
class AROR_TownWavesComponentClass : ScriptComponentClass {}

//! Server logic for one town's wave response. Waves go through Operations (RequestTownWave_S): the holding army's
//! wave recipe for that wave at its current Readiness (equipment), sized by its Crackdown (element count), paid by
//! the scenario, launched from a road 1-2 km out on the side of the army's nearest base, driven as a convoy that
//! dismounts short of the post. Session-only: kill progress and a running response are not saved.
class AROR_TownWavesComponent : ScriptComponent
{
	[Attribute("0.25", desc: "Share of the town's defenders (post, checkpoints and patrol) players must kill to bring the response", params: "0.05 1 0.05")]
	protected float m_fTriggerShare;
	[Attribute("3", desc: "Fewest player kills that bring the response", params: "1 30 1")]
	protected int m_iMinimumKills;
	[Attribute("3", desc: "Waves in one response", params: "1 5 1")]
	protected int m_iWaves;
	[Attribute("150", desc: "Seconds between one wave being sent and the next", params: "30 900 10")]
	protected int m_iWaveInterval;
	[Attribute("1000", desc: "Nearest road staging distance from the post, metres", params: "300 3000 50")]
	protected float m_fStagingMinDistance;
	[Attribute("2000", desc: "Furthest road staging distance from the post, metres", params: "500 4000 50")]
	protected float m_fStagingMaxDistance;
	[Attribute("220", desc: "Where the convoy's lead vehicle stops, metres short of the post; later vehicles stop behind it. Infantry walk in.", params: "100 290 10")]
	protected float m_fDismountDistance;
	[Attribute("0", desc: "Extra wave elements on top of the Crackdown size; cities always add one", params: "0 4 1")]
	protected int m_iExtraElements;
	[Attribute("1800", desc: "Seconds after a response ends before kills count towards a new one", params: "300 7200 60")]
	protected int m_iRearmSeconds;
	[Attribute("1500", desc: "The response stops sending waves once no Resistance player has been this close for the abandon time, metres", params: "300 4000 50")]
	protected float m_fAbandonDistance;
	[Attribute("180", desc: "Seconds without a Resistance player near before the remaining waves are called off", params: "30 900 10")]
	protected int m_iAbandonSeconds;
	[Attribute("1", desc: "The waves are this town's whole response: a change of control here never draws an ordinary capture QRF")]
	protected bool m_bReplacesCaptureQRF;

	protected static const int KILL_CREDIT_SECONDS = 30;	//!< A death counts for the player who hit the defender this recently
	protected static const int STAGING_RETRY_SECONDS = 20;
	protected static const int STAGING_ATTEMPTS = 6;

	protected AROR_LocationComponent m_Location;
	protected AROR_GarrisonComponent m_Garrison;
	protected AROR_TownLifeComponent m_TownLife;
	protected ref array<ref AROR_TownCasualtyWatch> m_aWatches = {};
	protected ref array<IEntity> m_aSeen = {};	//!< Every defender watched since the response last armed
	protected ref array<string> m_aWaveIds = {};
	protected AROR_EFactionRole m_eRole;
	protected int m_iClock;
	protected int m_iKills;
	protected int m_iWave;			//!< Waves sent in the running response; 0 when none is running
	protected int m_iNextWaveAt;
	protected int m_iRearmAt;
	protected int m_iLastResistanceNear;
	protected int m_iStagingFailures;
	protected bool m_bHasStaging;
	protected vector m_vStaging;

	//------------------------------------------------------------------------------------------------
	//! Every machine: true if this town's waves stand in for the capture QRF
	static bool ReplacesCaptureQRF(AROR_LocationComponent location)
	{
		if (!location)
			return false;
		AROR_TownWavesComponent waves = AROR_TownWavesComponent.Cast(location.GetOwner().FindComponent(AROR_TownWavesComponent));
		return waves && waves.m_bReplacesCaptureQRF;
	}

	bool IsResponding() { return m_iWave > 0; }

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
			if (m_iWave > 0 || m_iKills > 0 || !m_aWatches.IsEmpty())
				Disarm(false);
			return;
		}
		if (role != m_eRole)
		{
			// A new holder starts its own count
			Disarm(false);
			m_eRole = role;
		}

		RefreshWatches();
		if (m_iWave > 0)
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
		m_iWave = 0;
		m_iStagingFailures = 0;
		m_bHasStaging = false;
		m_aWaveIds.Clear();
		m_iLastResistanceNear = m_iClock;
		Print(string.Format("[AROR TOWN] %1 response triggered: %2 player kills of %3 defenders seen", m_Location.GetId(), m_iKills, m_aSeen.Count()), LogLevel.NORMAL);
		if (!LaunchWave(1))
			m_iWave = Math.Max(m_iWave, 1);	// Keep the response running so the retries and later waves follow
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateResponse()
	{
		if (IsResistanceNear())
			m_iLastResistanceNear = m_iClock;
		else if (m_iClock - m_iLastResistanceNear >= m_iAbandonSeconds)
		{
			Print(string.Format("[AROR TOWN] %1 response called off after %2 of %3 waves: no Resistance near", m_Location.GetId(), WavesSent(), m_iWaves), LogLevel.NORMAL);
			Disarm(true);
			return;
		}
		if (m_iClock < m_iNextWaveAt)
			return;
		if (WavesSent() >= m_iWaves)
		{
			Disarm(true);
			return;
		}
		LaunchWave(WavesSent() + 1);
	}

	protected int WavesSent()
	{
		return m_aWaveIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! False when the wave could not be requested yet (retried shortly) or has no recipe (skipped)
	protected bool LaunchWave(int wave)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		AROR_OperationExecutorComponent executor;
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (gameMode)
			executor = AROR_OperationExecutorComponent.Cast(gameMode.FindComponent(AROR_OperationExecutorComponent));
		m_iWave = Math.Max(m_iWave, wave);
		m_iNextWaveAt = m_iClock + STAGING_RETRY_SECONDS;
		if (!operations || !executor)
			return false;

		int readiness = Math.Clamp(operations.GetReadinessLevel(m_eRole), 1, 5);
		int crackdown = Math.Clamp(operations.GetCrackdownLevel(m_eRole), 1, 5);
		string recipe = executor.FindTownWaveRecipeId_S(m_eRole, wave, readiness);
		if (recipe.IsEmpty())
		{
			Print(string.Format("[AROR TOWN] %1 has no wave %2 recipe for role %3 at readiness %4; wave skipped", m_Location.GetId(), wave, m_eRole, readiness), LogLevel.WARNING);
			m_aWaveIds.Insert(string.Empty);
			m_iNextWaveAt = m_iClock + m_iWaveInterval;
			return false;
		}

		vector post = PostPosition();
		if (!m_bHasStaging || !executor.IsWaveStagingSafe_S(m_vStaging, post))
		{
			m_bHasStaging = executor.FindWaveStaging_S(post, NearestFriendlyBase(post), m_fStagingMinDistance, Math.Max(m_fStagingMinDistance, m_fStagingMaxDistance), m_vStaging);
			if (!m_bHasStaging)
			{
				m_iStagingFailures++;
				Print(string.Format("[AROR TOWN] %1 wave %2 has no safe road staging %3-%4m out (attempt %5)", m_Location.GetId(), wave, m_fStagingMinDistance, m_fStagingMaxDistance, m_iStagingFailures), LogLevel.WARNING);
				if (m_iStagingFailures >= STAGING_ATTEMPTS)
				{
					m_aWaveIds.Insert(string.Empty);
					m_iStagingFailures = 0;
					m_iNextWaveAt = m_iClock + m_iWaveInterval;
				}
				return false;
			}
		}

		int elements = WaveElements(wave, crackdown);
		string id = operations.RequestTownWave_S(m_eRole, m_Location.GetId(), recipe, readiness, crackdown, m_vStaging, elements, m_fDismountDistance);
		if (id.IsEmpty())
			return false;
		m_aWaveIds.Insert(id);
		m_iStagingFailures = 0;
		m_iNextWaveAt = m_iClock + m_iWaveInterval;
		Announce(wave);
		Print(string.Format("[AROR TOWN] %1 wave %2/%3 requested: %4 recipe=%5 elements<=%6 staging=%7", m_Location.GetId(), wave, m_iWaves, id, recipe, elements, m_vStaging), LogLevel.NORMAL);
		return true;
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
	protected void Announce(int wave)
	{
		string name = m_Location.GetName();
		name.ToUpper();
		string text = string.Format("%1 UNDER ATTACK / Enemy convoy inbound, wave %2 of %3", name, wave, m_iWaves);
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.HEADLINE, text, m_Location.GetMapIcon(), AROR_UIColors.Get(AROR_UIColors.DANGER), "INTEL"));
	}

	//------------------------------------------------------------------------------------------------
	//! End the response (waves already on the road carry on under Operations); kills count again after the rearm
	//! time when it ran, at once when the town changed hands
	protected void Disarm(bool rearmDelay)
	{
		if (m_iWave > 0)
			Print(string.Format("[AROR TOWN] %1 response ended after %2 waves", m_Location.GetId(), WavesSent()), LogLevel.NORMAL);
		m_iWave = 0;
		m_iKills = 0;
		m_aWaveIds.Clear();
		m_aSeen.Clear();
		m_bHasStaging = false;
		ClearWatches();
		if (rearmDelay)
			m_iRearmAt = m_iClock + m_iRearmSeconds;
		else
			m_iRearmAt = m_iClock;
	}

	//------------------------------------------------------------------------------------------------
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
