[WorkbenchPluginAttribute(name: "Audit fortified outposts (current placements)", description: "Read-only current-placement check of Chicken's 27 military sites, including all fortified trials. Reports specific blocked pads and gun arcs; does not search for or apply relocations.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_AuditFortifiedOutpostsPlugin : AROR_AuditDefenceSitesPlugin
{
	override void Run()
	{
		RunSiteAudit(string.Empty, false);
	}
}
