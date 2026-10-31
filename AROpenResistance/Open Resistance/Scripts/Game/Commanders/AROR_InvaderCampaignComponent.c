[ComponentEditorProps(category: "OpenResistance/Commanders", description: "Optional resource-funded Invader push north from the support corridor: distant US/USSR battles are simulated, Resistance-held sites get a physical attack. No second bank.")]
class AROR_InvaderCampaignComponentClass : SCR_BaseGameModeComponentClass {}

class AROR_InvaderCampaignComponent : SCR_BaseGameModeComponent
{
	[Attribute("0", desc: "Enable the Invader territorial simulation for this scenario; shared scenarios default off.")]
	protected bool m_bInvaderCampaign;
	[Attribute("everon_ussr_support_corridor", desc: "Stable ID of the fixed Invader support corridor marker.")]
	protected string m_sCorridorId;
	[Attribute("everon_fortress_vostok", desc: "Stable ID of the initial land base (the Invader stronghold). If lost, re-establish this foothold before expanding.")]
	protected string m_sForwardBaseId;
	[Attribute("20000", desc: "One-time opening grant into the existing Invader faction bank; saved to prevent repeat grants.", params: "0 1000000 100")]
	protected int m_iOpeningGrant;
	[Attribute("900", desc: "Seconds between Invader pushes on the southernmost site they do not hold (simulated against the Occupiers, a physical attack against the Resistance); checked on enemy commander evaluations.", params: "30 3600 1")]
	protected int m_iBattleInterval;
	[Attribute("120", desc: "Initial preparation seconds after the first grant, before the first engagement.", params: "30 3600 1")]
	protected int m_iInitialDelay;
	[Attribute("40", desc: "Baseline desired percentage of military installations, excluding the corridor. Soft balance target, not a hard limit.", params: "10 80 1")]
	protected int m_iBaselineShare;
	[Attribute("1000", desc: "Engagement resource cost before site-type scaling; both armies pay from their existing available faction funds.", params: "100 100000 100")]
	protected int m_iBattleCost;
	[Attribute("3500", desc: "Maximum distance between a target and a friendly military origin; keeps expansion on the connected front.", params: "500 10000 100")]
	protected float m_fFrontierReach;
	[Attribute("3000", desc: "Minimum all-player distance beyond the location radius for simulated engagements. Active garrisons/operations also block simulation.", params: "1000 10000 100")]
	protected float m_fPlayerClearance;
	[Attribute("1500", desc: "Additional Invader external support per existing Income payment while this campaign is enabled.", params: "0 100000 100")]
	protected int m_iExternalSupport;
	[Attribute("750", desc: "Additional support per readiness above two, paid only by the existing Income system.", params: "0 100000 50")]
	protected int m_iFundingPerReadiness;
	[Attribute("30", desc: "Additional support per crackdown percentage point, paid only by the existing Income system.", params: "0 10000 1")]
	protected int m_iFundingPerCrackdown;

	protected static AROR_InvaderCampaignComponent s_Instance;
	protected AROR_CommanderManagerComponent m_Commanders;
	protected AROR_LocationManagerComponent m_Locations;
	protected AROR_OperationManagerComponent m_Operations;
	protected AROR_EconomyComponent m_Economy;
	[RplProp()]
	protected bool m_bSeeded;
	[RplProp()]
	protected bool m_bQuarantined;
	protected int m_iNextBattleAt;
	protected int m_iRestoredDelay = -1;
	protected ref array<AROR_LocationComponent> m_aSites = {};
	protected ref array<vector> m_aPlayers = {};
	protected ref array<AROR_Operation> m_aOperations = {};
	protected string m_sAdvanceHold;	//!< Last logged reason the push is waiting; logged again only when it changes

	protected static const int ADVANCE_RETRY_SECONDS = 30;	//!< A held push checks its target again this soon

	static AROR_InvaderCampaignComponent GetInstance() { return s_Instance; }
	bool IsAdvanceActive() { return m_bInvaderCampaign && m_bSeeded && !m_bQuarantined; }
	bool HasOpeningGrant() { return m_bSeeded; }
	bool IsQuarantined() { return m_bQuarantined; }

	//! While this scenario simulates the two armies, their ownership changes must not launch physical QRFs.
	static bool SuppressAICaptureQRF_S(AROR_EFactionRole previous, AROR_EFactionRole current)
	{
		return Replication.IsServer() && s_Instance && s_Instance.IsAdvanceActive()
			&& AROR_GarrisonPolicy.IsEnemyRole(previous) && AROR_GarrisonPolicy.IsEnemyRole(current);
	}

