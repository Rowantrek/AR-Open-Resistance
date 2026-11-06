# Open Resistance build audit and Rowan gap analysis

Audit date: **6 November 2026 (Australia/Sydney)**. This is a source, configuration, resource-index and automated-harness audit. **It is not an Enforce compilation or native gameplay certification.**

**Later same-day follow-up:** Rowan's PR #59 merged after this audit. See [the submission review](PR_SUBMISSION_REVIEW_2026-11-06.md) for the newer baseline, Rowan-first conflict resolutions, committed radio media and current PR-candidate status. The snapshots and measurements below remain historical evidence.

## Assessment

**The checked-out build contains Rowan's latest code, but is not ready to call release-verified.** Its main problems are incomplete delivery of radio media, unresolved WCS/RHS kit assets, gaps in the new airborne launch/package logic, and obsolete verification evidence. There is also a substantial split between this checkout and the separate local Chicken build: neither branch represents all the work in the repository.

The recommended integration base is the current Rowan-derived code, with deliberate ports of wanted Chicken features. Do not merge the other developer branch wholesale merely because it has more features: it uses different addon identity, content/preset structures and shared spawning interfaces.

No gameplay files, settings, branches, saves or resource databases were changed for this audit. The pre-existing radio edits and untracked samples were preserved. Only this report, dated evidence and audit runners were added. AGENTS.md and ARCHITECTURE.md were left unchanged.

## 1. Exact builds compared

| Build | Revision | Meaning |
|---|---|---|
| Current checkout | `rowan-balance` at `0075d8dc9d609161d6fd65c0ed057a1b54eb85e0` | Primary audited build, including the existing working-tree radio edits |
| Rowan integration | `upstream/master-dev` at `3dba0bd82c825807b15f7542eb6324dfb0bac476` | Latest remote integration branch checked through GitHub during this audit |
| Rowan developer | `upstream/rowantrek-dev` at `a770bf021f45b467e7a94cb279ac5c094801825d` | Latest remote developer branch; all its commits are already ancestors of the integration branch and current checkout |
| Separate local Chicken build | `chicken-dev` at `0d2e5e82e7d8c27a16aa64028a04aef448ebaefa` | Audited as an additional source of Claude/Chicken features and integration gaps |
| Separate merge worktree | `chicken-rowan-merge` at `f31c2d57557cb176e962ccbd3d9e38bc3589cf3b` | Located at `C:/Open Resistance/AR-OR-merge`; branch differences inspected, not independently certified |

