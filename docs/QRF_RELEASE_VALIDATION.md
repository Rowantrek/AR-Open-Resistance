# Chicken QRF and attack release validation

**Decision: current tier-one/linear-balance changes require compilation and native gameplay acceptance.** The user reported successful compilation of the preceding full-cabin, boarding and USSR origin changes on 28 October 2026. The subsequent balance changes on 29 October 2026 have not been compiled or played by this audit. Current driving, flight, actual landings and packaged dedicated-server replication remain unverified. No route is labelled as driven or flown.

Scope: Chicken only, both enemy roles, ordinary capture QRFs, trusted attacks, automatic Occupier attacks and MARS. The first audit started from `6c03716ff3cdc573f56b8f428f436c249afc1d52`; this follow-up starts from `3614c1b` on `chicken-dev`. Historical evidence is retained in Git. The requested work budget was two hours; simulated ticks and assertion counts are not elapsed native gameplay time.

## Evidence and repeatable checks

Current source also targets a saved 30% running airborne/joint share of ordinary dispatched QRFs per army. It excludes failed preparation and explicit/debug/scripted requests, and falls back to ground when aviation is not feasible before funding/construction. Policy serializer version 4 retains versions 1-3 with empty historical mix counters. The new actual-source suite covers routing, exact running quotas, duplicate/reentrant accounting and save migration. Native compilation and Play acceptance of this change remain pending.

