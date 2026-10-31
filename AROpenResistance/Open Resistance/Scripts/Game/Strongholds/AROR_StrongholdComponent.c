[ComponentEditorProps(category: "OpenResistance/Locations", description: "Faction stronghold: a home base only its owner or the Resistance can take. Early player raids provoke a retaliation; a later Resistance assault draws a final-defence QRF from a friendly base or the coast. Put on the stronghold's location entity.")]
class AROR_StrongholdComponentClass : ScriptComponentClass {}

//! Server logic for one stronghold. Responses go through Operations (RequestScriptedQRF_S): a named recipe at a
//! frozen level, paid by the scenario rather than the faction bank, launched from a friendly base or a coastal landing.
class AROR_StrongholdComponent : ScriptComponent
{
	[Attribute("2", UIWidgets.ComboBox, desc: "Role whose home base this is. Other AI roles can never take it.", enums: AROR_FactionRoles.MilitaryRoleParams())]
	protected AROR_EFactionRole m_eHomeRole;

	[Attribute("3600", desc: "Early-game window: campaign seconds, counted while players are online, during which player raids provoke a retaliation", params: "0 14400 60", category: "Retaliation")]
	protected int m_iProtectionSeconds;
	[Attribute("4", desc: "Stronghold defenders killed by players within the early-game window before a retaliation launches. The count restarts at each launch.", params: "1 50 1", category: "Retaliation")]
	protected int m_iRetaliationKills;
	[Attribute("invader_ground_l5", desc: "Ground QRF recipe ID for the retaliation; deployed in two waves", category: "Retaliation")]
	protected string m_sRetaliationRecipeId;

	[Attribute("invader_ground_l5", desc: "Ground QRF recipe ID sent when the Resistance assaults the stronghold after the early-game window", category: "Final defence")]
	protected string m_sFinalDefenceRecipeId;
	[Attribute("1800", desc: "Seconds after a final-defence response ends before another assault can draw one", params: "300 7200 60", category: "Final defence")]
	protected int m_iFinalDefenceRearm;

	[Attribute("5", desc: "Readiness and Crackdown level both responses are frozen at, regardless of the campaign levels and funds", params: "1 5 1")]
	protected int m_iResponseLevel;

	[Attribute("0", desc: "Launch responses from the home role's best friendly military site when one is available; the coastal landing is the fallback. Off: always land on the coast.", category: "Landing")]
	protected bool m_bLaunchFromFriendlyBase;
	[Attribute("", desc: "Optional authored landing points (world X/Z). Tried in order before the coastal search.", category: "Landing")]
	protected ref array<vector> m_aLandingPoints;
	[Attribute("0", desc: "Centre of the coastal search arc as a compass bearing from the stronghold (0 = north)", params: "0 359 1", category: "Landing")]
	protected float m_fLandingBearing;
	[Attribute("180", desc: "Width of the coastal search arc, degrees", params: "30 360 10", category: "Landing")]
	protected float m_fLandingArc;
	[Attribute("1200", desc: "Nearest coastal landing distance from the stronghold, metres", params: "300 5000 50", category: "Landing")]
	protected float m_fLandingMinDistance;
	[Attribute("4500", desc: "Furthest coastal landing distance from the stronghold, metres", params: "500 8000 50", category: "Landing")]
	protected float m_fLandingMaxDistance;

	protected static const int KILL_CREDIT_SECONDS = 30;	//!< A death counts for the player who hit the defender this recently
	protected static const int RETRY_SECONDS = 30;			//!< Wait after a response could not be requested

	protected AROR_LocationComponent m_Location;
	protected AROR_GarrisonComponent m_Garrison;
	protected ref array<ref AROR_StrongholdCasualtyWatch> m_aWatches = {};
	protected int m_iClock;
	protected int m_iProtectedElapsed;		//!< Saved
	protected int m_iPlayerKills;			//!< Saved: player kills inside the early-game window since the last retaliation
	protected string m_sResponseId;
	protected bool m_bFinalDefenceActive;
	protected bool m_bResponseDeployed;	//!< The outstanding response reached ACTIVE (its forces exist)
	protected bool m_bResponseFromBase;	//!< Remembered for the announcement after actual deployment, not request acceptance
	protected int m_iNextResponseAt;

	//------------------------------------------------------------------------------------------------
	//! Every machine: the owner may retake its home base and the Resistance may take it; no other AI role can
	bool AllowsCaptureBy(AROR_EFactionRole role)
	{
		return role == m_eHomeRole || role == AROR_EFactionRole.RESISTANCE;
	}

	AROR_EFactionRole GetHomeRole() { return m_eHomeRole; }
	int GetProtectedElapsed() { return m_iProtectedElapsed; }
	int GetPlayerKills() { return m_iPlayerKills; }

	bool IsProtectionWindow()
	{
		return m_iProtectedElapsed < m_iProtectionSeconds;
	}

