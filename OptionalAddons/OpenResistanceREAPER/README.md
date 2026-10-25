# REAPER standalone diagnostics

**The QRF adapter has moved into the normal Open Resistance project.** Open `AROpenResistance/Open Resistance/addon.gproj` as usual and use Everon_Chicken. No CMD launcher or separate project is required for the QRF test. See [the current guide](../../docs/AIRBORNE_QRF_PROTOTYPE.md).

This optional project now retains only the disabled `ORRA_ReaperFlightProbeComponent` and its standalone `Worlds/ORRA_REAPER_FlightTest.ent` diagnostic scene. It is not required for QRF dispatch. Its launcher is retained for diagnosing basic transport flight independently of the campaign.

The normal project directly depends on installed REAPER (`69A4D664A6284E06`) on this experimental branch. On Jasper's machine, a junction in the standard Tools Workbench/addons directory exposes the existing downloaded REAPER files. Save and restart Workbench once so it reloads the dependency list. Other developers must install REAPER and register its existing addon.gproj with Workbench or supply a suitable search path.

Chicken explicitly enables **Reaper QRF Test** on its executor; other executors default off. Clear that switch to restore ground QRF behavior. REAPER remains required at project load until its dependency and adapter are removed separately.

The first air test inserts one occupied transport and one gunship at 120 m above local terrain. Landing, completed passenger exits and faction checks gate infantry deployment. The first run confirmed basic flight, landing and unloading. New LZ/formation/engagement/egress/retention behavior requires fresh compilation and testing; follow the current guide above.

The diagnostic project GUID `2DE8626701CA4426` and world GUID `197F7076150C4F77` were generated outside Workbench, including the new world metadata. Have Workbench validate/save these resources before publishing. Existing backend resource GUIDs were read from installed indexes.
