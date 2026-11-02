# Campaign gameplay readiness

## Current assessment

The playable milestone is: approach a defended installation, clear physical hostiles, capture its flag, survive the previous owner's QRF, and retain the resulting campaign state. Earlier user tests completed that sequence at individual sites. The latest source changes still need Workbench compilation and native acceptance; offline checks cannot establish navigation, difficulty, replication or performance.

The latest reviewed gameplay log (`logs_2026-10-25_18-22-12`) explained two failures. Large virtual rosters occupied most of the 96-person garrison allowance while earlier sites stayed inside the retirement radius. Later sites generated recipes but deployed nobody before their deployment window expired. Levie's subsequent capture did queue, admit and fund a tier-3 QRF; its bounded objective road search failed after selecting Laruns as origin. That was a delivery-planning failure, not a missing capture trigger.

## Changes prepared

- Admit complete infantry teams and crewed assets incrementally against submitted/physical personnel commitments. Rotate nearby sites through construction instead of reserving an entire virtual recipe first.
- Extend capacity waiting within a bound for empty sites. Close an established capacity-limited deployment before player clearance, preventing late additions when casualties free capacity.
- Reclaim distant garrison capacity sooner only after continuous all-player/site/actor separation and without recent combat. Preserve losses, health and captured/player assets.
- Let Chicken use locally validated infantry when a full compound footprint fails. Heavy assets remain omitted for that activation; individual safety and capture-area checks still apply.
- Add optional specialist infantry variants and bounded local patrol/contact tasks. Keep MG and vehicle crews mounted; use reported contact snapshots rather than tracking players for tasking.
- Prefer paid QRF recipes, with Chicken opting into a complete tier-1 capture response when no paid recipe fits. Overhead/losses remain bank liabilities or saved debt; no money is granted.
- Plan road delivery before spawning hulls/groups, allow bounded nearby road dismount followed by infantry approach, and retry explicit pre-activation capture planning failures through eligible alternatives.
- Require friendly land origins in Chicken. The USSR corridor is not a ground spawn source; taking its last land base can legitimately leave no QRF origin.
- Audit all 23 saved military roots read-only, retaining grouped authoring, stable IDs and the Chotain castle. Restore the intended initial USSR Durras ownership/name overwritten by the last editor save; saved campaign ownership remains authoritative.

## Saved-world caution

[CAMPAIGN_SITE_READINESS.csv](CAMPAIGN_SITE_READINESS.csv) records all 23 military sites. Pinewood Lake, Chotain Industrial, Old Wood/Wolf Hill and Andres Beacon have saved transforms differing from the catalogue. Current saved roots produce a worst-case 1 km activation overlap with 102 logical personnel, above the 96-person cap; incremental admission is therefore necessary even before multiplayer splits players across the island.

The user's ordinary-outpost prefab has no generated sandbag walls. Chotain retains its manual castle. Preserve both. The existing generated-prefab/world equality checks fail on this authored content, and strict saved-child grounding parsing still cannot safely round-trip the castle. Do not run generators with `-Apply` over these files. Reconcile saved poses and add authored-subtree preservation before regeneration. Historical native footprint passes do not validate the current edits.

## Progression smoke test before the full lifecycle

Stop Play, compile Game/Workbench scripts and reload Chicken. A fresh campaign should show readiness 1 and crackdown 20% (response level 2) for each enemy role. Shooting defenders can raise crackdown but must not dispatch a QRF. Capture one US site: expect a light truck/M2 response if funds/capacity/origin permit, with no LAV. Repeat against USSR after it has a second friendly land origin: expect Ural/PKM-UAZ elements, with driver/gunner occupancy and light-vehicle map captions.

In a disposable test copy, use the operation manager's starting attributes to check readiness 1/crackdown 80% and readiness 5/crackdown 0% in fresh campaigns. The former must remain light at response 5; the latter may use the late pool at low response intensity. Test readiness 3 separately for mechanized introduction. Funds can downgrade intensity; zero-funds fallback must remain in the allowed readiness pool. Verify restored schema-1/2 campaign state in a save-enabled mission and independent live changes in RebIntel on two clients.

