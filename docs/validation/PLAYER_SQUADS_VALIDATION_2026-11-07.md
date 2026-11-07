# Player Squads validation — 7 November 2026

Source validation found and corrected lifecycle faults in the first squad implementation. All focused tests pass after the fixes. Native engine acceptance of these fixes remains pending.

## Findings corrected

- An arrival observed before equipping could die and still be attached through its cached character pointer. Dead arrivals now release capacity without explicit corpse deletion. Arrivals wait while their native AI control is initializing.
- Immediate roster publication could dereference a joined character deleted since the last periodic update. Publication now skips absent/dead characters safely.
- Last-player cleanup ran before checking native release/transfer state. A released soldier, or one transferred to another group, could therefore be deleted by the former squad. Each transaction now retains its original AI subgroup; cleanup checks both the actor's current AI parent and native membership before retiring it. This also protects transfers when the original player group is already gone or its roster is stale.
- An outfitted arrival moved to another AI group could be reclaimed through its cached pointer. Pending transfers now recheck the actual AI parent and release the transaction instead.
- The phone's disabled squad-name field could keep an old name after another player renamed the squad. Nonleader views now follow the replicated native name while leader edits remain intact.

These fixes change no extender resource references, public request signatures or configuration. The additional subgroup reference is session-only.

## Executed checks

| Check | Result | Scope |
| --- | --- | --- |
| `Test-PlayerSquads.js` | 408 assertions pass | Actual server policy, shared name validator, roster serialization, phone layout/actions, owned-controller bridge, flag inheritance and lifecycle methods against mocked native services |
| `Test-PlayerSquadMutations.js` | All 10 broken policies detected | Server authority, pending capacity, leadership, cross-squad dismissal, AI ownership, released membership, deleted publication, stale arrival transfer, name validation and redundant replication |
| Full JavaScript suite | 23/36 suites pass | All 13 failures also reproduce against pre-squad commit `c6797ce`; no newly failing existing suite |
| `Test-OperationUIResources.ps1` | Pass | Layout metadata, script/config GUID bindings and local Workbench registration |
| `Test-StrongholdResources.ps1` | Pass | 2 local references, 12 installed resource GUIDs and local metadata GUID uniqueness |
| `Test-CampaignReadiness.ps1` | Existing failure | Delaware helicopter expectation; reproduced from archived baseline inputs |
| `Test-CampaignGroundingData.ps1` | Existing failure | Grouped prefab parser fixture; reproduced from archived baseline inputs |
| Native API reference check | Pass | Installed SDK confirms `AIControlComponent.GetControlAIAgent(): AIAgent` and `AIAgent.GetParentGroup(): AIGroup` |
| Whitespace validation | Pass | `git diff --check` |

The multi-squad run covers 32 independent squads and 192 pending recruits, staggered equipment completion, 60 stable update cycles, 64 vehicle-occupancy status changes, per-squad dismissal, sixteen replacement timeouts, mass departure and player takeover protection. Stable updates cause no additional replication bumps in the mock. This is a deterministic logic exercise, not a native server performance benchmark.

The phone checks execute its actual methods against mock widget pools. They cover empty-squad guidance, editing, disabled nonleader controls, replicated name changes, hidden-app actions, pagination, inline dismissal confirmation and clearing stale confirmation after changing groups. They do not certify visual rendering, input focus or screen scaling in the engine.

The mutation suite changes source strings only inside isolated JavaScript VMs. It confirms that the focused tests fail when meaningful protection is removed; no mutation is written into game files.

## Existing failures

The machine-readable [suite evidence](PLAYER_SQUADS_SUITE_2026-11-07.json) records each failure and its matched baseline signature. Existing JavaScript failures affect GarrisonCrew, GarrisonsAndSupport, MarsAndAttacks, MissionLifecycle, PostsAndPads, QRFBoarding, QRFIngress, QRFInterruptions, QRFMutations, QRFProgression, QRFReleaseLifecycle, QRFStrategy and RebIntel. Most failures are missing mock helpers or stale method expectations; the progression test also disagrees with existing defaults. Their systems were left unchanged.

Baseline comparison supplied the historical addon source/resource files from `git show c6797ce:<path>` to each failing JavaScript harness. The two failing PowerShell checks were rerun from a temporary archive of their baseline inputs. Matching failures establish that the squad changes did not introduce these observed failures; they do not establish that the affected systems work in the engine.

## Native acceptance still needed

The user reports that the earlier signature-fix revision `0113eaf` works. That is user-provided evidence; the agent did not observe its run. After the lifecycle fixes, compile in Script Editor, then Play the Chicken world in World Editor. Prioritize recruitment and casualty handling, follow/move/get-in/get-out commands, a shared squad across death/respawn and leadership transfer, and two-client dedicated-server testing. See the [feature checklist](../PLAYER_SQUADS.md).

Enforce compilation, real RPC ownership/replication, AI navigation, native vehicle seating, entity streaming, save/load behaviour and visual rendering were not executed here. The JSON evidence identifies the tested base commit and normalized source hashes for the validation working tree.
