# Combined stronghold, garrison and QRF validation

Reconciled the pending Claude changes with the QRF review fixes already committed at `1d53820` on `chicken-dev`. No contributor changes were discarded. There were no textual Git conflicts; the work addressed behavior and validation dependencies across the combined source.

PR #31 integration follow-up: merged `master-dev` at `27c0876` into `chicken-dev`. The only textual conflict was the saved-state list in `ARCHITECTURE.md`; it now retains both Resistance Garage vehicle snapshots and stronghold state. Rowan's Garage code and phone dock changes are preserved. Added the existing Roboto Condensed resource binding to the new quick-store glyph after the combined UI test caught its missing font. All 29 regression suites still pass; lexical checks now cover 207 addon scripts. Garage native store/retrieve/placement and save/load remain separate acceptance work.

## Preserved content

- Fortress Vostok and Stronghold Delaware, their compositions/generators and stronghold state serializer.
- Extra garrison infantry teams, saved parked-vehicle variants and three unmanned Delaware helicopters. Existing saved rosters are not refilled.
- Gun embrasures in shared/fortified compositions and nearby tripod-height obstruction checks.
- Owner-coloured map labels outside icon frames and national flags shared with HICOMM.
- Vostok as the new Invader starting base, Crow Hill's fresh-campaign ownership and the Morton port relocation.
- Previous package-level dispatch failure, boarding deadlines, player-safe retention, funding reconciliation and light/joint response families.

## Integration corrections

| Area | Combined behavior |
|---|---|
| Scripted admission | Scenario-funded responses can bypass scheduler cadence and the per-role cap, but cannot overlap another nonterminal operation for the same role and target. Server authority, Invader admission and executor budgets remain enforced. |
| Coast launch/recovery | Fixed landing points remain eligible for construction but do not become owned installations or safe recovery destinations. Terrain search is bounded to at most 884 candidates and 48 road queries at the maximum supported range. |
| Stronghold completion | The tactical dispatch receipt distinguishes a deployed response from construction failure even if ACTIVE and terminal transitions occur between stronghold polls. |
| Kill attribution | Missing/retired entity references alone do not create player kills. Their damage subscriptions are detached; actual character/vehicle death checks remain. |
| Port identity | Morton Port retains `everon_saint_pierre_port` and the original entity name. Display/coordinates change without replacing the stable location identity. |
| Garrison validation | Harnesses include new roster fields, helicopter variant rebinding and the full stronghold pad inventory. Clearance tests exercise both high and low firing lanes. |
| Evidence | Mutation checks first require each unmodified suite to pass, preventing a broken harness from being reported as successful mutation detection. |

## Evidence

- **29 suites / 97,348 mocked assertions**, including **40 source mutations**. Native factories, geometry, replication and driving/flight remain mocked. Counts include repeated scheduler/lifecycle invariants, not distinct gameplay scenarios.
- `tools/Test-StrongholdIntegration.js`: 89 actual-source assertions covering admission, funding, forced waves, coast budgets, lifecycle, capture policy, persistence and port identity.
- Shared/fortified layout, defence audit, garrison, capture, Invader and QRF regression suites pass together.
- Stronghold generator consistency checks and read-only gun-embrasure checks pass.
- Stronghold resource check passes both metadata paths, two local references, ten installed base-game GUIDs and repository metadata GUID uniqueness. **The two stronghold GUIDs remain hand-authored; Workbench discovery/loading is NOT_RUN.**
- Chicken now has **45 eligible locations, 21 possible launch sites and 924 directed non-self pairs**, including 546 military attack pairs and 63 port-target pairs. Every native route result remains `NOT_RUN`.
- All 27 military roots contribute to capacity estimates. Peak estimated overlapping demand is 112 personnel against a shared cap of 96; native staging/performance acceptance remains required. Four legacy catalogue/world transform differences remain reported.
- Source/artifact hashes and individual suite output are in [automated results](validation/QRF_AUTOMATED_RESULTS.json). The [release report](QRF_RELEASE_VALIDATION.md) retains the broader limits and native gates.

## Native acceptance still required

Compile Game and Workbench scripts, discover the two new compositions, and reload Chicken. Check stronghold placement/grounding, gun arcs, helicopter pads, garrison queue pacing, map labels/flags and dedicated-server replication. Exercise retaliation, construction failure/retry, final defence, coastal withdrawal and terminal retention. Compare fresh campaigns with existing saves: saved holdings and casualties must survive, while new stronghold state starts from its default when absent. The campaign does not yet end when a stronghold falls.

Use [stronghold acceptance](STRONGHOLDS.md), [QRF acceptance](QRF_LIGHT_RESPONSE_ACCEPTANCE.md) and [review dispositions](QRF_REVIEW_FOLLOWUP.md). No native release approval is implied by this reconciliation.
