//! Finds whether any living character hostile to a faction is inside a location's radius. Helicopter occupants are
//! ignored, as in AROR_LocationForces: an aircraft overhead does not block a capture.
class AROR_HostilesQuery : Managed
{
	protected Faction m_Faction;
	protected vector m_vCenter;
	protected float m_fRadius;
	protected bool m_bFound;

	//------------------------------------------------------------------------------------------------
	//! True if a living character whose faction is an enemy of the given faction is within the radius (XZ)
	static bool HasHostilesInRadius(notnull Faction faction, vector center, float radius)
	{
		AROR_HostilesQuery query = new AROR_HostilesQuery();
		query.m_Faction = faction;
		query.m_vCenter = center;
		query.m_fRadius = radius;

		// Sphere query catches characters on slopes inside the circle; the XZ check trims the sphere to the circle
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, query.OnEntity, null, EQueryEntitiesFlags.DYNAMIC);
		return query.m_bFound;
	}

	//------------------------------------------------------------------------------------------------
	//! Returning false stops the query
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

		if (AROR_LocationForces.IsInAircraft(character))
			return true;

		Faction characterFaction = SCR_Faction.GetEntityFaction(character);
		if (characterFaction && m_Faction.IsFactionEnemy(characterFaction))
		{
			m_bFound = true;
			return false;
		}

		return true;
	}
}
