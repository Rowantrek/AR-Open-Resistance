//! A squad the Resistance can raise from HICOMM Unit Management (see ARCHITECTURE.md -> High Command).
//! Bought with Resistance funds AND manpower; both are refunded in full when the campaign ends.
[BaseContainerProps()]
class AROR_HighCommandSquadType
{
	[Attribute("RFL", desc: "Callsign prefix; squads are numbered per type, e.g. RFL-1, RFL-2")]
	string m_sCallsign;

	[Attribute("Rifle Squad", desc: "Name shown in Unit Management")]
	string m_sName;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Character prefab for each member, in order (the first one leads)", params: "et")]
	ref array<ResourceName> m_aMembers;

	[Attribute("2000", desc: "Resistance funds taken when the squad is raised", params: "0 inf")]
	int m_iFundsCost;

	[Attribute("6", desc: "Resistance manpower taken when the squad is raised", params: "0 inf")]
	int m_iManpowerCost;

	//------------------------------------------------------------------------------------------------
	int GetSize()
	{
		if (!m_aMembers)
			return 0;

		return m_aMembers.Count();
	}
}
