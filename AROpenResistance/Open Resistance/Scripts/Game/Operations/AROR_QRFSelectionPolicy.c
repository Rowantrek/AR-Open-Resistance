//! Commander-assessed threat tier and territory policy; no live-player discovery.
class AROR_QRFSelectionContext
{
	int m_iDesiredTier = 1;
	int m_iReadinessLevel = 1;
	int m_iMinimumTier = 1;
	int m_iLocationTierCeiling = 5;
	bool m_bAllowDowngrade = true;
	int m_iArrivalDeadlineSeconds = 180;
}

//! A feasibility result only. Nothing here reserves resources or creates an operation.
class AROR_QRFSelectionDecision
{
	AROR_GroundQRFRecipe m_Recipe;
	string m_sReason;
	string m_sOriginLocationId;
	int m_iRequiredGroups;
	int m_iRequiredPersonnel;
	int m_iRequiredVehicles;
	bool m_bDowngraded;
}

class AROR_QRFSelectionPolicy
{
	static AROR_QRFSelectionDecision Select(array<ref AROR_GroundQRFRecipe> recipes, array<ref AROR_ForceProfile> profiles, AROR_EFactionRole role, string factionKey, AROR_QRFSelectionContext context, AROR_QRFAvailabilitySnapshot availability, int now)
	{
		AROR_QRFSelectionDecision decision = new AROR_QRFSelectionDecision();
		decision.m_sReason = "Missing selection context or availability; no resources assumed";
		if (!context || !availability || !recipes || !profiles)
			return decision;
		decision.m_sReason = "Invalid readiness level, desired tier, location ceiling or arrival deadline";
		if (context.m_iDesiredTier < 1 || context.m_iDesiredTier > 5)
			return decision;
		if (context.m_iReadinessLevel < 1 || context.m_iReadinessLevel > 5)
			return decision;
		if (context.m_iMinimumTier < 1 || context.m_iLocationTierCeiling > 5)
			return decision;
		if (context.m_iMinimumTier > context.m_iLocationTierCeiling || context.m_iArrivalDeadlineSeconds < 1)
			return decision;
		decision.m_sReason = "Stale, malformed or faction-incompatible resource snapshot";
		if (!availability.IsValidAt(now) || availability.m_eRole != role || availability.m_sFactionKey != factionKey)
			return decision;
		decision.m_sReason = "Ground route unavailable or response cannot arrive before deadline";
		if (!availability.m_bGroundRouteReady || availability.m_iArrivalSeconds < 0)
			return decision;
		if (availability.m_iArrivalSeconds > context.m_iArrivalDeadlineSeconds)
			return decision;
		int ceiling = Math.Min(context.m_iDesiredTier, context.m_iLocationTierCeiling);
		decision.m_sReason = "No complete affordable ground recipe within permitted tiers";
		foreach (AROR_GroundQRFRecipe recipe : recipes)
		{
			// This provider describes ground stock/routes only; no aircraft availability is implied.
			if (recipe && recipe.m_eResponseType == AROR_EResponseType.AIR_GROUND)
				continue;
			if (!recipe || recipe.m_iLevel < context.m_iMinimumTier || recipe.m_iLevel > ceiling)
				continue;
			if (!context.m_bAllowDowngrade && recipe.m_iLevel != context.m_iDesiredTier)
				continue;
			if (decision.m_Recipe && recipe.m_iLevel <= decision.m_Recipe.m_iLevel)
				continue;
			array<AROR_ForceProfile> elements = {};
			if (!recipe.Resolve(profiles, role, factionKey, elements, context.m_iReadinessLevel))
				continue;
			int groups = 0;
			int members = 0;
			int vehicles = 0;
			bool fits = true;
			array<string> counted = {};
			foreach (AROR_ForceProfile element : elements)
			{
				groups += element.GetGroupBudget();
				members += element.m_iPersonnelBudget;
				if (element.m_eType != AROR_EForceType.INFANTRY)
					vehicles++;
				if (counted.Contains(element.m_sId))
					continue;
				counted.Insert(element.m_sId);
				int required = 0;
				foreach (AROR_ForceProfile other : elements)
				{
					if (other.m_sId == element.m_sId)
						required++;
				}
				if (availability.GetReadyPackages(element.m_sId) < required)
					fits = false;
			}
			if (groups > availability.m_iFreeGroups || members > availability.m_iFreePersonnel || vehicles > availability.m_iFreeVehicles)
				fits = false;
			if (!fits)
				continue;
			decision.m_Recipe = recipe;
			decision.m_sOriginLocationId = availability.m_sOriginLocationId;
			decision.m_iRequiredGroups = groups;
			decision.m_iRequiredPersonnel = members;
			decision.m_iRequiredVehicles = vehicles;
			decision.m_bDowngraded = recipe.m_iLevel < context.m_iDesiredTier;
			decision.m_sReason = "Feasible planning result; stock is not reserved and dispatch is not authorized";
		}
		return decision;
	}
}
