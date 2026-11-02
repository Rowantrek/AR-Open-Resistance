//! Authored ground or joint mix: level is a tier; deploy the element list exactly once.
[BaseContainerProps()]
class AROR_GroundQRFRecipe
{
	//! Attributed fields must not have script initializers: Enfusion would overwrite the loaded resource values.
	[Attribute("", desc: "Stable recipe ID, e.g. combined_arms_l1")]
	string m_sId;
	[Attribute("5", UIWidgets.ComboBox, desc: "Response family for the shared two-axis selector. Existing mixes default to combined arms.", enums: ParamEnumArray.FromEnum(AROR_EResponseType))]
	AROR_EResponseType m_eResponseType;
	[Attribute("1", desc: "Authored response tier; does not multiply the element list", params: "1 5 1")]
	int m_iLevel;
	[Attribute("1", desc: "First readiness using this equipment-pool recipe", params: "1 5 1")]
	int m_iMinimumReadinessLevel;
	[Attribute("5", desc: "Last readiness using this equipment-pool recipe", params: "1 5 1")]
	int m_iMaximumReadinessLevel;
	[Attribute("1", UIWidgets.ComboBox, desc: "Responding campaign role", enums: AROR_FactionRoles.MilitaryRoleParams())]
	AROR_EFactionRole m_eRole;
	[Attribute("AROR_OCCUPIERS", desc: "Required faction from the shared role mapping")]
	string m_sFactionKey;
	[Attribute("", desc: "Ordered force-profile IDs. Joint recipes start with one airborne pair, then ground elements. Each entry is deployed once.")]
	ref array<string> m_aProfileIds;

	//! Script-created fixtures have no resource container to apply Attribute defaults. Resource loading never calls this.
	static AROR_GroundQRFRecipe CreateForScript()
	{
		AROR_GroundQRFRecipe recipe = new AROR_GroundQRFRecipe();
		recipe.m_eResponseType = AROR_EResponseType.COMBINED_ARMS;
		recipe.m_iMinimumReadinessLevel = 1;
		recipe.m_iMaximumReadinessLevel = 5;
		return recipe;
	}

	bool IsValid()
	{
		if (m_iMinimumReadinessLevel < 1 || m_iMaximumReadinessLevel > 5 || m_iMinimumReadinessLevel > m_iMaximumReadinessLevel)
			return false;
		if (m_sId.IsEmpty() || m_sFactionKey.IsEmpty() || m_iLevel < 1 || m_iLevel > 5)
			return false;
		if (m_eRole != AROR_EFactionRole.OCCUPIERS && m_eRole != AROR_EFactionRole.INVADERS)
			return false;
		if (!m_aProfileIds || m_aProfileIds.Count() < 1 || m_aProfileIds.Count() > 8)
			return false;
		foreach (string id : m_aProfileIds)
		{
			if (id.IsEmpty())
				return false;
		}
		return true;
	}
	bool AllowsReadinessLevel(int readinessLevel)
	{
		return readinessLevel >= m_iMinimumReadinessLevel && readinessLevel <= m_iMaximumReadinessLevel;
	}

	//! Validate every element before any spawning or reservation conversion.
	bool Resolve(array<ref AROR_ForceProfile> profiles, AROR_EFactionRole role, string factionKey, array<AROR_ForceProfile> resolved, int readinessLevel = 5)
	{
		resolved.Clear();
		if (!IsValid() || m_eRole != role || m_sFactionKey != factionKey)
			return false;
		if (readinessLevel < 1 || readinessLevel > 5 || !AllowsReadinessLevel(readinessLevel))
			return false;
		int airborne;
		int ground;
		foreach (string id : m_aProfileIds)
		{
			AROR_ForceProfile selected = null;
			foreach (AROR_ForceProfile profile : profiles)
			{
				if (profile && profile.m_sId == id)
					selected = profile;
			}
			if (!selected || !selected.IsValid() || !selected.m_bEnabled)
				return false;
			if (selected.m_iMinimumReadinessLevel > readinessLevel)
				return false;
			if (selected.m_eRole != role || selected.m_sFactionKey != factionKey)
				return false;
			if (selected.GetWeight(AROR_EOperationType.QRF) <= 0)
				return false;
			if (selected.m_eType == AROR_EForceType.AIRBORNE)
			{
				// Joint recipes start with one airborne package, with an optional gunship.
				if (m_eResponseType != AROR_EResponseType.AIR_GROUND || !resolved.IsEmpty())
					return false;
				airborne++;
			}
			else
			{
				if (!selected.HasNativeExecutor())
					return false;
				ground++;
			}
			resolved.Insert(selected);
		}
		if (m_eResponseType == AROR_EResponseType.AIR_GROUND && (airborne != 1 || ground < 1))
			return false;
		return true;
	}
}
