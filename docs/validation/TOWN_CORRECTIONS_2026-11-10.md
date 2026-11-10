# Town warfare corrections — 10 November 2026

Implemented against the reviewed local `chicken-dev` snapshot `b7648ab`. The earlier review and its JSON are historical evidence for that snapshot; the results below describe the corrections. The user confirmed successful Workbench compilation of `97babeb` on 10 November 2026. Native gameplay acceptance remains outstanding.

## Changes

| Area | Result |
| --- | --- |
| Militia survival and persistence | Teams observe explicit deaths through native damage callbacks and retain pending/dormant strength. Missing initial deliveries stay purchased and retry at a bounded interval. Saving applies confirmed losses and publishes book changes. |
| Militia vehicles and crews | Destroyed or taken hulls leave the militia vehicle book once. Surviving crews and their waypoints move into a tracked dismounted collection. Cleanup preserves player occupants and fleet claims; deferred crews/wrecks retain their owner until safe cleanup. |
| Site handover | Former workers, patrols, trucks and aircraft remain owned in retirement collections. New activity waits while former forces remain. Safe retirement checks nearby commanded units and opposing fighters; claimed helicopters transfer to the fleet. |
| Native simulation | Active garrisons, militia and site soldiers renew finite leases for living AI, excluding player-controlled characters. Dormant survivors request paced reactivation instead of being recorded as deaths. |
| HICOMM boarding | Accepted boarding actions reserve distinct actor/seat assignments across retries. Successful readiness requires an actual living squad driver. Failed readiness releases stranded AI cargo; player-controlled members are protected from automatic dismount. |
| Remote defence | Known commanded ground forces activate military/town activity and prevent human-only abandonment. Surviving raised militia also count as town defenders. Militia do not activate themselves indefinitely. |
| Kill credit | Town casualties resolve commanded AI through the leaderboard's existing commander attribution. Known friendly hits cannot manufacture town kill credit or escalation. |
| Spotting/map tracks | Precise threat tracks use confirmed, expiring last-seen positions. The chosen physical element supplies its own ground/air type. Planned helicopter LZs remain private; separate per-asset spotting symbols remain available. |
| Shared admission | Operations, military garrisons, preset-spawned groups and native recruits share a personnel ceiling. Queued slots count before the next native producer runs; neither admission nor pressure forcibly deletes existing soldiers. |
| Ambient pressure | Site activity has its own shared personnel ceiling and leaves combat headroom. Site camp construction is staggered across startup slots instead of scheduling every site at the same instant. |
| Aircraft catalogue | The combined WCS/RHS US preset now references the offered UH-1H variant, preserving the intended MH-6/UH-1 choice. Transport insertions and gunship responses remain enabled. |

Militia now owns infantry and crew waypoints explicitly; removing a group also removes those independently spawned waypoints. This closes an additional accumulation path found while implementing the review fixes.

## Initial balancing configuration

These are provisional playtest settings, not measured server performance guarantees:

- Shared admission ceiling: 320 reserved mod-owned AI personnel. Site activity leaves 72 slots of combat headroom and has a separate 48-person ceiling.
- Existing local caps still apply: the operation executor's 220-person limit and military garrisons' 144-person Chicken limit do not disappear.
- Preset-created groups conservatively retain their queued reservation until their owner deletes the group. This prevents partial native materialization from freeing slots prematurely, but can delay new admissions while a partially depleted group remains owned.
- Player-commanded native recruits are counted by their recruitment owner after leaving temporary spawn groups; they are not counted twice through those groups.
- Ground wave size is the lower of the original Crackdown allocation and `ceil(nearby friendly fighters / 8)`, with the existing city and final-wave bonuses. The cap is recalculated for each new wave; it never removes a committed force. Designer attributes control scaling, distance and fighter ratio.
- For an ordinary town, two players or two players plus six recruits request `[1, 1, 2]` ground elements. A nearby force of 20 at high Crackdown is capped at `[3, 3, 4]`. Air support is a separate request and retains native capacity/launch/landing checks.
- USSR retains faster shock tasking and split approaches when a wave has enough elements. BLUFOR retains deliberate convoy/support behaviour. Scaling does not duplicate the allocation across split axes.
- Chicken gunship support is 120 seconds on station. Transport aircraft remain common and town air-assault recipes remain enabled at early Readiness.
- After ten quiet minutes, Chicken cools Readiness by 30 pressure/minute and Crackdown by 40 points/minute. Pressure 700 returns to the starting Readiness floor of 100 after roughly 20 further quiet minutes; reaching a nearby level boundary is faster. This replaces the earlier five-hour recovery tail.

The shared limit applies to actors produced by these mod systems, not unrelated Game Master/external-addon spawns. It remains subordinate to the engine's own AI budget. Full capacity can defer or reject a request; it does not guarantee that every planned wave or aircraft can launch in every saturated world. The single active town-response slot is retained.

## Executed validation

Run `node tools/Validate-TownCorrections.js` with a Node-compatible runtime. The runner returns failure unless every selected child finishes its assertion/check report. Existing suite artifacts are redirected to temporary storage so historical checked-in validation results are preserved.

