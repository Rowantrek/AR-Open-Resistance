[WorkbenchPluginAttribute(name: "Audit Fortress Vostok clearing", description: "Read-only dense 25m-grid search around Fortress Vostok for a dry site that fits its complete footprint, eight orientations, without relaxing terrain limits. Does not move entities.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_AuditVostokDefenceSitePlugin : AROR_AuditDefenceSitesPlugin
{
	override void Run()
	{
		RunSiteAudit("everon_fortress_vostok");
	}
}
