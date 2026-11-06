[WorkbenchPluginAttribute(name: "Print resource name", description: "Print the resource name of each selected resource to the log console.", wbModules: {"ResourceManager"}, resourceTypes: {"et", "conf", "c", "layout", "ent", "layer", "xob", "edds", "emat", "imageset", "anm", "asi", "agr", "ast", "acp", "aw", "wav", "fnt", "ptc"}, category: "Open Resistance")]
class AROR_PrintResourceNamePlugin : ResourceManagerPlugin
{
	//------------------------------------------------------------------------------------------------
	override void OnResourceContextMenu(notnull array<ResourceName> resources)
	{
		foreach (ResourceName resource : resources)
		{
			Print(resource);
		}
	}
}
