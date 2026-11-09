//! "Capture" interaction on a military location's flag (see ARCHITECTURE.md -> Location Capture).
//! Goes on the capture flag prop, which must be a child of the location entity.
//! Shown when the user's role does not control the location; performable only when no hostiles are inside the
//! location's radius and the flip cooldown allows it. The server re-checks everything before changing control.
class AROR_CaptureLocationAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! Location types captured through this action (towns and cities use support instead; the HQ is fixed)
	static bool IsCapturableType(AROR_ELocationType type)
	{
		switch (type)
		{
			case AROR_ELocationType.AIRPORT:
			case AROR_ELocationType.FACTORY:
			case AROR_ELocationType.MILITARY_BASE:
			case AROR_ELocationType.PORT:
			case AROR_ELocationType.HELIPORT:
			case AROR_ELocationType.RESOURCES:
			case AROR_ELocationType.OUTPOST:
				return true;
		}

		return false;
	}

	//! Towns and cities always use support, including legacy flag-bearing prefabs.
	static bool IsCapturableLocation(AROR_LocationComponent location)
	{
		return location && IsCapturableType(location.GetLocationType());
	}

	//! The flag path never changes settlement support.
	static bool CaptureLocation_S(AROR_LocationComponent location, AROR_EFactionRole role)
	{
		if (!Replication.IsServer() || !IsCapturableLocation(location) || !location.CanBeTakenBy(role))
			return false;
		return location.SetControllingRole_S(role);
	}

	//! An AI army occupies a town or city (an Invader attack order or simulated battle): towns follow support, so the
	//! army's support there is raised to at least the given share, which flips control through the normal rule. The
	//! garrison, town life, flag and building follow the new holder on their own. True when the army now holds it.
	static bool OccupyTown_S(AROR_LocationComponent location, AROR_EFactionRole role, float support)
	{
		if (!Replication.IsServer() || !location || !AROR_GarrisonPolicy.IsEnemyRole(role) || location.GetControllingRole() == role || !location.CanBeTakenBy(role))
			return false;
		AROR_ELocationType type = location.GetLocationType();
		if (type != AROR_ELocationType.TOWN && type != AROR_ELocationType.CITY)
			return false;
		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(location.GetOwner().FindComponent(AROR_SettlementComponent));
		if (!settlement)
			return false;
		AROR_EFactionRole previous = location.GetControllingRole();
		float before = settlement.GetSupport(role);
		settlement.SetSupport_S(role, Math.Max(before, support));
		bool occupied = location.GetControllingRole() == role;
		Print(string.Format("[AROR BALANCE] town_occupied town=%1 role=%2 previous=%3 support=%4->%5 occupied=%6", location.GetId(), role, previous, before, settlement.GetSupport(role), occupied), LogLevel.NORMAL);
		return occupied;
	}

	//! A town or city's garrison post, if the location has one (see AROR_GarrisonComponent.IsSettlementPost)
	static AROR_GarrisonComponent GetSettlementPost(AROR_LocationComponent location)
	{
		if (!location)
			return null;
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(location.GetOwner().FindComponent(AROR_GarrisonComponent));
		if (!garrison || !garrison.IsSettlementPost())
			return null;
		return garrison;
	}

	//! Taking a settlement post's flag never changes control directly: it swings support towards the taker (which
	//! may flip the town through Settlements) and breaks the post until it re-forms
	static bool TakeSettlementPost_S(AROR_LocationComponent location, AROR_EFactionRole role)
	{
		AROR_GarrisonComponent post = GetSettlementPost(location);
		if (!Replication.IsServer() || !post || post.IsPostBroken() || role == location.GetControllingRole())
			return false;
		AROR_EFactionRole holder = location.GetControllingRole();
		post.BreakSettlementPost_S();
		AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(location.GetOwner().FindComponent(AROR_SettlementComponent));
		float supportBefore;
		if (settlement)
		{
			supportBefore = settlement.GetSupport(role);
			settlement.AddSupport_S(role, post.GetPostSupportSwing());
			Print(string.Format("[AROR BALANCE] post_taken town=%1 holder=%2 support=%3->%4", location.GetId(), holder, supportBefore, settlement.GetSupport(role)), LogLevel.NORMAL);
		}
		// Losing a garrison post escalates the holder like losing a location (through Towns: Capture compiles before
		// the Operations file that adds the escalation call)
		AROR_TownWavesComponent.RecordPostTaken_S(location, holder);
		string name = location.GetName();
		name.ToUpper();
		string text = string.Format("%1 GARRISON POST TAKEN / Support for the Resistance rises", name);
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.HEADLINE, text, location.GetMapIcon(), AROR_UIColors.Get(AROR_UIColors.REBEL), "INTEL"));
		return true;
	}

	//! Locations whose flag this action works on: military captures and settlement posts
	protected static bool IsActionLocation(AROR_LocationComponent location)
	{
		return IsCapturableLocation(location) || GetSettlementPost(location) != null;
	}

	//! The hostile check queries a large area, so cache it while the player looks at the flag
	protected static const float HOSTILE_CHECK_INTERVAL_MS = 1000;

	protected AROR_LocationComponent m_Location;
	protected float m_fLastHostileCheckTime = -HOSTILE_CHECK_INTERVAL_MS;
	protected bool m_bHostilesPresent;

	//------------------------------------------------------------------------------------------------
	//! The location component is on the flag's parent (or further up the hierarchy)
	protected AROR_LocationComponent GetLocation()
	{
		if (m_Location)
			return m_Location;

		IEntity entity = GetOwner();
		while (entity)
		{
			m_Location = AROR_LocationComponent.Cast(entity.FindComponent(AROR_LocationComponent));
			if (m_Location)
				return m_Location;

			entity = entity.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool GetUserRole(IEntity user, out AROR_EFactionRole role, out Faction faction)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!roles || !user)
			return false;

		faction = SCR_Faction.GetEntityFaction(user);
		return roles.GetRoleOfFaction(faction, role);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasHostiles(Faction userFaction, bool useCache)
	{
		float now = GetGame().GetWorld().GetWorldTime();
		if (useCache && now - m_fLastHostileCheckTime < HOSTILE_CHECK_INTERVAL_MS)
			return m_bHostilesPresent;

		AROR_LocationComponent location = GetLocation();
		// A settlement post is cleared around its flag, not across the whole town (patrols and checkpoints roam)
		AROR_GarrisonComponent post = GetSettlementPost(location);
		if (post)
			m_bHostilesPresent = AROR_HostilesQuery.HasHostilesInRadius(userFaction, post.GetDefenceCenter(), post.GetPostClearRadius());
		else
			m_bHostilesPresent = AROR_HostilesQuery.HasHostilesInRadius(userFaction, location.GetCenter(), location.GetRadius());
		m_fLastHostileCheckTime = now;
		return m_bHostilesPresent;
	}
	protected string GetDefenceBlockReason()
	{
		AROR_LocationComponent location = GetLocation();
		if (!location)
			return string.Empty;
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(location.GetOwner().FindComponent(AROR_GarrisonComponent));
		if (!garrison)
			return string.Empty;
		return garrison.GetDefenceCaptureBlockReason();
	}

	//------------------------------------------------------------------------------------------------
	//! Needs the flag attached to its location: the flag prefab must have a Hierarchy component
	override bool CanBeShownScript(IEntity user)
	{
		AROR_LocationComponent location = GetLocation();
		if (!IsActionLocation(location) || location.IsControlFixed())
			return false;

		AROR_EFactionRole role;
		Faction faction;
		if (!GetUserRole(user, role, faction))
			return false;

		// A settlement post is only taken from an AI army that holds the town, and only while it stands
		AROR_GarrisonComponent post = GetSettlementPost(location);
		if (post)
			return role == AROR_EFactionRole.RESISTANCE && AROR_GarrisonPolicy.IsEnemyRole(location.GetControllingRole()) && !post.IsPostBroken();

		return role != location.GetControllingRole();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		AROR_LocationComponent location = GetLocation();
		AROR_EFactionRole role;
		Faction faction;
		if (!IsActionLocation(location) || !GetUserRole(user, role, faction))
			return false;

		AROR_GarrisonComponent post = GetSettlementPost(location);
		if (post && post.IsPostBroken())
		{
			SetCannotPerformReason("Garrison post already taken");
			return false;
		}

		if (!post && !location.CanBeTakenBy(role))
		{
			SetCannotPerformReason("Recently captured");
			return false;
		}

		if (HasHostiles(faction, true))
		{
			SetCannotPerformReason("Hostiles in the area");
			return false;
		}
		string deploymentReason = GetDefenceBlockReason();
		if (!deploymentReason.IsEmpty())
		{
			SetCannotPerformReason(deploymentReason);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		AROR_LocationComponent location = GetLocation();
		AROR_EFactionRole role;
		Faction faction;
		if (!IsActionLocation(location) || !GetUserRole(pUserEntity, role, faction))
			return;

		// Authoritative re-check without the cache; the player is told why it failed
		bool settlementPost = GetSettlementPost(location) != null;
		string rejection;
		string deploymentReason = GetDefenceBlockReason();
		if (HasHostiles(faction, false))
			rejection = "Can't capture: hostiles in the area";
		else if (!deploymentReason.IsEmpty())
			rejection = "Can't capture: " + deploymentReason;
		else if (settlementPost && (role != AROR_EFactionRole.RESISTANCE || !TakeSettlementPost_S(location, role)))
			rejection = "Can't capture: the garrison post is already taken";
		else if (!settlementPost && !CaptureLocation_S(location, role))
			rejection = "Can't capture: recently captured";

		if (!rejection.IsEmpty())
		{
			Print(string.Format("[AROR] Capture of '%1' rejected by the server: %2", location.GetName(), rejection), LogLevel.NORMAL);

			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
			if (playerId > 0)
				AROR_NotificationManagerComponent.SendHint_S(playerId, rejection);

			return;
		}

		Print(string.Format("[AROR] '%1' captured by %2", location.GetName(), typename.EnumToString(AROR_EFactionRole, role)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		AROR_LocationComponent location = GetLocation();
		if (!location)
			return false;

		outName = string.Format("Capture %1", location.GetName());
		if (GetSettlementPost(location))
			outName = string.Format("Take %1 garrison post", location.GetName());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The server performs the capture; control replicates to clients
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}
}
