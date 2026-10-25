//! REAPER integration signatures checked against ArmaForces/DefenseInDepth
//! AFM_M_REAPER_AiHelicopterControllerComponent and AFM_DiDAirSpawnerComponent,
//! commit 9972e5bb83310850c75adb23b31ba7efa505c321. Requires installed-version compile.
//! This hook filters REAPER's perceived targets; it never supplies unseen enemies.
modded class REAPER_AiHelicopterControllerComponent
{
	protected bool m_bAROR_Configured;
	protected bool m_bAROR_CombatEnabled;
	protected string m_sAROR_Faction;
	protected vector m_vAROR_AreaCenter;
	protected float m_fAROR_AreaRadius;
	protected float m_fAROR_FriendlyClearance;

	void AROR_ConfigureQRF(string factionKey, vector center, float radius, float friendlyClearance, bool combatEnabled)
	{
		if (!Replication.IsServer())
			return;
		m_bAROR_Configured = true;
		m_sAROR_Faction = factionKey;
		m_vAROR_AreaCenter = center;
		m_fAROR_AreaRadius = radius;
		m_fAROR_FriendlyClearance = friendlyClearance;
		m_bAROR_CombatEnabled = combatEnabled;
		// No unlimited ammunition. Both AI and players may be legitimate enemies.
		REAPER_SetWeaponProperties(combatEnabled, false, false, 30, false, 20);
	}

	override private bool REAPER_IsAttackTargetAllowed(BaseTarget target)
	{
		if (!super.REAPER_IsAttackTargetAllowed(target))
			return false;
		if (!m_bAROR_Configured)
			return true;
		if (!Replication.IsServer() || !m_bAROR_CombatEnabled || !target)
			return false;
		IEntity entity = target.GetTargetEntity();
		if (!entity || vector.DistanceXZ(entity.GetOrigin(), m_vAROR_AreaCenter) > m_fAROR_AreaRadius)
			return false;
		Faction ours = GetGame().GetFactionManager().GetFactionByKey(m_sAROR_Faction);
		Faction theirs = SCR_Faction.GetEntityFaction(entity);
		if (!theirs)
			theirs = SCR_Faction.GetEntityFaction(entity.GetRootParent());
		if (!ours || !theirs || !ours.IsFactionEnemy(theirs))
			return false;
		return !AROR_ReaperFriendlyQuery.HasNonHostiles(ours, entity.GetOrigin(), m_fAROR_FriendlyClearance);
	}
}

//! Conservative explosive-footprint check, including friendly vehicles and civilians.
class AROR_ReaperFriendlyQuery
{
	protected Faction m_Faction;
	protected bool m_bFound;

	static bool HasNonHostiles(Faction faction, vector center, float radius)
	{
		AROR_ReaperFriendlyQuery query = new AROR_ReaperFriendlyQuery();
		query.m_Faction = faction;
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, query.OnEntity, null, EQueryEntitiesFlags.DYNAMIC);
		return query.m_bFound;
	}

	protected bool OnEntity(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character && (!character.GetCharacterController() || character.GetCharacterController().IsDead()))
			return true;
		if (!character && !Vehicle.Cast(entity))
			return true;
		Faction faction = SCR_Faction.GetEntityFaction(entity);
		if (faction && !m_Faction.IsFactionEnemy(faction))
		{
			m_bFound = true;
			return false;
		}
		return true;
	}
}
