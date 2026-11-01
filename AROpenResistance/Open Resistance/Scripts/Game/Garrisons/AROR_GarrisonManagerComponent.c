[ComponentEditorProps(category: "OpenResistance/Garrisons", description: "Server proximity activation and shared garrison budgets. Uses canonical ownership; no simulated territory capture.")]
class AROR_GarrisonManagerComponentClass : SCR_BaseGameModeComponentClass {}

class AROR_GarrisonManagerComponent : SCR_BaseGameModeComponent
{
	protected static AROR_GarrisonManagerComponent s_Instance;
	static AROR_GarrisonManagerComponent GetInstance() { return s_Instance; }
	bool CanDefendRole(AROR_EFactionRole role) { return m_bGarrisonsActive && FactionFor(role) != null; }
	[Attribute("1", desc: "Garrisons Active: allow proximity activation of hostile military-site defenders.")]
	protected bool m_bGarrisonsActive;
	[Attribute("1000", desc: "Activate when any living player is within this many metres of the location centre, including after teleporting", params: "250 5000 50")]
	protected float m_fActivationRadius;
	[Attribute("1", desc: "Seconds between proximity and retirement scans; physical spawn requests remain paced at 250 ms.", params: "1 2 0.25")]
	protected float m_fProximityInterval;
	[Attribute("6", desc: "Minimum player clearance at each defender pad.", params: "3 20 1")]
	protected float m_fPlayerSpawnClearance;
	[Attribute("2500", desc: "All players must be beyond this distance from the site AND its actors before retirement", params: "500 6000 50")]
	protected float m_fRetirementRadius;
	[Attribute("120", desc: "Seconds continuously outside the retirement radius before virtualisation", params: "30 900 5")]
	protected int m_iRetirementDelay;
	[Attribute("1200", desc: "Under personnel pressure, allow safe retirement beyond this all-player distance from both site and actors. Always exceeds activation distance.", params: "1100 6000 50")]
	protected float m_fPressureRetirementRadius;
	[Attribute("15", desc: "Continuous safe distance required before retiring a distant garrison to admit a waiting site. Recent combat prevents pressure retirement.", params: "10 120 5")]
	protected int m_iPressureRetirementDelay;
	[Attribute("96", desc: "Maximum live/reserved garrison personnel, separate from Operations' QRF budget", params: "48 512 1")]
	protected int m_iMaxPersonnel;
	[Attribute("0", desc: "Allow a locally validated infantry-only defence when the full compound terrain check fails. Omit unspawned heavy assets for that activation; never bypass individual navigation, collision or capture-area checks.")]
	protected bool m_bInfantryFallback;
	[Attribute("", desc: "Role/faction/native-asset bindings. Missing or mismatched bindings disable spawning for that role.")]
	protected ref array<ref AROR_GarrisonFaction> m_aFactions;
	protected AROR_GameFlowComponent m_Flow;
	protected AROR_LocationManagerComponent m_Locations;
	protected ref array<AROR_GarrisonComponent> m_aGarrisons = {};
	protected ref array<AROR_GarrisonComponent> m_aNearby = {};
	protected int m_iNextProximityAt;
	protected int m_iCursor;

	// Bindings resolved against the role's faction preset, and the preset state each was resolved for
	protected ref map<int, ref AROR_GarrisonFaction> m_mResolvedFactions = new map<int, ref AROR_GarrisonFaction>();
	protected ref map<int, string> m_mResolvedFor = new map<int, string>();

