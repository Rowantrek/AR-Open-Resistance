[WorkbenchPluginAttribute(name: "Ground fortified outposts (preserve Chotain castle)", description: "Undoable grounding of the fortified ordinary/military outpost trial props in Chicken. Excludes Chotain castle and strategic installations. Reload resources first, then inspect and Save.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_GroundFortifiedOutpostsPlugin : AROR_GroundDefenceWallsPlugin
{
	override protected bool ShouldGroundLocation(AROR_LocationComponent location)
	{
		if (location.GetId() == "everon_military_outpost_chotain")
			return false;
		AROR_ELocationType type = location.GetLocationType();
		return type == AROR_ELocationType.OUTPOST || type == AROR_ELocationType.MILITARY_BASE;
	}
}