	//! Called by Income's existing payout path; never pays money itself.
	int GetExternalSupportBonus()
	{
		if (!IsAdvanceActive())
			return 0;
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return 0;
		return AROR_InvaderCampaignPolicy.Funding(m_iExternalSupport, m_iFundingPerReadiness, m_iFundingPerCrackdown,
			operations.GetReadinessLevel(AROR_EFactionRole.INVADERS), operations.GetCrackdownPercent(AROR_EFactionRole.INVADERS));
	}

	int RemainingDelay()
	{
		if (m_iRestoredDelay >= 0)
			return m_iRestoredDelay;
		return Math.Clamp(Math.Ceil((m_iNextBattleAt - GetGame().GetWorld().GetWorldTime()) / 1000.0), 0, 3600);
	}
	void RestoreCampaign_S(bool granted, bool quarantined, int delay)
	{
		if (!Replication.IsServer())
			return;
		m_bSeeded = granted;
		m_bQuarantined = quarantined;
		m_iRestoredDelay = delay;
		Replication.BumpMe();
	}
	void Quarantine_S()
	{
		if (!Replication.IsServer())
			return;
		m_bQuarantined = true;
		Replication.BumpMe();
		Print("[AROR INVADER] Unreadable campaign state quarantined; no grant, income bonus or simulation", LogLevel.ERROR);
	}

