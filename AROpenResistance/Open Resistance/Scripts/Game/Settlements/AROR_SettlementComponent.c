[ComponentEditorProps(category: "OpenResistance/Settlements", description: "Town or city data: population and support per role. Put on the Town and City location prefabs next to AROR_LocationComponent.")]
class AROR_SettlementComponentClass : ScriptComponentClass
{
}

//! Settlement (town or city): population and support (see ARCHITECTURE.md -> Settlements).
//! Support is per role, in percent, always summing to 100. It starts at 100% for the location's initial
//! controlling role; the server changes it, it replicates and is saved (AROR_SettlementComponentSerializer).
//! When a role other than the controller goes above 50%, control flips to it.
class AROR_SettlementComponent : ScriptComponent
{
	protected static const float TOTAL = 100;
	protected static const float MAJORITY = 50;

	[Attribute("0", UIWidgets.Slider, desc: "Number of inhabitants. Set per placed town or city.", params: "0 100000 10")]
	protected int m_iPopulation;

	[RplProp(onRplName: "OnSupportReplicated")]
	protected float m_fResistance;

	[RplProp(onRplName: "OnSupportReplicated")]
	protected float m_fOccupiers;

	[RplProp(onRplName: "OnSupportReplicated")]
	protected float m_fInvaders;

	protected AROR_LocationComponent m_Location;

	//------------------------------------------------------------------------------------------------
	AROR_LocationComponent GetLocation()
	{
		return m_Location;
	}

	//------------------------------------------------------------------------------------------------
	int GetPopulation()
	{
		return m_iPopulation;
	}

