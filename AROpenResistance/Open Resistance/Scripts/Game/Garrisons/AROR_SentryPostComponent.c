[ComponentEditorProps(category: "OpenResistance/Garrisons", description: "Marks a guard tower, bunker or other post inside a defence composition. The garrison posts one marksman in it, who holds the post instead of joining a team.")]
class AROR_SentryPostComponentClass : ScriptComponentClass {}

//! Read by AROR_GarrisonComponent: the nth post among the composition's direct children holds the nth sentry
class AROR_SentryPostComponent : ScriptComponent
{
	[Attribute("3.5", desc: "Approximate height of the post's floor above the terrain, in metres; 0 for a ground-level post such as a bunker. A raised sentry stands on the lowest floor found near this height; without one he stands at the foot of the post.", params: "0 20 0.1")]
	protected float m_fFloorHeight;

	float GetFloorHeight()
	{
		return m_fFloorHeight;
	}
}