Remote verification: [Rowan's branches](https://github.com/Rowantrek/AR-Open-Resistance/branches), [integration revision](https://github.com/Rowantrek/AR-Open-Resistance/commit/3dba0bd82c825807b15f7542eb6324dfb0bac476), [developer revision](https://github.com/Rowantrek/AR-Open-Resistance/commit/a770bf021f45b467e7a94cb279ac5c094801825d). The local upstream refs matched the live GitHub branch SHAs when checked; no fetch or branch checkout was necessary.

Git ancestry results:

- Current versus Rowan integration: **7 commits ahead, 0 behind**; 23 committed files changed, approximately 6,430 additions and 103 deletions.
- Current versus Rowan developer: **16 ahead, 0 behind**. The additional count includes integration merge history; it does not mean 16 missing Rowan features.
- Current versus local `chicken-dev`: **47 current-only / 37 Chicken-only commits**.
- Current versus `chicken-rowan-merge`: **49 current-only / 30 merge-branch-only commits**.

The latter counts describe ancestry, not unique feature counts. Some work was ported or implemented differently. File and interface differences are more useful than blindly cherry-picking these lists.

The seven current-only commits add heavy Chicken responses, farther airborne origins, first-seaport AT imports, extra joint transports, WCS/RHS vehicles and kits, vehicle radio controls, and radio audio wiring/level changes.

## 2. Findings, ordered by practical priority

Priority meanings: **P1** should be resolved before treating the affected build/feature as releasable; **P2** is a functional or integration correction; **P3** is documentation or operational improvement. Confidence distinguishes direct source evidence from native behavior that still needs testing.

### F01 — P1: new WCS/RHS kits reference locally unavailable primary weapons

**Attribution:** current-only WCS/RHS additions. **Evidence:** installed resource-index audit; native loading not run.

The two WCS/RHS preset override files contain **229 unique prefab references**. Against **289 available installed resource indexes**, 213 match by path (case-insensitively) and GUID. Of the remaining 16:

- **8 have neither a matching path nor their GUID in any checked index**: two helmets, one pair of boots, and five primary weapons.
- **8 are helicopter references with matching GUIDs under different paths**. Those are stale names, not evidence that the aircraft are absent; native GUID-based resolution may still succeed.

The unavailable primary weapons are `Rifle_AK105_SQUAD`, `MG_RPK74M_SPECTER_SQUAD`, `Rifle_M27IAR_CTR_GRIP_SQUAD`, `Rifle_M4A1_RAS_GRIP_SQUAD` and `MG_M240_MDO_SQUAD`.

Examples: `OptionalAddons/OpenResistanceWCSRHSCompat/OpenResistanceWCSRHSCompat/Configs/FactionPresets/AROR_Preset_RHS_USAF_FORECON.conf:164`, `:667`, `:1166`; Russian counterpart `AROR_Preset_RHS_AFRF_MSV_VPKO3_EMR.conf:154`, `:496`.

`AROR_LoadoutOutfitter.Apply_S` applies whatever it can and logs failures. A partially broken kit therefore does not automatically reject the soldier/package or revert to a complete known kit. The practical risk is soldiers fielding incomplete clothing or missing their intended primary weapon.

**Action:** align the installed dependency versions with the authored kits, or reimport the kits from resources that actually resolve. Verify every affected unit at each loadout tier in Workbench. Do not replace GUIDs by guesswork. All 20 declared WCS/RHS direct dependencies were found in the local/repository project inventory; presence of a dependency does not establish that its installed version supplies these eight resources.

Evidence: [installed asset report](validation/BUILD_AUDIT_2026-11-06_WCS_RHS.json).

### F02 — P1: committed vehicle radio depends on 18 untracked audio resources

**Attribution:** current-only radio work and current working tree. **Confidence:** confirmed repository completeness defect.

`Sounds/Radio/AROR_Radio.acp:324` onwards references 18 local WAV resources. Their WAVs and `.meta` files exist locally under `Sounds/Radio/Samples/`, but none is tracked. The folder contains 36 files totalling approximately **190.45 MB**. The committed ACP already references these GUIDs; this is not limited to the small uncommitted audio-level edit.

A fresh clone cannot reproduce this music bank. Local Workbench success would not prove the committed addon is complete.

**Action:** include the intended samples and generated metadata through the project's chosen asset delivery process, then open/package from a clean checkout and play every bank entry. Preserve the existing working-tree files during that work. The audit did not stage or commit media.

### F03 — P1: the new air origin is not revalidated as the committed origin

**Attribution:** introduced by the farther-airborne-origin addition. **Confidence:** confirmed source control-flow gap; native reproduction pending.

`AROR_ReaperQRFExecutor.c:79` selects an independent `airSite`. At `:629` it records that site's launch cooldown, but neither the operation nor the force retains its location ID for ownership validation. `CreateQRFTacticalCommander` retains the **ground** origin. Later, `MonitorReaperPackage` at `:1540` checks `TacticalOriginAvailable(operation)`, which validates that ground location (`AROR_OperationExecutorComponent.c:2482`).

**Reproduction:** plan a joint response with different ground and air bases; capture or contest the air base while aircraft construction is queued, leaving the ground base owned. The queued aircraft path has no air-base ownership predicate to reject that launch. Its geometric/player checks still run, but they do not establish ownership.

**Action:** retain the air origin's stable location ID and check its ownership, eligibility and contention before construction. Define whether losing it cancels/replans unbuilt aircraft. Preserve the explicit fixed-origin policy for trusted scripted responses.

### F04 — P2: unescorted extra transports are priced as escorted packages

**Attribution:** new extra-joint-transport feature. **Confidence:** reproduced using the extracted source method with mocked budgets.

`JointExtraTransports` (`AROR_ReaperQRFExecutor.c:693`) uses `profile.ReplacementQuote()` to decide whether an extra is affordable. `financialElements.Insert(profile)` at `:613` and `ConfigureForceFunding` also retain that escorted profile's quote. Physical construction suppresses the extra escort through `m_bUnescorted`, and group/hull reservations are adjusted, but the quote is not.

For the current US profile, the conservative personnel allowance is 32 × 25, the transport is 1,600 and the escort 2,000: **4,400 reserved per extra instead of 2,400 using the same personnel allowance without an escort**. A source-method probe supplied exactly 2,400 after the base package/overhead and got zero extras despite room for the transport-only package.

This is an affordability/collateral problem, not proof that nonexistent escorts are permanently billed as combat losses: actual loss accounting still observes physical assets and unused margin can later be released.

**Action:** derive a package-specific plan/quote for an unescorted extra and use it consistently in selection, finance, materialization and cleanup. Cover declining funds between planning and reservation too: the chosen extra count is cached before actual funding succeeds.

### F05 — P2: extra transport construction still consumes the escorted profile's live cost

**Attribution:** new extra-joint-transport feature. **Confidence:** extracted source predicate reproduced; native reproduction pending.

At `AROR_ReaperQRFExecutor.c:1554`, queued construction passes `force.m_Profile.GetVehicleBudget()` to `CanConstruct`. An extra transport has a one-hull reservation but retains the escorted profile whose budget is two.

**Concrete supported configuration:** set joint transport percentage to 100, use four ground elements, sufficient global reservations, a six-aircraft live limit and a lead wave containing five transports plus one escort. After five actual aircraft are constructed, the final single transport is blocked by `5 + 2 > 6`, although `5 + 1` fits. The lead activation barrier can then wait until preparation/spawn failure.

The checked-in Chicken percentage is 50, so this exact six-aircraft boundary is an **extender/configuration edge case**, not a claim that every current Chicken response deadlocks.

**Action:** use actual package hull cost for admission and construction, including retained aircraft. Test one-hull extras at the exact live cap.

Evidence for F04–F05: [focused probes](validation/BUILD_AUDIT_2026-11-06_PROBES.json). These execute adapted method bodies with mocked services, not native AI.

### F06 — P2: the 1,500 m air appearance clearance is best effort, not enforced

**Attribution:** farther-airborne-origin addition. **Confidence:** confirmed source behavior.

`OutOfPlayerSight` (`AROR_ReaperQRFExecutor.c:139`) returns its best candidate even if the requested clearance was never reached. A coast, map boundary or unsafe intermediate probe can stop the search. Final package construction calls the generic `IsLaunchPositionSafe`, whose default player distance is only **100 m** (`AROR_OperationExecutorComponent.c:194`–`:197`, `:663`). It does not recheck `m_fReaperLaunchPlayerClearance`.

Consequently, the feature does not guarantee its description that every player is 1,500 m away. Formation offsets can also move individual aircraft closer than the central point. No visibility/line-of-sight test establishes that a player cannot see them appear.

**Action:** either make the setting explicitly a preference, or enforce it on each final aircraft position and defer/fall back when no suitable origin exists. Check again just before queued construction.

### F07 — P1 verification gap: the historical green release evidence no longer describes either current build

**Attribution:** shared with Rowan, plus current feature changes. **Confidence:** reproduced on pinned snapshots.

The historical `QRF_AUTOMATED_RESULTS.json` is dated **29 October 2026** and reports success. **106 of its 441 recorded source hashes now differ** from this working tree. It should be treated as historical evidence.

Fresh JavaScript results:

| Source snapshot | Passing | Failing | Interpretation |
|---|---:|---:|---|
| Current `rowan-balance` + existing radio edits | 21 / 34 | 13 | Current harnesses are not green |
| Rowan `master-dev` at `3dba0bd` | 21 / 34 | 13 | The same suite names fail; do not attribute these 13 failures to our seven additions |
| Local `chicken-dev` at `0d2e5e8` | 26 / 36 | 10 | Includes additional FIELD/session/WCS checks; a different code and harness set, not a directly comparable quality score |

The 13 current/Rowan failures are:

| Suite | First failure |
|---|---|
| GarrisonCrew | Missing `AROR_ResistanceFleet` mock |
| GarrisonsAndSupport | Extractor expects removed `InfantryPrefab` method |
| MarsAndAttacks | Missing `GetAvailableProfiles` fixture method |
| MissionLifecycle | Missing `AROR_ForceSpawner` mock |
| PostsAndPads | Extractor expects removed `InfantryPrefab` method |
| QRFBoarding | Extractor expects removed `ReplaceIntoSeat` method |
| QRFIngress | Missing `GetMountRejection` dependency |
| QRFInterruptions | Missing `AROR_EUnitType` mock |
| QRFMutations | Its interruption-suite baseline already fails |
| QRFProgression | Expects readiness-one/tier-one defaults that have changed |
| QRFReleaseLifecycle | Profile fixture lacks `HasPresetVehicles` |
| QRFStrategy | Missing `GetAvailableProfiles` dependency |
| RebIntel | Enforce-to-JavaScript adapter leaves a typed player-controller declaration |

These are principally stale adapters/fixtures and outdated expectations. They are **not 13 demonstrated engine defects**. They do prevent the suites from completing their intended checks. Passing tests use mocked Enfusion services and cannot certify driving, flight or replication.

The new radio and extra-transports changes have no dedicated coverage in the existing release suite. The focused probes in this audit cover only the two stated budget predicates.

**Action:** repair fixtures against the selected architecture, retain independent behavioral expectations, add coverage of the actual new package/origin paths, regenerate evidence, then perform native acceptance. Do not change expected results simply to recover a green badge.

### F08 — P2 integration gap: Chicken behavior is not exposed by the current mission header

**Attribution:** split between the current Rowan-derived branch and the separate Chicken branch. **Confidence:** confirmed resource/configuration wiring.

The current core has one mission header, `Missions/AROR_Everon.conf`, targeting `Worlds/AROR_Everon/AROR_Everon.ent`. It does **not** target `AROR_Everon_Chicken.ent`.

The main Everon managers retain readiness 1, Crackdown 0, 30% airborne share, tier-3 wave splitting and 20-group/160-personnel/10-vehicle budgets. The Chicken managers use readiness 2, Crackdown 20, 100% airborne/joint share, tier-5 splitting, 50% extra transports and 24-group/220-personnel/12-vehicle budgets.

Therefore testing Chicken in World Editor does not test the mission a server selects through the existing header. `chicken-dev` has `AROR_Everon_Chicken.conf`, a WCS mission header and a dedicated-server configuration; this checkout does not.

**Action:** explicitly choose the release scenario and provide a mission header for it. Validate packaged behavior through that header. Do not silently copy experimental Chicken balance into the normal mission.

### F09 — P2 shared unfinished feature: outpost automatic stock is currently empty

`AROR_OutpostSupplyComponent.StockPool` at `:65` is an empty method with a TODO explaining that faction presets no longer list the source items. `Stock_S` therefore has no items to choose. Container inventory wiring also needs native verification.

This is present in Rowan's latest code too, not introduced by our seven commits. The architecture's Garrisons section still describes preset-sourced stocking in places, while its Factions section acknowledges that stores are unstocked. Any separately authored native container contents are a different source; this finding concerns the automatic stocking path.

**Action:** choose Content Packs or army loadouts as the source, use real native storage, and verify loot depletion/save/load. Coordinate this with the logical garrison roster so repairing an empty stock source does not inadvertently refill previously looted saves.

### F10 — P2 shared architecture gap: REAPER is still a mandatory core dependency

The core `addon.gproj` lists `69A4D664A6284E06`, and game scripts directly reference REAPER classes. The optional REAPER addon does not isolate those core dependencies. Disabling airborne execution at runtime does not remove the compile/load dependency.

This exception is documented and shared with Rowan. It conflicts with the intended base-game-only core/extender boundary, not with an already delivered base-game-only release.

**Action:** either explicitly ship a REAPER-dependent core for this milestone or move the adapter and its configuration behind an actual optional-addon boundary before promising a base-game-only package.

### F11 — P2 shared campaign completeness gaps

- **Live QRFs/attacks are session-only.** The operation manager serializer saves escalation, debt and mix counters, not physical deployments. Saving mid-response does not preserve an identical battle on reload.
- **Garrisons save logical survivors/losses**, not an exact inventory/position restoration. Replenishment is unfinished.
- **Stronghold capture has no campaign victory/end flow.** The stronghold system launches retaliation/final defence and constrains capture; it does not finish a campaign.
- **Missions restore their logical identity and timer, then redeploy**, rather than restoring the exact physical objective scene.
- **HICOMM squads are session-only with economy refunds**, not persistent live squads. Personal recruits in the separate Chicken branch follow a similar policy.
- **Initial-save failure is only logged.** Game Flow marks the campaign ready after requesting the save; `OnInitialSaveCompleted` (`:1176`) does not roll back or notify the setup caller that persistence failed. Native failure-handling acceptance is required before presenting successful setup as proof a save exists.
- **Unattended dedicated-server setup remains incomplete.** Admin-created setup works in source; the documented server-config fallback is still planned. Permission behavior also needs native acceptance: setup uses its own role check, whereas other admin tools use `AROR_AdminAccess`.

These are design/status limitations or operational risks, not newly introduced regressions. Decide which are acceptable for the next milestone and document them in player/admin-facing instructions.

### F12 — P3: documentation, generators and scenario evidence have drifted

`docs/FORCE_POOL_QUICKSTART.md` describes an old eight-profile fixture, disabled airborne execution, old launch assumptions and obsolete test expectations. Current Chicken has 12 profiles and extensive ground/joint recipes. `FORCE_POOLS.md` also retains implementation-era instructions alongside later feature descriptions.

Current PowerShell checks expose more drift (the four geometry/generator failures reproduce on Rowan's snapshot too):

| Check | Observed result |
|---|---|
| `Test-QRFSourceStructure.ps1` | Pass: balanced lexical structure in 261 core addon scripts; not compilation |
| `Test-GarrisonAssets.ps1` | Pass: 59 local/installed references and placement/policy checks |
| `Test-ReaperAssets.ps1` | Pass: all five installed pilot/flight-waypoint references |
| `Test-OperationUIResources.ps1` | Pass: MARS, Black Market and setup metadata/bindings/local registration |
| `Test-StrongholdResources.ps1` | Pass: stronghold metadata, two local and 12 installed references |
| `Test-ForcePoolAssets.ps1` | Current fails old `us_airborne` unlock expectation; Rowan proceeds farther but fails old starting-Crackdown expectation. Neither full suite is green |
| `Test-CampaignReadiness.ps1` | Fails its Delaware three-helicopter assertion |
| `Test-CampaignGroundingData.ps1` | Fails grouped-prefab syntax parsing |
| `Test-EveronCampaignLocations.ps1` | Generator cannot identify civilian location boundary |
| `Test-MilitaryDefencePresets.ps1` | Authored outpost prefab differs from generated definition |

The force-pool script checks its base-game GUID manifest before the reported profile failure; that does not mean the subsequent recipe/budget assertions passed. Generator checks were run without `-Apply`; world/prefab files were not regenerated.

**Action:** select authoritative authored resources, reconcile generator/catalogue drift, and rewrite the quickstart around the selected mission. Do not regenerate manually edited sites from obsolete assumptions merely to satisfy a test.

## 3. System-by-system gap matrix

“Shared” below means the implementation is already in both the current checkout and Rowan's integration revision. It does not imply native acceptance.

| System | Current versus Rowan | Remaining gap / acceptance |
|---|---|---|
| Core game mode and scenario lifecycle | Shared | Dedicated startup, setup permissions, save failures and packaged mission acceptance |
| Faction roles and role base characters | Shared | Check all selected armies, clothing and replicated affiliation |
| Preset loadouts, AT/AA specialist types | Shared Rowan implementation retained | WCS/RHS additions need resource and complete-kit acceptance |
| Content Packs / hierarchical selection | Shared, plus eight AT weapon/ammunition import-gate edits | Validate disabled groups/replacements and requirements; no broad new AA gate change appears in this delta |
| Storage / arsenal / saved loadout slots | Shared, including Rowan's Reforger 1.9 validation fix | Concurrent deposits/withdrawals, partial magazines, failed transactions and reconnect/save acceptance |
| Garage / claimed fleet | Shared | Claimed QRF/garrison/mission hull preservation, duplicate avoidance and condition/cargo restore |
| Economy / income | Shared | Live liabilities, debt, no duplicate refunds, save/reload consistency |
| Locations / capture / recapture | Shared | Hostile clearance, deploying garrison gate, cooldown, aircraft occupants and concurrent AI roles |
| Settlements / regional support | Shared | Support-only ownership and contested occupation acceptance |
| Garrisons / compositions | Shared | Specialist kits, crew seats, authored geometry, persistence and empty-stock path |
| Strongholds | Shared | Physical retaliation/final defence; campaign end missing |
| OPS missions, six subclasses | Shared | Native objectives and save/redeploy behavior; lifecycle harness currently broken |
| HICOMM | Shared | Leadership/ownership, complete squad spawning, refunds and network UI |
| QRF scheduling / escalation / finance | Shared | Current verification debt; physical stock and durable live operations unfinished |
| Ground force execution | Shared | Far-origin wake/seating, complete passenger unload, actual roads, cancellation and retention |
| Airborne execution | Shared REAPER adapter plus our farther origins | Air-origin/clearance findings and actual asset controller compatibility |
| Joint responses | Shared framework; our heavier Chicken recipes and added transports | F04/F05, lead/reserve timing, exact budgets and native combined-arms behavior |
| Invader campaign / territory attacks | Shared | Simulation-to-physical transition, fronts, funding, attack deduplication and no-player behavior |
| Notifications / REBINTEL / spotting | Shared | Native rendering, long labels, late join and owner-filtered spotting |
| Phone / administration / MARS | Shared, plus vehicle radio interactions outside the phone | Packaged authorization/revocation, role changes and current harness repairs |
| Vehicle music radio | Current-only | Missing tracked media; native seek/volume/audio synchronization; modded vehicle action contexts |
| FIELD annotations / personal recruits | Absent here and Rowan; present on local Chicken | Deliberate port required, including privacy, serializer, UI and refund integration |
| Ambient patrols / civilians | Absent here and Rowan; present on local Chicken | Deliberate port, global performance budget, native navigation and cleanup acceptance |
| Dedicated Chicken/WCS mission wiring | Absent here; present on local Chicken | Choose addon identity, dependency graph, header and compatible server config |

## 4. What the separate Chicken build adds, and why it needs a deliberate port

The additional local `chicken-dev` snapshot contains implementations of:

- FIELD phone app and private/squad/Resistance annotations, recipient-filtered snapshots, persistence and route-point notes.
- Personal recruitment with ownership, transfers, vehicle seats, dismiss/refund behavior and caps.
- Ambient enemy foot/motorized patrols with territory and player-distance rules.
- Ambient settlement pedestrians and occupied civilian cars with paced spawning and retention.
- Chicken and WCS mission headers/server setup, additional player map/navigation features, alternate catalogue/preset/outfitting work and session fixes.

The current branch retains Rowan's `Players/AROR_PlayerMarkersComponent.c` for player-position markers; that is not equivalent to Chicken's FIELD annotation system. The similarly named classes and moved files must be reconciled, not copied alongside one another.

Important integration conflicts:

1. **Core identity:** current core GUID is `6AA282336705EED2` (“Open Resistance”); Chicken uses `6C68A5E5654D7210` (“Open Resistance Ultimate”). Their dependencies/server configs must reference the intended product. Existing save addon compatibility must be tested rather than presumed across identity changes.
2. **Force Spawner interface:** Rowan/current `SpawnGroup` takes `array<AROR_EUnitType>` and dresses role base characters through `AROR_GroupOutfitter`. Chicken's same helper takes `array<ResourceName>` and uses its alternative roster/outfitter path. Recruitment, patrols and civilians cannot simply be pasted over without adapting callers.
3. **Preset/content schema:** Chicken has catalogue classifiers and alternative additions/registration; current has Rowan's role-base/loadout and content-pack-group model. Choose a coherent schema and migrate data rather than mixing models.
4. **Class overlap:** FIELD relocates/redefines player marker classes; outfitting/storage helpers also differ. Resolve duplicate definitions and signatures before compiling.
5. **Budget ownership:** patrols, civilians and personal recruits add their own server populations on top of existing garrisons, QRFs and HICOMM. Their individual caps do not establish a safe combined population or frame time.

Chicken's additional tests passed for FIELD (274 assertions), session fixes (27) and WCS integration (41), but the branch still has ten failing suites and native acceptance gaps. More features and more passing tests do not make it an automatic replacement for Rowan's latest architecture.

## 5. What checked well

- Rowan's latest arsenal validation, shared Force Spawner, specialist types and far-origin AI waking are present; no missing Rowan commits were found.
- Current admin/MARS changes are server-authorized through the player-controller path and role checks; a client UI credential alone does not authorize mutation.
- Shop prices and purchase requirements are resolved server-side; the client sends purchase intent. Garage import failure has a refund path.
- Radio requests derive player identity from the owning controller, are throttled and verify actual seating server-side. Audio playback excludes dedicated servers, limits concurrent sounds and removes timers on deletion. These are source observations, not proof of native audio APIs.
- Server authority, paced callqueue work, versioned serializers and player-asset retention are established patterns across the existing systems.
- The tracked metadata scan found no local reference GUID mismatch or metadata-name mismatch. Two repeated preset GUIDs belong to the intentional RHS/WCS-RHS overrides of the same resources; they are not reported as accidental collisions.
- Read-only installed resource checks succeeded for garrison, stronghold, core UI and REAPER references as detailed above.
- `git diff --check` reported no whitespace errors. Existing radio files produced line-ending normalization notices only.

## 6. Native acceptance required before release

No command-line build was attempted. In **Arma Reforger Tools**, compile all scripts in **Script Editor**, then **Play in World Editor**. Follow with a packaged dedicated server and at least two clients; Workbench-host success is insufficient for replication acceptance.

| Order | Acceptance scenario | Required evidence |
|---:|---|---|
| 1 | Clean checkout, intended addon stack and mission header | All resources discovered; no missing WAVs, kit assets or unknown components; successful full script compile |
| 2 | Army/loadout matrix | Every selected army/type/readiness spawns with clothing, primary weapon, compatible magazines and specialist launcher; log contains no outfitter omissions |
| 3 | Ground response per role | Wake, all crew seats, full passenger cabin, real drive, normal exits, objective order and exact reservation cleanup |
| 4 | Air response per role and selected aircraft | Correct pilot/controller, verified boarding, launch outside enforced clearance, real flight, ground contact, animated unloading and safe departure |
| 5 | Joint responses | Current tiers 1–5; exact extra count/quotes; lead and reserve barriers; one-hull construction at cap; no duplicate package indices |
| 6 | Origin changes | Capture/contest ground or air origin after planning; move players into a queued launch; coastal/no-safe-launch fallback |
| 7 | Interruption and asset claims | Cancel during construction/boarding/transit/unload, lose crew/hulls, board enemy hull, store claimed hull; no nearby deletion, fabricated losses or leaked queues |
| 8 | Persistence | New initial save, injected save failure, manual/autosave/load, older serializer versions, looted garrisons, fleet cargo/condition, mission redeploy and HICOMM/refund accounting |
| 9 | Network authority and late join | Non-admin mutations rejected, rights revocation, all UI/state on late join, correct shop/garage concurrency; dedicated server runs no UI/audio |
| 10 | Radio | Every track, two clients entering range mid-track, station/volume changes, several moving vehicles, destruction/empty shutoff, action contexts on RHS/WCS vehicles |
| 11 | Sustained populated campaign | Multiple players separated across the island, overlapping garrisons plus air/ground QRFs and missions; measure AI population, script time, network traffic and cleanup |
| 12 | If Chicken features are ported | Marker privacy/save/identity, recruit ownership/refunds, native patrol/civilian routing, combined population limits and clean shutdown |

Source-level capacity risk: current Chicken admits 220 QRF personnel, before garrisons, HICOMM, missions or players. The historical site-readiness evidence estimates overlapping garrison populations, but its generator checks now fail. Neither those estimates nor the mocked 10,000-tick scheduler soak are dedicated-server performance measurements.

## 7. Recommended work sequence

1. **Choose the integration product and mission.** Keep the current Rowan-derived architecture as the starting point; explicitly select which Chicken features to port and which addon identity to ship.
2. **Repair deliverable resources.** Resolve the eight unavailable kit assets, canonicalize stale aircraft paths after native resolution checks, and include the radio media/metadata.
3. **Correct the new force-package/origin paths.** Retain air-origin identity, enforce/document clearance, and unify actual unescorted package budgets/quotes across selection, construction and cleanup.
4. **Restore trustworthy checks.** Repair the shared 13 failing adapters/expectations, reconcile authored geometry/generator checks, and produce evidence pinned to the chosen source state.
5. **Complete native acceptance.** Start with one package per role, then joint waves, persistence/concurrency, and populated dedicated-server play.
6. **Close or explicitly defer campaign gaps.** Outpost stock, live-operation restore, victory flow, unattended setup and optional REAPER isolation need milestone decisions.
7. **Rewrite onboarding docs last**, against the mission and behavior that actually passed acceptance.

## 8. Evidence and reproduction

- [Current JavaScript results](validation/BUILD_AUDIT_2026-11-06_TESTS.json)
- [Rowan snapshot JavaScript results](validation/BUILD_AUDIT_2026-11-06_ROWAN_TESTS.json)
- [Separate Chicken snapshot JavaScript results](validation/BUILD_AUDIT_2026-11-06_CHICKEN_TESTS.json)
- [Tracked resource and historical-evidence audit](validation/BUILD_AUDIT_2026-11-06_RESOURCES.json)
- [Installed WCS/RHS asset/dependency checks](validation/BUILD_AUDIT_2026-11-06_WCS_RHS.json)
- [Focused extra-transport budget probes](validation/BUILD_AUDIT_2026-11-06_PROBES.json)

The audit runners are `tools/Audit-Build-2026-11-06.js`, `tools/Audit-BuildResources-2026-11-06.js`, `tools/Audit-InstalledAssets-2026-11-06.js` and `tools/Audit-ForcePool-2026-11-06.js`. They write their own dated JSON reports; they do not replace the historical release report. Installed-asset paths are specific to this workstation.

Use Node directly, or this workstation's existing VS Code runtime:

```powershell
$env:ELECTRON_RUN_AS_NODE = '1'
& "$env:LOCALAPPDATA/Programs/Microsoft VS Code/Code.exe" tools/Audit-Build-2026-11-06.js | Out-Host
& "$env:LOCALAPPDATA/Programs/Microsoft VS Code/Code.exe" tools/Audit-BuildResources-2026-11-06.js | Out-Host
& "$env:LOCALAPPDATA/Programs/Microsoft VS Code/Code.exe" tools/Audit-InstalledAssets-2026-11-06.js | Out-Host
& "$env:LOCALAPPDATA/Programs/Microsoft VS Code/Code.exe" tools/Audit-ForcePool-2026-11-06.js | Out-Host
```

For snapshot comparisons, export a pinned revision with `git archive`, expand it into a temporary directory, then pass that directory, revision label and report filename to `Audit-Build-2026-11-06.js`. Snapshot tests were run without switching or modifying the user's checkout. Electron emitted Crashpad access-denied diagnostics; suite classification required completion output as well as process status, so those diagnostics were not mistaken for assertions or successful test completion.

Limitations: the broad audit combines full review of the seven-commit functional delta with targeted checks of shared authority, spawning, persistence, economy, UI/resource wiring and the separate Chicken feature interfaces. It does not claim exhaustive symbolic verification of every line, validity of every native API signature, native compilation, gameplay success or performance acceptance. No automatic merge or fixes were performed.