	//------------------------------------------------------------------------------------------------
	void Restore_S(int elapsed, int kills)
	{
		if (!Replication.IsServer())
			return;
		m_iProtectedElapsed = Math.Clamp(elapsed, 0, m_iProtectionSeconds);
		m_iPlayerKills = Math.Max(kills, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! A watched defender died after a recent player hit
	protected void OnDefenderKilledByPlayer_S(int playerId)
	{
		if (!IsProtectionWindow())
			return;
		m_iPlayerKills++;
		Print(string.Format("[AROR STRONGHOLD] %1 defender killed by player %2 (%3/%4 in the early-game window)", m_Location.GetId(), playerId, m_iPlayerKills, m_iRetaliationKills), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!Replication.IsServer())
			return;
		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!m_Location || !commanders || !commanders.IsCampaignActive())
			return;
		m_iClock++;
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		if (!players.IsEmpty() && IsProtectionWindow())
			m_iProtectedElapsed++;

		if (m_Location.GetControllingRole() != m_eHomeRole)
		{
			ClearWatches();
			return;
		}

		bool playerKill = RefreshWatches();
		UpdateResponse();
		if (m_iClock < m_iNextResponseAt || !m_sResponseId.IsEmpty())
			return;
		if (IsProtectionWindow())
		{
			if (m_iPlayerKills >= m_iRetaliationKills)
				Launch(false);
			return;
		}
		if (playerKill || IsResistanceInside())
			Launch(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Track the outstanding response; once it ends, a final defence re-arms after its cooldown
	protected void UpdateResponse()
	{
		if (m_sResponseId.IsEmpty())
			return;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		AROR_Operation operation;
		if (operations)
			operation = operations.GetOperation(m_sResponseId);
		// A deployed operation can end between polls. The tactical dispatch receipt survives its terminal state.
		bool deployed;
		if (operation)
		{
			if (operation.GetState() == AROR_EOperationState.ACTIVE)
				deployed = true;
			if (operation.m_TacticalCommander && operation.m_TacticalCommander.m_iFirstDispatch >= 0)
				deployed = true;
		}
		if (deployed && !m_bResponseDeployed)
		{
			m_bResponseDeployed = true;
			Announce(m_bFinalDefenceActive, m_bResponseFromBase);
		}
		if (operation && !operation.IsTerminal())
			return;
		if (!m_bResponseDeployed)
		{
			// Never deployed (capacity, no route or launch site): retry soon, and keep the raid's kill count
			Print(string.Format("[AROR STRONGHOLD] %1 response %2 ended before deploying; retrying", m_Location.GetId(), m_sResponseId), LogLevel.WARNING);
			if (!m_bFinalDefenceActive)
				m_iPlayerKills = Math.Max(m_iPlayerKills, m_iRetaliationKills);
			m_iNextResponseAt = m_iClock + RETRY_SECONDS;
		}
		else if (m_bFinalDefenceActive)
			m_iNextResponseAt = m_iClock + m_iFinalDefenceRearm;
		m_sResponseId = string.Empty;
		m_bFinalDefenceActive = false;
		m_bResponseDeployed = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void Launch(bool finalDefence)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		BaseGameMode gameMode = GetGame().GetGameMode();
		AROR_OperationExecutorComponent executor;
		if (gameMode)
			executor = AROR_OperationExecutorComponent.Cast(gameMode.FindComponent(AROR_OperationExecutorComponent));
		if (!operations || !executor)
			return;
		m_iNextResponseAt = m_iClock + RETRY_SECONDS;

		bool fromBase = m_bLaunchFromFriendlyBase && executor.HasMarsOrigin_S(m_eHomeRole, m_Location.GetId());
		vector landing;
		if (!fromBase && !FindLanding(executor, landing))
		{
			Print(string.Format("[AROR STRONGHOLD] %1 has no friendly launch site or safe coastal landing; retrying in %2s. Author landing points or widen the search.", m_Location.GetId(), RETRY_SECONDS), LogLevel.WARNING);
			return;
		}
		string recipe = m_sRetaliationRecipeId;
		if (finalDefence)
			recipe = m_sFinalDefenceRecipeId;
		string id = operations.RequestScriptedQRF_S(m_eHomeRole, m_Location.GetId(), recipe, m_iResponseLevel, !fromBase, landing, !finalDefence);
		if (id.IsEmpty())
			return;
		m_sResponseId = id;
		m_bFinalDefenceActive = finalDefence;
		m_bResponseDeployed = false;
		m_bResponseFromBase = fromBase;
		if (!finalDefence)
			m_iPlayerKills = 0;
		string from = "friendly base";
		if (!fromBase)
			from = landing.ToString();
		Print(string.Format("[AROR STRONGHOLD] %1 %2 requested: %3 recipe=%4 from=%5; announcement waits for deployment", m_Location.GetId(), FinalOrRetaliation(finalDefence), id, recipe, from), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected string FinalOrRetaliation(bool finalDefence)
	{
		if (finalDefence)
			return "final defence";
		return "retaliation";
	}

	//------------------------------------------------------------------------------------------------
	protected bool FindLanding(notnull AROR_OperationExecutorComponent executor, out vector landing)
	{
		vector center = m_Location.GetCenter();
		if (m_aLandingPoints)
		{
			foreach (vector point : m_aLandingPoints)
			{
				point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
				if (executor.IsLaunchPointSafe_S(point, center))
				{
					landing = point;
					return true;
				}
			}
		}
		return executor.FindCoastalLanding_S(center, m_fLandingMinDistance, Math.Max(m_fLandingMinDistance, m_fLandingMaxDistance), m_fLandingBearing, m_fLandingArc, landing);
	}

	//------------------------------------------------------------------------------------------------
	protected void Announce(bool finalDefence, bool fromBase)
	{
		string name = m_Location.GetName();
		name.ToUpper();
		string text = name + " RETALIATION / Heavy response inbound";
		if (finalDefence)
			text = name + " UNDER ASSAULT / Heavy reinforcements inbound";
		if (fromBase)
			text += " from nearby bases";
		else
			text += " from the coast";
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.HEADLINE, text, m_Location.GetMapIcon(), AROR_UIColors.Get(AROR_UIColors.DANGER), "INTEL"));
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsResistanceInside()
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (AROR_QRFSpotting.IsResistance(player) && m_Location.IsPositionInside(player.GetOrigin()))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Watch every living defender of the garrison; returns true if a watched defender died to a player this tick
	protected bool RefreshWatches()
	{
		bool playerKill;
		for (int index = m_aWatches.Count() - 1; index >= 0; index--)
		{
			AROR_StrongholdCasualtyWatch watch = m_aWatches[index];
			if (!watch.m_Entity)
			{
				// Garrison retirement can delete a living defender after a player hit; that is not a kill.
				watch.Detach_S();
				m_aWatches.Remove(index);
				continue;
			}
			if (!watch.IsDead())
				continue;
			if (watch.m_iPlayerId > 0 && m_iClock - watch.m_iHitAt <= KILL_CREDIT_SECONDS)
			{
				playerKill = true;
				OnDefenderKilledByPlayer_S(watch.m_iPlayerId);
			}
			watch.Detach_S();
			m_aWatches.Remove(index);
		}
		if (!m_Garrison)
			return playerKill;
		array<ref AROR_GarrisonSlot> slots = {};
		m_Garrison.GetSlots(slots);
		foreach (AROR_GarrisonSlot slot : slots)
		{
			// Infantry and crews: the people a player can kill
			if ((slot.m_iKind != 0 && slot.m_iKind != 4) || !slot.m_Entity || IsWatched(slot.m_Entity))
				continue;
			AROR_StrongholdCasualtyWatch added = new AROR_StrongholdCasualtyWatch();
			if (added.Attach_S(slot.m_Entity, this))
				m_aWatches.Insert(added);
		}
		return playerKill;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsWatched(IEntity entity)
	{
		foreach (AROR_StrongholdCasualtyWatch watch : m_aWatches)
		{
			if (watch.m_Entity == entity)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearWatches()
	{
		foreach (AROR_StrongholdCasualtyWatch watch : m_aWatches)
			watch.Detach_S();
		m_aWatches.Clear();
	}

	//------------------------------------------------------------------------------------------------
	int GetClock()
	{
		return m_iClock;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Location = AROR_LocationComponent.Cast(owner.FindComponent(AROR_LocationComponent));
		m_Garrison = AROR_GarrisonComponent.Cast(owner.FindComponent(AROR_GarrisonComponent));
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;
		if (!m_Location || !AROR_GarrisonPolicy.IsEnemyRole(m_eHomeRole))
		{
			Print("[AROR STRONGHOLD] Needs a location component and an AI home role; disabled", LogLevel.ERROR);
			return;
		}
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

//! One defender's damage subscription. Remembers the last player who hurt it; the stronghold checks for death.
class AROR_StrongholdCasualtyWatch
{
	IEntity m_Entity;
	int m_iPlayerId;
	int m_iHitAt;
	protected SCR_DamageManagerComponent m_DamageManager;
	protected AROR_StrongholdComponent m_Stronghold;

	bool Attach_S(IEntity entity, AROR_StrongholdComponent stronghold)
	{
		if (!Replication.IsServer() || !entity || !stronghold)
			return false;
		m_DamageManager = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if (!m_DamageManager || m_DamageManager.IsDestroyed())
			return false;
		m_Entity = entity;
		m_Stronghold = stronghold;
		m_DamageManager.GetOnDamage().Insert(OnDamage);
		return true;
	}

	//! Only observed deaths count. Missing references alone may be ordinary garrison retirement.
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
		if (!m_Stronghold || !context || !context.instigator || context.damageValue <= 0)
			return;
		if (context.damageType == EDamageType.HEALING || context.damageType == EDamageType.REGENERATION)
			return;
		int playerId = context.instigator.GetInstigatorPlayerID();
		if (playerId <= 0)
			return;
		m_iPlayerId = playerId;
		m_iHitAt = m_Stronghold.GetClock();
	}

	void Detach_S()
	{
		if (m_DamageManager)
			m_DamageManager.GetOnDamage().Remove(OnDamage);
		m_DamageManager = null;
		m_Entity = null;
		m_Stronghold = null;
	}
}
