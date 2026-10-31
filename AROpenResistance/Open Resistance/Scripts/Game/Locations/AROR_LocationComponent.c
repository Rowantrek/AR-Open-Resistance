[ComponentEditorProps(category: "OpenResistance/Locations", description: "Marks the owner entity as a map location (town, etc.). Place it via the AROR_Location_* prefabs.")]
class AROR_LocationComponentClass : ScriptComponentClass
{
}

//! A named area on the map. Registers itself with AROR_LocationManagerComponent when play starts.
//! The centre is the owner entity's position; the area is a circle of m_fRadius on the XZ plane.
//! Control: always held by a role. Starts as the initial role set on the placed location; the server changes it,
//! it replicates to clients and is saved (AROR_LocationComponentSerializer), so a loaded campaign restores it.
//! Replicated control needs an RplComponent on the location prefab.
class AROR_LocationComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Stable unique ID used by saves, e.g. \"everon_provins\". Never change it once saves exist.")]
	protected string m_sId;

	[Attribute("", UIWidgets.EditBox, desc: "Display name of the location, e.g. the town name")]
	protected string m_sName;

	[Attribute("0", UIWidgets.ComboBox, desc: "Type of location", enums: ParamEnumArray.FromEnum(AROR_ELocationType))]
	protected AROR_ELocationType m_eType;

	[Attribute("250", UIWidgets.Slider, desc: "Radius of the location in metres. Set per town in the World Editor.", params: "10 2000 5")]
	protected float m_fRadius;

	[Attribute("0", desc: "Ground this static scenario location on terrain before registration. Use for coordinate-catalog placements whose height is not authored; leave off for manually positioned locations.", category: "Placement")]
	protected bool m_bGroundOnInit;

	[Attribute("", desc: "Regional support override. Military sites name a settlement ID; settlements can share that ID to form a larger region. Empty uses this settlement's own ID / nearest settlement for military sites.", category: "Region")]
	protected string m_sSupportRegionId;

	string GetSupportRegionId()
	{
		return m_sSupportRegionId;
	}

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Icon drawn on the map for this location. White on transparent so it can be tinted.", params: "edds imageset")]
	protected ResourceName m_sMapIcon;

	[Attribute("1", UIWidgets.ComboBox, desc: "Role controlling this location when a new campaign starts. Set per placed location.", enums: AROR_FactionRoles.MilitaryRoleParams(), category: "Control")]
	protected AROR_EFactionRole m_eInitialControllingRole;

	[Attribute("0", desc: "Control can never change (e.g. the Resistance HQ)", category: "Control")]
	protected bool m_bControlFixed;

	//! After the Resistance takes a location, seconds before it can flip to the Occupiers or Invaders (fixed by design)
	protected static const float FLIP_COOLDOWN_SECONDS = 300;

	//! True while the post-Resistance-capture flip cooldown runs; server-owned, replicated, not saved
	[RplProp()]
	protected bool m_bFlipLocked;


	//! Current controlling role; server-owned, replicated, saved
	[RplProp(onRplName: "OnControllingRoleReplicated")]
	protected AROR_EFactionRole m_eControllingRole;

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	AROR_ELocationType GetLocationType()
	{
		return m_eType;
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetMapIcon()
	{
		return m_sMapIcon;
	}

	//------------------------------------------------------------------------------------------------
	AROR_EFactionRole GetControllingRole()
	{
		return m_eControllingRole;
	}

	//------------------------------------------------------------------------------------------------
	AROR_EFactionRole GetInitialControllingRole()
	{
		return m_eInitialControllingRole;
	}

	//------------------------------------------------------------------------------------------------
	bool IsControlFixed()
	{
		return m_bControlFixed;
	}

	//------------------------------------------------------------------------------------------------
	bool IsFlipLocked()
	{
		return m_bFlipLocked;
	}

	//------------------------------------------------------------------------------------------------
	//! False while control is fixed, already held by the role, the flip cooldown blocks the role, or a stronghold
	//! belongs to another AI role
	bool CanBeTakenBy(AROR_EFactionRole role)
	{
		if (m_bControlFixed || role == m_eControllingRole)
			return false;

		if (m_bFlipLocked && role != AROR_EFactionRole.RESISTANCE)
			return false;

		AROR_StrongholdComponent stronghold = AROR_StrongholdComponent.Cast(GetOwner().FindComponent(AROR_StrongholdComponent));
		return !stronghold || stronghold.AllowsCaptureBy(role);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Returns false if the change is not allowed (see CanBeTakenBy).
	bool SetControllingRole_S(AROR_EFactionRole role)
	{
		if (!Replication.IsServer() || !CanBeTakenBy(role))
			return false;
		if (m_eType == AROR_ELocationType.TOWN || m_eType == AROR_ELocationType.CITY)
		{
			AROR_SettlementComponent support = AROR_SettlementComponent.Cast(GetOwner().FindComponent(AROR_SettlementComponent));
			if (!support || support.GetSupport(role) <= 50)
				return false;
		}

		m_eControllingRole = role;

		// Resistance captures lock the location against flipping back for a while
		if (role == AROR_EFactionRole.RESISTANCE)
		{
			m_bFlipLocked = true;
			GetGame().GetCallqueue().Remove(UnlockFlip);
			GetGame().GetCallqueue().CallLater(UnlockFlip, FLIP_COOLDOWN_SECONDS * 1000);
		}

		Replication.BumpMe();
		OnControllingRoleReplicated();
		SendCapturedNotification_S(role);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Admin tool: gives the location to a role whatever the fixed-control, flip-cooldown and stronghold
	//! rules say (the caller checks IsControlFixed for locations that must never move), and announces it.
	//! Returns false if the role already holds it.
	bool ForceControllingRole_S(AROR_EFactionRole role)
	{
		if (!Replication.IsServer() || role == m_eControllingRole)
			return false;

		m_eControllingRole = role;
		m_bFlipLocked = false;
		GetGame().GetCallqueue().Remove(UnlockFlip);
		Replication.BumpMe();
		OnControllingRoleReplicated();
		SendCapturedNotification_S(role);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tell everyone who now controls the location (not used when restoring saves)
	protected void SendCapturedNotification_S(AROR_EFactionRole role)
	{
		string factionName = typename.EnumToString(AROR_EFactionRole, role);
		Color color;

		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (roles)
		{
			Faction faction = roles.GetFaction(role);
			if (faction)
			{
				factionName = faction.GetFactionName();
				color = faction.GetFactionColor();
			}
		}

		string text = string.Format("%1 captured by %2", m_sName, factionName);
		AROR_NotificationManagerComponent.Send_S(new AROR_Notification(AROR_ENotificationType.NOTIFICATION, text, m_sMapIcon, color, "Location captured"));
	}

	//------------------------------------------------------------------------------------------------
	protected void UnlockFlip()
	{
		m_bFlipLocked = false;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: restore saved control, bypassing the fixed-control rule (used by the serializer)
	void RestoreControllingRole_S(AROR_EFactionRole role)
	{
		if (!Replication.IsServer() || role == m_eControllingRole)
			return;

		m_eControllingRole = role;
		Replication.BumpMe();
		OnControllingRoleReplicated();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on clients when the replicated value arrives, and on the server when it changes
	protected void OnControllingRoleReplicated()
	{
		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (locationManager)
			locationManager.GetOnLocationControlChanged().Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	vector GetCenter()
	{
		return GetOwner().GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! True if the position lies within the location's radius (height is ignored)
	bool IsPositionInside(vector position)
	{
		return vector.DistanceXZ(GetCenter(), position) <= m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);

		// Every machine starts from the placed location's initial role; replication and saves override it
		m_eControllingRole = m_eInitialControllingRole;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame().InPlayMode())
			return;

		// Static scenario geometry, resolved identically on server and clients before registry use.
		// This does not change campaign ownership or any saved economic/support state.
		if (m_bGroundOnInit)
		{
			vector position = owner.GetOrigin();
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			owner.SetOrigin(position);
			owner.Update();
			if (position[1] <= 0)
				Print(string.Format("[AROR] Location '%1' catalog coordinate is at/below sea level; verify its placement", m_sId), LogLevel.WARNING);
		}

		if (m_sId.IsEmpty())
			Print(string.Format("[AROR] Location '%1' has no ID and cannot be referenced by saves", m_sName), LogLevel.WARNING);

		Print(string.Format("[AROR] Location '%1' (%2) initialised (%3, radius %4 m)", m_sName, m_sId, typename.EnumToString(AROR_ELocationType, m_eType), m_fRadius), LogLevel.NORMAL);
		AROR_LocationManagerComponent.RegisterLocation(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(UnlockFlip);
		AROR_LocationManagerComponent.UnregisterLocation(this);
		super.OnDelete(owner);
	}
}
