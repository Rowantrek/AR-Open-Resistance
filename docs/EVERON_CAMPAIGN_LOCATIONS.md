# Everon campaign locations

Chicken now places **41** campaign locations: **7 cities, 11 towns, 14 outposts, 5 military outposts, 1 airport, 1 factory and 2 resource sites**. Scope is location prefabs and capturable flagpoles, using the existing location registry, map icons, support, control, income, capture and persistence systems. No garrisons or scenery compounds are spawned; factory/resource markers align with Rowan placements.

## Reusable prefabs

| Template in Prefabs/Locations | Location type | Capture |
|---|---|---|
| AROR_Location_City_Flag.et | CITY | Flag capture updates existing settlement support to 100% for the capturing role |
| AROR_Location_Town_Flag.et | TOWN | Same opt-in settlement flag behavior |
| AROR_Location_Outpost.et | OUTPOST | Existing military flag capture |
| AROR_Location_MilitaryOutpost.et | MILITARY_BASE | Existing military flag capture, larger radius |

Settlements and outposts reference these templates; the airport, factory and resource sites use their existing templates. Every site has one inherited or attached AROR_Prop_CaptureFlag.et. Chicken now saves an explicit child-pole transform at every site and disables runtime relocation for those baked flags. Other scenarios retain the shared server placement component. Move a location to an accessible patch of ground in Workbench if its catalogue anchor is obstructed; maintain its stable ID. New template resource GUIDs and entity IDs are hand-authored, recorded in sibling .meta files, and need Workbench resource discovery/registration on reload. No existing asset GUID was invented or reassigned.

Original AROR_Location_City.et and AROR_Location_Town.et remain support-only. The new flag variants set m_bSettlementFlagCapture=1. The player flag action keeps the existing server hostile-clearance and flip-cooldown checks. AI weighted recapture uses the same support-aware capture helper for opted-in settlements; other settlements retain their existing support-only behavior. Military capture behavior remains direct ownership through the existing authority. No second support, capture or money system is introduced.

## Scenario data

All 41 sites begin as OCCUPIERS (US assets). INVADERS/USSR admission and REAPER dispatch remain disabled. All previous Provins, Morton and Lamentin stable IDs and coordinates are preserved; Morton/Lamentin are classified as cities. Both Provins test defenders remain unchanged and assigned AROR_OCCUPIERS.

