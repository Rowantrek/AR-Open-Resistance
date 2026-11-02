[ComponentEditorProps(category: "OpenResistance/Garrisons", description: "Marks a helipad (or other vehicle deck) inside a defence composition. Parked vehicles spawned on it stand on its surface, its own props do not count as obstructions, and the space its helicopter needs is kept clear.")]
class AROR_LandingPadComponentClass : ScriptComponentClass {}

//! Tag for the garrison (see AROR_GarrisonComponent) and the helipad clearing editor tool
class AROR_LandingPadComponent : ScriptComponent
{
	[Attribute("8", desc: "Rotor radius of the helicopter parked here, in metres. The disc must be clear of buildings, trees and poles at rotor height.", params: "2 15 0.1")]
	protected float m_fRotorRadius;
	[Attribute("15", desc: "Overall length of the helicopter parked here, nose to tail, in metres. Kept clear along the pad's heading at fuselage height.", params: "4 30 0.1")]
	protected float m_fLength;

	float GetRotorRadius()
	{
		return m_fRotorRadius;
	}

	float GetLength()
	{
		return m_fLength;
	}
}
