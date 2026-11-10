# Town warfare validation — 2026-11-09

Status: **implemented source, native acceptance outstanding**. No Enforce compilation or gameplay test was run.
See [the design and five-mod review](../TOWN_AI_REVIEW.md).

**Latest audit:** [town gameplay and WCS validation](TOWN_GAMEPLAY_VALIDATION_2026-11-09.md).
The tables below preserve the earlier baseline run; its 17 failures are not the current result.

This records the earlier doctrine-validation run. Subsequent spotting and aviation changes are documented in
[spotting and air response](../SPOTTING_AND_AIR_RESPONSE.md); the linked runner JSON now contains that newer run.

## Automated checks

| Check | Result | What it establishes |
|---|---|---|
| `tools/Test-TownDoctrine.js` | 496 assertions passed | Actual-source policy methods under mocked native services: role settings, timing, bounded geometry, contact age/role isolation, damage reports, finite splits, fallback/rejection paths, post-trigger support baseline and terminal cleanup. Includes source integration guards; not every assertion executes engine behaviour. |
| `tools/Test-QRFSourceStructure.ps1` | Passed, 277 scripts | Balanced delimiters, strings, comments and preprocessor directives. This is a lexical check, not an Enforce parser. |
| `tools/Test-ExecutorSettings.ps1` | Passed | Both Everon scenario executor blocks pass 68 range-check terms; their 12 vehicle profiles are checked against 8 presets. Town wave recipe coverage is present for both roles. |
| `tools/Validate-QRFRelease.js` | 17 of 34 suites passed | The complete existing regression runner plus the town suite. Release remains unapproved. |
| Pre-change comparison | Same result for all 33 existing suites | The five changed Enforce files were supplied from Git HEAD to the existing test processes. All 17 broad-suite failures also occur before this change. |
| `git diff --check` | Passed | No whitespace errors in the patch. |

Runtime: VS Code's Electron executable in Node mode (`ELECTRON_RUN_AS_NODE=1`), Node v24.22.1. Its sandboxed
Crashpad initialization prints a diagnostic; the harness requires an assertion report as well as successful
process completion, because this launcher can return zero after an uncaught JavaScript exception.

Detailed results: [current runner output](QRF_AUTOMATED_RESULTS.json) and
[pre-change comparison](TOWN_WARFARE_BASELINE_RESULTS.json). The latter records the baseline commit and per-suite
outcomes; no checkout or user work was replaced to run it.

The executor settings audit retains existing warnings: standalone RHS presets do not supply vehicles, and some
base presets have only one matching transport/support asset. These are configuration limitations, not a failed
range or recipe-coverage check. This change does not certify all faction/content-pack combinations.

## Existing broad-suite failures

Do not treat these as new runtime defects proved by a mock exception, or silently mark them passed. Several
harnesses need to be reconciled with the revised gameplay, shared spawning helpers and faction presets.

| Suite | Observed baseline/current failure |
|---|---|
| Test-MarsAndAttacks | `GetAvailableProfiles` missing from mock context |
| Test-OperationAlerts | Countdown resync expectation fails |
| Test-QRFReleaseLifecycle | `HasPresetVehicles` missing from profile mock |
| Test-QRFInterruptions | `AROR_EUnitType` missing from mock environment |
| Test-QRFMutations | Its interruption-suite baseline fails |
| Test-QRFStrategy | `GetAvailableProfiles` unresolved in translated harness |
| Test-QRFProgression | Starting readiness/tier defaults differ from expectation |
| Test-QRFBoarding | Extracted `ReplaceIntoSeat` method no longer found |
| Test-QRFIngress | `GetMountRejection` unresolved in translated harness |
| Test-EveronQRFIntegration | Capture action expectation is still military-only |
| Test-InvaderCampaign | Expected 27 military locations; town revision removed locations |
| Test-RebIntel | Translation fails at `playerController` |
| Test-MissionLifecycle | `AROR_ForceSpawner` missing from mock environment |
| Test-CapturePlacement | `Math3D` missing from mock environment |
| Test-GarrisonsAndSupport | Extracted `InfantryPrefab` method no longer found |
| Test-GarrisonCrew | `AROR_ResistanceFleet` missing from mock environment |
| Test-PostsAndPads | Extracted `InfantryPrefab` method no longer found |

## Required Workbench and dedicated-server checks

First compile **Game scripts in Script Editor**, then use **Play in World Editor** on `AROR_Everon_Chicken`.
Restart after changing doctrine attributes so an existing commander's session snapshot is not mistaken for new
configuration. Keep a console log for each role and the readiness/crackdown settings used.

| Case | Acceptance evidence |
|---|---|
| Occupier-held town, Readiness 1–2, Crackdown 1 | Three finite responses; 30–90 s alert, 120–180 s intervals; final split when at least 3 elements and two safe roads are available. |
| Invader-held town, Crackdown 3+ | Shorter alert/intervals; two columns may launch from the first wave; total packages equal the recipe limit, not two full copies. |
| Clear and immediately capture a post between kill polls | Relief still queues for its original holder after support flips; a later loss restores the pre-capture support baseline. |
| One safe road / blocked staging / full executor | Single-road fallback preserves the finite wave; rejected launches skip after six attempts; no permanent town-slot deadlock. Capacity exhaustion after a request remains subject to the existing operation spawn timeout. |
| Contact on one responding team | Friendly columns react on the next allowed retask; shock teams flank then close, deliberate infantry retains a covering element where package count permits. Existing security dwell must not delay a fresh report for a full minute. |
| Stop firing and move unseen | After report age and the next allowed retask, contact tasks stop following the old report. Hidden players produce no new report merely by moving. Test infantry and vehicle weapon instigator attribution separately. |
| No report; Resistance owns the town | Teams secure/assault the actual objective; they do not remain at an invented contact outside town. Confirm navmesh reachability on hills, buildings and road approaches. |
| Readiness 4–5 | Air support launches only after ground pressure, observes airspace/capacity constraints and remains tracked until its operation ends. Test failed insertion and ground fallback. |
| Wave lockdown vs attack-order occupation | Town HUD counts and timers agree with the server; support does not also drift from QRF pacification. Test an ordinary QRF alongside a response. |
| Abandonment / army handover / timeout | Outstanding children cancel, no further packages launch, retained survivors obey existing preservation rules, and another town can receive the response slot. |
| Cooldown and recapture | Cooldown kills escalate but do not accumulate the next trigger; newly friendly post troops are not watched as enemy defenders. |
| Extender settings | Disable `m_bFactionDoctrine` and check legacy shared tactics; use supported alternate presets and confirm role-based equipment/affiliation. |
| Dedicated server, two clients and a late joiner | One authoritative set of waves, consistent town ownership/support and HUD, no client spawning, no duplicate notifications, and no UI execution on the dedicated server. Record server frame time with town life and overlapping waves. |

Until those checks are recorded, claims about road driving, actual suppressive fire, simultaneous arrivals,
building entry, helicopter reliability, replication and performance remain unverified.
