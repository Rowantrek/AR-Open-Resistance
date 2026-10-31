# Everon campaign locations

Chicken places **41** campaign locations: **7 cities, 11 towns, 14 outposts, 5 military outposts, 1 airport, 1 factory and 2 resource sites**. The 18 settlements are support-only with no poles; the 23 military sites retain flag capture. Existing location, support, ownership, income and persistence authorities are reused. Runtime defenders and regional support are described in [Garrisons and regional support](GARRISONS_AND_REGIONAL_SUPPORT.md); static perimeter compositions and runtime defenders use separate authorities. Factory/resource relocation is authorised in Chicken.

## Reusable prefabs

| Template in Prefabs/Locations | Location type | Capture |
|---|---|---|
| AROR_Location_City.et | CITY | Support majority only; no flag |
| AROR_Location_Town.et | TOWN | Support majority only; no flag |
| AROR_Location_Outpost.et | OUTPOST | Existing military flag capture |
| AROR_Location_MilitaryBase.et | MILITARY_BASE | Existing military flag capture, larger radius |

Settlements and outposts reference these templates; the airport, factory and resource sites use their existing templates. Only military installations have the capture-pole child hierarchy and baked transforms. Unused Town_Flag/City_Flag aliases are removed; use the Town/City templates. MilitaryOutpost is removed in favour of MilitaryBase. Outpost and MilitaryBase directly inherit Location_Base. Other scenarios retain the shared server placement component for military flags. New template resource GUIDs and entity IDs are hand-authored, recorded in sibling .meta files, and need Workbench resource discovery/registration on reload. No existing asset GUID was invented or reassigned.

All towns/cities reject flag capture and weighted military recapture, even if an older placed resource retains the deprecated settlement flag attribute. Military flag capture retains hostile-clearance and flip-cooldown checks, with pending unmaterialised defenders also blocking capture. Town support gradually follows control of regional military installations through the existing settlement API. No second support, capture or money system is introduced.

## Scenario data

Military placement binds all 23 measured clearings through `DefencePreset`, `DefenceYaw`, `DefenceAudit` and `DefenceAnchored`, with no remaining unmeasured drafts. Bindings require their named native evidence. The airport root stays fixed; authorised factory/resource moves affect Chicken only. Saved wall offsets are imported separately and reused only for unchanged poses. The latest native audit passes all 23 current footprints, including East Resources after its final move. All 1526 walls were grounded without warnings, saved and imported; navigation and full Play acceptance remain outstanding.

`DefenceMaxRelief` optionally overrides the garrison's whole-footprint terrain allowance (0.25–4m, military sites only). Levie Military Outpost uses 3m against its measured 2.78125m range; all other rows inherit 2m. This does not change flag placement, waive individual-pad checks or establish native activation acceptance.

The 41-site flag measurements below are historical evidence from before settlement poles were removed. CSV/JSON settlement samples are retained for provenance; the generator emits their root/population only. Current editor auditing should inspect 23 military poles and skip the 18 settlements.

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
| Barley Field Outpost | OUTPOST | everon_outpost_farm | 100 m | 0 |
| Gull Point Outpost | OUTPOST | everon_outpost_meaux_harbour | 100 m | 0 |
| Raven Ridge Outpost | OUTPOST | everon_outpost_gorey | 100 m | 0 |
| Pinewood Lake Outpost | OUTPOST | everon_outpost_pinewood_lake | 100 m | 0 |
| Stag Hill Outpost | OUTPOST | everon_outpost_levie_hillside | 100 m | 0 |
| Crow Hill Outpost | OUTPOST | everon_outpost_durras_hill | 100 m | 0 |
| Old Sawmill Outpost | OUTPOST | everon_outpost_sawmill | 100 m | 0 |
| Regina Heights Outpost | OUTPOST | everon_outpost_regina_south | 100 m | 0 |
| Ironworks Outpost | OUTPOST | everon_outpost_chotain_industrial | 100 m | 0 |
| Wolf Wood Outpost | OUTPOST | everon_outpost_old_wood | 100 m | 0 |
| Fox Valley Outpost | OUTPOST | everon_outpost_morton_valley | 100 m | 0 |
| Hawk Ridge Outpost | OUTPOST | everon_outpost_laruns | 100 m | 0 |
| Grey Quarry Outpost | OUTPOST | everon_outpost_quarry | 100 m | 0 |
| Beacon Point Outpost | OUTPOST | everon_outpost_andres_beacon | 100 m | 0 |
| Stone Ridge Base | MILITARY_BASE | everon_military_outpost_chotain | 150 m | 0 |
| Eagle Hill Base | MILITARY_BASE | everon_military_outpost_levie | 150 m | 0 |
| Mercy Hill Field Hospital | MILITARY_BASE | everon_military_outpost_hospital | 150 m | 0 |
| Seawatch Base | MILITARY_BASE | everon_military_outpost_lamentin | 150 m | 0 |
| Morton Bluff Base | MILITARY_BASE | everon_military_outpost_morton | 150 m | 0 |
| Stronghold Delaware (Everon International Airport) | AIRPORT | everon_airport | 500 m | 0 |
| North Ridge Works | FACTORY | everon_factory_north_ridge | 250 m | 0 |
| Montignac Ridge Depot | RESOURCES | everon_resources_north_ridge_west | 250 m | 0 |
| Entre-Deux Ridge Depot | RESOURCES | everon_resources_north_ridge_east | 250 m | 0 |

