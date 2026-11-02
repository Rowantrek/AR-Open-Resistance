# Invader QRF framework — Chicken ground responses enabled

Chicken explicitly enables physical Invader ground QRFs for native acceptance testing. The shared script default remains off. The [distant territorial campaign](INVADER_CAMPAIGN.md) still simulates US/USSR battles without physical counter-QRFs; Resistance captures can request the displaced Invader owner's ground response.

A [resource-aware ground planning preview](QRF_SELECTION_PLANNER.md) now evaluates affordable tiers, route/ETA, stock and runtime capacity through a readiness-provider seam. It has no automatic caller, default stock or dispatch side effects. Enabled capture responses use the existing money-backed ground selector.

The new [QRF tactical commander](QRF_TACTICAL_COMMANDER.md) shares report-driven search, delivery/support, withdrawal, retention and finite two-wave logic between Occupiers and Invaders. Readiness-5 Invader responses 3/4/5 split 3+2, 4+3 and 4+4 elements under a six-live-ground-package ceiling; earlier equipment bands have their own smaller recipes. Ground dispatch is enabled in Chicken; compilation and runtime acceptance remain outstanding.

Confirmed against current CLAUDE/ARCHITECTURE and shared role mapping: **Invaders = REDFOR / `AROR_INVADERS`, with USSR assets**. Occupiers remain BLUFOR / `AROR_OCCUPIERS`, with US assets. Vehicles, crew and infantry resolve through their campaign role; native USSR affiliations are converted once at spawning under the existing declared-asset policy.

This prepares the Invader side using the shared scheduler, force profiles, composite ground executor, outpost origin selection, search, faction checks, reservations and cleanup. Chicken's garrisons and territorial campaign supply Invader-held objectives. Each role retains its own recipes; no automatic physical attack target or damage-triggered response is configured.

## Dispatch gate

`AROR_OperationManagerComponent` now exposes **Invader Operations Enabled**, default **off**, explicitly **on in Chicken**. While off, both Invader scheduled attacks and QRF requests return an empty operation ID before creating requests, consuming cooldowns or notifying an executor. This covers commander scheduling, direct trusted calls, damage-report QRF requests and simulated reports. Passive role-scoped combat snapshots can still be recorded; they cannot spawn an Invader response or divert Occupier gunships.

Changing a recipe/profile does not bypass this gate. Invader test and automatic attack target fields stay empty. The commander context still exists; this gate disables operation admission, not faction identity or the existing location-capture system. It does not retroactively cancel operations from a previously enabled session; configuration changes require a fresh Play session. Chicken enables capture triggers for both AI roles; combat and flight responses remain disabled. AI-versus-AI control changes remain simulated and suppress physical counter-QRFs while the territorial campaign is active.

The scheduler smoke runner checks rejection and zero created operations when the gate is off. Its dual-role scheduling tests run only after explicit enabling in a disposable test scenario; it never enables Invaders automatically.

## Faction assets already prepared

| Element | Concrete profile/assets | Readiness |
|---|---|---|
| Motorized | `ussr_motorized`: Ural-4320 transport, USSR crew, light fireteam | Ground executor available; Invader engine acceptance outstanding |
| Mechanized | `ussr_mechanized`: BTR-70 APC, three crew, light fireteam | Ground delivery/support executor available; the BTR is an APC, not a claimed IFV equivalent |
| Armour | `ussr_armour`: BRDM-2 and two crew | Existing wheeled-armour policy; no tank asset introduced |
| Airborne | `ussr_airborne`: Mi-8 transport and Mi-8 gunship | Verified installed resource references; disabled in native random selection and not flight-validated for Invaders |

The existing shared REAPER adapter already chooses the USSR pilot and Invader profiles by role. It provides the same landing, unloading, departure, report-driven gunship support, advisory traffic avoidance and retention paths. Its current air package remains one transport/fireteam plus one gunship, repeated at levels 1-5 when explicitly tested. This configuration does not claim that Mi-8 flight, weapon crew or seating has been verified in-engine. It does not enable mixed ground/air recipes or independent escort ratios.

## Readiness-gated ground escalation

