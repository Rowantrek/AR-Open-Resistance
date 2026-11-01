[ComponentEditorProps(category: "OpenResistance/Garrisons", description: "Saved military defence roster; proximity manager owns its physical activation. Never put on settlements.")]
class AROR_GarrisonComponentClass : ScriptComponentClass {}

class AROR_GarrisonComponent : ScriptComponent
{
	[Attribute("1", desc: "Allow a defence roster for this installation")]
	protected bool m_bHasGarrison;
	[Attribute("0", desc: "Defence level override, 0 = type/random recipe", params: "0 4 1")]
	protected int m_iDefenceLevel;
	[Attribute("0", desc: "Static weapon count override; zero uses the defence-level recipe. Authored positions and crew staging are required for every added weapon.", params: "0 6 1")]
	protected int m_iStaticWeaponCountOverride;
	[Attribute("0", desc: "Omit the spare uncrewed armour hull in a compact compound. The manned vehicle recipe and all static crews remain unchanged.")]
	protected bool m_bOmitParkedArmour;
	[Attribute("0", desc: "Infantry teams added to the defence-level recipe (strongholds). Each needs an authored position.", params: "0 8 1")]
	protected int m_iExtraInfantryTeams;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Extra unmanned vehicles parked after the spare armour hull, e.g. helicopters on pads. Not for outposts. Each needs an authored parked position.", params: "et")]
	protected ref array<ResourceName> m_aExtraParkedVehicles;
	[Attribute("", desc: "Optional authored world-space positions for statics/vehicles. Otherwise use bounded clear terrain search.")]
	protected ref array<vector> m_aDefencePositions;
	[Attribute("0", desc: "Perimeter half-width in metres; zero uses the standard location-type preset. Independent of capture radius.", params: "0 64 1")]
	protected float m_fDefenceHalfExtent;
	[Attribute("0", desc: "Perimeter half-depth (local front-to-back) in metres for a rectangular compound on a narrow site, such as a quay; zero keeps the perimeter square", params: "0 64 1")]
	protected float m_fDefenceHalfDepth;
	[Attribute("2", desc: "Maximum terrain height range across the full perimeter footprint", params: "0.25 4 0.25")]
	protected float m_fMaxDefenceRelief;
	[Attribute("800", desc: "Editor-only clearing search radius; does not move runtime defenders or alter activation distance.", params: "25 1600 25")]
	protected float m_fEditorSearchRadius;
	[Attribute("6", desc: "Infantry defend-order radius around its authored perimeter position", params: "2 12 1")]
	protected float m_fDefenceHoldRadius;
	[Attribute("1", desc: "Alternate infantry teams patrol inside their defence footprint and react to recent validated contact. Static/vehicle crews keep their seats.")]
	protected bool m_bLocalDefencePatrols;
	[Attribute("45", desc: "Minimum seconds between local infantry patrol/defend orders; native AI retains combat control.", params: "20 120 5")]
	protected int m_iInfantryOrderInterval;
	[Attribute("30", desc: "Seconds of failed pad placement before omitting that unqueued asset/team for this activation. Living and queued defenders still block capture.", params: "10 120 5")]
	protected int m_iPlacementRetrySeconds;
	[Attribute("0", desc: "Keep the complete defence footprint within this location's canonical capture radius. Used for fixed strategic anchors.")]
	protected bool m_bFixedDefenceAnchor;
	[Attribute("3", desc: "Player clearance for crew staging near the flag. Collision/navigation checks still apply.", params: "2 6 0.5")]
	protected float m_fCrewPlayerClearance;
	[Attribute("100", desc: "Maximum distance from the assigned hull for recovering its own living AI crew into empty seats; never moves players or foreign occupants.", params: "10 200 5")]
	protected float m_fCrewSeatRecoveryDistance;
	[Attribute("2", desc: "Seconds between crew seating requests. Readiness always requires observed occupancy, not an accepted request.", params: "1 10 1")]
	protected int m_iCrewSeatRetrySeconds;
	[Attribute("60", desc: "Maximum seconds to deploy after player proximity, including capacity/terrain failures. Unfinished queues are stopped before capture unlocks.", params: "10 180 1")]
	protected int m_iDeploymentGraceSeconds;
	[Attribute("30", desc: "Maximum seconds for one submitted native personnel queue before retaining its survivors and omitting its unspawned slots.", params: "5 120 1")]
	protected int m_iSpawnQueueTimeoutSeconds;
	[Attribute("180", desc: "Additional bounded deployment time while other sites occupy the shared personnel budget. Does not replenish casualties.", params: "0 600 5")]
	protected int m_iCapacityWaitSeconds;
	protected int m_iCapacityWaitSince = -1;
	protected int m_iCapacityWaited;
	protected int m_iNextCapacityLog;
	protected bool m_bCapacityLimited;
	protected bool m_bHadDeployedPersonnel;
	protected bool m_bInfantryTerrainFallback;
	protected int m_iDeploymentStartedAt = -1;
	protected bool m_bDeploymentClosed;
	protected bool m_bRestoreQuarantined;
	protected bool m_bAssetsBound;
	protected int m_iTerrainRetryAt;
	protected bool m_bDefenceTerrainChecked;
	protected bool m_bDefenceTerrainFits;

	protected AROR_LocationComponent m_Location;
	protected AROR_EFactionRole m_eRosterRole;
	protected string m_sFactionKey;
	protected string m_sAssetFactionKey;
	protected int m_iLevel;
	protected bool m_bGenerated;
	protected bool m_bActive;
	[RplProp()]
	protected bool m_bPendingDefense;
	[RplProp()]
	protected string m_sPendingDefenseReason;
	protected bool m_bDeploymentDeferred;
	protected int m_iNextGroup;
	protected int m_iNextAsset;
	protected int m_iOutsideSince = -1;
	protected int m_iFailureCount;
	protected void ReportFailure(string reason)
	{
		m_bDeploymentDeferred = true;
		m_iFailureCount++;
		if (m_iFailureCount == 1 || Math.Mod(m_iFailureCount, 30) == 0)
			Print(string.Format("[AROR GARRISON] %1 deferred: %2 (attempt %3)", m_Location.GetId(), reason, m_iFailureCount), LogLevel.WARNING);
	}
	protected ref array<ref AROR_GarrisonSlot> m_aSlots = {};
	protected ref array<ref AROR_GarrisonGroup> m_aGroups = {};