## Workbench acceptance order

1. **Compile and reload.** Stop Play, compile Game/Workbench scripts, reload Chicken from disk, and confirm resource discovery. Inspect Durras's USSR initial role and the saved Chotain castle. Run the native military footprint audit on the actual scene; retain results separately from historical evidence.
2. **Arrival and crowded activation.** Start fresh Play. Approach and directly teleport to Levie Military, Laruns and Andres/Gillnet Pond. Construction should begin on the next proximity scan (normally one second) and proceed through the native queue. Move between a cluster of sites: logs should show commitments, bounded capacity waiting and safe distant retirement. Test two players near different clusters. No active/queued personnel count may exceed the configured garrison cap.
3. **Clear and capture.** Leave one hostile alive: capture stays blocked. Kill the last defender and let submitted queues finish/cancel: capture becomes available. Free capacity elsewhere during clearance and verify no late teams appear. Damage alone must not dispatch QRFs. Verify driver/gunner/MG occupancy; deliberately parked armour stays unmanned. Test one unsuitable full footprint with safe local infantry pads and one wholly unsafe site: neither may bypass pad checks or lock capture indefinitely.
4. **Paid and minimum QRFs.** Capture a US site with sufficient funds, then repeat with zero funds using an existing trusted Economy/admin mechanism. Expect the strongest affordable recipe or only tier 1 when none fits. Verify one overhead charge, deliberate deaths/hull losses charged once, debt without negative balances, and repayment from later funds. Funding fallback must not bypass role cooldown, actor capacity or friendly-origin safety.
5. **Delivery and recovery.** Repeat Levie's former Laruns approach. Verify every lead/reserve package's physical delivery, safe unloading, infantry arrival, full security hold, withdrawal, return and distance/age cleanup. A remote road stop must not start the hold. For a planning failure, verify bounded retries retain cooldown/original age and create no empty hull before a stop is found. Healthy active packages must survive another package's failure.
6. **USSR, persistence and multiplayer.** Let Invaders hold at least two land installations before capturing one. Verify USSR garrisons, Ural/BTR/BRDM crews, capture-only QRF and return. Use a save-enabled mission for roster/loss/readiness/crackdown/debt and territorial-campaign save/load. Active QRF deployments/queues remain session-local. Test dedicated-server ownership, late joining, capture feedback, non-debug spotting and sustained combined AI performance.

Keep `[AROR GARRISON]`, `[AROR STRATEGY]`, `[AROR FINANCE]` and `[AROR TACTICAL]` logs for each failure, naming the site and whether the campaign was new or loaded. Native AI pathing and the 15-minute hold require actual elapsed gameplay, not offline assertions.

## Offline evidence and limits

The current source-adapted suites pass 11,932 assertions across garrison lifecycle/crew, QRF strategy/funding/planning/delivery, layouts/editor tools, capture integration, RebIntel and the Invader campaign. This includes every faction/readiness/crackdown combination, funding monotonicity/downgrades, startup/legacy-save checks, a 13-site delayed-queue stress case, bank/debt idempotency and pre-construction planning failures. Engine services are mocked throughout.

Installed/local metadata checks cover 28 garrison/shared references and 27 QRF references; grounding parser fixtures and the read-only saved-scene audit pass. Generated-content equality remains blocked by intentional saved authoring as described above. There is no command-line Enforce build here; compile, native navigation, server/client state and difficulty remain unverified.

## Following milestone

After this loop passes, make held territory useful: Resistance recruitment and assigned persistent defenders, coordinated with the existing economy/persistence owners. On the opposing-faction side, introduce limited reinforcement and supply between held installations, making factories/resources affect readiness and giving players a supply target to raid. The new QRF progression already separates long-term equipment capability from short-term crackdown; validate it before extending military supply. Physical stock, replenishment, construction persistence and durable deployed-operation restoration are still future work.
