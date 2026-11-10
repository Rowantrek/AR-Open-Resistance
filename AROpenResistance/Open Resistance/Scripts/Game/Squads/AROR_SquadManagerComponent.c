[ComponentEditorProps(category: "OpenResistance/Squads", description: "Flag recruitment and phone management of native player squads.")]
class AROR_SquadManagerComponentClass : SCR_BaseGameModeComponentClass {}

//! Personal squads are native playable groups and their native AI subgroups, independent of HICOMM forces.
//! Only the server recruits/dismisses. Pending spawns reserve capacity before returning to the caller.
//! Recruits are session-only, free for now, and never use the campaign garrison roster or its resources.
class AROR_SquadManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("6", desc: "Maximum recruited riflemen per player squad (also respects the native command limit)", params: "1 16 1")]
	protected int m_iMaxRiflemen;

	[Attribute("5", desc: "Maximum distance from the recruiting flag, metres", params: "1 10 0.5")]
	protected float m_fFlagDistance;

	[Attribute("15", desc: "Search radius for a clear recruit spawn beside the flag, metres", params: "3 30 1")]
	protected float m_fSpawnRadius;

	[Attribute("30", desc: "Seconds before a failed recruit spawn is cancelled", params: "5 120 1")]
	protected float m_fSpawnTimeout;

	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Temporary single-rifleman spawn group", params: "et")]
	protected ResourceName m_sRecruitGroup;

	protected static const float HELD_SITE_MIN_RADIUS = 60;	//!< Phone recruiting works this close to a held site's centre even if its radius is smaller
	protected static AROR_SquadManagerComponent s_Instance;
	protected ref array<ref AROR_SquadRecruit> m_aRecruits = {};
	protected int m_iNextId = 1;

	[RplProp(onRplName: "OnRosterReplicated")]
	protected string m_sRoster;
	protected ref array<ref AROR_SquadMemberInfo> m_aRoster = {};

	static AROR_SquadManagerComponent GetInstance()
	{
		return s_Instance;
	}

	int GetLimit()
	{
		SCR_CommandingManagerComponent commanding = SCR_CommandingManagerComponent.GetInstance();
		if (!commanding)
			return 0;
		return Math.Min(m_iMaxRiflemen, commanding.GetMaxAIPerGroup());
	}

	array<ref AROR_SquadMemberInfo> GetRoster()
	{
		return m_aRoster;
	}

	//! Resolve the actual flag hierarchy, never a nearby arbitrary territory selected by a client.
	static AROR_LocationComponent GetFlagLocation(IEntity flag)
	{
		if (!flag || !flag.FindComponent(AROR_CaptureFlagPlacementComponent))
			return null;
		IEntity parent = flag.GetParent();
		while (parent)
		{
			AROR_LocationComponent location = AROR_LocationComponent.Cast(parent.FindComponent(AROR_LocationComponent));
			if (location)
				return location;
			parent = parent.GetParent();
		}
		return null;
	}

	protected bool IsReadyPlayer(int playerId, out IEntity player)
	{
		AROR_GameFlowComponent flow = AROR_GameFlowComponent.GetInstance();
		player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		return flow && flow.IsPlaythroughReady() && AROR_ForceSpawner.IsAlive(player) && roles && SCR_Faction.GetEntityFaction(player) == roles.GetFaction(AROR_EFactionRole.RESISTANCE);
	}

	//! Shared display/server policy. PerformAction always re-evaluates it on the authority.
	bool CanRecruit(int playerId, IEntity flag, out string reason)
	{
		reason = string.Empty;
		IEntity player;
		if (!IsReadyPlayer(playerId, player))
			reason = "Recruitment requires a living Resistance player in a running campaign";
		AROR_LocationComponent location = GetFlagLocation(flag);
		if (reason.IsEmpty() && (!location || location.GetControllingRole() != AROR_EFactionRole.RESISTANCE))
			reason = "Recruit at a Resistance-owned territory flag";
		if (reason.IsEmpty() && vector.Distance(player.GetOrigin(), flag.GetOrigin()) > m_fFlagDistance)
			reason = "Move closer to the flag";
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (reason.IsEmpty() && (!groups || !SCR_CommandingManagerComponent.GetInstance()))
			reason = "Native squad commanding is unavailable in this scenario";
		if (!reason.IsEmpty())
			return false;

		SCR_AIGroup group = groups.GetPlayerGroup(playerId);
		if (group && !group.IsPlayerLeader(playerId))
			reason = "Only your squad leader can recruit riflemen";
		else if (CountMembers(group) >= GetLimit())
			reason = "Your squad is full";
		return reason.IsEmpty();
	}

	protected int CountNativeMembers(SCR_AIGroup group)
	{
		int count;
		if (!group)
			return count;
		SCR_AIGroup slave = group.GetSlave();
		if (slave)
		{
			array<SCR_ChimeraCharacter> members = slave.GetAIMembers();
			foreach (SCR_ChimeraCharacter member : members)
			{
				if (AROR_ForceSpawner.IsAlive(member))
					count++;
			}
		}
		return count;
	}

	protected int CountMembers(SCR_AIGroup group)
	{
		if (!group)
			return 0;
		int count = CountNativeMembers(group);
		// Clients use the replicated pending roster, the authority uses transactions immediately.
		if (Replication.IsServer())
		{
			foreach (AROR_SquadRecruit recruit : m_aRecruits)
			{
				if (recruit.m_Master == group && !recruit.m_bJoined)
					count++;
			}
		}
		else
		{
			int managed;
			int pending;
			foreach (AROR_SquadMemberInfo info : m_aRoster)
			{
				if (info.m_iGroupId != group.GetGroupID())
					continue;
				if (info.m_sState == "Arriving")
					pending++;
				else
					managed++;
			}
			// Replicated entries still count when their distant characters are not streamed locally.
			count = Math.Max(count, managed) + pending;
		}
		return count;
	}

	//! Never take leadership from another player or move a player out of an existing squad.
	SCR_AIGroup EnsureSquad_S(int playerId)
	{
		if (!Replication.IsServer())
			return null;
		IEntity player;
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!IsReadyPlayer(playerId, player) || !groups || !SCR_CommandingManagerComponent.GetInstance())
			return null;
		SCR_AIGroup group = groups.GetPlayerGroup(playerId);
		if (group)
			return group;
		SCR_PlayerControllerGroupComponent controller = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(playerId);
		if (!controller)
			return null;
		Faction faction = SCR_Faction.GetEntityFaction(player);
		if (!groups.CanCreateNewGroup(faction))
			return null;
		group = groups.CreateNewPlayableGroup(faction);
		if (!group)
			return null;
		// Run the native server join handler so the owning controller receives its group ID
		// and commanding listeners. Adding a player to the manager alone does not do that.
		controller.RPC_AskJoinGroup(group.GetGroupID());
		if (groups.GetPlayerGroup(playerId) != group)
		{
			groups.DeleteAndUnregisterGroup(group);
			return null;
		}
		groups.SetGroupLeader(group.GetGroupID(), playerId);
		return group;
	}

	void Recruit_S(int playerId, IEntity flag)
	{
		if (!Replication.IsServer())
			return;
		string reason;
		if (!CanRecruit(playerId, flag, reason))
		{
			Hint(playerId, reason);
			return;
		}
		AROR_LocationComponent location = GetFlagLocation(flag);
		string siteName;
		if (location)
			siteName = location.GetName();
		SpawnRecruit_S(playerId, flag.GetOrigin(), siteName);
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: the Resistance-held site (the HQ, a town, an outpost...) whose ground a position is on, or null
	static AROR_LocationComponent FindHeldSiteAt(vector position)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return null;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		AROR_LocationComponent best;
		float bestDistance;
		foreach (AROR_LocationComponent location : all)
		{
			if (!location || location.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
				continue;

			float distance = vector.DistanceXZ(position, location.GetCenter());
			if (distance > Math.Max(location.GetRadius(), HELD_SITE_MIN_RADIUS))
				continue;

			if (!best || distance < bestDistance)
			{
				best = location;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: recruit from the phone while standing on any Resistance-held site, the HQ included, so a squad never
	//! depends on finding a flag. The rifleman turns up beside the player.
	void RecruitHere_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		IEntity player;
		if (!IsReadyPlayer(playerId, player))
		{
			Hint(playerId, "Recruitment requires a living Resistance player in a running campaign");
			return;
		}

		AROR_LocationComponent site = FindHeldSiteAt(player.GetOrigin());
		if (!site)
		{
			Hint(playerId, "Recruit at the HQ or any site the Resistance holds");
			return;
		}

		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups || !SCR_CommandingManagerComponent.GetInstance())
		{
			Hint(playerId, "Native squad commanding is unavailable in this scenario");
			return;
		}

		SCR_AIGroup current = groups.GetPlayerGroup(playerId);
		if (current && !current.IsPlayerLeader(playerId))
		{
			Hint(playerId, "Only your squad leader can recruit riflemen");
			return;
		}

		if (CountMembers(current) >= GetLimit())
		{
			Hint(playerId, "Your squad is full");
			return;
		}

		SpawnRecruit_S(playerId, player.GetOrigin(), site.GetName());
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnRecruit_S(int playerId, vector near, string siteName)
	{
		SCR_AIGroup group = EnsureSquad_S(playerId);
		if (!group || !group.IsPlayerLeader(playerId) || !group.GetSlave())
		{
			Hint(playerId, "Could not prepare your native squad command group");
			return;
		}

		vector position;
		if (!AROR_ForceSpawner.FindClearPosition(near, m_fSpawnRadius, 1, position))
		{
			Hint(playerId, "No clear ground nearby; try again");
			return;
		}

		array<AROR_EUnitType> types = {AROR_EUnitType.RIFLEMAN};
		SCR_AIGroup spawned = AROR_ForceSpawner.SpawnGroup(m_sRecruitGroup, AROR_EFactionRole.RESISTANCE, types, position);
		if (!spawned)
		{
			Hint(playerId, "Rifleman could not be spawned from the Resistance preset");
			return;
		}

		AROR_SquadRecruit recruit = new AROR_SquadRecruit();
		recruit.m_iId = m_iNextId++;
		recruit.m_Master = group;
		recruit.m_SpawnGroup = spawned;
		recruit.m_fStarted = GetGame().GetWorld().GetWorldTime();
		m_aRecruits.Insert(recruit);
		Publish_S();
		Hint(playerId, "Rifleman arriving. Use the native squad command wheel to give orders");
		Print(string.Format("[AROR SQUAD] player %1 recruited a rifleman at %2", playerId, siteName), LogLevel.NORMAL);
	}

	void Rename_S(int playerId, int groupId, string name)
	{
		if (!Replication.IsServer())
			return;
		SCR_AIGroup group = GetLedSquad(playerId, groupId);
		if (!group)
			return;
		string reason;
		// Same bounded plain-text rules as campaign names; no roster delimiters or rich-text markup.
		if (!AROR_ResistanceName.Validate(name, reason))
		{
			Hint(playerId, "Squad name: " + reason);
			return;
		}
		group.SetCustomName(name, playerId);
		Hint(playerId, "Squad renamed to " + name);
	}

	protected SCR_AIGroup GetLedSquad(int playerId, int groupId)
	{
		IEntity player;
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!IsReadyPlayer(playerId, player) || !groups)
			return null;
		SCR_AIGroup group = groups.GetPlayerGroup(playerId);
		if (!group || group.GetGroupID() != groupId || !group.IsPlayerLeader(playerId))
		{
			Hint(playerId, "Only the current squad leader can manage this squad");
			return null;
		}
		return group;
	}

	void Dismiss_S(int playerId, int groupId, int recruitId)
	{
		if (!Replication.IsServer())
			return;
		SCR_AIGroup group = GetLedSquad(playerId, groupId);
		if (!group)
			return;
		foreach (int index, AROR_SquadRecruit recruit : m_aRecruits)
		{
			if (recruit.m_iId != recruitId || recruit.m_Master != group)
				continue;
			SCR_AIGroup slave = group.GetSlave();
			if (recruit.m_bJoined && (!slave || !slave.IsAIControlledCharacterMember(recruit.m_Character)))
			{
				RemoveRecruit_S(index, false);
				Publish_S();
				Hint(playerId, "That rifleman is no longer in your squad");
				return;
			}
			if (IsPlayerControlled(recruit.m_Character))
			{
				Hint(playerId, "A player is controlling this rifleman");
				return;
			}
			RemoveRecruit_S(index, true);
			Publish_S();
			Hint(playerId, "Rifleman dismissed");
			return;
		}
	}

	protected bool IsPlayerControlled(IEntity entity)
	{
		return entity && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity) > 0;
	}

	protected AIGroup GetCharacterGroup(IEntity character)
	{
		if (!character)
			return null;
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (!control)
			return null;
		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
			return null;
		return agent.GetParentGroup();
	}

	//! Cleanup can run before the next poll, after a native release/transfer or the master group's deletion.
	//! Keep the original command subgroup so a departed squad cannot retire another group's soldier.
	protected bool CanRetireRecruit(AROR_SquadRecruit recruit)
	{
		if (!AROR_ForceSpawner.IsAlive(recruit.m_Character) || IsPlayerControlled(recruit.m_Character))
			return false;
		SCR_AIGroup expected = recruit.m_SpawnGroup;
		if (recruit.m_bJoined)
			expected = recruit.m_CommandGroup;
		AIGroup actual = GetCharacterGroup(recruit.m_Character);
		if (actual && actual != expected)
			return false;
		if (recruit.m_bJoined && expected && !expected.IsAIControlledCharacterMember(recruit.m_Character))
			return false;
		return true;
	}

	//! In the squad's native command group, by membership flag or by the agent's actual group (either can update first)
	protected bool IsInCommandGroup(SCR_ChimeraCharacter character, SCR_AIGroup commandGroup)
	{
		if (!character || !commandGroup)
			return false;
		return commandGroup.IsAIControlledCharacterMember(character) || GetCharacterGroup(character) == commandGroup;
	}

	//! The recruit now belongs to the native squad; its temporary spawn group is no longer needed
	protected void MarkJoined(notnull AROR_SquadRecruit recruit, SCR_AIGroup commandGroup)
	{
		recruit.m_bJoined = true;
		recruit.m_CommandGroup = commandGroup;
		if (recruit.m_SpawnGroup)
			SCR_EntityHelper.DeleteEntityAndChildren(recruit.m_SpawnGroup);
		recruit.m_SpawnGroup = null;
	}

	//! The engine may spawn the member later. Do not attach it until the preset outfitter has dressed it.
	protected void Update_S()
	{
		if (!Replication.IsServer())
			return;
		float now = GetGame().GetWorld().GetWorldTime();
		for (int i = m_aRecruits.Count() - 1; i >= 0; i--)
		{
			AROR_SquadRecruit recruit = m_aRecruits[i];
			if (!recruit.m_Master || recruit.m_Master.GetPlayerCount() == 0)
			{
				RemoveRecruit_S(i, true);
				continue;
			}
			if (recruit.m_bJoined)
			{
				SCR_AIGroup slave = recruit.m_Master.GetSlave();
				if (!AROR_ForceSpawner.IsAlive(recruit.m_Character) || !IsInCommandGroup(recruit.m_Character, slave))
					RemoveRecruit_S(i, false);
				continue;
			}
			array<IEntity> members = {};
			AROR_ForceSpawner.CountLiving(recruit.m_SpawnGroup, members);
			if (!members.IsEmpty())
				recruit.m_Character = SCR_ChimeraCharacter.Cast(members[0]);
			AIGroup currentGroup = GetCharacterGroup(recruit.m_Character);
			SCR_AIGroup commandGroup = recruit.m_Master.GetSlave();
			if (AROR_ForceSpawner.IsAlive(recruit.m_Character) && commandGroup && currentGroup == commandGroup)
			{
				// The native transfer requested on an earlier poll completed after that poll's membership check.
				// The squad's own command group is the destination, not an external transfer.
				MarkJoined(recruit, commandGroup);
				continue;
			}
			if (recruit.m_Character && (!AROR_ForceSpawner.IsAlive(recruit.m_Character) || (currentGroup && currentGroup != recruit.m_SpawnGroup)))
			{
				// Never reuse a stale character pointer after death or an external AI-group transfer.
				RemoveRecruit_S(i, false);
				continue;
			}
			if (IsPlayerControlled(recruit.m_Character))
			{
				// Game Master/player takeover transfers this entity out of our recruitment lifecycle.
				recruit.m_SpawnGroup = null;
				RemoveRecruit_S(i, false);
				continue;
			}
			if (recruit.m_Character && currentGroup && !AROR_LoadoutOutfitter.IsUndressed(recruit.m_Character))
			{
				// Native wheel recruitment can fill the subgroup while our spawn is pending.
				if (CountNativeMembers(recruit.m_Master) >= GetLimit())
				{
					Hint(recruit.m_Master.GetLeaderID(), "Squad filled before the rifleman arrived; recruitment cancelled");
					RemoveRecruit_S(i, true);
					continue;
				}
				SCR_PlayerControllerGroupComponent controller = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(recruit.m_Master.GetLeaderID());
				SCR_AIGroup subgroup = recruit.m_Master.GetSlave();
				if (controller && subgroup)
				{
					// Native transfer updates agents, recruited flags, radio tuning and replicated AI membership.
					controller.AddAIToSlaveGroup(recruit.m_Character, recruit.m_Master);
					// May complete later; the next poll then sees the character in the command group (above)
					if (IsInCommandGroup(recruit.m_Character, subgroup))
						MarkJoined(recruit, subgroup);
				}
			}
			if (!recruit.m_bJoined && now - recruit.m_fStarted > m_fSpawnTimeout * 1000)
			{
				Hint(recruit.m_Master.GetLeaderID(), "Rifleman recruitment failed; squad space released");
				RemoveRecruit_S(i, true);
			}
		}
		Publish_S();
	}

	protected void RemoveRecruit_S(int index, bool deleteCharacter)
	{
		AROR_SquadRecruit recruit = m_aRecruits[index];
		if (deleteCharacter && CanRetireRecruit(recruit))
		{
			if (recruit.m_bJoined && recruit.m_Master)
			{
				SCR_PlayerControllerGroupComponent controller = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(recruit.m_Master.GetLeaderID());
				if (controller)
					controller.RemoveAiFromSlaveGroup(recruit.m_Character, recruit.m_Master);
			}
			SCR_EntityHelper.DeleteEntityAndChildren(recruit.m_Character);
		}
		if (recruit.m_SpawnGroup)
		{
			// AI characters are not necessarily hierarchy children of their group. Cancel pending
			// members explicitly as well as the group, including a member not observed by Update yet.
			array<IEntity> pendingMembers = {};
			AROR_ForceSpawner.CountLiving(recruit.m_SpawnGroup, pendingMembers);
			bool preserveGroup = recruit.m_SpawnGroup.GetPlayerCount() > 0;
			foreach (IEntity pendingMember : pendingMembers)
			{
				if (IsPlayerControlled(pendingMember))
					preserveGroup = true;
				else if (GetCharacterGroup(pendingMember) == recruit.m_SpawnGroup)
					SCR_EntityHelper.DeleteEntityAndChildren(pendingMember);
			}
			if (!preserveGroup)
				SCR_EntityHelper.DeleteEntityAndChildren(recruit.m_SpawnGroup);
		}
		m_aRecruits.RemoveOrdered(index);
	}

	protected void Publish_S()
	{
		string snapshot;
		foreach (AROR_SquadRecruit recruit : m_aRecruits)
		{
			if (!recruit.m_Master)
				continue;
			// A character may be deleted between the periodic update and an immediate request publication.
			if (recruit.m_bJoined && !AROR_ForceSpawner.IsAlive(recruit.m_Character))
				continue;
			string state = "Arriving";
			if (recruit.m_bJoined)
			{
				state = "Ready";
				CompartmentAccessComponent access = CompartmentAccessComponent.Cast(recruit.m_Character.FindComponent(CompartmentAccessComponent));
				if (access && access.IsInCompartment())
					state = "In vehicle";
			}
			snapshot += string.Format("%1|%2|%3;", recruit.m_iId, recruit.m_Master.GetGroupID(), state);
		}
		if (snapshot == m_sRoster)
			return;
		m_sRoster = snapshot;
		Replication.BumpMe();
		OnRosterReplicated();
	}

	protected void OnRosterReplicated()
	{
		m_aRoster.Clear();
		array<string> rows = {};
		m_sRoster.Split(";", rows, true);
		foreach (string row : rows)
		{
			array<string> fields = {};
			row.Split("|", fields, false);
			if (fields.Count() != 3)
				continue;
			AROR_SquadMemberInfo info = new AROR_SquadMemberInfo();
			info.m_iId = fields[0].ToInt();
			info.m_iGroupId = fields[1].ToInt();
			info.m_sState = fields[2];
			m_aRoster.Insert(info);
		}
	}

	protected void Hint(int playerId, string message)
	{
		// Logged too, so a refusal the player saw can be traced afterwards
		Print(string.Format("[AROR SQUAD] player %1: %2", playerId, message), LogLevel.NORMAL);
		AROR_NotificationManagerComponent.SendHint_S(playerId, message);
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		s_Instance = this;
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(Update_S, 1000, true);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Update_S);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}
}
