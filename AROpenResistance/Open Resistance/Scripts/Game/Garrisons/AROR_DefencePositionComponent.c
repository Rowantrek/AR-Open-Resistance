[ComponentEditorProps(category: "OpenResistance/Garrisons", description: "Authored defence position inside a location preset. Kind: 0 infantry team, 1 MG, 2 parked hull, 3 manned hull, 4 crew staging. Ordinals start at zero.")]
class AROR_DefencePositionComponentClass : ScriptComponentClass {}

class AROR_DefencePositionComponent : ScriptComponent
{
	[Attribute("0", desc: "Defence position kind", params: "0 4 1")]
	protected int m_iKind;
	[Attribute("0", desc: "Ordinal within that kind", params: "0 15 1")]
	protected int m_iOrdinal;
	[Attribute("0", desc: "MG only: approximate height of a raised platform (tower, roof) above the terrain, in metres. The gun is placed on the first solid surface found near this height; 0 places it on the ground.", params: "0 20 0.1")]
	protected float m_fElevation;
	bool MatchesDefencePosition(int kind, int ordinal)
	{
		return m_iKind == kind && m_iOrdinal == ordinal;
	}
	int GetKind()
	{
		return m_iKind;
	}
	float GetElevation()
	{
		if (m_iKind != 1)
			return 0;
		return m_fElevation;
	}
}
