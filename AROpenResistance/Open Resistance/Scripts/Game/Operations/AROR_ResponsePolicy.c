//! One two-axis equipment/intensity policy for campaign selection and debug previews.
//! Designers extend the existing profiles and recipe tiers; the UI never invents a roster.
class AROR_ResponsePolicy
{
	static bool IsProfileUnlocked(AROR_ForceProfile profile, int readiness)
	{
		return profile && readiness >= 1 && readiness <= 5 && profile.m_iMinimumReadinessLevel <= readiness;
	}

	static bool IsRecipeEligible(AROR_GroundQRFRecipe recipe, int readiness, int crackdown)
	{
		if (!recipe || readiness < 1 || readiness > 5 || crackdown < 1 || crackdown > 5)
			return false;
		return recipe.m_iLevel <= crackdown && recipe.AllowsReadinessLevel(readiness);
	}

	//! Extenders may append response types and supply recipes with the corresponding type.
	static void GetTypes(notnull array<int> types)
	{
		types.Clear();
		types.Insert(AROR_EResponseType.MOTORIZED);
		types.Insert(AROR_EResponseType.MECHANIZED);
		types.Insert(AROR_EResponseType.ARMOUR);
		types.Insert(AROR_EResponseType.AIRBORNE);
		types.Insert(AROR_EResponseType.COMBINED_ARMS);
		types.Insert(AROR_EResponseType.LIGHT_SUPPORT);
		types.Insert(AROR_EResponseType.AIR_GROUND);
	}

	static string TypeName(int type)
	{
		switch (type)
		{
			case AROR_EResponseType.MOTORIZED: return "MOTORIZED";
			case AROR_EResponseType.MECHANIZED: return "MECHANIZED";
			case AROR_EResponseType.ARMOUR: return "ARMOUR";
			case AROR_EResponseType.AIRBORNE: return "AIRBORNE";
			case AROR_EResponseType.INFANTRY: return "INFANTRY";
			case AROR_EResponseType.COMBINED_ARMS: return "COMBINED ARMS";
			case AROR_EResponseType.LIGHT_SUPPORT: return "LIGHT SUPPORT";
			case AROR_EResponseType.AIR_GROUND: return "AIR + GROUND";
		}
		return "AUTHORED RESPONSE";
	}

	static bool MatchesProfileType(AROR_ForceProfile profile, int type)
	{
		if (type == AROR_EResponseType.LIGHT_SUPPORT)
			return profile.m_bLightVehicle;
		if (type == AROR_EResponseType.ARMOUR && profile.m_bLightVehicle)
			return false;
		return profile.m_eType == type;
	}

	static AROR_ResponseSelection Select(array<ref AROR_ForceProfile> profiles, array<ref AROR_GroundQRFRecipe> recipes, AROR_EFactionRole role, string faction, int type, int readiness, int crackdown, bool airborneEnabled)
	{
		AROR_ResponseSelection result = new AROR_ResponseSelection();
		result.m_iReadiness = readiness;
		result.m_iCrackdown = crackdown;
		result.m_sReason = "No authored response at these Readiness / Crackdown levels";
		if (!profiles || readiness < 1 || readiness > 5 || crackdown < 1 || crackdown > 5)
			return result;
		if (type == AROR_EResponseType.AIR_GROUND && !airborneEnabled)
		{
			result.m_sReason = "Airborne adapter is disabled";
			return result;
		}
		AROR_GroundQRFRecipe best;
		if (recipes)
		{
			foreach (AROR_GroundQRFRecipe recipe : recipes)
			{
				if (!IsRecipeEligible(recipe, readiness, crackdown) || recipe.m_eResponseType != type)
					continue;
				if (best && best.m_iLevel >= recipe.m_iLevel)
					continue;
				array<AROR_ForceProfile> elements = {};
				if (recipe.Resolve(profiles, role, faction, elements, readiness))
					best = recipe;
			}
		}
		if (best)
		{
			best.Resolve(profiles, role, faction, result.m_aElements, readiness);
			result.m_sRecipeId = best.m_sId;
			result.m_iTier = best.m_iLevel;
		}
		else
		{
			if (type == AROR_EResponseType.AIRBORNE && !airborneEnabled)
			{
				result.m_sReason = "Airborne adapter is disabled";
				return result;
			}
			AROR_ForceProfile selected;
			int minimumUnlock = 6;
			foreach (AROR_ForceProfile profile : profiles)
			{
				if (!profile || !profile.m_bEnabled || !profile.IsValid())
					continue;
				if (profile.m_eRole != role || profile.m_sFactionKey != faction || profile.m_iQRFWeight <= 0)
					continue;
				if (!MatchesProfileType(profile, type))
					continue;
				if (!profile.HasNativeExecutor() && !(airborneEnabled && profile.m_eType == AROR_EForceType.AIRBORNE))
					continue;
				if (!IsProfileUnlocked(profile, readiness))
				{
					minimumUnlock = Math.Min(minimumUnlock, profile.m_iMinimumReadinessLevel);
					continue;
				}
				if (!selected || profile.m_iMinimumReadinessLevel > selected.m_iMinimumReadinessLevel)
					selected = profile;
			}
			if (!selected)
			{
				if (minimumUnlock <= 5)
					result.m_sReason = string.Format("Requires Readiness %1 (selected %2)", minimumUnlock, readiness);
				return result;
			}
			result.m_sProfileId = selected.m_sId;
			result.m_iTier = crackdown;
			for (int packageIndex = 0; packageIndex < crackdown; packageIndex++)
				result.m_aElements.Insert(selected);
		}
		foreach (AROR_ForceProfile element : result.m_aElements)
		{
			result.m_iPersonnel += element.m_iPersonnelBudget;
			result.m_iGroups += element.GetGroupBudget();
			result.m_iVehicles += element.GetVehicleBudget();
		}
		result.m_sReason = string.Empty;
		return result;
	}
}