New coordinates come from [EnfusionMapMaker's extracted Everon location data](https://github.com/nickludlam/EnfusionMapMaker/blob/main/Web/everon/everon-locations.js). They are source reference coordinates, not certified placements in the installed terrain version. The source has X/Z only. After underground flags were reported, all 41 accepted flag world positions from the 2026-10-24 23:15 Workbench log were baked into the CSV (FlagX/FlagY/FlagZ) and inherited-child offsets in the layers. The 34 formerly zero-height roots now use the nearby flag candidate ground elevation; this is not an exact measurement at the location centre. Their X/Z anchors and the seven established authored root transforms are preserved. All Chicken roots disable GroundOnInit and all baked poles disable runtime relocation. The subsequent 23:50 editor audit measured the actual model minimum Y as -0.805946 m at every site. Saved pole heights now use measured physical terrain minus that model minimum plus 0.1 m; FlagInteractionY preserves the capture context at terrain +1.6 m. Evidence is recorded in EVERON_FLAG_EDITOR_AUDIT.json and EVERON_FLAG_PLACEMENTS.json. Coordinates are log-rounded; visual accessibility remains unverified after this correction.

The [CSV catalogue](EVERON_CAMPAIGN_LOCATIONS.csv) is the editable scenario dataset. Source snapshot, attribution, modifications and APL-SA terms are in [coordinate provenance](third-party/everon-coordinates/README.md). Population values are provisional gameplay tuning, not a census. Total settlement population is **1,230**; with the existing default income rules and initial ownership, this contributes $6,150 per income cycle to Occupiers, plus the existing resource/factory income and $500 external support. The aligned two resources and one factory contribute $1,300 at default rates, making the initial total $7,950 per cycle. Outposts do not gain a new income rule. Captures move existing population income between roles through Rowan's system.

## Locations

| Name | Type | Stable ID | Radius | Population |
|---|---|---|---:|---:|
| Saint-Philippe | CITY | everon_saint_philippe | 350 m | 140 |
| Montignac | CITY | everon_montignac | 350 m | 160 |
| Entre-Deux | TOWN | everon_entre_deux | 200 m | 35 |
| Meaux | TOWN | everon_meaux | 220 m | 50 |
| Saint-Pierre | CITY | everon_saint_pierre | 300 m | 100 |
| Morton | CITY | everon_morton_town | 300 m | 100 |
| Figari | TOWN | everon_figari | 200 m | 40 |
| Lamentin | CITY | everon_lamentin_town | 300 m | 100 |
| Chotain | CITY | everon_chotain | 300 m | 100 |
| Levie | CITY | everon_levie | 300 m | 120 |
| Durras | TOWN | everon_durras | 200 m | 40 |
| Camurac | TOWN | everon_camurac | 180 m | 25 |
| Provins | TOWN | everon_provins | 250 m | 45 |
| Kermovan | TOWN | everon_kermovan | 220 m | 35 |
| Gravette | TOWN | everon_gravette | 220 m | 40 |
| Regina | TOWN | everon_regina | 200 m | 40 |
| Villeneuve | TOWN | everon_villeneuve | 200 m | 35 |
| Tyrone | TOWN | everon_tyrone | 180 m | 25 |
| Farm Outpost | OUTPOST | everon_outpost_farm | 100 m | 0 |
| Meaux Harbour Outpost | OUTPOST | everon_outpost_meaux_harbour | 100 m | 0 |
| Gorey Outpost | OUTPOST | everon_outpost_gorey | 100 m | 0 |
| Pinewood Lake Outpost | OUTPOST | everon_outpost_pinewood_lake | 100 m | 0 |
| Levie Hillside Outpost | OUTPOST | everon_outpost_levie_hillside | 100 m | 0 |
| Durras Hill Outpost | OUTPOST | everon_outpost_durras_hill | 100 m | 0 |
| Sawmill Outpost | OUTPOST | everon_outpost_sawmill | 100 m | 0 |
| Regina South Outpost | OUTPOST | everon_outpost_regina_south | 100 m | 0 |
| Chotain Industrial Outpost | OUTPOST | everon_outpost_chotain_industrial | 100 m | 0 |
| Old Wood Outpost | OUTPOST | everon_outpost_old_wood | 100 m | 0 |
| Morton Valley Outpost | OUTPOST | everon_outpost_morton_valley | 100 m | 0 |
| Laruns Outpost | OUTPOST | everon_outpost_laruns | 100 m | 0 |
| Quarry Outpost | OUTPOST | everon_outpost_quarry | 100 m | 0 |
| Andres Beacon Outpost | OUTPOST | everon_outpost_andres_beacon | 100 m | 0 |
| Chotain Military Outpost | MILITARY_BASE | everon_military_outpost_chotain | 150 m | 0 |
| Levie Military Outpost | MILITARY_BASE | everon_military_outpost_levie | 150 m | 0 |
| Military Hospital Outpost | MILITARY_BASE | everon_military_outpost_hospital | 150 m | 0 |
| Lamentin Military Outpost | MILITARY_BASE | everon_military_outpost_lamentin | 150 m | 0 |
| Morton Military Outpost | MILITARY_BASE | everon_military_outpost_morton | 150 m | 0 |
| Everon International Airport | AIRPORT | everon_airport | 500 m | 0 |
| North Ridge Factory | FACTORY | everon_factory_north_ridge | 250 m | 0 |
| North Ridge Resources West | RESOURCES | everon_resources_north_ridge_west | 250 m | 0 |
| North Ridge Resources East | RESOURCES | everon_resources_north_ridge_east | 250 m | 0 |

## QRF connections

Ordinary OUTPOST, military MILITARY_BASE and AIRPORT locations are eligible dispatch/return origins, using the existing nearest friendly location and launch-distance/land/player checks. The target is excluded. This is not proof of an unobstructed vehicle spawn or a viable native route; road/launch clearance still requires Play verification. Explicit configured coordinates remain the fallback when no eligible source exists. Captured origins are excluded and active commanders can retarget their return origin.

Chicken enables m_bOutpostCaptureQRFs alongside m_bTownCaptureQRFs. Canonical capture events can queue the previous AI owner's response, subject to money, role admission, cooldown, current operations and capacity. Initial registration/load establishes the baseline and does not spawn historical responses. The compatibility ReportTownCapture_S API remains; new callers can use ReportLocationCapture_S. The military capture switch also covers airports, factories, ports and resources; initial registration still never generates a historical response. No Invader operation is activated by this change.

## Editing and verification

Run tools/Set-EveronCampaignLocations.ps1 without arguments to check that the layers match the CSV. After reviewing catalogue changes, use -Apply to regenerate Chicken's two location layers. This preserves the existing defender prefix but replaces generated location blocks; reconcile manual World Editor placement edits back into the CSV before regenerating. Shared Everon and Rowan's layers are not written.

Offline validation: tools/Test-EveronCampaignLocations.ps1 checks counts, unique IDs, reference coordinate provenance, preserved anchors, template GUIDs, flag inheritance and origin integration. node tools/Test-EveronQRFIntegration.js checks actual-source capture opt-ins, support/control coherence, server guards and QRF type/admission routing with mocked native services (38 assertions).

In Workbench: stop Play, compile Game scripts, reload Everon_Chicken and let Workbench discover the four new prefab/meta pairs. Start a fresh test campaign. Confirm 41 locations register, inspect flag placement, capture a city and an outpost, verify support/control/income ownership agree, and confirm the queued QRF uses another eligible friendly outpost. Check capture-flag placement logs; investigate any placement-verification warnings and check no location-at/below-sea-level warnings, no duplicate IDs and no immediate AI response just from initialization. Dedicated-server replication, save/load, flag clearance, vehicle launch clearance and native route acceptance remain unverified.

Rowan alignment preserves everon_airport at 4913.476,28.251,11787.395 and replaces the duplicate military-outpost airport. North Ridge Factory and both North Ridge Resources sites retain the coordinates authored in his separate layers; those three previously unnamed sites receive stable Chicken IDs. His layers are not rewritten. Airport/factory/resource templates already have the shared capture pole, so they receive the same grounding fix. The generic/military-capture option name m_bOutpostCaptureQRFs remains for serialized compatibility.

## Editor-grounding follow-up

Jasper reports Chotain, Durras, Entre-Deux, Camurac, Levie and Montignac underground in World Editor. The saved-coordinate checks and prior Play placement messages did not establish editor mesh placement. All 41 sites remain visually unverified; none should be marked accepted from the CSV alone.

Two Workbench commands now live in Scripts/WorkbenchGame/Plugins, following the module/layout of Bohemia's official SampleMod_WorkbenchPlugin. Reload Workbench scripts or restart Workbench if necessary. Open Chicken in edit mode and use Plugins > Open Resistance:

1. **Audit all campaign flags**: read-only. Reports all 41 actual child world transforms, collision-traced terrain height, model bounds, projected model-base height and clearance. It does not require a Play session.
2. **Ground all campaign flags**: writes editor source coords in one Undo action, converting the desired world position through the actual parent transform. Aligns the model bounds base with physical terrain plus 0.1 m, preserving X/Z and location roots. Save the world after inspecting the result. Obstructed sites are warned about; no horizontal relocation, navmesh accessibility or reachable capture action is certified by this command.

Run the read-only audit first so its log can identify whether the remaining issue is actual hierarchy position, terrain elevation or the model pivot. Both commands refuse Play, prefab-edit mode, another world or an incomplete 41-location catalogue. There is no per-frame/runtime work. Native plugin compilation and editor results remain pending; mocked checks are not visual acceptance.

After a grounding pass and world save, reconcile changed child coordinates back into the CSV/evidence before running Set-EveronCampaignLocations.ps1 -Apply, which would otherwise overwrite editor changes. The existing read-only generator detects that divergence. Rowan's separate scene and shared prefab source files are not edited by the command.

Primary plugin reference: https://github.com/BohemiaInteractive/Arma-Reforger-Samples/blob/main/SampleMod_WorkbenchPlugin/Scripts/WorkbenchGame/SamplePlugins/SampleWorldEditorPlugin.c

The native read-only editor audit has now run: 41 inspected, 0 clearance warnings. It showed the model bottom extends 0.805946 m below its pivot. All 41 corrected heights and capture-context offsets are already baked into the layers/CSV. **Reload Chicken from disk; do not run Ground to apply this correction.** A previously loaded origin-based Ground command would undo the model-base correction. Reload Workbench scripts before any future use of the updated model-aware editor commands. Inspect the resulting poles and test a capture action; neither visual acceptance nor a gameplay capture is inferred from the offline math checks.
