# WCS game mode validation — 7 November 2026

The source and installed-resource audit found and corrected WCS integration defects. The combined WCS/RHS addon now defaults new campaigns to its enemy armies, as requested. Native compilation, addon discovery and gameplay have not been executed here.

## Changes

- Corrected 85 resource-path occurrences covering 77 helicopter GUIDs in the WCS catalogue and combined Russian preset. Every correction retains the existing GUID and uses the canonical path found in an installed resource index within the declared addon dependency tree. Examples include AH6 `M134_X2`, Mi24 `UB1657_X4`/`B8V20A_X4` and Mi8 `B8V20A_X6`. This also aligns the AH6 catalogue entry with its preset reference. Native ResourceName alias normalization was not reproduced; this is an identity-preserving correction, not proof that every old alias failed to load.
- Added the WCS woodland FMTV under `SUPPLY_TRUCK` in the combined US preset, keeping its existing `MOTOR_TRUCK` role. Chicken motorized QRF profiles and convoy, supply theft and sabotage missions explicitly request `SUPPLY_TRUCK`; this type was previously empty for that army.
- Replaced the combined US preset's unavailable vanilla static-AA entry with the already catalogued WCS US ZU-23 counterpart.
- Removed one Mi24 catalogue entry whose resource is supplied only by the unrelated installed FTA Vehicle Core addon, outside this addon's declared dependency tree. Neither effective army preset references it.
- Corrected catalogue types for three ZU-23 tripods (`STATIC_AA`), Kornet and TOW tripods (`STATIC_AT`), and Igla/Stinger launchers (`LAUNCHER_AA`). Prices and import requirements retain their existing values.
- Added a same-resource game-mode override inside the **combined compatibility addon**: Occupiers default to `RHS_USAF_FORECON`, Invaders to `RHS_AFRF_MSV_VPKO3_EMR`. Core-only defaults remain US/Soviet. Existing valid saved selections take precedence; a missing saved army uses the configured role default. Resistance remains FIA and civilians remain Everon civilians by default.
- Updated architecture status and fallback descriptions to reflect implemented vehicle selection and configurable role defaults.

The new prefab override explicitly inherits the underlying game-mode prefab, following the existing Radio addon's overlay pattern, and copies the core resource identity `{49BA4199470D70DA}` and component identity `{5C71FA2147F19FB3}`. No existing asset GUID was invented. The new supply-role array instance ID `{DC0E37B108334E92}` and override entity ID `D1841E48D5FA1903` are hand-authored inline IDs, not resource references. The text override model preserves the core squad component, optional Radio component and new defaults in either overlay order; native coexistence remains a Workbench check. Workbench should discover and round-trip these changes before publication.

Extension impact: new campaigns using the combined addon have different enemy defaults. Preset IDs, registry APIs, RPCs and public class names are unchanged. Extenders can still override defaults or select other armies. The removed catalogue hull needs its own FTA integration if an extender wants to offer it. Existing stored vehicle snapshots keep their original prefabs and require their original mods.

## Routes inspected

| System | How the selected assets reach it |
| --- | --- |
| New campaign / saved campaign | Setup draft reads `GetDefaultPresetId`; server `ResolvePresetId` accepts a valid requested ID before consulting the default. Restore uses the same server path. |
| Infantry and vehicle crews | `GetSpawnPrefab` supplies the campaign role's gear-less body; `FillGroup_S` and the group outfitter apply that army's readiness loadout. Clothing, weapons, attachments, loaded magazines and nested items read through content replacements. A vanilla-looking base-character path does not identify the finished kit. |
| Garrisons and strongholds | Parked, manned and static slots request types from the selected preset. Legacy hulls supply type hints. Spawned hulls receive campaign role affiliation. Restore rebinds saved slot ordinals while preserving casualties and damage. |
| Ground QRFs / attacks | Force profiles select a typed hull from their role's preset. Crew and passenger infantry use the shared preset spawning path. Missing enabled types make profiles unavailable rather than selecting another army's hull. |
| Airborne QRFs / attacks | REAPER adapter selects transport and escort hulls through the profile's preset query. Infantry uses the shared outfitter. REAPER retains its own pilot characters for its flight integration. |
| Missions | Mission board selects enabled typed entries from the enemy role's preset, preferring readiness-eligible choices. Its existing fallback remains within that preset. Mission guard groups use `AROR_ForceSpawner` and `FillGroup_S`. |
| HICOMM / flag recruits | Both use the shared Force Spawner with the Resistance role. FIA remains the default Resistance kit; selecting WCS/RHS enemies does not turn Resistance recruits into those armies. |
| Player initial kit | Spawn finalization applies the selected Resistance preset's player loadout. |
| Black Market / Imports / arsenal | Read the enabled content catalogue, purchase flags and import rules. The WCS pack contributes its gear and vehicles; enabling it does not itself replace every vanilla item. |
| Captured vehicles / garage | Preserve the actual stored vehicle and cargo snapshot. They are not automatically converted when campaign defaults change. |

## Executed checks

