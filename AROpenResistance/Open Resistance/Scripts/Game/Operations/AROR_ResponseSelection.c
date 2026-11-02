//! Immutable preview/result of the shared two-axis policy. No spawning, finance or campaign mutation.
class AROR_ResponseSelection
{
	string m_sProfileId;
	string m_sRecipeId;
	string m_sReason;
	int m_iReadiness;
	int m_iCrackdown;
	int m_iTier;
	int m_iGroups;
	int m_iPersonnel;
	int m_iVehicles;
	protected string m_sComposition;
	ref array<AROR_ForceProfile> m_aElements = {};

	bool IsAvailable()
	{
		return !m_aElements.IsEmpty();
	}

	string GetComposition()
	{
		if (!m_sComposition.IsEmpty())
			return m_sComposition;
		array<string> names = {};
		array<int> counts = {};
		foreach (AROR_ForceProfile profile : m_aElements)
		{
			int type = profile.m_eType;
			if (profile.m_bLightVehicle)
				type = AROR_EResponseType.LIGHT_SUPPORT;
			string name = AROR_ResponsePolicy.TypeName(type);
			int index = names.Find(name);
			if (index < 0)
			{
				names.Insert(name);
				counts.Insert(1);
			}
			else
				counts[index] = counts[index] + 1;
		}
		foreach (int i, string label : names)
		{
			if (!m_sComposition.IsEmpty())
				m_sComposition += " + ";
			m_sComposition += counts[i].ToString() + " " + label;
		}
		return m_sComposition;
	}
}