## QRF connections

Ordinary OUTPOST, military MILITARY_BASE and AIRPORT locations are eligible dispatch/return origins, using the existing nearest friendly location and launch-distance/land/player checks. The target is excluded. This is not proof of an unobstructed vehicle spawn or a viable native route; road/launch clearance still requires Play verification. Explicit configured coordinates remain the fallback when no eligible source exists. Captured origins are excluded and active commanders can retarget their return origin.

Chicken enables m_bOutpostCaptureQRFs alongside m_bTownCaptureQRFs. Canonical capture events can queue the previous AI owner's response, subject to money, role admission, cooldown, current operations and capacity. Initial registration/load establishes the baseline and does not spawn historical responses. The compatibility ReportTownCapture_S API remains; new callers can use ReportLocationCapture_S. The military capture switch also covers airports, factories, ports and resources; initial registration still never generates a historical response. No Invader operation is activated by this change.

## Editing and verification

Run tools/Set-EveronCampaignLocations.ps1 without arguments to check that the layers match the CSV. After reviewing catalogue changes, use -Apply to regenerate Chicken's two location layers. This preserves the existing defender prefix but replaces generated location blocks; reconcile manual World Editor placement edits back into the CSV before regenerating. Shared Everon and Rowan's layers are not written.

Offline validation: tools/Test-EveronCampaignLocations.ps1 checks counts, unique IDs, reference coordinate provenance, preserved anchors, template GUIDs, flag inheritance and origin integration. node tools/Test-EveronQRFIntegration.js checks actual-source capture opt-ins, support/control coherence, server guards and QRF type/admission routing with mocked native services (38 assertions).

In Workbench: stop Play, compile Game scripts, reload Everon_Chicken and let Workbench discover the new prefab/meta pairs. Start a fresh test campaign. Confirm 41 locations register, inspect flag placement, change settlement support and capture a military outpost, verify support/control/income ownership agree, and confirm the queued QRF uses another eligible friendly outpost. Check capture-flag placement logs; investigate any placement-verification warnings and check no location-at/below-sea-level warnings, no duplicate IDs and no immediate AI response just from initialization. Dedicated-server replication, save/load, flag clearance, vehicle launch clearance and native route acceptance remain unverified.

Chicken preserves the airport anchor at 4913.476,28.251,11787.395. Factory/resources retain stable IDs but may relocate to measured clearings under explicit authorisation. Rowan's separate scene files remain untouched. The compatibility option m_bOutpostCaptureQRFs covers all military capture types.

## Editor-grounding follow-up

Jasper reports Chotain, Durras, Entre-Deux, Camurac, Levie and Montignac underground in World Editor. The saved-coordinate checks and prior Play placement messages did not establish editor mesh placement. All 41 sites remain visually unverified; none should be marked accepted from the CSV alone.

