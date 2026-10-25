[WorkbenchPluginAttribute(name: "Audit all campaign flags", description: "Read-only terrain and model-bounds report for all 41 Chicken campaign flags.", wbModules: {"WorldEditor"}, category: "Open Resistance")]
class AROR_AuditCampaignFlagsPlugin : AROR_GroundCampaignFlagsPlugin
{
	override void Run()
	{
		RunAudit(false);
	}
}