	AROR_LocationComponent GetLocation() { return m_Location; }
	bool IsGarrisonActive() { return m_bActive; }
	bool HasRoster() { return m_bGenerated; }
	bool HasDeploymentStarted() { return m_iDeploymentStartedAt >= 0; }
	bool IsDeploymentClosed() { return m_bDeploymentClosed; }
	bool IsRestoreQuarantined() { return m_bRestoreQuarantined; }
	bool IsCapacityWaiting() { return m_iCapacityWaitSince >= 0 && !m_bDeploymentClosed; }
	int GetLevel() { return m_iLevel; }
	//! Designer recipe override, available to read-only editor audits before a roster exists.
	int GetDefenceLevelOverride() { return m_iDefenceLevel; }
	bool AllowsParkedArmour() { return !m_bOmitParkedArmour; }
	int GetExtraInfantryTeams() { return Math.Clamp(m_iExtraInfantryTeams, 0, 8); }
	int GetExtraParkedCount()
	{
		if (!m_aExtraParkedVehicles)
			return 0;
		return m_aExtraParkedVehicles.Count();
	}
	int GetStaticWeaponCount(int level)
	{
		if (m_iStaticWeaponCountOverride > 0)
			return Math.Clamp(m_iStaticWeaponCountOverride, 1, 6);
		return AROR_GarrisonPolicy.StaticCount(level);
	}
	AROR_EFactionRole GetRosterRole() { return m_eRosterRole; }
	string GetRosterFaction() { return m_sFactionKey; }
	string GetAssetFaction() { return m_sAssetFactionKey; }
	void GetSlots(array<ref AROR_GarrisonSlot> slots)
	{
		// Copy() cannot accept this owned-reference element type in Enforce.
		slots.Clear();
		foreach (AROR_GarrisonSlot slot : m_aSlots)
			slots.Insert(slot);
	}
	int ReservedPersonnel()
	{
		int count;
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			if (slot.m_bLost || (slot.m_bPlacementUnavailable && !slot.m_bSpawned))
				continue;
			if (slot.m_iKind == 0)
				count++;
			if (slot.m_iKind == 4 && (slot.m_iVehicle < 0 || m_aSlots[slot.m_iVehicle].m_bLost))
				count++;
			// Reserve conservatively before crew seats have been discovered.
			if (slot.m_iKind == 1)
				count++;
			if (slot.m_iKind == 3)
				count += 4;
		}
		return count;
	}
	//! Charge the shared actor budget for physical assets and submitted queues, not
	//! every virtual soldier in the saved recipe. A hull includes its conservative crew allowance.
	int CommittedPersonnel()
	{
		int count;
		foreach (int index, AROR_GarrisonSlot slot : m_aSlots)
		{
			if (slot.m_bLost || (slot.m_bPlacementUnavailable && !slot.m_bSpawned))
				continue;
			if (slot.m_iKind == 1 && slot.m_bSpawned)
				count++;
			else if (slot.m_iKind == 3 && slot.m_bSpawned)
				count += 4;
			else if (slot.m_iKind == 0 || (slot.m_iKind == 4 && (slot.m_iVehicle < 0 || m_aSlots[slot.m_iVehicle].m_bLost)))
			{
				if (slot.m_bSpawned || IsGroupRequested(slot.m_iGroup))
					count++;
			}
		}
		return count;
	}
	protected bool IsGroupRequested(int groupId)
	{
		foreach (AROR_GarrisonGroup pending : m_aGroups)
		{
			if (pending.m_iId == groupId)
				return true;
		}
		return false;
	}
	//! Smallest complete remaining team or crewed asset; never split a submitted team.
	int NextPersonnelCost()
	{
		int smallest = 513;
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			if (slot.m_bLost || slot.m_bSpawned || slot.m_bPlacementUnavailable)
				continue;
			if (slot.m_iKind == 2)
				return 0;
			if (slot.m_iKind == 1)
				smallest = Math.Min(smallest, 1);
			if (slot.m_iKind == 3)
				smallest = Math.Min(smallest, 4);
			if ((slot.m_iKind != 0 && slot.m_iKind != 4) || IsGroupRequested(slot.m_iGroup))
				continue;
			if (slot.m_iVehicle >= 0 && !m_aSlots[slot.m_iVehicle].m_bSpawned)
				continue;
			int team;
			foreach (AROR_GarrisonSlot member : m_aSlots)
			{
				if (member.m_iGroup != slot.m_iGroup || member.m_bLost || member.m_bSpawned || member.m_bPlacementUnavailable)
					continue;
				if (member.m_iKind != 0 && member.m_iKind != 4)
					continue;
				if (member.m_iKind == 4 && member.m_iVehicle >= 0 && !m_aSlots[member.m_iVehicle].m_bLost && m_aSlots[member.m_iVehicle].m_bSpawned)
					continue;
				team++;
			}
			// Count orphaned survivors even if a migrated group also contains attached crew.
			smallest = Math.Min(smallest, team);
		}
		if (smallest == 513)
			return 0;
		return smallest;
	}
	void SetCapacityWait_S(int now, bool waiting, int committed, int limit)
	{
		if (!Replication.IsServer() || m_bDeploymentClosed)
			return;
		if (waiting && m_iCapacityWaitSince < 0)
			m_iCapacityWaitSince = now;
		if (waiting)
			m_bCapacityLimited = true;
		if (!waiting && m_iCapacityWaitSince >= 0)
		{
			m_iCapacityWaited += now - m_iCapacityWaitSince;
			m_iCapacityWaitSince = -1;
		}
		if (waiting && now >= m_iNextCapacityLog)
		{
			m_iNextCapacityLog = now + 30000;
			Print(string.Format("[AROR GARRISON] %1 waiting for personnel capacity: committed=%2 limit=%3 nextCost=%4", m_Location.GetId(), committed, limit, NextPersonnelCost()), LogLevel.NORMAL);
		}
	}
	void ClearAssignments_S()
	{
		if (!Replication.IsServer())
			return;
		foreach (AROR_GarrisonGroup group : m_aGroups)
		{
			if (!group.m_Group)
				continue;
			AROR_QRFDefenseComponent defense = AROR_QRFDefenseComponent.Cast(group.m_Group.FindComponent(AROR_QRFDefenseComponent));
			if (defense)
				defense.ClearDefense_S();
		}
	}
	bool IsEligible()
	{
		return m_bHasGarrison && m_Location && AROR_CaptureLocationAction.IsCapturableLocation(m_Location);
	}
	//! Once real personnel have deployed and all submitted native queues have
	//! resolved, capture depends on actual hostile presence, not unused recipe slots.
	//! A deployment is resolved only when every requested personnel slot has an outcome.
	protected bool HasUnresolvedQueue(AROR_GarrisonGroup pending)
	{
		if (pending.m_bQueueClosed)
			return false;
		foreach (int index : pending.m_aSlots)
		{
			AROR_GarrisonSlot slot = m_aSlots[index];
			if (!slot.m_bLost && !slot.m_bPlacementUnavailable && !slot.m_bSpawned)
				return true;
		}
		return false;
	}
	//! Pending native spawns cannot make an uncleared garrison look empty to a flag interaction.
	bool IsDefencePending()
	{
		if (!Replication.IsServer())
			return m_bPendingDefense;
		if (!IsEligible() || !AROR_GarrisonPolicy.IsEnemyRole(m_Location.GetControllingRole()) || m_bDeploymentClosed || m_bRestoreQuarantined)
			return false;
		AROR_GarrisonManagerComponent manager = AROR_GarrisonManagerComponent.GetInstance();
		if (!manager || !manager.CanDefendRole(m_Location.GetControllingRole()))
			return false;
		if (!m_bGenerated)
			return true;
		if (m_eRosterRole != m_Location.GetControllingRole())
			return false;
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			if (!slot.m_bLost && !slot.m_bPlacementUnavailable && slot.m_iKind != 2 && !slot.m_bSpawned)
				return true;
		}
		return false;
	}
	void RefreshPending_S()
	{
		if (!Replication.IsServer())
			return;
		UpdateDeployment_S(GetGame().GetWorld().GetWorldTime());
		bool pending = IsDefencePending();
		string reason = GetDefenceCaptureBlockReason();
		if (pending == m_bPendingDefense && reason == m_sPendingDefenseReason)
			return;
		m_bPendingDefense = pending;
		m_sPendingDefenseReason = reason;
		Replication.BumpMe();
	}
	//! Pending virtual defenders are a deployment gate, never proof of living hostiles.
	string GetDefenceCaptureBlockReason()
	{
		if (!Replication.IsServer())
			return m_sPendingDefenseReason;
		if (!IsDefencePending())
			return string.Empty;
		if (m_bDefenceTerrainChecked && !m_bDefenceTerrainFits && !m_bInfantryTerrainFallback)
			return "Defence site unsuitable - relocation required";
		if (m_bDeploymentDeferred)
			return "Defence deployment delayed";
		return "Defenders awaiting deployment";
	}

	//! Start even when budget or terrain prevents admission. This clock is never a callback count.
	void BeginDeployment_S(int now)
	{
		if (!Replication.IsServer() || m_iDeploymentStartedAt >= 0 || m_bRestoreQuarantined)
			return;
		m_iDeploymentStartedAt = now;
	}

	void UpdateDeployment_S(int now)
	{
		if (!Replication.IsServer() || m_iDeploymentStartedAt < 0 || m_bDeploymentClosed)
			return;
		if (m_bGenerated && m_eRosterRole != m_Location.GetControllingRole())
		{
			CloseDeployment_S("ownership changed");
			return;
		}
		bool unresolved;
		foreach (AROR_GarrisonGroup pending : m_aGroups)
		{
			AssignMembers(pending);
			if (HasUnresolvedQueue(pending) && now - pending.m_iQueuedAt >= m_iSpawnQueueTimeoutSeconds * 1000)
				CancelQueuedGroup_S(pending);
			if (HasUnresolvedQueue(pending))
				unresolved = true;
		}
		if (m_bCapacityLimited && m_bHadDeployedPersonnel && !unresolved)
		{
			CloseDeployment_S("capacity-limited defence established; no late additions during clearance");
			return;
		}
		int capacityDelay = m_iCapacityWaited;
		if (m_iCapacityWaitSince >= 0)
			capacityDelay += now - m_iCapacityWaitSince;
		capacityDelay = Math.Min(capacityDelay, Math.Max(0, m_iCapacityWaitSeconds) * 1000);
		if (now - m_iDeploymentStartedAt - capacityDelay >= m_iDeploymentGraceSeconds * 1000)
			CloseDeployment_S("deployment window elapsed");
	}

	//! Cancel queued work by removing its source group. Detach physical members first:
	//! existing defenders, players and their assets must survive a queue timeout.
	protected void CancelQueuedGroup_S(AROR_GarrisonGroup pending)
	{
		if (pending.m_bQueueClosed)
			return;
		pending.m_bQueueClosed = true;
		SCR_AIGroup previous = pending.m_Group;
		array<IEntity> survivors = {};
		if (previous)
		{
			previous.SetMaxUnitsToSpawn(0);
			previous.SetNumberOfMembersToSpawn(0);
			previous.m_aUnitPrefabSlots.Clear();
			array<AIAgent> agents = {};
			previous.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;
				IEntity entity = agent.GetControlledEntity();
				if (!entity || GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity) > 0)
					continue;
				previous.RemoveAgentFromControlledEntity(entity);
				if (IsPlayerOrForeignAsset(entity))
					continue;
				survivors.Insert(entity);
			}
			AROR_QRFDefenseComponent defense = AROR_QRFDefenseComponent.Cast(previous.FindComponent(AROR_QRFDefenseComponent));
			if (defense)
				defense.ClearDefense_S();
			if (previous.GetPlayerCount() == 0)
				SCR_EntityHelper.DeleteEntityAndChildren(previous);
		}
		if (pending.m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(pending.m_Waypoint);
		pending.m_Waypoint = null;
		pending.m_Group = null;
		foreach (int index : pending.m_aSlots)
		{
			AROR_GarrisonSlot slot = m_aSlots[index];
			if (!slot.m_bSpawned && !slot.m_Entity)
				slot.m_bPlacementUnavailable = true;
		}
		if (!survivors.IsEmpty())
		{
			pending.m_Group = CreateGroup_S(survivors[0].GetOrigin());
			if (pending.m_Group)
			{
				foreach (IEntity survivor : survivors)
					pending.m_Group.AddAgentFromControlledEntity(survivor);
			}
		}
		pending.m_bOrdered = false;
		Print(string.Format("[AROR GARRISON] %1 queue %2 closed; physical defenders preserved, missing slots omitted without losses", m_Location.GetId(), pending.m_iId), LogLevel.WARNING);
	}

	void CloseDeployment_S(string reason)
	{
		if (!Replication.IsServer() || m_bDeploymentClosed)
			return;
		m_bDeploymentClosed = true; // Stop producers before cancelling any submitted work.
		foreach (AROR_GarrisonGroup pending : m_aGroups)
		{
			AssignMembers(pending);
			if (HasUnresolvedQueue(pending))
				CancelQueuedGroup_S(pending);
			pending.m_bQueueClosed = true;
		}
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			if (!slot.m_bSpawned && !slot.m_Entity)
				slot.m_bPlacementUnavailable = true;
		}
		Print(string.Format("[AROR GARRISON] %1 deployment closed: %2; capture now follows living hostiles", m_Location.GetId(), reason), LogLevel.WARNING);
	}

	//! A lost hull does not erase its surviving crew or leave them waiting for a new hull.
	protected void DetachOrphanedCrew_S()
	{
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			if (slot.m_iKind == 4 && slot.m_iVehicle >= 0 && m_aSlots[slot.m_iVehicle].m_bLost)
			{
				slot.m_iVehicle = -1;
				slot.m_iSeat = -1;
				slot.m_bPlaced = false;
			}
		}
	}

	//! Bound failed pad placement without inventing deaths, deleting live actors,
	//! or releasing personnel already submitted to the native spawn queue.
	protected void ReportPlacementFailure(int index, string reason)
	{
		if (!Replication.IsServer())
			return;
		AROR_GarrisonSlot slot = m_aSlots[index];
		if (slot.m_bSpawned || slot.m_Entity || slot.m_bPlacementUnavailable)
			return;
		foreach (AROR_GarrisonGroup queued : m_aGroups)
		{
			if (slot.m_iGroup >= 0 && queued.m_iId == slot.m_iGroup)
				return;
		}
		int now = GetGame().GetWorld().GetWorldTime();
		if (slot.m_iPlacementFailureSince < 0)
			slot.m_iPlacementFailureSince = now;
		ReportFailure(string.Format("slot=%1 %2", index, reason));
		if (now - slot.m_iPlacementFailureSince < m_iPlacementRetrySeconds * 1000)
			return;
		foreach (AROR_GarrisonSlot candidate : m_aSlots)
		{
			bool sameTeam = (slot.m_iKind == 0 || slot.m_iKind == 4) && candidate.m_iGroup == slot.m_iGroup;
			if ((candidate == slot || sameTeam || candidate.m_iVehicle == index) && !candidate.m_bSpawned && !candidate.m_Entity)
				candidate.m_bPlacementUnavailable = true;
		}
		Print(string.Format("[AROR GARRISON] %1 slot=%2 omitted for this activation after failed pad placement; no casualty recorded. Live/queued defenders still block capture.", m_Location.GetId(), index), LogLevel.WARNING);
	}

	protected int AddSlot(ResourceName prefab, int kind, int group = -1, int vehicle = -1, int seat = -1)
	{
		AROR_GarrisonSlot slot = new AROR_GarrisonSlot();
		slot.m_sPrefab = prefab;
		slot.m_iKind = kind;
		slot.m_iGroup = group;
		slot.m_iVehicle = vehicle;
		slot.m_iSeat = seat;
		m_aSlots.Insert(slot);
		return m_aSlots.Count() - 1;
	}

	bool Generate_S(AROR_GarrisonFaction faction)
	{
		if (!Replication.IsServer() || !IsEligible() || m_bDeploymentClosed || m_bGenerated || !faction || faction.m_eRole != m_Location.GetControllingRole())
			return false;
		m_eRosterRole = faction.m_eRole;
		m_sFactionKey = faction.m_sFactionKey;
		m_sAssetFactionKey = faction.m_sAssetFactionKey;
		m_iLevel = AROR_GarrisonPolicy.DefenceLevel(m_Location.GetLocationType(), Math.RandomInt(0, 3));
		if (m_iDefenceLevel > 0)
			m_iLevel = Math.Clamp(m_iDefenceLevel, 1, 4);
		if (m_Location.GetLocationType() == AROR_ELocationType.AIRPORT)
			m_iLevel = 4;
		// An outpost override may strengthen infantry/statics, never add armour.
		int group;
		for (int team = 0; team < AROR_GarrisonPolicy.InfantryTeams(m_iLevel) + GetExtraInfantryTeams(); team++)
		{
			for (int member = 0; member < 4; member++)
			{
				int infantry = AddSlot(CharacterPrefab(InfantryType(member)), 0, group);
				m_aSlots[infantry].m_iVariant = member;
			}
			group++;
		}
		int staticVariants = 1;
		if (faction.m_aStaticVariants)
			staticVariants += faction.m_aStaticVariants.Count();
		for (int mg = 0; mg < GetStaticWeaponCount(m_iLevel); mg++)
		{
			int staticVariant = Math.Mod(mg, staticVariants);
			int weapon = AddSlot(StaticPrefab(faction, staticVariant), 1, group++);
			m_aSlots[weapon].m_iVariant = staticVariant;
		}
		if (m_Location.GetLocationType() != AROR_ELocationType.OUTPOST)
		{
			if (m_iLevel >= 3 && !m_bOmitParkedArmour)
				AddSlot(faction.m_sParkedArmour, 2);
			// Variant k identifies extra vehicle k-1, so a reload re-binds the same vehicle rather than armour
			for (int extra = 0; extra < GetExtraParkedCount(); extra++)
			{
				int parked = AddSlot(m_aExtraParkedVehicles[extra], 2);
				m_aSlots[parked].m_iVariant = extra + 1;
			}
			for (int hull = 0; hull < AROR_GarrisonPolicy.MannedCount(m_iLevel); hull++)
			{
				if (!faction.m_aMannedVehicles || faction.m_aMannedVehicles.IsEmpty())
					break;
				int variant = Math.RandomInt(0, faction.m_aMannedVehicles.Count());
				int hullSlot = AddSlot(faction.m_aMannedVehicles[variant], 3, group++);
				m_aSlots[hullSlot].m_iVariant = variant;
			}
		}
		// One marksman per guard tower, each his own group, after every other group so authored team, gun and crew
		// ordinals are unchanged. The variant records which post he holds.
		for (int post = 0; post < SentryPosts().Count(); post++)
		{
			int sentry = AddSlot(CharacterPrefab(InfantryType(SENTRY_VARIANT + post)), 0, group++);
			m_aSlots[sentry].m_iVariant = SENTRY_VARIANT + post;
		}
		// Crew slots are added from each actual hull's seat list on first activation.
		m_bGenerated = true;
		// Stores are stocked with the roster, once: a saved roster is never regenerated, so looted stores stay looted
		StockStores_S(GetOwner(), 0);
		Print(string.Format("[AROR GARRISON] %1 roster role=%2 level=%3 personnel reservation=%4", m_Location.GetId(), m_eRosterRole, m_iLevel, ReservedPersonnel()), LogLevel.NORMAL);
		return true;
	}
	protected ResourceName StaticPrefab(AROR_GarrisonFaction faction, int variant)
	{
		if (variant > 0 && faction.m_aStaticVariants && variant <= faction.m_aStaticVariants.Count())
		{
			ResourceName weapon = faction.m_aStaticVariants[variant - 1];
			if (!weapon.IsEmpty())
				return weapon;
		}
		return faction.m_sStaticMG;
	}
	//! Variant 0 is the role's spare armour; higher variants are this site's extra parked vehicles
	protected ResourceName ParkedPrefab(AROR_GarrisonFaction faction, int variant)
	{
		if (variant > 0 && variant <= GetExtraParkedCount())
			return m_aExtraParkedVehicles[variant - 1];
		return faction.m_sParkedArmour;
	}
	//! Stock every store (AROR_OutpostSupplyComponent) in the composition from the roster army's faction preset
	protected void StockStores_S(IEntity entity, int depth)
	{
		if (!entity || depth > 4)
			return;
		IEntity child = entity.GetChildren();
		while (child)
		{
			AROR_OutpostSupplyComponent store = AROR_OutpostSupplyComponent.Cast(child.FindComponent(AROR_OutpostSupplyComponent));
			if (store)
				store.Stock_S(m_eRosterRole);
			else
				StockStores_S(child, depth + 1);
			child = child.GetSibling();
		}
	}

	static const int SENTRY_VARIANT = 1000;	//!< Infantry variants from here are tower marksmen; variant - SENTRY_VARIANT is the post

	//! Guard towers (AROR_SentryPostComponent) among the composition's direct children, in order
	protected array<IEntity> SentryPosts()
	{
		array<IEntity> posts = {};
		IEntity child = GetOwner().GetChildren();
		while (child)
		{
			if (child.FindComponent(AROR_SentryPostComponent))
				posts.Insert(child);
			child = child.GetSibling();
		}
		return posts;
	}

	protected bool IsSentry(AROR_GarrisonSlot slot)
	{
		return slot.m_iKind == 0 && slot.m_iVariant >= SENTRY_VARIANT;
	}

	//! The authored marker for a slot: a sentry's tower, otherwise the matching defence position
	protected IEntity DefenceMarker(int index)
	{
		AROR_GarrisonSlot slot = m_aSlots[index];
		if (IsSentry(slot))
		{
			array<IEntity> posts = SentryPosts();
			int post = slot.m_iVariant - SENTRY_VARIANT;
			if (post < posts.Count())
				return posts[post];
			return null;
		}
		return FindDefencePosition(slot.m_iKind, PositionOrdinal(index));
	}

	//! The preset unit type behind a saved infantry variant: team members 0-3 are rifleman, automatic rifleman,
	//! grenadier and team leader; tower sentries are marksmen
	protected static AROR_EUnitType InfantryType(int variant)
	{
		if (variant >= SENTRY_VARIANT)
			return AROR_EUnitType.MARKSMAN;
		switch (variant)
		{
			case 1: return AROR_EUnitType.AUTORIFLEMAN;
			case 2: return AROR_EUnitType.GRENADIER;
			case 3: return AROR_EUnitType.TEAMLEADER;
		}
		return AROR_EUnitType.RIFLEMAN;
	}

	//! The preset unit type of a team or crew slot
	protected static AROR_EUnitType SlotUnitType(AROR_GarrisonSlot slot)
	{
		if (slot.m_iKind == 4)
			return AROR_EUnitType.CREW;
		return InfantryType(slot.m_iVariant);
	}

	//! The character a roster unit spawns as (the role's base character, dressed from its preset loadout)
	protected ResourceName CharacterPrefab(AROR_EUnitType type)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return string.Empty;
		return presets.GetSpawnPrefab(m_eRosterRole, type);
	}

	protected bool InitializeFaction(IEntity entity)
	{
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return false;
		string key = affiliation.GetAffiliatedFactionKey();
		if (!key.IsEmpty() && key != m_sFactionKey && key != m_sAssetFactionKey)
			return false;
		affiliation.SetAffiliatedFactionByKey(m_sFactionKey);
		return affiliation.GetAffiliatedFactionKey() == m_sFactionKey;
	}

	float GetDefenceHalfExtent()
	{
		if (m_fDefenceHalfExtent > 0)
			return Math.Clamp(m_fDefenceHalfExtent, 12, 64);
		return AROR_DefenceLayout.HalfExtent(m_Location.GetLocationType());
	}
	//! Local front-to-back half size; equals the half-extent for square compounds
	float GetDefenceHalfDepth()
	{
		if (m_fDefenceHalfDepth > 0)
			return Math.Clamp(m_fDefenceHalfDepth, 12, GetDefenceHalfExtent());
		return GetDefenceHalfExtent();
	}
	float GetMaxDefenceRelief() { return m_fMaxDefenceRelief; }
	float GetEditorSearchRadius() { return Math.Clamp(m_fEditorSearchRadius, 25, 1600); }
	bool IsDefenceCenterAllowed(vector position, float halfExtent)
	{
		if (!m_bFixedDefenceAnchor)
			return true;
		return m_Location && vector.DistanceXZ(position, m_Location.GetCenter()) + (halfExtent + 2) * 1.414214 <= m_Location.GetRadius();
	}
	vector GetDefenceCenter()
	{
		IEntity child = GetOwner().GetChildren();
		while (child)
		{
			if (child.FindComponent(AROR_CaptureFlagPlacementComponent))
				return child.GetOrigin();
			child = child.GetSibling();
		}
		return m_Location.GetCenter();
	}
	protected int PositionOrdinal(int index)
	{
		int kind = m_aSlots[index].m_iKind;
		int ordinal;
		for (int previous = 0; previous < index; previous++)
		{
			if (m_aSlots[previous].m_iKind == kind)
				ordinal++;
		}
		if (kind == 0)
			return ordinal / 4;
		if (kind == 4)
			return m_aSlots[index].m_iGroup;
		return ordinal;
	}
	protected IEntity FindDefencePosition(int kind, int ordinal)
	{
		IEntity child = GetOwner().GetChildren();
		while (child)
		{
			AROR_DefencePositionComponent position = AROR_DefencePositionComponent.Cast(child.FindComponent(AROR_DefencePositionComponent));
			if (position && position.MatchesDefencePosition(kind, ordinal))
				return child;
			child = child.GetSibling();
		}
		return null;
	}
	protected bool HasClearMGArc(vector point, float yaw)
	{
		return AROR_DefenceLayout.ClearMGArc(GetGame().GetWorld(), point, yaw);
	}
	bool HasSuitableDefenceTerrain_S()
	{
		if (!Replication.IsServer())
			return false;
		int now = GetGame().GetWorld().GetWorldTime();
		if (m_bDefenceTerrainChecked && !m_bDefenceTerrainFits && now >= m_iTerrainRetryAt)
			m_bDefenceTerrainChecked = false;
		if (!m_bDefenceTerrainChecked)
		{
			m_iTerrainRetryAt = now + 5000;
			if (!IsDefenceCenterAllowed(GetDefenceCenter(), GetDefenceHalfExtent()))
			{
				m_bDefenceTerrainChecked = true;
				m_bDefenceTerrainFits = false;
				ReportFailure("defence perimeter exceeds fixed strategic capture radius");
				return false;
			}
			float relief;
			vector siteAngles = GetOwner().GetYawPitchRoll();
			m_bDefenceTerrainFits = AROR_DefenceLayout.TerrainFits(GetGame().GetWorld(), GetDefenceCenter(), GetDefenceHalfExtent() + 2, m_fMaxDefenceRelief, relief, siteAngles[0], GetDefenceHalfDepth() + 2);
			m_bDefenceTerrainChecked = true;
			if (!m_bDefenceTerrainFits)
				ReportFailure(string.Format("perimeter terrain rejected: half-width=%1 relief=%2 limit=%3; relocate to an audited flat site", GetDefenceHalfExtent(), relief, m_fMaxDefenceRelief));
		}
		return m_bDefenceTerrainFits;
	}
	//! Scenario opt-in fallback for a usable flag/pad on an unsuitable full compound.
	//! Keep live actors and all casualties; omit only still-unspawned heavy assets.
	bool PrepareInfantryFallback_S()
	{
		if (!Replication.IsServer() || !m_bGenerated || m_bDeploymentClosed || m_bRestoreQuarantined)
			return false;
		if (m_bInfantryTerrainFallback)
			return true;
		m_bInfantryTerrainFallback = true;
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			if (!slot.m_bSpawned && slot.m_iKind > 0 && slot.m_iKind < 4)
				slot.m_bPlacementUnavailable = true;
			if (slot.m_iKind == 4 && !slot.m_bSpawned && slot.m_iVehicle >= 0)
			{
				AROR_GarrisonSlot hull = m_aSlots[slot.m_iVehicle];
				if (!hull.m_bSpawned && !hull.m_bLost)
					slot.m_bPlacementUnavailable = true;
			}
		}
		Print(string.Format("[AROR GARRISON] %1 infantry-only terrain fallback; individual pads still require clear land, navigation and player clearance", m_Location.GetId()), LogLevel.WARNING);
		return true;
	}

	//! Fixed flag-centred positions, not scattered points across the location's capture radius.
	protected bool PlaceSlot(int index, float playerExclusion)
	{
		AROR_GarrisonSlot slot = m_aSlots[index];
		if (!HasSuitableDefenceTerrain_S())
		{
			bool onFoot = slot.m_iKind == 0 || (slot.m_iKind == 4 && (slot.m_iVehicle < 0 || m_aSlots[slot.m_iVehicle].m_bLost));
			if (!m_bInfantryTerrainFallback || !onFoot)
				return false;
		}
		if (slot.m_bPlaced)
		{
			IEntity currentMarker = DefenceMarker(index);
			if (currentMarker && vector.DistanceXZ(slot.m_vPosition, currentMarker.GetOrigin()) > 1.5)
				slot.m_bPlaced = false; // Layout edits invalidate positions, never casualties or the stored recipe.
			vector savedLocal = GetOwner().VectorToLocal(slot.m_vPosition - GetDefenceCenter());
			if (Math.AbsFloat(savedLocal[0]) > GetDefenceHalfExtent() - 2 || Math.AbsFloat(savedLocal[2]) > GetDefenceHalfDepth() - 2)
				slot.m_bPlaced = false;
		}
		if (slot.m_bPlaced)
		{
			if (!IsClear(slot.m_vPosition, slot.m_iKind, index, playerExclusion))
				return false;
			return slot.m_iKind != 1 || HasClearMGArc(slot.m_vPosition, slot.m_vAngles[0]);
		}
		int ordinal = PositionOrdinal(index);
		IEntity marker = DefenceMarker(index);
		vector seed;
		vector ownerAngles = GetOwner().GetYawPitchRoll();
		float yaw = ownerAngles[0] + AROR_DefenceLayout.Facing(slot.m_iKind, ordinal);
		float elevation;
		if (marker)
		{
			seed = marker.GetOrigin();
			vector markerAngles = marker.GetYawPitchRoll();
			yaw = markerAngles[0];
			AROR_DefencePositionComponent authored = AROR_DefencePositionComponent.Cast(marker.FindComponent(AROR_DefencePositionComponent));
			if (authored)
				elevation = authored.GetElevation();
			AROR_SentryPostComponent post = AROR_SentryPostComponent.Cast(marker.FindComponent(AROR_SentryPostComponent));
			if (post && IsSentry(slot))
				elevation = post.GetFloorHeight();
		}
		else if (slot.m_iKind > 0 && slot.m_iKind < 4 && m_aDefencePositions && !m_aDefencePositions.IsEmpty())
		{
			int padOrdinal;
			for (int previous = 0; previous < index; previous++)
			{
				if (m_aSlots[previous].m_iKind > 0 && m_aSlots[previous].m_iKind < 4)
					padOrdinal++;
			}
			if (padOrdinal >= m_aDefencePositions.Count())
				return false; // Explicit layouts never fall through to scattered automatic positions.
			seed = m_aDefencePositions[padOrdinal];
		}
		else
			seed = GetDefenceCenter() + GetOwner().VectorToParent(AROR_DefenceLayout.LocalPosition(slot.m_iKind, ordinal, GetDefenceHalfExtent()));
		for (int attempt = 0; attempt < 25; attempt++)
		{
			vector position = seed + AROR_DefenceLayout.RepairOffset(attempt);
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			if (elevation > 0 && attempt == 0)
			{
				// Try the authored platform first. A missing tower, or one whose parapet masks a gun, falls back
				// to the ground beside it through the repair offsets rather than losing the gun or sentry.
				float platform;
				if (FindPlatform(position, elevation, platform))
				{
					vector raised = position;
					raised[1] = platform;
					if (IsClear(raised, slot.m_iKind, index, playerExclusion) && (slot.m_iKind != 1 || HasClearMGArc(raised, yaw)))
					{
						slot.m_vPosition = raised;
						slot.m_vAngles = Vector(yaw, 0, 0);
						slot.m_bPlaced = true;
						return true;
					}
				}
				continue;
			}
			if (slot.m_iKind == 2)
				position[1] = PadSurface(position);
			if (!IsClear(position, slot.m_iKind, index, playerExclusion))
				continue;
			if (slot.m_iKind == 1 && !HasClearMGArc(position, yaw))
				continue;
			slot.m_vPosition = position;
			slot.m_vAngles = Vector(yaw, 0, 0);
			slot.m_bPlaced = true;
			return true;
		}
		return false;
	}

	//! The lowest surface with standing room over it between two metres above the authored platform height and two
	//! below (never under a metre and a half off the terrain): a tower's floor rather than the roof over it.
	//! False when nothing is there (tower missing or destroyed), so nobody is left floating.
	protected bool FindPlatform(vector position, float elevation, out float height)
	{
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(position[0], position[2]);
		float bottom = ground + Math.Max(elevation - 2, 1.5);
		float start = ground + elevation + 2;
		bool found;
		for (int surface = 0; surface < 6 && start > bottom; surface++)
		{
			TraceParam down = new TraceParam();
			down.Start = Vector(position[0], start, position[2]);
			down.End = Vector(position[0], bottom, position[2]);
			down.Flags = TraceFlags.ENTS;
			float fraction = world.TraceMove(down, null);
			if (fraction >= 1)
				break;
			float hit = start + (bottom - start) * fraction;
			TraceParam up = new TraceParam();
			up.Start = Vector(position[0], hit + 0.1, position[2]);
			up.End = Vector(position[0], hit + PLATFORM_HEADROOM, position[2]);
			up.Flags = TraceFlags.ENTS;
			if (world.TraceMove(up, null) >= 1)
			{
				height = hit;
				found = true;
			}
			// Keep looking under it: a roof also has open sky above
			start = hit - 0.3;
		}
		return found;
	}
	static const float PLATFORM_HEADROOM = 1.8;

	//! Every entity of the helipads (AROR_LandingPadComponent children) in this composition, including their props
	protected void CollectLandingPads(notnull array<IEntity> pads)
	{
		IEntity child = GetOwner().GetChildren();
		while (child)
		{
			if (child.FindComponent(AROR_LandingPadComponent))
				CollectHierarchy(child, pads);
			child = child.GetSibling();
		}
	}

	protected void CollectHierarchy(IEntity entity, notnull array<IEntity> entities)
	{
		entities.Insert(entity);
		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectHierarchy(child, entities);
			child = child.GetSibling();
		}
	}

	static const float ROTOR_CLEARANCE_BOTTOM = 2.5;	//!< Above walls and sandbags, below a parked helicopter's rotor
	static const float ROTOR_CLEARANCE_TOP = 6;

	//! The helipad (direct child) whose centre is within three metres of a position, if any
	protected AROR_LandingPadComponent LandingPadAt(vector position)
	{
		IEntity child = GetOwner().GetChildren();
		while (child)
		{
			AROR_LandingPadComponent pad = AROR_LandingPadComponent.Cast(child.FindComponent(AROR_LandingPadComponent));
			if (pad && vector.DistanceXZ(child.GetOrigin(), position) <= 3)
				return pad;
			child = child.GetSibling();
		}
		return null;
	}

	//! A parked vehicle stands on a helipad's deck where one is under it, otherwise on the terrain
	protected float PadSurface(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(position[0], position[2]);
		array<IEntity> pads = {};
		CollectLandingPads(pads);
		if (pads.IsEmpty())
			return ground;
		TraceParam trace = new TraceParam();
		trace.Start = Vector(position[0], ground + 2, position[2]);
		trace.End = Vector(position[0], ground - 0.5, position[2]);
		trace.Flags = TraceFlags.ENTS;
		float fraction = world.TraceMove(trace, null);
		if (fraction >= 1 || !pads.Contains(trace.TraceEnt))
			return ground;
		return Math.Max(ground, trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction);
	}

	protected bool HasPlayerClearance(vector position, int kind, float playerExclusion)
	{
		if (kind == 4)
			playerExclusion = Math.Min(playerExclusion, m_fCrewPlayerClearance);
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (player && vector.DistanceXZ(player.GetOrigin(), position) < playerExclusion)
				return false;
		}
		return true;
	}

	protected bool IsClear(vector position, int kind, int index, float playerExclusion)
	{
		BaseWorld world = GetGame().GetWorld();
		float size = 5;
		if (kind == 1)
			size = 1.5;
		if (kind == 0)
			size = 2.5; // Check room for a four-person team, not just its first soldier.
		if (kind == 4)
			size = 0.6;
		if (position[1] <= world.GetOceanHeight(position[0], position[2]) + 0.2 || !m_Location.IsPositionInside(position))
			return false;
		vector local = GetOwner().VectorToLocal(position - GetDefenceCenter());
		if (Math.AbsFloat(local[0]) > GetDefenceHalfExtent() - 2 || Math.AbsFloat(local[2]) > GetDefenceHalfDepth() - 2)
			return false;
		// A gun or sentry on a tower or roof: the platform replaces the ground, slope and navmesh checks (crews are seated
		// directly, and a sentry is spawned in place and holds it)
		bool raised = (kind == 1 || kind == 0) && position[1] - world.GetSurfaceY(position[0], position[2]) > 1.5;
		// A sentry stands inside his post (a bunker's walls and roof are around him): one man's room, as on a platform
		bool posted = kind == 0 && index >= 0 && index < m_aSlots.Count() && IsSentry(m_aSlots[index]);
		if (raised || posted)
		{
			if (!HasPlayerClearance(position, kind, playerExclusion))
				return false;
			TraceBox platform = new TraceBox();
			platform.Start = position + Vector(0, 0.25, 0);
			platform.End = platform.Start + Vector(0, 0.05, 0);
			platform.Mins = Vector(-0.6, 0, -0.6);
			platform.Maxs = Vector(0.6, 1.6, 0.6);
			platform.Flags = TraceFlags.ENTS | TraceFlags.VISIBILITY;
			return world.TraceMove(platform, null) == 1;
		}
		if (Math.AbsFloat(world.GetSurfaceY(position[0] + size, position[2]) - position[1]) > 1 || Math.AbsFloat(world.GetSurfaceY(position[0], position[2] + size) - position[1]) > 1 || Math.AbsFloat(world.GetSurfaceY(position[0] - size, position[2]) - position[1]) > 1 || Math.AbsFloat(world.GetSurfaceY(position[0], position[2] - size) - position[1]) > 1)
			return false;
		for (int other = 0; other < m_aSlots.Count(); other++)
		{
			AROR_GarrisonSlot existing = m_aSlots[other];
			float spacing = 12;
			if (kind == 0 || kind == 4)
				spacing = 7;
			if (other != index && existing.m_iKind > 0 && existing.m_iKind < 4 && existing.m_bPlaced && !existing.m_bLost && vector.DistanceXZ(position, existing.m_vPosition) < spacing)
				return false;
		}
		if (!HasPlayerClearance(position, kind, playerExclusion))
			return false;
		TraceBox trace = new TraceBox();
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		NavmeshWorldComponent soldiers;
		if (aiWorld)
			soldiers = aiWorld.GetNavmeshWorldComponent("Soldiers");
		if (!soldiers)
			return false;
		if (!soldiers.IsTileLoaded(position))
		{
			soldiers.LoadTileIn(position);
			return false;
		}
		vector reachable;
		if (!soldiers.GetReachablePoint(position, 2, reachable) || vector.DistanceXZ(position, reachable) > 3 || Math.AbsFloat(position[1] - reachable[1]) > 1)
			return false;
		trace.Start = position + Vector(0, 0.25, 0);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-size, 0, -size);
		trace.Maxs = Vector(size, 3, size);
		trace.Flags = TraceFlags.ENTS | TraceFlags.VISIBILITY;
		if (kind == 2)
		{
			// A helipad's deck, markings and lights are where the helicopter belongs, not obstructions
			array<IEntity> pads = {};
			CollectLandingPads(pads);
			if (!pads.IsEmpty())
				trace.ExcludeArray = pads;
			// The helicopter on it needs its whole rotor disc clear at rotor height (buildings, trees, poles, towers)
			AROR_LandingPadComponent pad = LandingPadAt(position);
			if (pad)
			{
				TraceBox rotor = new TraceBox();
				rotor.Start = position + Vector(0, ROTOR_CLEARANCE_BOTTOM, 0);
				rotor.End = rotor.Start + Vector(0, 0.05, 0);
				rotor.Mins = Vector(-pad.GetRotorRadius(), 0, -pad.GetRotorRadius());
				rotor.Maxs = Vector(pad.GetRotorRadius(), ROTOR_CLEARANCE_TOP - ROTOR_CLEARANCE_BOTTOM, pad.GetRotorRadius());
				rotor.Flags = TraceFlags.ENTS | TraceFlags.VISIBILITY;
				rotor.ExcludeArray = pads;
				if (world.TraceMove(rotor, null) < 1)
					return false;
			}
		}
		return world.TraceMove(trace, null) == 1;
	}

	protected IEntity Spawn(ResourceName prefab, vector position, vector angles)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		vector rotation[3];
		Math3D.AnglesToMatrix(angles, rotation);
		for (int axis = 0; axis < 3; axis++)
			spawnParams.Transform[axis] = rotation[axis];
		spawnParams.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
	}

	//! One hull per call; native crew seat order is saved alongside the actual asset recipe.
	bool SpawnNextAsset_S(AROR_GarrisonFaction faction, float exclusion, int personnelAvailable = 512)
	{
		if (!Replication.IsServer() || m_bDeploymentClosed)
			return false;
		int count = m_aSlots.Count();
		for (int offset = 0; offset < count; offset++)
		{
			int index = Math.Mod(m_iNextAsset + offset, count);
			AROR_GarrisonSlot slot = m_aSlots[index];
			if (slot.m_bLost || slot.m_bPlacementUnavailable || slot.m_bSpawned || slot.m_iKind == 0 || slot.m_iKind == 4)
				continue;
			int crewReservation;
			if (slot.m_iKind == 1)
				crewReservation = 1;
			if (slot.m_iKind == 3)
				crewReservation = 4;
			if (crewReservation > personnelAvailable)
				continue;
			m_iNextAsset = index + 1;
			if (!PlaceSlot(index, exclusion))
			{
				ReportPlacementFailure(index, "no clear asset pad (terrain, spacing, player clearance, navigation or obstruction)");
				return false;
			}
			IEntity entity = Spawn(slot.m_sPrefab, slot.m_vPosition, slot.m_vAngles);
			if (!entity)
			{
				ReportPlacementFailure(index, "asset resource failed: " + slot.m_sPrefab);
				return false;
			}
			if (!InitializeFaction(entity))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				ReportPlacementFailure(index, "asset faction binding failed: " + slot.m_sPrefab);
				return false;
			}
			array<BaseCompartmentSlot> availableSeats = {};
			if (slot.m_iKind != 2)
			{
				BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
				if (compartments)
					compartments.GetCompartments(availableSeats);
				int crewCount;
				foreach (BaseCompartmentSlot available : availableSeats)
				{
					if (available && !CargoCompartmentSlot.Cast(available))
						crewCount++;
				}
				int maximumCrew = 4;
				if (slot.m_iKind == 1)
					maximumCrew = 1;
				if (crewCount < 1 || crewCount > maximumCrew)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(entity);
					ReportPlacementFailure(index, "asset has no usable crew compartments or exceeds its reserved crew allowance");
					return false;
				}
			}
			slot.m_iPlacementFailureSince = -1;
			slot.m_Entity = entity;
			slot.m_bSpawned = true;
			m_bActive = true;
			DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
			if (damage)
				damage.SetHealthScaled(slot.m_fHealth);
			if (slot.m_iKind != 2)
				ReconcileCrew_S(index, availableSeats, faction);
			Print(string.Format("[AROR GARRISON] %1 asset slot=%2 kind=%3 at %4", m_Location.GetId(), index, slot.m_iKind, slot.m_vPosition), LogLevel.NORMAL);
			m_bDeploymentDeferred = false;
			return true;
		}
		return false;
	}

	//! Re-map a restored hull's old crew to current seats without replacing casualties.
	protected void ReconcileCrew_S(int vehicleIndex, array<BaseCompartmentSlot> seats, AROR_GarrisonFaction faction)
	{
		array<int> usable = {};
		for (int seat = 0; seat < seats.Count(); seat++)
		{
			if (seats[seat] && !CargoCompartmentSlot.Cast(seats[seat]))
				usable.Insert(seat);
		}
		array<ref AROR_GarrisonSlot> previous = {};
		foreach (AROR_GarrisonSlot crew : m_aSlots)
		{
			if (crew.m_iKind == 4 && crew.m_iVehicle == vehicleIndex)
				previous.Insert(crew);
		}
		for (int ordinal = 0; ordinal < previous.Count(); ordinal++)
		{
			AROR_GarrisonSlot saved = previous[ordinal];
			if (ordinal < usable.Count())
				saved.m_iSeat = usable[ordinal];
			else
			{
				saved.m_iVehicle = -1;
				saved.m_iSeat = -1; // Fewer seats must not erase an existing survivor.
			}
		}
		for (int extra = previous.Count(); extra < usable.Count(); extra++)
		{
			int added = AddSlot(CharacterPrefab(AROR_EUnitType.CREW), 4, m_aSlots[vehicleIndex].m_iGroup, vehicleIndex, usable[extra]);
			if (!previous.IsEmpty() || m_aSlots[vehicleIndex].m_bCrewRecorded)
				m_aSlots[added].m_bLost = true; // A changed catalogue cannot grant replacement personnel.
		}
		m_aSlots[vehicleIndex].m_bCrewRecorded = true;
	}

	protected SCR_AIGroup CreateGroup_S(vector position)
	{
		SCR_AIGroup.IgnoreSpawning(true);
		IEntity entity = Spawn("{A22D9D957ABE44DF}Prefabs/Groups/AROR_GarrisonGroup.et", position, vector.Zero);
		SCR_AIGroup.IgnoreSpawning(false);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return null;
		}
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(m_sFactionKey);
		bool assigned;
		if (faction)
		{
			if (!group.GetFaction())
				assigned = group.InitFactionKey(m_sFactionKey);
			else
				assigned = group.SetFaction(faction);
		}
		if (!assigned)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return null;
		}
		group.SetCanDeleteIfNoPlayer(false);
		group.SetPrivate(true);
		group.SetPrivacyChangeable(false);
		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		return group;
	}

	//! Queue one team through the engine's shared AI queue, never spawn every soldier in one tick.
	bool SpawnNextGroup_S(int now, float exclusion, int personnelAvailable = 512)
	{
		if (!Replication.IsServer() || m_bDeploymentClosed)
			return false;
		DetachOrphanedCrew_S();
		int maximum = -1;
		foreach (AROR_GarrisonSlot member : m_aSlots)
			maximum = Math.Max(maximum, member.m_iGroup);
		int firstGroup = m_iNextGroup;
		for (int groupOffset = 0; groupOffset <= maximum; groupOffset++)
		{
			int groupId = Math.Mod(firstGroup + groupOffset, maximum + 1);
			bool requested;
			foreach (AROR_GarrisonGroup existing : m_aGroups)
			{
				if (existing.m_iId == groupId)
					requested = true;
			}
			if (requested)
				continue;
			array<int> indices = {};
			foreach (int index, AROR_GarrisonSlot candidate : m_aSlots)
			{
				if (candidate.m_iGroup == groupId && !candidate.m_bLost && !candidate.m_bPlacementUnavailable && (candidate.m_iKind == 0 || candidate.m_iKind == 4))
				{
					if (candidate.m_iVehicle >= 0 && !m_aSlots[candidate.m_iVehicle].m_bSpawned)
						continue;
					indices.Insert(index);
				}
			}
			if (indices.IsEmpty())
			{
				continue;
			}
			int additionalPersonnel;
			foreach (int budgetIndex : indices)
			{
				AROR_GarrisonSlot budgetMember = m_aSlots[budgetIndex];
				if (budgetMember.m_iKind == 4 && budgetMember.m_iVehicle >= 0 && !m_aSlots[budgetMember.m_iVehicle].m_bLost)
					continue;
				additionalPersonnel++;
			}
			if (additionalPersonnel > personnelAvailable)
				continue;
			m_iNextGroup = groupId + 1;
			AROR_GarrisonSlot first = m_aSlots[indices[0]];
			if (!PlaceSlot(indices[0], exclusion))
			{
				ReportPlacementFailure(indices[0], "no clear walkable team/crew spawn point");
				return false;
			}
			vector position = first.m_vPosition;
			SCR_AIGroup group = CreateGroup_S(position);
			if (!group)
			{
				ReportPlacementFailure(indices[0], "group resource/faction setup failed");
				return false;
			}
			first.m_iPlacementFailureSince = -1;
			// Members spawn as the role's base character and are dressed from their preset loadout at current Readiness
			array<AROR_EUnitType> types = {};
			foreach (int slotIndex : indices)
				types.Insert(SlotUnitType(m_aSlots[slotIndex]));
			AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
			if (!presets || presets.FillGroup_S(group, m_eRosterRole, types) != indices.Count())
			{
				SCR_EntityHelper.DeleteEntityAndChildren(group);
				ReportPlacementFailure(indices[0], "role has no character for a roster unit (faction presets)");
				return false;
			}
			AROR_GarrisonGroup pending = new AROR_GarrisonGroup();
			pending.m_iId = groupId;
			pending.m_Group = group;
			pending.m_aSlots.Copy(indices);
			pending.m_iQueuedAt = now;
			m_aGroups.Insert(pending);
			m_bActive = true;
			AROR_QRFDefenseComponent defense = AROR_QRFDefenseComponent.Cast(group.FindComponent(AROR_QRFDefenseComponent));
			if (defense)
				defense.BindDefense_S(m_eRosterRole, m_Location.GetId());
			group.RequestSpawn(indices.Count());
			m_bDeploymentDeferred = false;
			Print(string.Format("[AROR GARRISON] %1 group=%2 queued personnel=%3", m_Location.GetId(), groupId, indices.Count()), LogLevel.NORMAL);
			m_iNextGroup = groupId + 1;
			return true;
		}
		return false;
	}

	protected void AssignMembers(AROR_GarrisonGroup pending)
	{
		if (!pending.m_Group)
			return;
		array<AIAgent> agents = {};
		pending.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			IEntity entity;
			if (agent)
				entity = agent.GetControlledEntity();
			if (!entity)
				continue;
			bool known;
			foreach (int knownIndex : pending.m_aSlots)
			{
				if (m_aSlots[knownIndex].m_Entity == entity)
					known = true;
			}
			if (known)
				continue;
			AROR_GarrisonSlot slot;
			foreach (int availableIndex : pending.m_aSlots)
			{
				AROR_GarrisonSlot candidate = m_aSlots[availableIndex];
				if (!candidate.m_bLost && !candidate.m_bPlacementUnavailable && !candidate.m_Entity)
				{
					slot = candidate;
					break;
				}
			}
			if (!slot)
				continue;
			slot.m_Entity = entity;
			slot.m_bSpawned = true;
			if (!InitializeFaction(entity))
			{
				if (!IsPlayerOrForeignAsset(entity))
					SCR_EntityHelper.DeleteEntityAndChildren(entity);
				slot.m_Entity = null;
				slot.m_bSpawned = false;
				slot.m_bPlacementUnavailable = true;
				continue;
			}
			m_bHadDeployedPersonnel = true;
			DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
			if (damage)
				damage.SetHealthScaled(slot.m_fHealth);
		}
	}

	//! Seat occupancy is maintained after initial readiness, including partial queue recovery.
	protected bool MaintainCrewSeats_S(AROR_GarrisonGroup pending)
	{
		if (!Replication.IsServer() || !pending.m_Group)
			return false;
		int now = GetGame().GetWorld().GetWorldTime();
		bool retry = now >= pending.m_iNextSeatAttempt;
		bool attempted;
		bool ready = true;
		foreach (int index : pending.m_aSlots)
		{
			AROR_GarrisonSlot slot = m_aSlots[index];
			if (slot.m_iKind != 4 || slot.m_iVehicle < 0 || slot.m_bLost || slot.m_bPlacementUnavailable || !slot.m_Entity)
				continue;
			AROR_GarrisonSlot vehicle = m_aSlots[slot.m_iVehicle];
			if (!vehicle.m_Entity || vehicle.m_bLost || vehicle.m_iKind == 2)
				continue; // Parked armour is intentionally unmanned; lost-hull crew defend on foot.
			DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.m_Entity.FindComponent(DamageManagerComponent));
			if (damage && damage.IsDestroyed())
				continue;
			if (IsPlayerOrForeignAsset(vehicle.m_Entity) || IsPlayerOrForeignAsset(slot.m_Entity))
				continue;
			BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.m_Entity.FindComponent(BaseCompartmentManagerComponent));
			array<BaseCompartmentSlot> seats = {};
			if (manager)
				manager.GetCompartments(seats);
			if (slot.m_iSeat < 0 || slot.m_iSeat >= seats.Count() || !seats[slot.m_iSeat])
			{
				ready = false;
				continue;
			}
			// Register before boarding so native compartment-enter events know the group's vehicle.
			SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.m_Entity.FindComponent(SCR_AIVehicleUsageComponent));
			if (usage && pending.m_Group.GetGroupUtilityComponent())
				pending.m_Group.GetGroupUtilityComponent().AddUsableVehicle(usage);
			BaseCompartmentSlot seat = seats[slot.m_iSeat];
			IEntity occupant = seat.GetOccupant();
			bool ownCrew;
			foreach (int candidateIndex : pending.m_aSlots)
			{
				AROR_GarrisonSlot candidate = m_aSlots[candidateIndex];
				if (occupant && candidate.m_iKind == 4 && candidate.m_iVehicle == slot.m_iVehicle && !candidate.m_bLost && candidate.m_Entity == occupant && !IsPlayerOrForeignAsset(occupant))
				{
					ChimeraCharacter seated = ChimeraCharacter.Cast(occupant);
					if (seated && seated.GetCharacterController() && !seated.GetCharacterController().IsDead())
						ownCrew = true;
				}
			}
			if (ownCrew)
				continue; // Native swaps between surviving assigned crew need no forced reseating.
			ready = false;
			if (!retry)
				continue;
			attempted = true;
			if (occupant || seat.IsOccupied() || vector.DistanceXZ(slot.m_Entity.GetOrigin(), vehicle.m_Entity.GetOrigin()) > m_fCrewSeatRecoveryDistance)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(slot.m_Entity);
			if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
				continue;
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(slot.m_Entity.FindComponent(CompartmentAccessComponent));
			if (!access || access.IsGettingIn() || access.IsGettingOut() || access.IsSwitchingSeatsAnim())
				continue;
			if (access.IsInCompartment() && access.GetVehicleCompartmentManagerOwner() != vehicle.m_Entity)
				continue;
			access.GetInVehicle(vehicle.m_Entity, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
		}
		if (attempted)
		{
			pending.m_iNextSeatAttempt = now + Math.Max(1, m_iCrewSeatRetrySeconds) * 1000;
			pending.m_iSeatAttempts++;
			if (pending.m_iSeatAttempts == 1 || Math.Mod(pending.m_iSeatAttempts, 10) == 0)
				Print(string.Format("[AROR GARRISON] %1 group=%2 awaiting physical crew occupancy, seating attempt=%3", m_Location.GetId(), pending.m_iId, pending.m_iSeatAttempts), LogLevel.NORMAL);
		}
		return ready;
	}

	protected bool HasMountedDefence(AROR_GarrisonGroup pending)
	{
		foreach (int index : pending.m_aSlots)
		{
			AROR_GarrisonSlot slot = m_aSlots[index];
			if (slot.m_iKind != 4 || slot.m_iVehicle < 0 || slot.m_bLost || slot.m_bPlacementUnavailable)
				continue;
			AROR_GarrisonSlot vehicle = m_aSlots[slot.m_iVehicle];
			if (vehicle.m_Entity && !vehicle.m_bLost && (vehicle.m_iKind == 1 || vehicle.m_iKind == 3) && !IsPlayerOrForeignAsset(vehicle.m_Entity))
			{
				DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.m_Entity.FindComponent(DamageManagerComponent));
				if (!damage || !damage.IsDestroyed())
					return true;
			}
		}
		return false;
	}

	protected void CrewAndOrder(AROR_GarrisonGroup pending)
	{
		if (!pending.m_Group || !MaintainCrewSeats_S(pending))
			return;
		bool mounted = HasMountedDefence(pending);
		int now = GetGame().GetWorld().GetWorldTime();
		if (pending.m_bOrdered && pending.m_bMountedDefence == mounted && (mounted || !m_bLocalDefencePatrols || now < pending.m_iNextFootOrder))
			return;
		foreach (int expected : pending.m_aSlots)
		{
			AROR_GarrisonSlot slot = m_aSlots[expected];
			if (!slot.m_bLost && !slot.m_bPlacementUnavailable && !slot.m_Entity)
				return;
		}
		if (pending.m_Waypoint)
		{
			pending.m_Group.RemoveWaypoint(pending.m_Waypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(pending.m_Waypoint);
			pending.m_Waypoint = null;
		}
		pending.m_bMountedDefence = mounted;
		// A tower marksman holds his post: a defend waypoint would walk him off the platform
		AROR_GarrisonSlot sentry = m_aSlots[pending.m_aSlots[0]];
		if (IsSentry(sentry))
		{
			if (!pending.m_bOrdered)
			{
				// Group spawns put members on the terrain, inside the tower's base: lift him onto the platform found
				// for the slot (on the ground fallback the heights already match and he stays where he is)
				BaseGameEntity marksman = BaseGameEntity.Cast(sentry.m_Entity);
				vector standing;
				if (marksman)
					standing = marksman.GetOrigin();
				if (marksman && !IsPlayerOrForeignAsset(marksman) && Math.AbsFloat(standing[1] - sentry.m_vPosition[1]) > 1)
				{
					vector transform[4];
					Math3D.AnglesToMatrix(Vector(sentry.m_vAngles[0], 0, 0), transform);
					transform[3] = sentry.m_vPosition;
					marksman.Teleport(transform);
				}
				float above = sentry.m_vPosition[1] - GetGame().GetWorld().GetSurfaceY(sentry.m_vPosition[0], sentry.m_vPosition[2]);
				Print(string.Format("[AROR GARRISON] %1 group=%2 sentry holding post %3m above ground", m_Location.GetId(), pending.m_iId, above), LogLevel.NORMAL);
			}
			pending.m_bOrdered = true;
			return;
		}
		if (mounted)
		{
			// Infantry defend waypoints can order vehicle crews out. Keep the assigned
			// hull/static occupied; native vehicle AI still owns combat and targeting.
			pending.m_bOrdered = true;
			Print(string.Format("[AROR GARRISON] %1 group=%2 mounted defence ready with observed seat occupancy", m_Location.GetId(), pending.m_iId), LogLevel.NORMAL);
			return;
		}
		AROR_GarrisonSlot lead = m_aSlots[pending.m_aSlots[0]];
		vector goal = lead.m_vPosition;
		if (m_bLocalDefencePatrols && Math.Mod(pending.m_iId, 2) == 1)
			goal = InfantryDefenceGoal(pending, lead);
		pending.m_iNextFootOrder = now + Math.Max(20, m_iInfantryOrderInterval) * 1000 + Math.Mod(pending.m_iId, 4) * 3000;
		IEntity waypointEntity = Spawn("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", goal, vector.Zero);
		pending.m_Waypoint = AIWaypoint.Cast(waypointEntity);
		if (!pending.m_Waypoint)
			return;
		pending.m_Waypoint.SetCompletionRadius(m_fDefenceHoldRadius);
		pending.m_Group.AddWaypoint(pending.m_Waypoint);
		pending.m_bOrdered = true;
		Print(string.Format("[AROR GARRISON] %1 group=%2 on-foot defence ready", m_Location.GetId(), pending.m_iId), LogLevel.NORMAL);
	}
	//! React to a last-reported position, never read a player's live position for tasking.
	//! Keep every task inside the footprint and capture radius so defenders remain discoverable.
	protected vector InfantryDefenceGoal(AROR_GarrisonGroup pending, AROR_GarrisonSlot lead)
	{
		vector local = GetOwner().VectorToLocal(lead.m_vPosition - GetDefenceCenter());
		AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();
		AROR_CombatContact contact;
		if (operations)
			contact = operations.GetRecentCombatContact(m_eRosterRole, GetDefenceCenter(), m_Location.GetRadius(), 60);
		if (contact && contact.m_sLocationId == m_Location.GetId())
		{
			local = GetOwner().VectorToLocal(contact.m_vPosition - GetDefenceCenter());
			pending.m_iContactSequence = contact.m_iSequence;
		}
		else
		{
			pending.m_iContactSequence = 0;
			pending.m_iPatrolLeg++;
			if (Math.Mod(pending.m_iPatrolLeg, 2) == 1)
			{
				local[0] = local[0] * 0.5;
				local[2] = local[2] * 0.5;
			}
		}
		float limit = Math.Max(2, GetDefenceHalfExtent() - 10);
		local[0] = Math.Clamp(local[0], -limit, limit);
		local[2] = Math.Clamp(local[2], -limit, limit);
		vector goal = GetDefenceCenter() + GetOwner().VectorToParent(local);
		goal[1] = GetGame().GetWorld().GetSurfaceY(goal[0], goal[2]);
		if (!m_Location.IsPositionInside(goal))
			return lead.m_vPosition;
		ChimeraAIWorld world = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		NavmeshWorldComponent soldiers;
		if (world)
			soldiers = world.GetNavmeshWorldComponent("Soldiers");
		vector reachable;
		if (!soldiers || !soldiers.GetReachablePoint(goal, 2, reachable))
			return lead.m_vPosition;
		if (!m_Location.IsPositionInside(reachable) || vector.DistanceXZ(goal, reachable) > 3)
			return lead.m_vPosition;
		return reachable;
	}

	//! Native dormancy is not a confirmed death. Keep its reserve until the engine rematerialises it.
	protected bool HasDormantReserve(AROR_GarrisonSlot slot)
	{
		if (slot.m_iKind != 0 && slot.m_iKind != 4)
			return false; // A dormant crew is not evidence that its missing hull still exists.
		foreach (AROR_GarrisonGroup group : m_aGroups)
		{
			if (group.m_iId == slot.m_iGroup && group.m_Group && group.m_Group.IsDormant() && group.m_Group.GetDormantAliveCount() > 0)
				return true;
		}
		return false;
	}

	//! Save losses before any retirement; virtualisation must never count as combat damage.
	void Observe_S(bool issueOrders = true)
	{
		if (!Replication.IsServer())
			return;
		foreach (AROR_GarrisonGroup pending : m_aGroups)
		{
			AssignMembers(pending);
			if (issueOrders && !pending.m_bQueueClosed && pending.m_Group && pending.m_Group.IsDormant() && pending.m_Group.GetDormantAliveCount() > 0)
			{
				pending.m_bOrdered = false;
				int now = GetGame().GetWorld().GetWorldTime();
				if (now >= pending.m_iNextWake)
				{
					pending.m_iNextWake = now + 30000;
					pending.m_Group.RequestSpawn();
				}
			}
		}
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			if (slot.m_bLost || !slot.m_bSpawned)
				continue;
			if (!slot.m_Entity)
			{
				if (HasDormantReserve(slot))
					continue;
				slot.m_bLost = true; // Missing previously physical assets must not regenerate.
				continue;
			}
			if (IsPlayerOrForeignAsset(slot.m_Entity))
			{
				slot.m_bLost = true;
				slot.m_Entity = null; // Explicit transfer; garrison never deletes or respawns it.
				continue;
			}
			DamageManagerComponent damage = DamageManagerComponent.Cast(slot.m_Entity.FindComponent(DamageManagerComponent));
			ChimeraCharacter character = ChimeraCharacter.Cast(slot.m_Entity);
			if ((damage && damage.IsDestroyed()) || (character && character.GetCharacterController() && character.GetCharacterController().IsDead()))
				slot.m_bLost = true;
			if (damage)
				slot.m_fHealth = Math.Min(slot.m_fHealth, damage.GetHealthScaled());
		}
		if (issueOrders && m_Location.GetControllingRole() == m_eRosterRole)
		{
			foreach (AROR_GarrisonGroup group : m_aGroups)
				CrewAndOrder(group);
		}
	}

	//! Never delete a stolen/foreign-occupied asset or a player-controlled soldier.
	protected bool IsPlayerOrForeignAsset(IEntity entity)
	{
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity) > 0)
			return true;
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (affiliation && !affiliation.GetAffiliatedFactionKey().IsEmpty() && affiliation.GetAffiliatedFactionKey() != m_sFactionKey)
			return true;
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
		if (!manager)
			return false;
		array<BaseCompartmentSlot> seats = {};
		manager.GetCompartments(seats);
		foreach (BaseCompartmentSlot seat : seats)
		{
			if (!seat)
				continue;
			IEntity occupant = seat.GetOccupant();
			if (!occupant)
				continue;
			bool occupantIsOwned;
			foreach (AROR_GarrisonSlot slot : m_aSlots)
			{
				if (slot.m_Entity == occupant)
					occupantIsOwned = true;
			}
			if (!occupantIsOwned || GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(occupant) > 0)
				return true;
		}
		return false;
	}

	bool CanRetire(float distance, array<vector> players)
	{
		foreach (vector position : players)
		{
			if (vector.DistanceXZ(position, m_Location.GetCenter()) <= distance)
				return false;
			foreach (AROR_GarrisonSlot slot : m_aSlots)
			{
				if (slot.m_Entity && vector.DistanceXZ(position, slot.m_Entity.GetOrigin()) <= distance)
					return false;
			}
		}
		return true;
	}

	void Retire_S()
	{
		if (!Replication.IsServer())
			return;
		Observe_S(false);
		ClearAssignments_S();
		foreach (AROR_GarrisonGroup group : m_aGroups)
		{
			if (group.m_Waypoint)
				SCR_EntityHelper.DeleteEntityAndChildren(group.m_Waypoint);
			if (group.m_Group && group.m_Group.GetPlayerCount() == 0)
				SCR_EntityHelper.DeleteEntityAndChildren(group.m_Group); // Cancels outstanding native queue requests.
		}
		m_aGroups.Clear();
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			if (slot.m_Entity)
			{
				if (IsPlayerOrForeignAsset(slot.m_Entity) || (slot.m_iKind > 0 && slot.m_iKind < 4 && !m_Location.IsPositionInside(slot.m_Entity.GetOrigin())))
					slot.m_bLost = true; // Transfer to the world; no subsequent respawn.
				else
					SCR_EntityHelper.DeleteEntityAndChildren(slot.m_Entity);
			}
			slot.m_Entity = null;
			slot.m_bSpawned = false;
			slot.m_bPlacementUnavailable = false;
			slot.m_iPlacementFailureSince = -1;
		}
		m_bActive = false;
		m_iCapacityWaitSince = -1;
		m_iCapacityWaited = 0;
		m_bCapacityLimited = false;
		m_bHadDeployedPersonnel = false;
		m_bInfantryTerrainFallback = false;
		m_iNextGroup = 0;
		m_iNextAsset = 0;
		m_bDefenceTerrainChecked = false;
		m_bDeploymentDeferred = false;
		m_iDeploymentStartedAt = -1;
		m_bDeploymentClosed = m_bRestoreQuarantined;
	}

	//! Ownership mismatch is reconciled only after old physical actors can safely retire.
	void ResetRoster_S()
	{
		if (!Replication.IsServer() || m_bActive)
			return;
		m_aSlots.Clear();
		m_bGenerated = false;
		m_bRestoreQuarantined = false;
		m_bAssetsBound = false;
		m_bDeploymentClosed = false;
		m_iDeploymentStartedAt = -1;
		m_iOutsideSince = -1;
		m_iCapacityWaitSince = -1;
		m_iCapacityWaited = 0;
		m_bCapacityLimited = false;
		m_bHadDeployedPersonnel = false;
		m_bInfantryTerrainFallback = false;
		m_iNextGroup = 0;
		m_iNextAsset = 0;
		m_bDefenceTerrainChecked = false;
		m_bDeploymentDeferred = false;
	}
	void RestoreRoster_S(AROR_EFactionRole role, string faction, string assetFaction, int level, array<ref AROR_GarrisonSlot> slots)
	{
		if (!Replication.IsServer() || m_bActive)
			return;
		m_eRosterRole = role;
		m_sFactionKey = faction;
		m_sAssetFactionKey = assetFaction;
		m_iLevel = level;
		m_aSlots.Clear();
		foreach (AROR_GarrisonSlot slot : slots)
			m_aSlots.Insert(slot);
		m_bGenerated = true;
		m_bAssetsBound = false;
		DetachOrphanedCrew_S();
	}
	//! Unsupported/damaged headers quarantine the section instead of generating free replacements.
	void QuarantineRoster_S(string reason)
	{
		if (!Replication.IsServer() || m_bActive)
			return;
		m_aSlots.Clear();
		if (!m_Location)
			m_Location = AROR_LocationComponent.Cast(GetOwner().FindComponent(AROR_LocationComponent));
		m_eRosterRole = AROR_EFactionRole.OCCUPIERS;
		string id = "unregistered location";
		if (m_Location)
		{
			m_eRosterRole = m_Location.GetControllingRole();
			id = m_Location.GetId();
		}
		m_iLevel = 1;
		m_bGenerated = true;
		m_bRestoreQuarantined = true;
		m_bDeploymentClosed = true;
		Print(string.Format("[AROR GARRISON] %1 saved roster quarantined: %2; no automatic regeneration", id, reason), LogLevel.ERROR);
	}

	//! Saved kinds/variants bind to today's role catalogue, never saved prefab paths.
	bool BindAssets_S(AROR_GarrisonFaction faction)
	{
		if (!Replication.IsServer() || m_bRestoreQuarantined || !faction || faction.m_eRole != m_eRosterRole)
			return false;
		if (m_bAssetsBound)
			return true;
		m_sFactionKey = faction.m_sFactionKey;
		m_sAssetFactionKey = faction.m_sAssetFactionKey;
		foreach (AROR_GarrisonSlot slot : m_aSlots)
		{
			switch (slot.m_iKind)
			{
				case 0: slot.m_sPrefab = CharacterPrefab(InfantryType(slot.m_iVariant)); break;
				case 1: slot.m_sPrefab = StaticPrefab(faction, slot.m_iVariant); break;
				case 2: slot.m_sPrefab = ParkedPrefab(faction, slot.m_iVariant); break;
				case 4: slot.m_sPrefab = CharacterPrefab(AROR_EUnitType.CREW); break;
				case 3:
				{
					if (!faction.m_aMannedVehicles || faction.m_aMannedVehicles.IsEmpty())
						slot.m_sPrefab = string.Empty;
					else
					{
						// Legacy saves may identify an unchanged choice by its old prefab.
						if (slot.m_bLegacyAsset && faction.m_aMannedVehicles.Contains(slot.m_sPrefab))
							slot.m_iVariant = faction.m_aMannedVehicles.Find(slot.m_sPrefab);
						slot.m_iVariant = Math.Clamp(slot.m_iVariant, 0, faction.m_aMannedVehicles.Count() - 1);
						slot.m_sPrefab = faction.m_aMannedVehicles[slot.m_iVariant];
					}
					break;
				}
			}
			slot.m_bLegacyAsset = false;
			// Layout is scenario data. Revalidate every restored placement without changing loss/health.
			slot.m_bPlaced = false;
		}
		m_bAssetsBound = true;
		return true;
	}

	bool OutsideLongEnough(bool outside, int now, int delay)
	{
		if (!outside)
			m_iOutsideSince = -1;
		else if (m_iOutsideSince < 0)
			m_iOutsideSince = now;
		return outside && now - m_iOutsideSince >= delay;
	}
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Location = AROR_LocationComponent.Cast(owner.FindComponent(AROR_LocationComponent));
	}
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && Replication.IsServer() && m_bActive)
			Retire_S();
		super.OnDelete(owner);
	}
}