Two Workbench commands now live in Scripts/WorkbenchGame/Plugins, following the module/layout of Bohemia's official SampleMod_WorkbenchPlugin. Reload Workbench scripts or restart Workbench if necessary. Open Chicken in edit mode and use Plugins > Open Resistance:

1. **Audit all campaign flags**: read-only. Reports all 41 actual child world transforms, collision-traced terrain height, model bounds, projected model-base height and clearance. It does not require a Play session.
2. **Ground all campaign flags**: writes editor source coords in one Undo action, converting the desired world position through the actual parent transform. Aligns the model bounds base with physical terrain plus 0.1 m, preserving X/Z and location roots. Save the world after inspecting the result. Obstructed sites are warned about; no horizontal relocation, navmesh accessibility or reachable capture action is certified by this command.

Run the read-only audit first so its log can identify whether the remaining issue is actual hierarchy position, terrain elevation or the model pivot. Both commands refuse Play, prefab-edit mode, another world or an incomplete 41-location catalogue. There is no per-frame/runtime work. Native plugin compilation and editor results remain pending; mocked checks are not visual acceptance.

After a grounding pass and world save, reconcile changed child coordinates back into the CSV/evidence before running Set-EveronCampaignLocations.ps1 -Apply, which would otherwise overwrite editor changes. The existing read-only generator detects that divergence. Rowan's separate scene and shared prefab source files are not edited by the command.

Primary plugin reference: https://github.com/BohemiaInteractive/Arma-Reforger-Samples/blob/main/SampleMod_WorkbenchPlugin/Scripts/WorkbenchGame/SamplePlugins/SampleWorldEditorPlugin.c

The native read-only editor audit has now run: 41 inspected, 0 clearance warnings. It showed the model bottom extends 0.805946 m below its pivot. All 41 corrected heights and capture-context offsets are already baked into the layers/CSV. **Reload Chicken from disk; do not run Ground to apply this correction.** A previously loaded origin-based Ground command would undo the model-base correction. Reload Workbench scripts before any future use of the updated model-aware editor commands. Inspect the resulting poles and test a capture action; neither visual acceptance nor a gameplay capture is inferred from the offline math checks.

## Seaports (hand-placed, outside the catalogue)

Three PORT locations are placed directly in Chicken's `MilitaryInstallations.layer` from `AROR_Location_Port.et`. They are the planned basis of the Black Market. They are not CSV rows: the catalogue requires measured, baked flag heights, and the generator check already fails on the authored fortified trials.

| Name | Stable ID | X, Z | Radius |
|---|---|---|---:|
| Lamentin Port | everon_lamentin_port | 1100, 6065 | 150 m |
| Saint-Philippe Port (Charlet Bay) | everon_saint_philippe_port | 4350, 11030 | 150 m |
| Morton Port | everon_saint_pierre_port (retained legacy ID) | 4957, 3876 | 150 m |

Lamentin and Saint-Philippe positions were read from in-game map grid references (010 061 and 043 110) on the quays. Morton Port uses the source `coastalBaseMorton` coordinate on the Morton harbour quay. Saint-Pierre Port (`everon_saint_pierre_port`) was removed: its harbour is now the Invader stronghold, Fortress Vostok (`everon_fortress_vostok`, see [Fortress Vostok](STRONGHOLDS.md)). Saves that hold the old port ID simply have no location to apply it to. Roots use `GroundOnInit 1` and the capture flag keeps the prefab's runtime auto-placement (clear flat ground within 60 m), so no height is baked. Lamentin Port reuses the base AROR_Everon world's stable ID. Each overlaps its town's support radius. Check the placement in Workbench, then bake the heights and add catalogue rows once the generator is reconciled.

## Site names

Military, industrial and resource sites carry names from their surroundings, shown beside their map icon (e.g. "Wolf Wood Outpost", "Seawatch Base"). The feature word comes from what is known about the site: high ground above the nearest town gives Hill, Ridge or Heights; low coastal sites give Point; existing hints are kept (Pinewood Lake, Old Sawmill, Grey Quarry, Beacon Point, Field Hospital). The first word is flavour. Stable IDs are unchanged, so saves and missions are unaffected; only `m_sName` and the CSV `Name` changed.
