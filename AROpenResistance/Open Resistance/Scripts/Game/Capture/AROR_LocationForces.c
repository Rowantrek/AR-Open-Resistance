//! Counts living characters inside a location's radius, per role, split into players and AI.
//! Characters whose faction fills no role are ignored. Arrays are indexed by role and sized from AROR_FactionRoles,
//! so every role (including Civilians) has a slot.
class AROR_LocationForces : Managed
{
	protected ref array<int> m_aPlayers = {};
	protected ref array<int> m_aAI = {};
	protected ref array<vector> m_aPositionSums = {};	//!< Sum of member positions per role, for GetCentroid

	//------------------------------------------------------------------------------------------------
	void AROR_LocationForces()
	{
		int roleCount = AROR_FactionRoles.Count();
		for (int i = 0; i < roleCount; i++)
		{
			m_aPlayers.Insert(0);
			m_aAI.Insert(0);
			m_aPositionSums.Insert(vector.Zero);
		}
	}

	protected vector m_vCenter;
	protected float m_fRadius;
	protected AROR_FactionRolesComponent m_Roles;

	//------------------------------------------------------------------------------------------------
	static AROR_LocationForces Count(notnull AROR_LocationComponent location)
	{
		AROR_LocationForces forces = new AROR_LocationForces();
		forces.m_vCenter = location.GetCenter();
		forces.m_fRadius = location.GetRadius();
		forces.m_Roles = AROR_FactionRolesComponent.GetInstance();

		if (forces.m_Roles)
			GetGame().GetWorld().QueryEntitiesBySphere(forces.m_vCenter, forces.m_fRadius, forces.OnEntity, null, EQueryEntitiesFlags.DYNAMIC);

		return forces;
	}

	//------------------------------------------------------------------------------------------------
	int GetPlayers(AROR_EFactionRole role)
	{
		return m_aPlayers[role];
	}

	//------------------------------------------------------------------------------------------------
	int GetAI(AROR_EFactionRole role)
	{
		return m_aAI[role];
	}

	//------------------------------------------------------------------------------------------------
	//! Average position of a role's living people inside the radius; false if there are none. A snapshot, as a
	//! sentry or recon element would report it: never a live reference to anyone.
	bool GetCentroid(AROR_EFactionRole role, out vector centroid)
	{
		int total = GetTotal(role);
		if (total < 1)
			return false;
		centroid = m_aPositionSums[role] * (1.0 / total);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	int GetTotal(AROR_EFactionRole role)
	{
		return m_aPlayers[role] + m_aAI[role];
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnEntity(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return true;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.IsDead())
			return true;

		if (vector.DistanceXZ(m_vCenter, character.GetOrigin()) > m_fRadius)
			return true;

		AROR_EFactionRole role;
		if (!m_Roles.GetRoleOfFaction(SCR_Faction.GetEntityFaction(character), role))
			return true;

		m_aPositionSums[role] = m_aPositionSums[role] + character.GetOrigin();
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) > 0)
			m_aPlayers[role] = m_aPlayers[role] + 1;
		else
			m_aAI[role] = m_aAI[role] + 1;

		return true;
	}
}