	//------------------------------------------------------------------------------------------------
	float GetSupport(AROR_EFactionRole role)
	{
		switch (role)
		{
			case AROR_EFactionRole.RESISTANCE:
				return m_fResistance;
			case AROR_EFactionRole.OCCUPIERS:
				return m_fOccupiers;
			case AROR_EFactionRole.INVADERS:
				return m_fInvaders;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! True while support is still the starting state (100% for the initial controlling role)
	bool IsInitialSupport()
	{
		if (!m_Location)
			return true;

		return GetSupport(m_Location.GetInitialControllingRole()) >= TOTAL;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Changes one role's support by delta (percentage points); the other roles scale
	//! proportionally so the total stays 100. May flip the location's control.
	void AddSupport_S(AROR_EFactionRole role, float delta)
	{
		SetSupport_S(role, GetSupport(role) + delta);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Sets one role's support (0-100); the other roles share the rest in their current proportion.
	void SetSupport_S(AROR_EFactionRole role, float value)
	{
		if (!Replication.IsServer())
			return;

		value = Math.Clamp(value, 0, TOTAL);

		array<AROR_EFactionRole> others = {};
		GetOtherRoles(role, others);

		float othersTotal = GetSupport(others[0]) + GetSupport(others[1]);
		float remainder = TOTAL - value;

		// Keep the other roles' ratio; split evenly if they had nothing
		float first, second;
		if (othersTotal > 0)
		{
			first = remainder * GetSupport(others[0]) / othersTotal;
			second = remainder - first;
		}
		else
		{
			first = remainder * 0.5;
			second = remainder - first;
		}

		SetValues(role, value, others[0], first, others[1], second);
	}

	//! Move the complete distribution together; sequential SetSupport calls would bias the last role.
	//! Each role changes by at most maxStep percentage points in one cycle.
	void ApplyRegionalSupport_S(float resistance, float occupiers, float invaders, float maxStep)
	{
		if (!Replication.IsServer() || !m_Location || m_Location.IsControlFixed() || maxStep <= 0)
			return;
		float total = resistance + occupiers + invaders;
		if (total <= 0 || resistance < 0 || occupiers < 0 || invaders < 0)
			return;
		float r = 100 * resistance / total;
		float o = 100 * occupiers / total;
		float i = 100 - r - o;
		float largest = Math.Max(Math.AbsFloat(r - m_fResistance), Math.Max(Math.AbsFloat(o - m_fOccupiers), Math.AbsFloat(i - m_fInvaders)));
		if (largest > 0.001)
		{
			float fraction = Math.Min(1, maxStep / largest);
			r = m_fResistance + (r - m_fResistance) * fraction;
			o = m_fOccupiers + (o - m_fOccupiers) * fraction;
			SetValues(AROR_EFactionRole.RESISTANCE, r, AROR_EFactionRole.OCCUPIERS, o, AROR_EFactionRole.INVADERS, 100 - r - o);
		}
		else
			UpdateControl_S(); // A previous majority may have been blocked by the flip cooldown.
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: restore saved values (used by the serializer); normalised to 100
	void RestoreSupport_S(float resistance, float occupiers, float invaders)
	{
		if (!Replication.IsServer())
			return;

		float total = resistance + occupiers + invaders;
		if (total <= 0)
			return;

		float scale = TOTAL / total;
		m_fResistance = resistance * scale;
		m_fOccupiers = occupiers * scale;
		m_fInvaders = invaders * scale;
		Replication.BumpMe();
		OnSupportReplicated();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetValues(AROR_EFactionRole roleA, float a, AROR_EFactionRole roleB, float b, AROR_EFactionRole roleC, float c)
	{
		SetValue(roleA, a);
		SetValue(roleB, b);
		SetValue(roleC, c);
		Replication.BumpMe();
		OnSupportReplicated();
		UpdateControl_S();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetValue(AROR_EFactionRole role, float value)
	{
		switch (role)
		{
			case AROR_EFactionRole.RESISTANCE:
				m_fResistance = value;
				break;
			case AROR_EFactionRole.OCCUPIERS:
				m_fOccupiers = value;
				break;
			case AROR_EFactionRole.INVADERS:
				m_fInvaders = value;
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void GetOtherRoles(AROR_EFactionRole role, notnull array<AROR_EFactionRole> outRoles)
	{
		array<AROR_EFactionRole> all = {};
		AROR_FactionRoles.GetMilitary(all);
		foreach (AROR_EFactionRole candidate : all)
		{
			if (candidate != role)
				outRoles.Insert(candidate);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Control goes to a role only once it holds more than 50%; otherwise the current controller keeps it
	protected void UpdateControl_S()
	{
		if (!m_Location)
			return;

		array<AROR_EFactionRole> all = {};
		AROR_FactionRoles.GetMilitary(all);
		foreach (AROR_EFactionRole role : all)
		{
			if (GetSupport(role) > MAJORITY && role != m_Location.GetControllingRole())
			{
				// A town with a standing garrison post only passes to the Resistance once its flag is taken: support
				// alone (a repelled response, regional support, missions) must not hand over a town still full of
				// the army's defenders. Taking the flag breaks the post and adds its swing, flipping it then.
				if (role == AROR_EFactionRole.RESISTANCE && IsPostStanding())
					continue;
				if (m_Location.SetControllingRole_S(role))
					Print(string.Format("[AROR] '%1' now controlled by %2 (support %3)", m_Location.GetName(), typename.EnumToString(AROR_EFactionRole, role), GetSupport(role)), LogLevel.NORMAL);

				return;
			}
		}
	}

	//! An AI army holds the town and its settlement garrison post has not been taken
	protected bool IsPostStanding()
	{
		AROR_GarrisonComponent post = AROR_CaptureLocationAction.GetSettlementPost(m_Location);
		return post && !post.IsPostBroken() && AROR_GarrisonPolicy.IsEnemyRole(m_Location.GetControllingRole());
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on clients when values arrive, and on the server when they change
	protected void OnSupportReplicated()
	{
		AROR_LocationManagerComponent locationManager = AROR_LocationManagerComponent.GetInstance();
		if (locationManager && m_Location)
			locationManager.GetOnLocationSupportChanged().Invoke(m_Location);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Location = AROR_LocationComponent.Cast(owner.FindComponent(AROR_LocationComponent));
		if (!m_Location)
		{
			Print("[AROR] AROR_SettlementComponent needs AROR_LocationComponent on the same entity", LogLevel.ERROR);
			return;
		}

		// Every machine starts at 100% for the initial controlling role; replication and saves override it
		SetValue(m_Location.GetInitialControllingRole(), TOTAL);
	}
}
