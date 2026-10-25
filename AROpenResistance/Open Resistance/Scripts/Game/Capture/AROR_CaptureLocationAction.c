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
			case AROR_ELocationType.RESOURCES:
			case AROR_ELocationType.OUTPOST:
				return true;
		}

		return false;
	}

	//! Existing military types remain unchanged; settlement flags are explicit prefab opt-ins.
	static bool IsCapturableLocation(AROR_LocationComponent location)
	{
		return location && (IsCapturableType(location.GetLocationType()) || location.AllowsSettlementFlagCapture());
	}

	//! Use the existing settlement authority so a flag capture cannot leave support contradicting control.
	static bool CaptureLocation_S(AROR_LocationComponent location, AROR_EFactionRole role)
	{
		if (!Replication.IsServer() || !IsCapturableLocation(location) || !location.CanBeTakenBy(role))
			return false;
		if (location.AllowsSettlementFlagCapture())
		{
			AROR_SettlementComponent settlement = AROR_SettlementComponent.Cast(location.GetOwner().FindComponent(AROR_SettlementComponent));
			if (!settlement)
				return false;
			settlement.SetSupport_S(role, 100);
			return location.GetControllingRole() == role;
		}
		return location.SetControllingRole_S(role);
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
		m_bHostilesPresent = AROR_HostilesQuery.HasHostilesInRadius(userFaction, location.GetCenter(), location.GetRadius());
		m_fLastHostileCheckTime = now;
		return m_bHostilesPresent;
	}

	//------------------------------------------------------------------------------------------------
	//! Needs the flag attached to its location: the flag prefab must have a Hierarchy component
	override bool CanBeShownScript(IEntity user)
	{
		AROR_LocationComponent location = GetLocation();
		if (!IsCapturableLocation(location) || location.IsControlFixed())
			return false;

		AROR_EFactionRole role;
		Faction faction;
		if (!GetUserRole(user, role, faction))
			return false;

		return role != location.GetControllingRole();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		AROR_LocationComponent location = GetLocation();
		AROR_EFactionRole role;
		Faction faction;
		if (!IsCapturableLocation(location) || !GetUserRole(user, role, faction))
			return false;

		if (!location.CanBeTakenBy(role))
		{
			SetCannotPerformReason("Recently captured");
			return false;
		}

		if (HasHostiles(faction, true))
		{
			SetCannotPerformReason("Hostiles in the area");
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
		if (!IsCapturableLocation(location) || !GetUserRole(pUserEntity, role, faction))
			return;

		// Authoritative re-check without the cache; the player is told why it failed
		string rejection;
		if (HasHostiles(faction, false))
			rejection = "Can't capture: hostiles in the area";
		else if (!CaptureLocation_S(location, role))
			rejection = "Can't capture: recently captured";

		if (!rejection.IsEmpty())
		{
			Print(string.Format("[AROR] Capture of '%1' rejected by the server", location.GetName()), LogLevel.NORMAL);

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
