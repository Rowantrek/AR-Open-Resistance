# Town gameplay and WCS validation — 2026-11-09

Implemented source on `chicken-dev`; **native acceptance outstanding**. No Enforce compilation or gameplay was run.
This is a development commit, not a release certification. The [earlier baseline](TOWN_WARFARE_BASELINE_RESULTS.json)
records the initial 17 broad-suite failures before the town changes. Several harnesses have now been reconciled
with the revised APIs; the two remaining failures are reported below.

## Evidence

Run `node tools/Validate-TownGameplay.js` from the repository root. Here, VS Code's bundled Electron ran in Node
mode (`ELECTRON_RUN_AS_NODE=1`). The runner requires completion output and rejects exceptions; exit status alone
is insufficient with that host. It writes [the complete audit](TOWN_GAMEPLAY_RESULTS_2026-11-09.json), and invokes
the existing [gameplay runner](QRF_AUTOMATED_RESULTS.json), which records source hashes and full failure output.

| Check | Result and scope |
|---|---|
| Gameplay regression | 33 of 35 suites pass. Exact assertion total is in the current runner output. Actual-source methods under mocked native services, source/configuration guards and bounded policy simulations. |
| Town doctrine | 496 assertions: roles, tempo, split budgets, fallback, report freshness, post-trigger baseline, terminal cleanup and finite launch attempts. |
| Spotting and aviation | 89 assertions: LOS/confirmation, map suppression, seven glyphs, stale snapshots, candidate roles, boarding exclusions, air requests, gunship timers, planning release and AI-versus-AI attack preservation. |
| Lifecycle / interruptions | 20,588 / 958 assertions, including incomplete construction, pilot/passenger occupancy, full joint reservations, cancellation, finance and retained actors. |
| Scheduler soak | 60,024 assertions over 10,000 deterministic mock ticks; bounded admission/history and nonrecursive callbacks. This is not an engine performance test. |
| Mutation checks | 60 isolated source mutations detected after every participating harness passes its unmodified baseline, including lockdown passengers, AI-versus-AI hostility and gunship station time. The WCS suite also exercises a priority-disabled negative control. No working source is mutated. |
| WCS assets | 2,941 unique prefab GUID/path references exactly match installed resource indexes in the declared dependency tree; no missing declared dependencies. This does not load native prefabs or inventories. |
| WCS selection | Actual preset, loadout, attack-profile and shared mission selector bodies against ID-merged WCS/RHS configs; preferred hulls, role isolation, readiness kits, disabled offerings, replacements and fallback. Exact count is in the audit output. |
| Resource bindings | All five REAPER pilot/flight-waypoint references match its installed index; UI metadata/script/layout bindings pass. |
| Source/configuration | Lexical balance across 277 scripts; 68 executor range terms and 12 profiles against 8 presets for each Everon scenario. Not an Enforce parser/compiler. |

**Unresolved:** `Test-GarrisonsAndSupport.js` and `Test-PostsAndPads.js` fail while extracting the removed
`InfantryPrefab` method. They require migration to the preset/unit-type garrison API. They are not skipped or
counted as passes; the aggregate audit exits unsuccessfully. Passing crew, layout and outpost suites do not
replace these missing garrison regressions. The historical baseline contains both failure names.

Existing executor-audit warnings remain for standalone RHS presets with no vehicle lists and single-hull base-game
presets. Use the combined addon for WCS/RHS attacks; this does not certify every optional-addon combination.

## Corrections found by validation

- Helicopter passengers no longer contribute to town lockdown while seated in an aircraft. Grounded,
  disembarked combatants enter the existing faction count.
- Campaign attacks on another hostile AI army survive the scheduler's target check. Debug requests retain their
  Resistance-target restriction; success still requires canonical ownership to change.
- Combined WCS/RHS presets prefer offered WCS combat hulls. A Russian motorized assault prefers a covered troop
  Ural over an Arsenal logistics body. Missing WCS types retain same-army fallbacks.
- Four low-Readiness Crackdown tier-2 recipes no longer exceed their tier-3 personnel. Their initial combined-arms
  minimum creates a plateau at tiers 1–2, then finite growth. Actor/funding/capacity limits still apply.
- Gunships receive their full support window near town; distant ingress uses the calculated flight deadline.
  Station time cannot restart after expiry or withdrawal. Delivered infantry and expired planning workspaces
  cannot permanently reserve the sky.
- Boarding regressions now check that refused seating preserves the original army's soldiers and kits instead
  of expecting vanilla default-occupant replacements. Native seat acknowledgement remains required.

## WCS vehicles and equipment