Readiness levels 1–2 use Ural infantry trucks and PKM-UAZ escorts, levels 3–4 introduce BTRs, and level 5 permits the larger combined BTR/BRDM mixes below. Crackdown independently requests response intensity 1–5 within that pool. Profile minimum-readiness gates also reject locked assets in a mislabeled recipe. No new aircraft are enabled. See [QRF_STRATEGY_ECONOMY.md](QRF_STRATEGY_ECONOMY.md) for the full early/middle matrix.

All elements below belong to one operation at readiness 5. Level is the authored tier; the list is not multiplied by level. Each Ural/BTR includes its own fireteam and required crew; each BRDM includes crew only. Reservations use actual configured profile budgets, not the nominal estimates from the general roster CSV.

| Tier / recipe | Ural transport | BTR-70 APC | BRDM-2 | Reserved groups | Reserved personnel slots | Vehicles |
|---|---:|---:|---:|---:|---:|---:|
| 1 / `invader_ground_l1` | 1 | 1 | 0 | 4 | 18 | 2 |
| 2 / `invader_ground_l2` | 1 | 2 | 0 | 6 | 28 | 3 |
| 3 / `invader_ground_l3` | 2 | 2 | 1 | 9 | 40 | 5 |
| 4 / `invader_ground_l4` | 3 | 2 | 2 | 12 | 52 | 7 |
| 5 / `invader_ground_l5` | 3 | 3 | 2 | 14 | 62 | 8 |

All five fit Chicken's configured 20-group/128-slot/10-vehicle ceiling in isolation; other active/pending/retained forces can reduce available capacity. These readiness-5 recipes are deliberately ground-only variants while the mixed land/air parent execution is unfinished, not implementations of the general roster's air-containing combined-arms entries. The previous Invader `combined_arms_l1` Ural/BTR alias remains available for compatibility.

**Invaders Ground QRF Recipe Id** is configured as `invader_ground_l1`. It overrides the shared recipe only for Invaders. Occupiers continue using `combined_arms_l1`. Clearing the Invader override falls back to the shared recipe; clear both relevant recipe selections for a future explicit air test. None of these settings enables Invader operation admission.

## Later territory and factory integration

Territory/garrison ownership supplies trusted defense assignments and stable location IDs. Invader origins must be controlled by the Invader role. The resolver uses the nearest eligible friendly OUTPOST, MILITARY_BASE or AIRPORT and can skip a failed origin on a bounded capture-planning retry. The offshore corridor is excluded. Chicken explicitly requires a friendly land origin, disabling configured-coordinate fallback after territory loss: let the USSR gain a second installation before capturing a site for the first QRF test. Capturing its only land base leaves no usable friendly origin. Stock/readiness-aware origin selection remains planned.

Chicken also opts into a tier-1 capture response without collateral when no paid recipe fits. This applies independently to both AI roles, retains actor/origin/cooldown gates, and records overhead or confirmed losses as ordinary bank spending or saved QRF debt. It does not fund the USSR bank or enable combat-triggered responses. Initial garrisons already remain independent of bank funds.

The future military-resource provider must supply available vehicles, crew, infantry, ammunition/fuel and reserve/commit/release behavior without duplicating Rowan's economy or persistence. Recipes currently reserve executor capacity only. Factories do not generate replacement units through this change. Aircraft readiness, road/LZ suitability and ETA must be checked before automatic selection is enabled. Do not tie resource production to damage callbacks or grant free replacements on cleanup.

## Acceptance status

Configuration checks verify all existing resource GUIDs, role/asset bindings, five Invader ground tiers, aggregate budgets, Chicken's enabled capture-response gate, the shared off default and empty Invader automatic/test target fields. Seven source-adapted scheduler/adapter checks passed with engine dependencies mocked, including disabled attack/QRF/damage requests, zero created operations, Occupier admission and explicit Invader enable behavior. Static checks are not Enforce compilation.

No Invader Play test has been verified. Compile/reload Chicken and start fresh Play. Let the USSR control at least two installations, clear/capture one as Resistance, then verify capture-only admission, funding, friendly origin selection, crew seating, Ural/BTR delivery, full hold, return and cleanup. Test escalating tiers and concurrent Occupier responses within shared executor limits. Dedicated-server and multiclient acceptance remain outstanding. Mi-8/REAPER dispatch remains disabled and requires separate configuration and acceptance.