	protected void Attach_S()
	{
		m_Commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!m_Commanders)
			return;
		m_Commanders.GetOnCommanderEvaluation().Insert(OnEvaluation);
		GetGame().GetCallqueue().Remove(Attach_S);
	}
	protected void OnEvaluation(AROR_Commander commander)
	{
		if (!Replication.IsServer() || !m_bInvaderCampaign || m_bQuarantined || !commander || commander.GetRole() != AROR_EFactionRole.INVADERS)
			return;
		AROR_GameFlowComponent flow = AROR_GameFlowComponent.GetInstance();
		if (!m_Commanders || !m_Commanders.IsCampaignActive() || !flow || !flow.IsPlaythroughReady())
			return;
		m_Locations = AROR_LocationManagerComponent.GetInstance();
		m_Operations = AROR_OperationManagerComponent.GetInstance();
		m_Economy = AROR_EconomyComponent.GetInstance();
		if (!m_Locations || !m_Operations || !m_Economy)
			return;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles)
			return;
		Faction occupiers = roles.GetFaction(AROR_EFactionRole.OCCUPIERS);
		Faction invadersFaction = roles.GetFaction(AROR_EFactionRole.INVADERS);
		if (!occupiers || !invadersFaction || !occupiers.IsFactionEnemy(invadersFaction) || !invadersFaction.IsFactionEnemy(occupiers))
			return;
		AROR_LocationComponent corridor = m_Locations.GetLocationById(m_sCorridorId);
		AROR_LocationComponent foothold = m_Locations.GetLocationById(m_sForwardBaseId);
		if (!corridor || !foothold || corridor.GetLocationType() != AROR_ELocationType.SUPPORT_CORRIDOR || !corridor.IsControlFixed() || corridor.GetControllingRole() != AROR_EFactionRole.INVADERS)
			return;
		int now = GetGame().GetWorld().GetWorldTime();
		if (!m_bSeeded)
		{
			m_bSeeded = true;
			m_Economy.AddFactionFunds_S(AROR_EFactionRole.INVADERS, m_iOpeningGrant);
			m_iNextBattleAt = now + m_iInitialDelay * 1000;
			m_iRestoredDelay = -1;
			Replication.BumpMe();
			Print(string.Format("[AROR INVADER] Corridor ready; opening grant=%1, forward base=%2", m_iOpeningGrant, m_sForwardBaseId), LogLevel.NORMAL);
		}
		if (m_iRestoredDelay >= 0)
		{
			m_iNextBattleAt = now + m_iRestoredDelay * 1000;
			m_iRestoredDelay = -1;
		}
		if (now < m_iNextBattleAt)
			return;
		Survey_S();
		string hold = Advance_S(corridor, foothold);
		if (!hold.IsEmpty())
		{
			// The push waits for its target rather than skipping to another one
			m_iNextBattleAt = now + ADVANCE_RETRY_SECONDS * 1000;
			if (hold != m_sAdvanceHold)
				Print("[AROR INVADER] push held: " + hold, LogLevel.NORMAL);
			m_sAdvanceHold = hold;
			return;
		}
		m_sAdvanceHold = string.Empty;
		m_iNextBattleAt = now + m_iBattleInterval * 1000;
		Counter_S(corridor, foothold);
	}

	//! The fixed-period push: the southernmost military site the Invaders do not hold, on their connected front.
	//! An Occupier site is fought out in the simulation; a Resistance site gets a physical attack.
	//! Returns why the push must wait, or empty when it went ahead (or there is nothing left to take).
	protected string Advance_S(AROR_LocationComponent corridor, AROR_LocationComponent foothold)
	{
		AROR_LocationComponent target = SelectTarget(AROR_EFactionRole.INVADERS, corridor.GetCenter(), foothold);
		if (!target)
			return string.Empty;
		if (target.GetControllingRole() == AROR_EFactionRole.RESISTANCE)
		{
			string id = m_Operations.LaunchTerritoryAttack_S(AROR_EFactionRole.INVADERS, target.GetId());
			if (id.IsEmpty())
				return "attack on " + target.GetId() + ": " + m_Operations.GetAttackFailure();
			Print(string.Format("[AROR INVADER] push: physical attack %1 on Resistance-held %2", id, target.GetId()), LogLevel.NORMAL);
			return string.Empty;
		}
		// Balance: at their goal share the Invaders consolidate instead of taking more Occupier ground
		if (ShareOverGoal() >= 0)
		{
			Print(string.Format("[AROR INVADER] push: at goal share, %1 left to the Occupiers this period", target.GetId()), LogLevel.NORMAL);
			return string.Empty;
		}
		if (!IsDistant(target))
			return target.GetId() + " has players, a garrison or an operation nearby";
		ResolveBattle_S(AROR_EFactionRole.INVADERS, target);
		return string.Empty;
	}

	//! Balance: while the Invaders hold more than their goal share of military sites (after losses elsewhere or a
	//! raised goal falling back), the Occupiers win back the Invaders' outermost site in the simulation, so the front
	//! settles around the goal instead of overrunning the island.
	protected void Counter_S(AROR_LocationComponent corridor, AROR_LocationComponent foothold)
	{
		if (ShareOverGoal() <= 0)
			return;
		AROR_LocationComponent target = SelectTarget(AROR_EFactionRole.OCCUPIERS, corridor.GetCenter(), foothold);
		if (target)
			ResolveBattle_S(AROR_EFactionRole.OCCUPIERS, target);
	}

	//! Invader-held share of the surveyed military sites against their goal share: positive above it, 0 at it
	protected int ShareOverGoal()
	{
		int invaders;
		foreach (AROR_LocationComponent site : m_aSites)
		{
			if (site.GetControllingRole() == AROR_EFactionRole.INVADERS)
				invaders++;
		}
		int goal = AROR_InvaderCampaignPolicy.TargetShare(m_iBaselineShare, m_Operations.GetReadinessLevel(AROR_EFactionRole.INVADERS), m_Operations.GetCrackdownPercent(AROR_EFactionRole.INVADERS));
		return invaders * 100 - goal * m_aSites.Count();
	}

	protected void Survey_S()
	{
		m_aSites.Clear();
		array<AROR_LocationComponent> locations = {};
		m_Locations.GetLocations(locations);
		foreach (AROR_LocationComponent location : locations)
		{
			if (location && !location.IsControlFixed() && AROR_InvaderCampaignPolicy.Cost(location.GetLocationType(), m_iBattleCost) > 0)
				m_aSites.Insert(location);
		}
		m_aPlayers.Clear();
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int id : players)
		{
			IEntity entity = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (entity)
				m_aPlayers.Insert(entity.GetOrigin());
		}
		m_Operations.GetOperations(m_aOperations);
	}
	protected bool IsDistant(AROR_LocationComponent site)
	{
		foreach (vector player : m_aPlayers)
		{
			if (vector.DistanceXZ(player, site.GetCenter()) <= m_fPlayerClearance + site.GetRadius())
				return false;
		}
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(site.GetOwner().FindComponent(AROR_GarrisonComponent));
		if (garrison && (garrison.IsGarrisonActive() || garrison.HasDeploymentStarted()))
			return false;
		foreach (AROR_Operation operation : m_aOperations)
		{
			if (!operation.IsTerminal() && (operation.GetTargetLocationId() == site.GetId() || (operation.m_TacticalCommander && operation.m_TacticalCommander.m_sOriginLocationId == site.GetId())))
				return false;
		}
		return !m_Operations.HasPendingQRFThreat(AROR_EFactionRole.OCCUPIERS, site.GetId()) && !m_Operations.HasPendingQRFThreat(AROR_EFactionRole.INVADERS, site.GetId());
	}
	//! Invaders: the southernmost site they do not hold, whoever holds it (the Resistance included), within reach of
	//! one of their sites. Occupiers: the Invader site furthest from the corridor that is clear of players.
	protected AROR_LocationComponent SelectTarget(AROR_EFactionRole attacker, vector corridor, AROR_LocationComponent foothold)
	{
		AROR_LocationComponent best;
		float score = float.MAX;
		foreach (AROR_LocationComponent candidate : m_aSites)
		{
			AROR_EFactionRole holder = candidate.GetControllingRole();
			if (holder == attacker || !candidate.CanBeTakenBy(attacker))
				continue;
			// The Occupiers only win back Invader sites, and only out of sight; the push waits on its target instead
			if (attacker == AROR_EFactionRole.OCCUPIERS && (holder != AROR_EFactionRole.INVADERS || !IsDistant(candidate)))
				continue;
			float nearest = float.MAX;
			foreach (AROR_LocationComponent origin : m_aSites)
			{
				if (origin.GetControllingRole() == attacker)
					nearest = Math.Min(nearest, vector.DistanceXZ(candidate.GetCenter(), origin.GetCenter()));
			}
			bool restoreFoothold = attacker == AROR_EFactionRole.INVADERS && candidate == foothold;
			if (attacker == AROR_EFactionRole.INVADERS && foothold.GetControllingRole() != attacker && !restoreFoothold)
				continue;
			if (nearest > m_fFrontierReach && !restoreFoothold)
				continue;
			// Invaders: lowest north coordinate first. Occupiers: reclaim the exposed outer front first.
			vector position = candidate.GetCenter();
			float rank = position[2];
			if (attacker == AROR_EFactionRole.OCCUPIERS)
				rank = -vector.DistanceXZ(corridor, position);
			if (!best || rank < score || (rank == score && candidate.GetId().Compare(best.GetId()) < 0))
			{
				best = candidate;
				score = rank;
			}
		}
		return best;
	}
	protected int Industry(AROR_EFactionRole role)
	{
		int value;
		foreach (AROR_LocationComponent site : m_aSites)
		{
			if (site.GetControllingRole() != role)
				continue;
			if (site.GetLocationType() == AROR_ELocationType.RESOURCES)
				value += 3;
			if (site.GetLocationType() == AROR_ELocationType.FACTORY)
				value += 5;
		}
		return value;
	}
	protected void ResolveBattle_S(AROR_EFactionRole attacker, AROR_LocationComponent target)
	{
		if (!Replication.IsServer() || !AROR_GarrisonPolicy.IsEnemyRole(attacker) || !IsDistant(target) || !target.CanBeTakenBy(attacker))
			return;
		AROR_EFactionRole defender = target.GetControllingRole();
		if (!AROR_GarrisonPolicy.IsEnemyRole(defender) || defender == attacker)
			return;
		int cost = AROR_InvaderCampaignPolicy.Cost(target.GetLocationType(), m_iBattleCost);
		AROR_QRFFinance finance = m_Operations.GetQRFFinance();
		if (!finance || finance.Available(attacker) < cost || !m_Economy.SpendFactionFunds_S(attacker, cost))
			return;
		int defence = Math.Min(cost, finance.Available(defender));
		if (defence > 0 && !m_Economy.SpendFactionFunds_S(defender, defence))
			defence = 0;
		int chance = AROR_InvaderCampaignPolicy.BattleChance(cost, defence, Industry(attacker), Industry(defender));
		bool captured;
		if (Math.RandomInt(0, 100) < chance)
			captured = target.SetControllingRole_S(attacker);
		Print(string.Format("[AROR INVADER] simulated target=%1 attacker=%2 attackCost=%3 defenceCost=%4 chance=%5 captured=%6", target.GetId(), attacker, cost, defence, chance, captured), LogLevel.NORMAL);
	}

	//! Keep range checks separate to stay within Enforce's expression complexity limit.
	protected bool HasValidConfiguration()
	{
		if (m_iOpeningGrant < 0 || m_iOpeningGrant > 1000000)
			return false;
		if (m_iBattleInterval < 30 || m_iBattleInterval > 3600)
			return false;
		if (m_iInitialDelay < 30 || m_iInitialDelay > 3600)
			return false;
		if (m_iBaselineShare < 10 || m_iBaselineShare > 80)
			return false;
		if (m_iBattleCost < 100 || m_iBattleCost > 100000)
			return false;
		if (m_fFrontierReach < 500 || m_fFrontierReach > 10000)
			return false;
		if (m_fPlayerClearance < 1000 || m_fPlayerClearance > 10000)
			return false;
		if (m_iExternalSupport < 0 || m_iExternalSupport > 100000)
			return false;
		if (m_iFundingPerReadiness < 0 || m_iFundingPerReadiness > 100000)
			return false;
		if (m_iFundingPerCrackdown < 0 || m_iFundingPerCrackdown > 10000)
			return false;
		return true;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode() || !Replication.IsServer() || !m_bInvaderCampaign)
			return;
		if (s_Instance || !HasValidConfiguration())
		{
			Print("[AROR INVADER] Duplicate or invalid campaign configuration; disabled", LogLevel.ERROR);
			return;
		}
		s_Instance = this;
		GetGame().GetCallqueue().CallLater(Attach_S, 1000, true);
	}
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Attach_S);
		if (m_Commanders)
			m_Commanders.GetOnCommanderEvaluation().Remove(OnEvaluation);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}
}
