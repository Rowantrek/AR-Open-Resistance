# Invader QRF framework — staged, dispatch disabled

A [resource-aware ground planning preview](QRF_SELECTION_PLANNER.md) now evaluates affordable tiers, route/ETA, stock and runtime capacity through a readiness-provider seam. It has no automatic caller, default stock or dispatch side effects; the Invader gate remains off.

The new [QRF tactical commander](QRF_TACTICAL_COMMANDER.md) shares report-driven search, delivery/support, withdrawal, retention and finite two-wave logic between Occupiers and Invaders. Prepared Invader tiers 3/4/5 split 3+2, 4+3 and 4+4 elements under a six-live-ground-package ceiling. This does not enable Invader dispatch; compilation and runtime acceptance remain outstanding.

Confirmed against current CLAUDE/ARCHITECTURE and shared role mapping: **Invaders = REDFOR / `AROR_INVADERS`, with USSR assets**. Occupiers remain BLUFOR / `AROR_OCCUPIERS`, with US assets. Vehicles, crew and infantry resolve through their campaign role; native USSR affiliations are converted once at spawning under the existing declared-asset policy.

This prepares the Invader side using the shared scheduler, force profiles, composite ground executor, outpost origin selection, search, faction checks, reservations and cleanup. It does not place Invader defenders, territories, factories or triggers. The existing Occupier truck/LAV test remains selected.

## Dispatch gate

`AROR_OperationManagerComponent` now exposes **Invader Operations Enabled**, default **off**, explicitly off in Chicken. While off, both Invader scheduled attacks and QRF requests return an empty operation ID before creating requests, consuming cooldowns or notifying an executor. This covers commander scheduling, direct trusted calls, damage-report QRF requests and simulated reports. Passive role-scoped combat snapshots can still be recorded; they cannot spawn an Invader response or divert Occupier gunships.

Changing a recipe/profile does not bypass this gate. Invader test and automatic attack target fields stay empty. The commander context still exists; this gate disables operation admission, not faction identity or the existing location-capture system. It does not retroactively cancel operations from a previously enabled session; configuration changes require a fresh Play session. No Invader gameplay trigger is added here.

The scheduler smoke runner checks rejection and zero created operations when the gate is off. Its dual-role scheduling tests run only after explicit enabling in a disposable test scenario; it never enables Invaders automatically.

## Faction assets already prepared

| Element | Concrete profile/assets | Readiness |
|---|---|---|
| Motorized | `ussr_motorized`: Ural-4320 transport, USSR crew, light fireteam | Ground executor available; Invader engine acceptance outstanding |
| Mechanized | `ussr_mechanized`: BTR-70 APC, three crew, light fireteam | Ground delivery/support executor available; the BTR is an APC, not a claimed IFV equivalent |
| Armour | `ussr_armour`: BRDM-2 and two crew | Existing wheeled-armour policy; no tank asset introduced |
| Airborne | `ussr_airborne`: Mi-8 transport and Mi-8 gunship | Verified installed resource references; disabled in native random selection and not flight-validated for Invaders |

The existing shared REAPER adapter already chooses the USSR pilot and Invader profiles by role. It provides the same landing, unloading, departure, report-driven gunship support, advisory traffic avoidance and retention paths. Its current air package remains one transport/fireteam plus one gunship, repeated at levels 1-5 when explicitly tested. This configuration does not claim that Mi-8 flight, weapon crew or seating has been verified in-engine. It does not enable mixed ground/air recipes or independent escort ratios.

## Authored ground escalation

All elements below belong to one operation. Level is the authored tier; the list is not multiplied by level. Each Ural/BTR includes its own fireteam and required crew; each BRDM includes crew only. Reservations use actual configured profile budgets, not the nominal estimates from the general roster CSV.

| Tier / recipe | Ural transport | BTR-70 APC | BRDM-2 | Reserved groups | Reserved personnel slots | Vehicles |
|---|---:|---:|---:|---:|---:|---:|
| 1 / `invader_ground_l1` | 1 | 1 | 0 | 4 | 18 | 2 |
| 2 / `invader_ground_l2` | 1 | 2 | 0 | 6 | 28 | 3 |
| 3 / `invader_ground_l3` | 2 | 2 | 1 | 9 | 40 | 5 |
| 4 / `invader_ground_l4` | 3 | 2 | 2 | 12 | 52 | 7 |
| 5 / `invader_ground_l5` | 3 | 3 | 2 | 14 | 62 | 8 |

All five fit Chicken's configured 20-group/128-slot/10-vehicle ceiling in isolation; other active/pending/retained forces can reduce available capacity. These are deliberately ground-only variants while the mixed land/air parent execution is unfinished, not implementations of the general roster's air-containing combined-arms entries. The previous Invader `combined_arms_l1` Ural/BTR alias remains available for compatibility.

**Invaders Ground QRF Recipe Id** is staged as `invader_ground_l1`. It overrides the shared recipe only for Invaders. Occupiers continue using `combined_arms_l1`. Clearing the Invader override falls back to the shared recipe; clear both relevant recipe selections for a future explicit air test. None of these settings enables Invader operation admission.

## Later territory and factory integration

When that work is ready, territory/garrison ownership supplies trusted defense assignments and stable location IDs. Invader origins must be controlled by the Invader role. The current resolver uses the nearest eligible friendly OUTPOST, with configured coordinates as a development fallback; stock/readiness-aware origin selection is still planned.

The future military-resource provider must supply available vehicles, crew, infantry, ammunition/fuel and reserve/commit/release behavior without duplicating Rowan's economy or persistence. Recipes currently reserve executor capacity only. Factories do not generate replacement units through this change. Aircraft readiness, road/LZ suitability and ETA must be checked before automatic selection is enabled. Do not tie resource production to damage callbacks or grant free replacements on cleanup.

## Acceptance status

Configuration checks verify all existing resource GUIDs, role/asset bindings, five Invader ground tiers, aggregate budgets, the disabled dispatch gate and empty Invader target fields. Seven source-adapted scheduler/adapter checks passed with engine dependencies mocked, including disabled attack/QRF/damage requests, zero created operations, Occupier admission and explicit Invader enable behavior. Static checks are not Enforce compilation.

No Invader Play test has been launched. After future approval to enable it in a disposable scenario, verify ownership/trigger admission first, then Ural/BTR tier 1, escalating ground tiers, losses/cleanup and finally individual Mi-8 transport/gunship behavior. Dedicated-server and multiclient acceptance remain outstanding. Leave the gate off until that phase is requested.
