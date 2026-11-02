//! Answers whether a Resistance garage prop already stands near a position (see AROR_GameFlowComponent.EnsureGarage_S)
class AROR_GarageSiteQuery
{
	protected bool m_bFound;

	//------------------------------------------------------------------------------------------------
	static bool HasGarageNear(vector center, float radius)
	{
		AROR_GarageSiteQuery query = new AROR_GarageSiteQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, query.OnEntity, null, EQueryEntitiesFlags.ALL);
		return query.m_bFound;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnEntity(IEntity entity)
	{
		EntityPrefabData prefabData = entity.GetPrefabData();
		if (prefabData && prefabData.GetPrefabName().Contains("AROR_Prop_Garage"))
		{
			m_bFound = true;
			return false;
		}

		return true;
	}
}