| Check | Result and limits |
| --- | --- |
| Installed resource-index audit | **2,940 unique GUID/path references match exactly**, with zero stale-path matches, outside-tree-only references or unresolved references remaining. Covers effective combined army kits/vehicles plus base-game, RHS and WCS catalogues. Uses 270 local resource databases and a 51-ID declared dependency closure. This does not load prefabs or certify addon loading. |
| `Test-WCSIntegration.js` | **774 assertions pass.** Executes actual preset entry filtering, vehicle selection, mission vehicle selection, Imports catalogue building, role-default resolution, server restore guards, unit selection, readiness/weight selection and loadout replacement getters against native mocks and effective authored configs. Covers all 12 Chicken force profiles, all standard formation types and crew at readiness 1–5, disabled vehicle availability, saved vanilla choices, addon defaults and game-mode overlay composition. |
| Existing preset vehicle suite | Passes actual-source checks for selected-army hulls, content filtering/replacements, role affiliation, saved garrison variants and runtime profile availability. |
| All JavaScript source/mock suites | **24/37 pass; 13 existing failures remain.** No new failure signatures relative to the previous squad validation run. This is not an all-green regression suite. |
| Squad validation | 408 assertions pass; all 10 deliberately broken policies detected. |
| Setup / MARS / Black Market resource bindings | PowerShell check passes metadata, source bindings and local Workbench registration. Native rendering remains unverified. |
| Stronghold resources | PowerShell check passes two local references, twelve installed resource GUIDs and metadata uniqueness. Native loading remains unverified. |

The thirteen existing JavaScript failures are recorded individually in the suite evidence. The previous [squad validation](PLAYER_SQUADS_VALIDATION_2026-11-07.md) also reproduced them against pre-squad source. Most are stale expectations or missing native mock helpers; their existence does not establish that the corresponding game systems work. The previously identified CampaignReadiness and CampaignGroundingData PowerShell failures were not changed or rerun in this WCS pass.

Repeat with Node:

```text
node tools/Validate-WCSIntegration.js
```

The command deliberately exits nonzero while the known failing suites remain. The resource audit expects the local Steam installation and downloaded-addon directory used by this workstation; `audit({ addonsRoot, baseGameRoot })` supports other locations. Exact successful reference rows are omitted from the checked-in JSON to keep the evidence readable; the audit function returns every row. Source hashes tie the resource results to the checked configs.

## Remaining findings and native acceptance

Two declared dependencies are absent from this workstation: [WCS_RHS_Sounds](https://reforger.armaplatform.com/workshop/6AAE2A3269AD5C39) (`6AAE2A3269AD5C39`, direct combined-addon dependency) and [WCS_RHS_Clothing](https://reforger.armaplatform.com/workshop/6AAEEB19367529A1-WCS_RHS_Clothing) (`6AAEEB19367529A1`, referenced by installed WCS Arsenal). The official Workshop pages identify both modules. Resource existence can be checked independently, but **the full addon load is not certified** until those dependencies are installed/synchronized. No downloaded mod files, generated resource databases or dependency declarations were modified.

The catalogue model assumes BASE_GAME, RHS and WCS are enabled and checks group requirements against the declared dependency tree. Bohemia documents [GUID-based partial overrides for configs and prefabs](https://community.bohemia.net/wiki/Arma_Reforger%3AData_Modding_Basics); the audit models those overrides rather than treating each sparse compatibility config as a standalone army. Native override loading is still pending. User-disabled packs/groups/categories can intentionally remove vehicles. Packs currently author no WCS replacement mappings, so they add choices rather than globally converting base-game armies or saved inventories.

The authored armies are mixed collections: US offers 23 of 25 vehicle entries (18 WCS catalogue entries, 2 RHS, 3 base-game); Russian offers 33 of 43 (17 WCS, 16 base-game). The US count includes the same FMTV hull under two roles. Catalogue provenance does not prove whether a loaded mod also overrides a base prefab. Russian gunship choices explicitly include three base-game Mi8 variants alongside WCS aircraft. Logistics, some static weapons, civilians and REAPER pilots retain existing source assets. No general policy to remove those choices was introduced.

Twelve existing preset entries are filtered because no enabled catalogue entry offers them. The two US entries are legacy LAV and UH1H base templates; enabled IFV and transport alternatives exist. Russian filtered entries include internal UAZ/Ural variants. Current Chicken force profiles still have every requested type, but the Russian preset has no offered ambulance or fuel-truck type; those are follow-ups if a system starts requesting them. Missing US radio-operator and Russian pilot unit types are absent from standard formations; native REAPER pilots are independent. Outpost stores also remain unstocked pending their existing content-source rework.

In Workbench, install/synchronize the missing dependencies, open the combined WCS/RHS addon and rescan resources. Compile in Script Editor, then Play the Chicken world in World Editor. Check:

1. New setup selects the two combined armies; loading a vanilla-choice save preserves its selections. With Radio also loaded, verify both radio functions and core game-mode components remain available.
2. Nearby garrisons and ground QRFs have the selected uniforms, weapons and vehicle faction. Check the log for outfit failures and missing resources at readiness 1 and 5.
3. Motorized US QRFs and supply/convoy missions field the FMTV. Check the WCS US static-AA variant when that type is requested.
4. MH6/AH6, Mi24/Mi8/Ka52 packages can fly, land, unload, reboard and withdraw with REAPER. Index matching does not verify flight components, seats or navigation.
5. Imports lists WCS helicopters and FMTVs with the intended categories, gates and prices; hidden test hulls remain absent. Verify real magazine, attachment and clothing-container compatibility.
6. On a dedicated server with two clients, verify enemy role affiliation, replicated setup/pack selections, saved campaign restore, captured vehicle retrieval and squad commands/recruitment.

Evidence: [resource summary](WCS_ASSET_AUDIT_2026-11-07.json), [GUID-preserving path corrections](WCS_RESOURCE_PATH_CORRECTIONS_2026-11-07.json), [full suite outcomes](WCS_SUITE_2026-11-07.json).