- **44/44 checks pass**, reporting **113,246 assertions** in passing JavaScript suites, plus the three successful PowerShell checks.
- `Test-TownCorrections.js`: 12,571 assertions, including 500 repeated handover/retirement cycles and 10,000 mixed queued-admission ticks. Executes extracted current methods against explicit mocked native services, including reservation before native enqueue and signed startup hashes.
- Existing lifecycle, interruption, manager-soak, recipe/strategy/progression, boarding/ingress/unloading, capture/garrison, geometry, HICOMM, player-squad, phone and mutation checks pass.
- Previously stale adapters now reflect current formation/preset spawning, reinforcement-only contests, track serialization, mechanized status and recruitment methods. Their assertions continue exercising current code; native UI rendering and native kit application are not inferred from fixture success.
- The reserve-window mutation now shortens the window to 120 seconds. A 300-second window is no longer necessarily invalid with the new 120-second gunship setting, so the old mutation did not represent the claimed regression.
- `git diff --check` passes.

Detailed results and source hashes: [TOWN_CORRECTIONS_2026-11-10.json](TOWN_CORRECTIONS_2026-11-10.json). The snapshot is the baseline HEAD plus the recorded working-tree source hashes; no native execution is claimed by the runner.

## Aircraft and equipment audit

[The fresh installed-resource audit](TOWN_CORRECTIONS_WCS_2026-11-10.json) finds **2,940 exact resource references, zero unresolved references, no missing installed dependencies and no skipped dependency groups** in the modeled combined stack. The offered UH-1 replacement removes the previously unavailable transport entry.

WCS vehicle priorities and RHS infantry loadouts are retained. Resource resolution does not establish native pilot control, cargo usability, weapons operation or kit application. Twelve other preset entries are still excluded by the enabled catalogue, and eight preset/catalogue category differences remain listed in the audit. They are not silently admitted by this patch. The existing US radio-operator and Russian pilot kit fallbacks also remain; this patch does not invent replacement equipment assets.

## Extender impact

- No campaign save-format or player RPC format changes. Existing militia books still load through serializer version 1.
- `AROR_ForceSpawner.SpawnGroup` gains an optional `trackPopulation` argument, defaulting to true. The recruitment subsystem uses false because it accounts for soldiers after transfer to native command groups. Extenders should retain the default unless they provide equivalent shared accounting; overrides must accept the updated signature.
- `AROR_MilitiaPost.m_aGroups` now contains owned `AROR_MilitiaTeam` records rather than raw native groups. This is runtime-only state; extenders accessing it directly must use each record's `m_Group`.
- `AROR_SpecialSiteGroup.Remove_S` and its truck override return success/failure so a cleanup owner can retain protected actors. Extender overrides must return the result.
- New configurable shared population/ambient headroom, site activity, and town-strength scaling fields affect scenarios using inherited defaults. Disable town strength scaling explicitly to retain the old ground allocation policy.

## Required native acceptance

Start a fresh world/server session after compiling; hot-reloading an already populated old world cannot reconstruct all new reservation ownership.

1. Workbench compilation of `97babeb` is user-confirmed passing. Next, Play the Chicken scenario in World Editor with the intended addon stack.
2. On a dedicated server, test two players with a shared recruit squad, then separate squads and two HICOMM leaders. Send gangs beyond human simulation range to attack and defend towns. Confirm defenders remain present, fight, and do not trigger an automatic human-only retake.
3. Repeat site capture/recapture, militia purchase/stand-down, destroyed-hull crew survival, claimed vehicle departure, and save/restart. Check both body and waypoint counts return to baseline after retirement.
4. Force delayed materialization/native dormancy and shared-budget saturation. Confirm stock is retained, queues stay finite, and late callbacks do not create duplicate units. Test a fresh world transition to clear the session ledger.
5. Board tiny/full WCS vehicles, refuse or kill the driver, and verify partial boarding feedback. Test WCS MH-6/AH-6, the offered UH-1 and Russian transport/gunship variants for real crew seats, cargo offload, safe LZs and departure.
6. Verify spotting requires observation, stale tracks expire, returning/claimed assets have correct symbols, and late joiners receive coherent map state.
7. Run the review's 45–60 minute dedicated soak around overlapping town/military sites. Measure server frame-time percentiles, live/dormant/queued population, entity and waypoint trends, replication traffic and failed routes/landings against a warmed-up baseline.

No native flight test or dedicated-server performance measurement was possible in this environment. Automated policy checks do not certify those outcomes.

### Follow-up to the supplied Workbench compile

The user's 20:19:36 Workbench log for `4034ff4` reports five Open Resistance diagnostics: two owned-reference-array `InsertAll` calls each produce a type/conversion error, and the inline hash/mask argument produces an incompatible-parameter error. These were missed by the JavaScript native-service mocks.

The follow-up replaces both bulk transfers with per-element insertion, retaining ownership before the original collections are cleared. Startup delay now uses a typed hash variable and a signed-modulo helper with no inline bit mask. Regression checks cover signed integer boundaries and reject `InsertAll` calls supplied with locally declared owned-reference arrays. The source guard tests its own rejection/acceptance against invalid bulk, valid weak-array, and valid per-element examples.

The same failed log contains base-game diagnostics. The user subsequently confirmed that recompilation of `97babeb` succeeds, resolving the reported compile failure. This is user-provided native compile evidence; the flight, multiplayer gameplay and dedicated-server performance checks above remain outstanding.