The [tier-one/linear-balance follow-up](QRF_REVIEW_FOLLOWUP.md#tier-one-start-and-linear-response-balance--29-october-2026) starts new campaigns at 0% Crackdown, adds five percentage points per recorded capture loss, and adds one complete package per tier within every ground/joint band. Prices, personnel budgets, lead/reserve counts and response previews reflect the smaller upper-tier recipes. Equipment Readiness, full native passenger loads and all boarding/exit safety checks are retained. Existing saved escalation is preserved.
[Native Vostok/Crow Hill failure](QRF_RETALIATION_FAILURE.md): Workbench confirmed that attributed script initializers overwrote recipe/profile resource values and disabled the executor. Those initializers are removed; script-created fixtures use explicit factories. Retaliation banners now require actual deployment rather than request acceptance. The fix needs a fresh compile and full Chicken reload; native deployment success is not yet verified.

The [combined reconciliation](RECONCILIATION_VALIDATION.md) includes the new stronghold/garrison/map content. Its five additional mutations cover scripted target conflicts, coastal recovery, dispatch receipts, retirement kill attribution and forced-wave integration. New stronghold resource metadata is hand-authored and needs native discovery.

The six review issues and their dispositions, shipped settings and native reproductions are recorded in [Rowan review follow-up](QRF_REVIEW_FOLLOWUP.md). Four additional mutations cover active dispatch failure, reserve boarding deadlines, unknown-removal classification and banner estimate consistency. Long Cyrillic/proportional-font rendering remains visually unverified.

Current combined result: **33 suites / 99,749 mocked assertions**, including **57 detected source mutations** and the 10,000-tick simulated scheduler soak. The original 27-suite / 95,936-assertion evidence remains in Git history. The current lexical check covers 216 addon scripts and is not an Enforce compilation.

The [full-load follow-up](QRF_REVIEW_FOLLOWUP.md#full-ground-transport-loads) fills native ground passenger capacity within updated reserved ceilings and routes passenger-carrying light/armoured profiles through delivery and infantry unloading. Driver/gunner crews remain separate. Prices and response coverage reflect the larger reservations. Later native logs confirm ground boarding with 15 occupants per truck and five per Humvee; delivery remains unverified. Other physical occupancy counts remain unverified; `EstimatedPersonnel` in the price CSV is `NOT_RUN`. Fewer affordability-loop assertions execute because larger full-load quotes exceed more sampled balances; all 50 funded army/Readiness/Crackdown combinations still pass.

The [armed-vehicle crew follow-up](QRF_REVIEW_FOLLOWUP.md#armed-vehicle-crew-follow-up) addresses the reported empty M2 Humvee gunner seat: every ground turret/attached-weapon seat now requires assigned living AI before dispatch. The observed gunner-only version compiled successfully, then a truck and Humvee failed native boarding and timed out. Later full-load ground boarding succeeded; the reported airborne run still timed out before dispatch. The current revision fills helicopter cargo seats, gives asynchronous requests distinct destinations, and budgets the full preparation window. Compilation passed, user-reported; native Play acceptance is required. See the airborne/USSR follow-up for log evidence and remaining uncertainty.

- [Automated results and source hashes](validation/QRF_AUTOMATED_RESULTS.json).
- [All 350 configured response cells](validation/CHICKEN_RESPONSE_COVERAGE.csv): two armies × seven families × five Readiness levels × five Crackdown levels. 280 cells resolve an available response; the remaining 70 are intentionally locked. Availability does not establish affordability, current origin ownership, free live capacity or successful native deployment.
- [All 924 directed origin/target pairs](validation/CHICKEN_OPERATION_ROUTES.csv), [placed locations](validation/CHICKEN_OPERATION_LOCATIONS.csv) and [generation metadata](validation/CHICKEN_OPERATION_ROUTES.json). These are authored endpoints, not a reconstruction of Everon's complete road graph.

A standalone `node` command is not on PATH. The first audit used the tool V8 host with source snapshots and `fs`/`path` adapters. This follow-up also executed the actual separate-process Node runner using VS Code's installed runtime: **Node v24.21.0 / Electron 43.7.7 in Node mode**. All 29 suites, including the Node `vm` mutation adapter, passed against filesystem sources. Enfusion services remain mocked. Electron emitted a nonfatal Crashpad-registration access-denied diagnostic; the runner exited 0 and every child suite passed.

```powershell
node tools/Validate-QRFRelease.js
node tools/Test-MarsAndAttacks.js --export-coverage
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Test-QRFSourceStructure.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Test-ForcePoolAssets.ps1 -ResourceDatabasePath '<base-game resourceDatabase.rdb>'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Test-ReaperAssets.ps1 -ResourceDatabasePath '<REAPER resourceDatabase.rdb>'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Export-ChickenOperationRoutes.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Test-QRFRouteImport.ps1
git diff --check
```

On this machine the equivalent Node invocation used the existing VS Code installation, without installing another runtime:

```powershell
$env:ELECTRON_RUN_AS_NODE = '1'
& "$env:LOCALAPPDATA/Programs/Microsoft VS Code/Code.exe" tools/Validate-QRFRelease.js | Out-Host
# Also supports tools/Test-MarsAndAttacks.js --export-coverage.
```

Installed-resource validation covers 27 base-game GUIDs, navigation bindings, 12 faction profiles, all 62 recipes and the 50 campaign army/Readiness/Crackdown budget combinations. All five REAPER pilot/flight-waypoint references match the installed REAPER database. These checks do not validate native group materialization, weapon behavior or aircraft control. Source-structure checks are lexical checks, not compilation.

The three MARS, Black Market and setup layout GUIDs match their metadata, script/config references and the current local Workbench resource database. Their metadata remains hand-authored; actual widget loading and rendering remain unverified. Repeat that check after resource discovery with `tools/Test-OperationUIResources.ps1`.

## Confirmed defects fixed

| Area | Failure before the change | Correction and evidence |
|---|---|---|
| Airborne boarding | Accepted seat requests immediately marked the package mounted; partial boarding could also fail instead of waiting for asynchronous occupancy. | Both pilots must occupy their own pilot compartments and every passenger must occupy the transport. Initial teleports permit paused simulation. Repeated checks preserve players, occupants and other actors' reservations. The existing spawn deadline bounds waiting. Release lifecycle regressions exercise delayed and partial occupancy. |
| Aircraft readiness | A package could lose a pilot, passenger or aircraft while waiting for the other lead elements, leaving a stale ready flag. | Dispatch rechecks aircraft health, living personnel and verified boarding before releasing physics, issuing flight orders or repositioning either aircraft. Seven regressions cover loss and healthy dispatch. |
| Attack rally | `BeginStaging` issued rally orders, but no ground update marked arrivals or called the release path. Route recovery could bypass the intended rally. | Ground tactical updates require proximity and low speed, wait for all staged packages or the deadline, and release each package once. Withdrawal takes priority. |
| Defender intel | The unused assault helper calculated a centroid directly from hidden defender world positions. Activating the rally path would have exposed those positions. | Assault intel uses only the existing role-, vicinity- and age-filtered combat-report authority. No fresh report means unknown defenders. |
| Town pacification | The support policy existed without a call path and compared the army only against Resistance forces. | The shared five-second commander timer calls the settlement authority once per operation after physical occupation. All hostile military roles count against the occupying army; ties and withdrawal cannot advance support. |
| Attack eligibility events | A subscriber could synchronously resolve an ACTIVE capture QRF before the territory policy observed ACTIVE. | Record eligibility before invoking subscribers. Nested and duplicate terminal callbacks retain one completion timestamp. |
| Debug combat evidence | Damage to a MARS force used the normal strategic-evidence default, despite debug dispatch bypassing campaign scaling. | Debug damage retains a contact snapshot but explicitly suppresses Crackdown evidence. Ordinary campaign damage still escalates. Debug actors can still affect the physical world and canonical territory; MARS is not an isolated simulation. |
| Debug authority | A shipped MARS credential and an enabled admin menu were sufficient to request debug effects. | `AROR_AdminAccess` requires administrator/session-administrator roles in packaged sessions. MARS and all six admin server mutations enforce it. Menu actions and teleport selection recheck local access. Workbench retains testing access. Native permission replication still needs acceptance. |
| Scheduler input | An unsupported operation-type enum could create a record never processed by either scheduler pass. | Reject unsupported types before allocating an ID, cooldown or operation slot. |
| Withdrawal | A contested friendly installation could be selected as a safe replacement origin. | Contested sites are excluded; no eligible destination leaves safe recovery false. |
| Configuration | Newly active attack staging, town support and origin-cost parameters were missing from tactical validation. | Validate their documented ranges before tactical execution and native route probes. |
| Harnesses | Missing dependencies, obsolete UI bindings/wording, stale serializer expectations, incorrect reference initialization, an unhandled boolean parameter and strict prefab rounding assumptions obscured results. | Repair the harnesses while retaining behavioral checks. Layout checks now cover grouped Workbench entities and millimetre authoring precision. |

The scheduler soak uses 10,000 **simulated** ticks, 2,935 admitted/deduplicated requests, 27,065 rejected requests and 9,349 notifications. It checks global attack capacity, per-role QRF capacity, conflicting targets, bounded terminal history and nonrecursive notification delivery. It is not a real-time server performance or network soak. Many assertions in the lifecycle suite are repeated state invariants; the total is not a count of distinct gameplay scenarios.

**43 intentional source mutations** are detected against isolated snapshots without editing game sources. The original ten cover paused boarding, unready reserves, staging, support, eligibility ordering, debug escalation, authorization, collateral release, group budgets and operation admission. Twenty-one follow-up mutations cover withdrawal dispatch, foreign drivers, pre-dispatch retention, origin/clearance rechecks, funding waits, player occupancy, duplicate overhead, closed-account collateral, unbounded exit retries, nested finance reconciliation, reinforcement timing, partial-hull holds, empty construction, unused package margin, virtual ownership handoff, escort ownership, preserved player-group queues and unissued retirement orders. The follow-up also replaced a coupled budget expectation with an independent physical-group/vehicle expectation. The actual Node `vm` adapter detected all 43; earlier V8-host results remain historical evidence.

The [site-readiness audit](validation/CHICKEN_SITE_READINESS.csv) includes all 27 military locations in activation-overlap estimates and reports four legacy-catalogue transform differences: Pinewood Lake, Chotain Industrial, Old Wood and Andres Beacon. The world geometry was preserved. Resolve catalogue/generator drift before regenerating location layers. Peak overlaps now estimate 112 garrison personnel, so capacity pacing and populated-server performance need particular attention. The Invader policy model now reads actual placed military transforms and all three ports, rather than the older 23-site subset.

## Follow-up interruption and route audit

The second source audit adds the following corrections. These changes need a fresh native compile and Play test.

| Area | Confirmed gap | Correction |
|---|---|---|
| Ground readiness | An occupied driver slot did not establish that its occupant belonged to the assigned AI crew; readiness could also become stale before dispatch. | Require an assigned AI driver and current mount completion, living crew, healthy hull, owned groups and valid waypoints before issuing orders. |
| Queued waves | The committed origin and air launch volumes could become unsafe after planning, before a later wave spawned. | Recheck launch-site ownership/type/contention and current player/airspace clearance immediately before construction. |
| Withdrawal | Newly ready reserves could still receive deployment orders after withdrawal began. | Stop activation and cancel unissued ground/air packages; retain physical actors and release unbuilt quotes through existing authority. |
| Pre-dispatch cancellation | Materialized actors could be deleted immediately because no orders had been issued yet. | Retain materialized tactical ground forces and aircraft using normal age/player-distance gates. Empty reservations release immediately. Held uncrewed aircraft remain held to avoid an artificial crash; direct player occupancy releases their simulation. |
| Aborted escort retirement | The retirement queue could issue an escort egress order even though its held package had never dispatched. | Limit retirement flight orders to dispatched packages. Unissued aircraft retain their boarding hold and receive no egress order. |
| Failed exits | A fixed security task could repeatedly renew its passenger-exit deadline until the manager's much longer timeout. | Default to two additional exit windows, then withdraw that package. Tests exercise limits 0, 1, 2 and 5; no landing or exit clearance checks are weakened. |
| Reentrant finance | A synchronous Economy subscriber could enter reconciliation while the outer pass still owned the closed-account array index. | Guard nested reconciliation; tests reproduce the invalid second removal when the guard is intentionally removed, for both armies. |
| Partial-package collateral | Failed construction could consume its virtual reservation without creating actors, or leave unused personnel margin reserved after final cleanup while other waves continued. | Release empty attempts once. Attribute assets to package indices and release only the unused margin after owned spawn queues are cancelled. Virtual proxies and escort children cannot finalize the parent package. Preserve collateral if a player-owned group prevents queue cancellation. |
| Heavy air reserves | Chicken's 300-second reserve deadline equalled the 300-second gunship support duration. With a six-aircraft live cap, the fifth heavy package could lose its opportunity before initial gunships completed egress. | Extend Chicken's finite reserve window to 450 seconds, leaving modeled support/egress headroom without raising the live cap. Native flight/egress timing still needs acceptance. |
| Multi-profile selection | An earlier heavy profile could hide a later light profile; an unfunded pool could be mistaken for invalid configuration. | Inspect all enabled/unlocked candidates, choose an affordable eligible profile, and preserve the bounded funding-wait path when no candidate is affordable. |

The interruption suite runs actual source methods with native factories, physics, compartments, geometry and Economy mocked. It injects transport/gunship resource failures, missing physics/groups, oversized infantry queues, failed waypoints and changed clearance. It also checks both armies across all five Readiness levels with funded/unfunded/debug selection, direct player occupancy, origin loss during preparation and active reinforcement, and finite exit recovery.

Funding checks cover cancellation before activation, repeated activation, duplicate asset registration, healthy retirement, confirmed losses, external spending, debt and closed-account removal. The 894-assertion interruption suite also runs actual tracking and final cleanup against missing hull/pilot references, confirmed pilot deaths, player-preserved vehicles and player-owned groups for both armies. Partial cleanup must preserve the next wave's full allocation and the value of physical assets still in the world. Historical registered values prevent a settled loss from being deducted twice when unused margin is released. Healthy cleanup releases collateral without adding money; overhead and confirmed losses are charged exactly once. Nested Economy callbacks must not remove a closed account twice or duplicate its charges. No test establishes native spawn-queue cancellation or physics behavior.

Route recovery was traced through the existing four-retry/30-second search pacing, ingress deadlines, native foot-path checks, landing rejection rules and actual-return requirements. Existing route/landing regressions remain part of the combined run. No road graph, vehicle turning clearance or physical foot route was inferred from endpoint coordinates.

Native reproduction steps are in [light-response acceptance](QRF_LIGHT_RESPONSE_ACCEPTANCE.md). The first audit's 27-suite/95,936-assertion evidence remains available in the preceding Git revision; the linked automated artifact contains current follow-up results.

## Joint airborne and ground responses

`AIR_GROUND = 7` remains the joint response family. Existing numeric values, original profile IDs and heavy recipe IDs are retained. Chicken enables `m_bAllowJointResponses`; the shared default remains off. Joint execution requires both airborne execution and tactical coordination.

### Equipment and intensity

| Readiness | Standalone airborne | Joint response equipment |
|---|---|---|
| 1 | Unescorted transport with a fireteam and one pilot | Unavailable |
| 2 | Unescorted transport packages | One unescorted air package plus trucks/light support |
| 3–4 | Unescorted transport packages | One unescorted air package plus truck/APC/IFV/light-support combinations |
| 5 | Escorted transport/fireteam/gunship packages | Existing escorted air package plus heavier ground combinations, including armour |

`us_airborne_light` and `ussr_airborne_light` unlock at Readiness 1. Each reserves two groups (infantry and pilot), 32 personnel slots and one aircraft. The 32-slot reservation is a conservative limit; usable native cargo capacity determines the infantry queue. The existing fireteam and transport resources are reused; no new asset GUIDs are introduced. At Crackdown 1 the light package requires 2,460 US or 2,660 USSR funds including overhead. Crackdown 1–5 controls the number of complete standalone packages.

The shared preview chooses the strongest unlocked profile. Automatic airborne execution also checks funding and can fall back to a light profile; if none can be funded, it retains the bounded spawn-wait path. It does not stop at an earlier locked profile in the catalogue. Legacy explicit-profile MARS requests retain their existing behavior without a simulated equipment axis.

The normal capture/attack campaign selector still uses authored ground/joint recipes. Standalone airborne is selected through MARS, an explicit profile request or an enabled scenario airborne override; adding a Readiness-1 profile does not silently replace the campaign's early ground responses.

Each joint recipe starts with one airborne package, followed by its ground elements. There are now **30 joint recipes**: five tiers in each of three equipment bands for each army. The complete Chicken catalogue contains **12 profiles and 62 recipes**. An authored list is deployed once; Crackdown never multiplies it again.

### Early joint recipes

All rows below include one unescorted transport/fireteam package.

| Readiness | Tier | Ground elements | Lead / reserve packages | US / USSR funds required |
|---|---|---|---:|---:|
| 2 | 1 | truck | 2 / 0 | 3,210 / 3,410 |
| 2 | 2 | truck + light support | 3 / 0 | 3,720 / 3,820 |
| 2 | 3 | truck + truck + light support | 2 / 2 | 4,480 / 4,580 |
| 2 | 4 | truck + truck + truck + light support | 3 / 2 | 5,240 / 5,340 |
| 2 | 5 | truck + truck + truck + light support + light support | 3 / 3 | 5,750 / 5,750 |
| 3–4 | 1 | truck | 2 / 0 | 3,210 / 3,410 |
| 3–4 | 2 | truck + APC/IFV | 3 / 0 | 4,320 / 4,420 |
| 3–4 | 3 | truck + APC/IFV + light support | 2 / 2 | 4,830 / 4,830 |
| 3–4 | 4 | truck + APC/IFV + APC/IFV + light support | 3 / 2 | 5,940 / 5,840 |
| 3–4 | 5 | truck + truck + APC/IFV + APC/IFV + light support | 3 / 3 | 6,700 / 6,600 |

The early IDs are `occupier_joint_r2_l1`–`l5`, `invader_joint_r2_l1`–`l5`, and the corresponding `*_joint_r3_l1`–`l5` bands. Readiness 3–4 permits mechanized elements; their presence still depends on the authored Crackdown tier. Genuine armour and gunships remain gated to Readiness 5.

### Heavy joint recipes at Readiness 5

Every row includes one transport/fireteam/gunship package.

| Tier | Ground elements | Lead / reserve packages | US / USSR funds required |
|---|---|---:|---:|
| 1 | Truck | 2 / 0 | 5,210 / 5,410 |
| 2 | Truck + APC/IFV | 3 / 0 | 6,320 / 6,420 |
| 3 | Truck + APC/IFV + armour | 2 / 2 | 7,430 / 7,080 |
| 4 | Truck + 2 APCs/IFVs + armour | 3 / 2 | 8,540 / 8,090 |
| 5 | 1 truck + 2 APCs/IFVs + armour + light support | 3 / 3 | 9,050 / 8,500 |

### Shared execution and compatibility

One operation owns funding, virtual reservations, tactical roster and outcome. Required lead packages must be physically ready before dispatch. Ground and airborne reserves share construction pacing. Joint attacks hold transport insertion at the air approach until ground assault release; ordinary joint QRFs retain immediate insertion. Optional escorts do not allocate phantom groups, aircraft, pilots or replacement charges.

At equal authored tier the campaign selector prefers an eligible, affordable joint recipe when enabled. It can retain a higher-tier ground recipe or select an affordable ground alternative. Emergency unsecured capture responses remain ground-only. Ground stock-provider previews exclude joint recipes because their provider does not assert aircraft availability.

This implementation supports one transport and zero or one gunship per airborne package, and one airborne package per joint recipe. Independent H/G counts, shared gunships, specialist anti-armour loadouts and the complete proposed [40-entry design roster](QRF_RESPONSE_ROSTER.md) remain outside implemented coverage. The design CSV is not an executable catalogue.

**Extender impact:** airborne `m_iEscortCount` now supports 0 or 1. `GetGroupBudget()` returns `2 + escortCount`; `GetVehicleBudget()` returns `1 + escortCount`. `DispatchReadyPackage` and `StartJointResponse` remain executor hooks. Ordinary recipes remain ground-only; joint recipes require airborne-first ordering. New `m_iQRFExitRetryLimit` allows 0–5 additional exit windows and defaults to 2. Existing funding `Register_S` remains compatible; core tracking uses `RegisterPackageAsset_S`. Custom constructors/cleanup must preserve unique package indices and one owner for the frozen package quote, and release unused margin only after their spawn queues are cancelled. No save-format change is introduced; live operations remain session-only.

Town security applies the configured support policy, whose default is six percentage points per minute while its conditions hold. That earlier behavior change and the new low-Readiness force availability require native balance acceptance.

## Native route sweep

Chicken has 45 placed locations and 21 possible military launch sites: **924 directed non-self pairs**, including **546 military attack pairs** and **63 port-target pairs**. Both responding armies can acquire different origins during a campaign, so the matrix includes prospective ownership states. Its preferred-origin-distance flags derive from the actual source/layer configuration, currently 600 m. Endpoint height differences cannot reveal intermediate slopes, bridges, water crossings or obstacles.

After Script Editor compilation, run an idle disposable Chicken campaign and invoke:

```c
AROR_QRFRouteAudit.Start_S();
// To cancel:
AROR_QRFRouteAudit.Stop_S();
```

Use the existing Script Editor/debug invocation mechanism for static smoke tests. The audit is Workbench-only and server-only. Do not use it as a background task during combat. It stops when a gameplay operation becomes outstanding and clears its timer/references on executor deletion.

The sweep first advances the production five-pad LZ search for each target at no more than two radial seeds per tick. It then performs one directed pair per second using the production bounded ground launch/approach planners and both transport-only and escorted airborne formation sizes 1–5. Escort headings follow selected pads; cached pads are rechecked. At the current 121-candidate limit the worst case is roughly 45 minutes for LZ searches plus 15 minutes for directed pairs, plus scheduler/query delays. It does not spawn actors, spend money, change territory or give AI orders; navmesh tiles may be loaded by existing foot-dismount checks.

Keep the log and import it separately:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Import-QRFRouteAudit.ps1 -LogPath '<Workbench log>'
```

The importer uses the latest audit start, checks topology counts, rejects unknown origin/target IDs and preserves missing results as `NOT_RUN`. Ground planning, escorted-air planning, light-air planning and pad counts are separate columns; missing safe landing sites produce `NO_SAFE_LZ` for both air modes. Older logs without light-air results retain `NOT_RUN` for that column. It never marks driving, actual landing or dedicated-server checks passed. Synthetic fixtures are written only to temporary files; no synthetic planning result is included as native evidence.

A planning PASS is a snapshot for one ground package and the configured air launch formations. It does not establish space for a complete ground convoy, a navigable infantry route from every outer LZ, vehicle turning clearance, bridge suitability, gunship engagement or successful travel. Player clearance, loaded tiles, garrisons and dynamic obstructions can change results. Repeat failures after tiles load and with player bodies away from the origin; moving the Game Master camera alone does not change player clearance. Archive the current source hashes with the native log.

## Release acceptance still required

| Gate | Required evidence | Current result |
|---|---|---|
| Compilation | Compile current tier-one/linear-balance changes; verify packaged and Workbench variants. | NOT_RUN for 29 October balance changes; preceding full-cabin version passed, user-reported on 28 October 2026 |
| Resource loading | Discover layouts and metadata; reload Chicken and check widget loading. | NOT_RUN |
| Memory-only native smoke tests | Existing operation/pool/strategy/tactical helpers plus `AROR_JointResponseSmokeTests.Run_S()`. | NOT_RUN |
| Route planning | Sweep all 924 pairs and 45 landing targets; triage failures, especially ports. | NOT_RUN |
| Ground travel | Both armies, every authored recipe tier, driver/passenger seating, full lead wave, convoy spacing, dismount, infantry arrival and return. | NOT_RUN |
| Air travel | Both armies, package counts 1–5, touchdown/normal exits, gunship support, alternate LZs, egress, retention and live-aircraft cap. | NOT_RUN |
| Joint responses | Both armies and tiers 1–5 as capture QRFs and attacks; shared lead barrier, ground rally/air hold, reserve loss, and canonical capture. | NOT_RUN |
| Military attack lifecycle | Grace/recovery/rest boundaries, failed-spawn exclusion, one global attack, target deletion/change, loss of origin and insufficient funds. | NOT_RUN |
| Settlement security | Support-only control, physical presence, both hostile armies, no hidden-position targeting and no per-package support multiplication. | NOT_RUN |
| Multiplayer authority | Dedicated server, admin and ordinary client, rejected raw debug requests, role revocation, reconnect/JIP and matching script/layout versions. | NOT_RUN |
| Save compatibility | Versions 1/2/3, funds/debt/Readiness/Crackdown restoration; documented session reset of operations and attack eligibility. | NOT_RUN |
| Runtime cleanup/performance | Cancellation during materialization, dormant groups, late agents, player takeover, aircraft loss, nearby bodies/loot and a populated multi-hour session. | NOT_RUN |

Prioritize Lamentin Port from Sawmill and Old Wood, North Ridge, all three ports, the longest directed pairs, low-elevation endpoints, nearby origins and routes with large endpoint height differences. Include road barriers/bridges, steep approaches, narrow entrances, moving obstructions and outer-pad infantry approaches. Those terrain features need native observation; this audit does not infer them from straight-line geometry.

Use a disposable fixture with shortened attack timers for boundary tests, then restore 900-second capture grace, 300-second post-QRF recovery and 900-second rest and observe the real cadence. An arrival without ownership change must not succeed. A spawn failure must not unlock attacks. Save/reload must not fabricate a historical capture QRF. No safe origin, insufficient capacity or an unsafe pad must produce a finite refusal/failure with reconciled reservations.

A two-hour session can cover the planning sweep and selected physical regressions. It cannot honestly certify every dynamically obstructed route and every response in multiplayer. Preserve the untested cells and continue native acceptance before labelling Chicken release-ready. The experimental REAPER dependency remains required; battlefield loot, live-operation persistence and physical military stock retain their documented limitations.
