# Mounted QRF town search

Ground QRF packages perform a bounded mounted search before resuming normal deployment. Jasper reported a successful play test but observed the vehicles taking similar routes. The coordinated contact/sector changes described here are new and still require Workbench compilation and runtime testing.

Each QRF operation owns one `AROR_QRFSearchPlan` with no attacker entity references. Validated defender damage calls `ReportAttackAt_S`, copying the attributed attacker's position at that event. This is damage-source intelligence, not a verified visual sighting or a perception sensor. Fresh reports can replace the snapshot until the first package starts searching; existing operations keep their force selection, preparation time and cooldown. The plan freezes at search start and rejects expired intelligence (120 seconds by default). Simulated/positionless reports simply patrol the town.

On entering the location radius plus the configured margin, packages replace their shared approach order with distinct search-sector assignments. Each tries up to two road stops around the last reported position, then patrols its sector around the marker. Reports far away are clamped to the permitted vicinity. Investigation stops must remain within the location radius plus margin; town patrol stops stay inside the location radius. The shared plan rejects road stops within 50 metres of any stop already assigned to the same QRF, with a bounded history of 64 positions. Lost packages are not replaced and do not cause surviving packages to collapse into one sector.

Road candidates use `RoadNetworkManager.GetReachableWaypointInRoad`. Queries must return a point within 75 metres of the candidate; failures get bounded retries (at most sixteen queries per stop), with no off-road fallback. Unreachable contact investigation falls back to town patrol. Different destinations cannot guarantee different road segments: native pathfinding may still route vehicles through the same junction or narrow approach.

The search never polls the attacker's live position. Native AI perception, combat and waypoint behavior determine spotting and engagement; this coordinator does not add a custom pursuit system. Chicken includes the verified base-game PerceptionManager prefab alongside AIWorld/navigation bindings, following Bohemia's [Everon example](https://github.com/BohemiaInteractive/Arma-Reforger-Samples/blob/main/SampleMod_NewFaction/Worlds/MP/CTI_Campaign_Eden_base_Layers/AI.layer).

## Settings

These attributes are on `AROR_OperationExecutorComponent`, with explicit settings in Chicken:

| Attribute | Default | Purpose |
|---|---|---|
| `m_bQRFVehicleSearch` | Enabled | Enable mounted searching for ground QRFs |
| `m_iQRFSearchDuration` | 180 seconds | Overall mounted-search limit |
| `m_iQRFSearchStops` | 6 | Maximum stops issued per package |
| `m_iQRFSearchDwell` | 8 seconds | Pause after physically reaching a stop |
| `m_iQRFSearchLegTimeout` | 60 seconds | Skip a stalled leg after this delay |
| `m_iQRFContactMaxAge` | 120 seconds | Ignore older contact intel when the first package starts searching |
| `m_fQRFSearchMargin` | 150 metres | Allowed investigation distance beyond the location radius; also determines when search assignments begin |
| `m_fQRFSearchStopSpacing` | 50 metres | Minimum spacing between all road stops assigned within one QRF |

These are server timer ticks, so delays may lengthen under load. Scheduled attacks, standalone infantry and unsupported airborne profiles do not enter this search phase. The normal operation timeout remains authoritative and can terminate a search earlier.

At the duration/stop limit, or if no further reachable road point is available, the search removes its own waypoint and returns control to the existing deployment policy. Motorized and mechanized packages return to their dismount point and unload; mechanized crews then receive their normal support order. Armour returns to its mounted objective order. A missing initial search route leaves the original deployment order intact. Search arrival does not capture territory or declare victory.

Terminal operation cleanup removes the search waypoint before deleting package-owned groups and vehicles. A destroyed vehicle before delivery still fails that package. Dormant groups retain their reservation; after they wake, the search may expire based on the original deadline. Existing preservation rules apply to player-controlled entities and occupied vehicles.

## Runtime acceptance

1. Recompile and reload `AROR_Everon_Chicken` from disk before saving. Confirm soldier/vehicle navmeshes load without empty-navmesh warnings and the PerceptionManager loads.
   The startup marker `QRF search policy: coordinated contact investigation and sector patrol` confirms this version is loaded. The 21:04 compile attempt failed on an unsupported remainder operator (now replaced); a failed reload left Play running the older independent search implementation. Unknown new search attributes and logs without package/phase fields indicate that older build, not a test of the new coordinator.
2. Complete campaign setup and damage a US-model Occupier defender as Resistance. The current fixture requests three Occupier LAV packages, all affiliated with `AROR_OCCUPIERS`.
3. On entering the vicinity, expect `Mounted town search started` and per-package logs with `phase=reported contact`, then `phase=town patrol`. Observe movement and `reached stop`; waypoint logs alone do not prove movement. Packages should have separate road destinations even if some approach roads are shared.
4. Fire from one position, then relocate without firing. Confirm the search investigates the original report rather than following the player. Repeat with a report older than the age limit and with a simulated positionless report: these should patrol the marker instead. Native AI may still react to enemies it detects.
5. Observe the stop/duration limit, return to deployment, passenger exits and infantry objective orders. Test an unavailable/stalled road and verify skip/fallback rather than endless waypoint creation.
6. Cancel or destroy the force during a search; verify no search waypoint, unowned troops or abandoned operation-owned vehicle remains. Repeat with mechanized and armour profiles and with Invaders in separate sessions.
7. Disable `m_bQRFVehicleSearch` and confirm direct deployment resumes. Scheduled attacks should retain direct deployment with either setting.

The existing Workbench scheduler smoke-test switch also runs `AROR_QRFSearchPlanSmokeTests`: checks include no invented contact, latest pre-search report, vicinity clamping, frozen contact, three-package sector separation, shared stop spacing, stale-report fallback and isolation between operations. These checks have been added but not executed here.

The road API and its parameter order were checked against the installed official API reference and Bohemia's [resupply task solver](https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/3d77cc212d5cda9922daf5f45635c7300d2d4cce/scripts/Game/TaskSolver/SCR_ResupplyTaskSolver.c). A successful query does not establish that every modded vehicle can navigate every chosen road; real routes and combat interruptions remain engine tests.