	protected AROR_GarrisonFaction FactionFor(AROR_EFactionRole role)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles || !m_aFactions)
			return null;
		foreach (AROR_GarrisonFaction faction : m_aFactions)
		{
			if (faction && faction.m_eRole == role && faction.m_sFactionKey == roles.GetFactionKey(role))
				return ResolveWithPreset(faction);
		}
		return null;
	}

	//! The binding with the role's faction preset applied (see AROR_GarrisonFaction.m_bUseFactionPreset). Re-resolved when
	//! the role's preset changes or gains entries (catalog augmentation runs a frame after loading) or the content packs change.
	protected AROR_GarrisonFaction ResolveWithPreset(AROR_GarrisonFaction faction)
	{
		if (!faction.m_bUseFactionPreset)
			return faction;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return faction;
		AROR_FactionPreset preset = presets.GetPreset(faction.m_eRole);
		if (!preset)
			return faction;
		array<AROR_FactionPresetEntry> vehicles = {};
		// Content packs decide which entries are offered (and replace prefabs): their revision is part of the state
		int packRevision = -1;
		AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
		if (packs)
			packRevision = packs.GetRevision();
		string state = string.Format("%1#%2#%3", preset.GetId(), preset.GetAllEntries(AROR_EFactionPresetCategory.VEHICLES, vehicles), packRevision);
		if (m_mResolvedFor.Get(faction.m_eRole) == state && m_mResolvedFactions.Contains(faction.m_eRole))
			return m_mResolvedFactions.Get(faction.m_eRole);
		AROR_GarrisonFaction resolved = faction.ResolveWithPreset(preset);
		m_mResolvedFactions.Set(faction.m_eRole, resolved);
		m_mResolvedFor.Set(faction.m_eRole, state);
		Print(string.Format("[AROR GARRISON] %1 binding resolved with preset %2: %3 static choices, %4 manned choices, assets %5",
			typename.EnumToString(AROR_EFactionRole, faction.m_eRole), preset.GetId(), resolved.m_aStaticVariants.Count() + 1, resolved.m_aMannedVehicles.Count(), resolved.m_sAssetFactionKey), LogLevel.NORMAL);
		return resolved;
	}

	protected bool EnsureRegistry_S()
	{
		if (!m_Flow)
			m_Flow = AROR_GameFlowComponent.Cast(GetOwner().FindComponent(AROR_GameFlowComponent));
		if (m_Locations)
			return m_Flow != null;
		m_Locations = AROR_LocationManagerComponent.GetInstance();
		if (!m_Locations)
			return false;
		m_Locations.GetOnLocationRegistered().Insert(OnRegistered);
		m_Locations.GetOnLocationUnregistered().Insert(OnUnregistered);
		m_Locations.GetOnLocationControlChanged().Insert(OnControlChanged);
		array<AROR_LocationComponent> locations = {};
		m_Locations.GetLocations(locations);
		foreach (AROR_LocationComponent location : locations)
			OnRegistered(location);
		return m_Flow != null;
	}

	protected void OnRegistered(AROR_LocationComponent location)
	{
		if (!location)
			return;
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(location.GetOwner().FindComponent(AROR_GarrisonComponent));
		if (garrison && !m_aGarrisons.Contains(garrison))
			m_aGarrisons.Insert(garrison);
		m_iNextProximityAt = 0;
	}

	protected void OnUnregistered(AROR_LocationComponent location)
	{
		for (int index = m_aGarrisons.Count() - 1; index >= 0; index--)
		{
			AROR_GarrisonComponent garrison = m_aGarrisons[index];
			if (!garrison || garrison.GetLocation() == location)
			{
				m_aNearby.RemoveItem(garrison);
				m_aGarrisons.Remove(index);
			}
		}
		m_iNextProximityAt = 0;
	}

	protected void OnControlChanged(AROR_LocationComponent location)
	{
		foreach (AROR_GarrisonComponent garrison : m_aGarrisons)
		{
			if (garrison && garrison.GetLocation() == location && garrison.HasRoster() && garrison.GetRosterRole() != location.GetControllingRole())
				garrison.CloseDeployment_S("ownership changed");
		}
		m_iNextProximityAt = 0;
	}

	protected bool PlayerNear(AROR_LocationComponent location, array<vector> players)
	{
		foreach (vector position : players)
		{
			if (vector.DistanceXZ(position, location.GetCenter()) <= m_fActivationRadius)
				return true;
		}
		return false;
	}
	protected float NearestPlayerDistance(AROR_LocationComponent location, array<vector> players)
	{
		float nearest = float.MAX;
		foreach (vector position : players)
			nearest = Math.Min(nearest, vector.DistanceXZ(position, location.GetCenter()));
		return nearest;
	}
	protected bool HasRecentCombat(AROR_GarrisonComponent garrison)
	{
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		if (!operations)
			return false;
		AROR_LocationComponent location = garrison.GetLocation();
		return operations.GetRecentCombatContact(garrison.GetRosterRole(), location.GetCenter(), Math.Max(location.GetRadius(), m_fRetirementRadius), 60) != null;
	}

	protected void ScanProximity_S(int now)
	{
		m_iNextProximityAt = now + Math.Max(1, m_fProximityInterval) * 1000;
		m_aNearby.Clear();
		array<vector> living = {};
		array<vector> allPlayers = {};
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity entity = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (!entity)
				continue;
			allPlayers.Insert(entity.GetOrigin());
			ChimeraCharacter player = ChimeraCharacter.Cast(entity);
			if (player && player.GetCharacterController() && !player.GetCharacterController().IsDead())
				living.Insert(player.GetOrigin());
		}
		bool pressure;
		foreach (AROR_GarrisonComponent waiting : m_aGarrisons)
		{
			if (waiting && waiting.IsCapacityWaiting())
				pressure = true;
		}
		foreach (AROR_GarrisonComponent garrison : m_aGarrisons)
		{
			if (!garrison || !garrison.IsEligible())
				continue;
			AROR_LocationComponent location = garrison.GetLocation();
			if (garrison.IsGarrisonActive())
				garrison.Observe_S();
			if (garrison.IsGarrisonActive() || garrison.HasDeploymentStarted())
			{
				float retirementRadius = Math.Max(m_fActivationRadius + 100, m_fRetirementRadius);
				int retirementDelay = m_iRetirementDelay;
				bool reclaim = pressure && !HasRecentCombat(garrison);
				if (reclaim)
				{
					retirementRadius = Math.Min(retirementRadius, Math.Max(m_fActivationRadius + 100, m_fPressureRetirementRadius));
					retirementDelay = Math.Max(10, m_iPressureRetirementDelay);
				}
				bool outside = garrison.CanRetire(retirementRadius, allPlayers);
				if (garrison.OutsideLongEnough(outside, now, retirementDelay * 1000))
				{
					Print(string.Format("[AROR GARRISON] %1 retiring safely; capacityPressure=%2 committed=%3", location.GetId(), reclaim, garrison.CommittedPersonnel()), LogLevel.NORMAL);
					garrison.Retire_S();
				}
			}
			if (garrison.HasRoster() && garrison.GetRosterRole() != location.GetControllingRole())
			{
				garrison.ClearAssignments_S();
				if (!garrison.IsGarrisonActive())
					garrison.ResetRoster_S();
				garrison.RefreshPending_S();
				continue;
			}
			if (AROR_GarrisonPolicy.IsEnemyRole(location.GetControllingRole()) && PlayerNear(location, living) && CanDefendRole(location.GetControllingRole()))
			{
				garrison.BeginDeployment_S(now);
				if (!garrison.IsDeploymentClosed())
					m_aNearby.Insert(garrison);
			}
			garrison.RefreshPending_S();
		}
		// Stable insertion sort over nearby sites; construction still rotates through
		// all of them so a large base cannot monopolise the native spawn queue.
		for (int current = 1; current < m_aNearby.Count(); current++)
		{
			AROR_GarrisonComponent candidate = m_aNearby[current];
			float distance = NearestPlayerDistance(candidate.GetLocation(), living);
			int previous = current - 1;
			while (previous >= 0 && NearestPlayerDistance(m_aNearby[previous].GetLocation(), living) > distance)
			{
				m_aNearby[previous + 1] = m_aNearby[previous];
				previous--;
			}
			m_aNearby[previous + 1] = candidate;
		}
	}

	protected void Tick()
	{
		if (!Replication.IsServer() || !m_bGarrisonsActive || !EnsureRegistry_S() || !m_Flow.IsPlaythroughReady())
			return;
		int now = GetGame().GetWorld().GetWorldTime();
		if (now >= m_iNextProximityAt)
			ScanProximity_S(now);
		if (m_aNearby.IsEmpty())
			return;
		int reserved;
		foreach (AROR_GarrisonComponent garrison : m_aGarrisons)
		{
			if (garrison)
				reserved += garrison.CommittedPersonnel();
		}
		m_iCursor = Math.Mod(m_iCursor, m_aNearby.Count());
		AROR_GarrisonComponent next = m_aNearby[m_iCursor++];
		next.UpdateDeployment_S(now);
		AROR_GarrisonFaction binding = FactionFor(next.GetLocation().GetControllingRole());
		if (!binding || next.IsDeploymentClosed() || !AROR_GarrisonPolicy.IsEnemyRole(next.GetLocation().GetControllingRole()))
			return;
		if (next.HasRoster() && next.GetRosterRole() != next.GetLocation().GetControllingRole())
			return;
		if (!next.HasRoster() && !next.Generate_S(binding))
			return;
		if (!next.BindAssets_S(binding))
			return;
		if (!next.HasSuitableDefenceTerrain_S())
		{
			if (!m_bInfantryFallback || !next.PrepareInfantryFallback_S())
				return;
		}
		int available = Math.Max(0, m_iMaxPersonnel - reserved);
		bool waiting = next.NextPersonnelCost() > available;
		next.SetCapacityWait_S(now, waiting, reserved, m_iMaxPersonnel);
		if (waiting)
			return;
		// Establish infantry before optional heavy assets. Admission is per complete
		// team/hull, rather than reserving an entire virtual roster at first proximity.
		if (!next.SpawnNextGroup_S(now, m_fPlayerSpawnClearance, available))
			next.SpawnNextAsset_S(binding, m_fPlayerSpawnClearance, available);
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		if (GetGame().InPlayMode() && Replication.IsServer() && m_bGarrisonsActive)
			GetGame().GetCallqueue().CallLater(Tick, 250, true);
	}
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Tick);
		if (m_Locations)
		{
			m_Locations.GetOnLocationRegistered().Remove(OnRegistered);
			m_Locations.GetOnLocationUnregistered().Remove(OnUnregistered);
			m_Locations.GetOnLocationControlChanged().Remove(OnControlChanged);
		}
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}
}