The [effective asset matrix](ATTACK_ASSET_MATRIX_2026-11-09.json) comes from the actual selectors with the installed
BASE_GAME, RHS and WCS offerings enabled. The [resource audit](WCS_ASSET_AUDIT_2026-11-09.json) contains exact refs,
dependency evidence and authored entries filtered out of the enabled catalogue.

| Role | Preferred combat vehicles / aircraft | Infantry equipment |
|---|---|---|
| Occupiers / BLUFOR | WCS FMTV, JLTV, Stryker family, MH-6M transport and AH-6M gunship | Complete RHS FORECON kits with WCS MK153 AT and Stinger specialist gear/ammunition. |
| Invaders / USSR | WCS armed UAZ, BTR KBA105, Mi-24 transport, Mi-24/Ka-52 gunships | Complete RHS MSV kits; AA specialists receive a WCS Igla and its WCS 9K39 ammunition. |

WCS does not supply every selected Russian cargo/static type. The covered Ural is a deliberate same-army fallback.
This is a mixed WCS/RHS loadout, not a claim that every soldier carries a WCS small arm. Standard rifle squads do
not automatically include AA specialists. REAPER pilots retain the author's dedicated pilot bodies and flight kits.

Aircraft preference follows the families explicitly named by the
[REAPER WCS Workshop description](https://reforger.armaplatform.com/workshop/69A4D664A6284E06-REAPER_AiHelicopters-WCS).
Mi-8 variants remain a lower-priority fallback in the combined preset, requiring separate native acceptance if the
preferred aircraft packs are disabled. Listing a supported family is not proof of a successful flight in this mode.

Load `OpenResistanceWCSRHSCompat` and its dependencies and select the combined enemy presets. Its defaults apply
to new campaigns. **Existing saves retain valid selected armies**; a saved vanilla army is not silently migrated.
Disabling packs/groups can remove preferred vehicles or leave a required type unavailable.

## Reference review

The [five-mod review](../TOWN_AI_REVIEW.md) was rechecked against the authors' Workshop descriptions: RC Dynamic
Counter Attacks, DCO Extensions, CRX Enfusion AI, REAPER Improved AI and BattleGen. REAPER AI Helicopters was
checked separately for aviation. These informed finite combined-arms waves, pressure before insertion, distinct
moving/covering tasks, respect for existing orders, bounded contact reports and dedicated-server authority. No
external AI source or new Workshop dependency was copied/added. This is not a popularity ranking, comparative
play test, or certification of compatibility with CRX/DCO/Improved AI behaviour overrides.

## Workbench and dedicated-server gates

1. Compile **Game scripts in Script Editor**, then **Play in World Editor** on `AROR_Everon_Chicken` with the
   combined WCS/RHS addon. Record compile errors, selected presets and loaded versions.
2. Trigger towns held by each army, at Readiness 1–5 and representative Crackdown levels. Confirm finite waves,
   fast split shock behaviour for Invaders, deliberate convoy/cover behaviour for Occupiers, missing-road fallback,
   new-contact retasking, and expiry after losing contact. Test post capture between kill polls and support flips.
3. Confirm actual selected WCS hulls, crew factions and complete infantry kits. Check loaded magazines, launcher
   ammunition, inventory capacity and AA Igla/Stinger use. Check smaller MH-6 passenger capacity and larger Mi-24
   cabins without actor-budget overflow. Verify ground garrison reactivation, reserves, sentries and support updates.
4. Observe at least one transport/gunship town set: confirmed pilot/passenger seats, flight, landing, unload,
   dismounted tactical advance, gunship engagement/friendly clearance, full station window and egress. Test distant
   origins, blocked LZ, missing origins, full executor, casualties, player takeover and later insertions while the
   first infantry remain deployed. An accepted request alone is not successful insertion evidence.
5. Spot all seven contact classes. Before confirmation, maps must not reveal exact positions. Open/close, pan/zoom,
   reorder snapshots, switch role colours, lose LOS, go prone, use smoke and expire sightings. Verify separate
   transport/gunship markers. HICOMM has its own renderer and is outside this marker change.
6. Repeat on a dedicated server with two Resistance clients and a late joiner: authoritative counts, consistent
   markers/timestamps, no duplicate spawns/rewards, cancellation/abandonment/handover and cooldown. Record server
   frame time with town life, convoys, transport and gunship support active.

Extenders: new optional vehicle selection priority defaults to zero; nonzero priorities change the candidate pool
for stable/random picks. Existing config entry IDs and saved roster ordinals remain, but hulls can rebind through
the selected preset. New town doctrine/air attributes, spotting kind 6 and the optional `RecordPostTaken_S` support
baseline argument require matching server/client scripts. No save schema or RPC signature changed.
