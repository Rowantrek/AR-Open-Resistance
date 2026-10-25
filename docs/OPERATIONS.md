# Enemy attacks and QRFs

`AROR_OperationManagerComponent` is attached to the base game-mode prefab. It schedules attacks for both enemy commanders and creates defensive QRF requests when trusted server code reports an attack. Resistance remains player-directed and is rejected by these enemy operation APIs.

This manager schedules and tracks operations. The optional ground executor on Everon_Chicken can spawn groups and vehicles, issue waypoint orders and monitor elimination; see [force-pool setup](FORCE_POOL_QUICKSTART.md). The optional [QRF defense component](QRF_COMBAT_REPORTS.md) reports real hostile damage on explicitly assigned defenders. The manager does not detect gunfire, determine ownership, capture locations or spend resources. An operation is a request for an executor, not evidence that a force exists. Queues, IDs, cooldowns and outcomes reset on world restart and are not persisted.

## Workbench setup

Compile in Script Editor and confirm one operation manager is present in the game-mode prefab. No resource GUIDs or component instance IDs were invented; let Workbench generate an instance ID on save if needed. Use an inherited test prefab or your test-world override for tuning.

Set `m_sOccupiersAttackTargetId` and/or `m_sInvadersAttackTargetId` to a registered location ID, for example `everon_provins`. Empty targets disable automatic attack scheduling for that role. Both may target the same location independently. These are explicit scenario/test targets: you must ensure they are hostile to the corresponding role. Future strategic selection will use the territory system rather than these fixed targets.

After campaign readiness, the next commander evaluation schedules each configured role's attack. Defaults are 60 seconds of preparation and at least 300 seconds between accepted attacks per role. The interval is measured from acceptance; capacity must also be available. Failed or cancelled requests do not refund cooldowns. Scheduling uses one-second campaign scheduler ticks, so delays are approximate and may lengthen under server load. Dispatch does not depend on waiting for another strategic evaluation.

QRFs have a separate slot and cooldown for each role: defaults are a 10-second reaction delay and 120 seconds between accepted requests. A role can prepare an attack against one location and a QRF for another at the same time. Repeated reports for an outstanding request return its existing ID without restarting its timer. One role cannot have an attack and QRF simultaneously against the same location; the conflicting request is rejected. Different roles never deduplicate one another's requests.

All counts include planned, spawning and active operations. QRFs dispatch before attacks on each scheduler tick; this does not preempt troops already spawned or replace the future shared AI budget. If spawning is never acknowledged, requests fail after 30 seconds. Active operations fail after 900 seconds by default. A missing/unregistered target cancels its outstanding operations. Terminal history is bounded, so old IDs can become unavailable.

## Trusted server integration

Wait until `AROR_OperationManagerComponent.GetInstance()` is non-null and the commander manager reports an active campaign. Calls before readiness return an empty ID. Do not expose these calls directly through unvalidated client RPCs.

```c
AROR_OperationManagerComponent operations = AROR_OperationManagerComponent.GetInstance();

// The strategy caller must verify this is a hostile target.
string attackId = operations.ScheduleAttack_S(AROR_EFactionRole.OCCUPIERS, "everon_provins", 60);

// A combat/territory system must verify that Invaders own this location
// and that a hostile attack is actually occurring. Use a different location
// if the same role already has an attack operation against this target.
string qrfId = operations.ReportAttack_S(AROR_EFactionRole.INVADERS, "everon_provins");
```

For a real validated hostile attack with an attributed position, `ReportAttackAt_S(role, locationId, reportedPosition)` adds a fixed contact snapshot to the same request. This is a trusted server API, not a client RPC. The defense component calls it only after its existing damage/faction/location checks. Repeated reports reuse the QRF and may update pre-search intel without resetting preparation/cooldown or rerolling force selection. Once search begins the shared plan freezes; missing/stale intel falls back to patrol around the marker. See [coordinated QRF search](QRF_TOWN_SEARCH.md).

These calls accept faction roles, never US/USSR/FIA keys. Empty return values mean rejection: invalid role/target/delay, unavailable campaign, cooldown, capacity or a conflicting operation. Matching outstanding requests return their existing ID. IDs are unique only within this world session; do not store them in campaign saves yet.

The registry confirms only that a location ID exists. It cannot yet verify ownership, hostility, access routes or a launch base. Ownership-changing callers must cancel stale requests through `ResolveOperation_S(id, AROR_EOperationState.CANCELLED, reason)`. Success does not automatically capture the location or silence later combat reports.

## Executor contract

Exactly one authoritative executor should subscribe with `GetOnOperationChanged().Insert(OnOperationChanged)` after the manager exists and remove the subscription on deletion. The handler signature is `void OnOperationChanged(AROR_Operation operation)`.

- `PLANNED`: an accepted request. No troops should spawn yet.
- `SPAWNING`: dispatch request. Queue the correct faction's force through the shared spawning system, using the operation ID to avoid duplicate execution.
- When the force exists and has orders, call `MarkActive_S(id)`. Acknowledgements before dispatch or after a terminal result are rejected.
- Report success, failure or cancellation through `ResolveOperation_S(id, state, reason)`. Success is accepted only for active operations. Planned/spawning requests can fail or cancel.
- On a terminal event, cancel pending spawns and reconcile/clean up any groups already created. Late spawn callbacks must check the current record; never leave newly spawned forces unowned after timeout/cancellation. A pruned/missing record also means the callback must not activate it.

Events normally dispatch synchronously. A state change requested inside a notification updates the operation immediately, but its notification is queued until the current invoker call returns: Enfusion ScriptInvoker rejects recursive Invoke calls. Notifications contain the live operation record, so always inspect its current state and make executor actions idempotent. Use the manager methods rather than mutating the record directly; the record's transition helper is internal to this implementation. Never interpret an arrival alone as a successful capture without territory-system confirmation.

## Verification

Codex cannot compile or run Enforce Script here. These checks remain to be executed in Workbench:

1. Compile, play your test world, and check `Operation scheduler initialised for Occupiers and Invaders`. Before campaign readiness, scheduling/reporting must return empty IDs. Verify clients have no manager singleton on a dedicated server.
2. Configure both automatic targets to `everon_provins`; after readiness and a commander evaluation, confirm two distinct planned operations, one per role. Repeated evaluations must not duplicate them. Observe preparation delay, SPAWNING, then FAILED after the spawn timeout with no executor. Each role may schedule again only after its own cooldown and capacity permit.
3. For request/state-machine smoke tests, start a fresh disposable world with automatic target IDs empty and default limits/cooldowns. After readiness, invoke `AROR_OperationSmokeTests.Run_S("everon_provins", "resistance_hq")` from temporary server-side test code or the Script Editor debugger. Both IDs must exist. This Workbench-only helper checks independent attacks/QRFs, deduplication, rejected invalid requests, invalid lifecycle transitions, cooldowns and late callbacks. It cancels its test operations synchronously before dispatch and logs `Operation smoke tests passed=1` on success. Restart before rerunning; it deliberately leaves cooldowns in effect.
4. In a temporary executor subscriber, acknowledge each SPAWNING request and resolve ACTIVE as succeeded. Verify one lifecycle per ID and rejection of a later conflicting result. Separately leave a request ACTIVE past a shortened test timeout and verify failure and executor cleanup.
5. Report a QRF for a second registered location while the same role has a scheduled attack. Confirm both are accepted, duplicate reports reuse the QRF ID, and a second role's QRF is independent. Test QRF/attack requests due on the same tick and confirm QRF dispatch events precede attack dispatch events.
6. Cancel a request or unregister/delete its target while pending/spawning/active. Verify terminal notification, rejection of late acknowledgements and eventual history pruning. Confirm no duplicate timers or subscriptions after stopping and replaying the world.

Do not use the smoke-test helper in an active campaign. The report API is a trusted server hook. Automatic responses to hostile damage can use `AROR_QRFDefenseComponent` on explicitly assigned defenders; authoritative territory ownership and capture invalidation remain integration responsibilities. Everon_Chicken also has a Workbench-only test component that can trigger these smoke tests or submit simulated QRF reports without temporary script edits; see [the setup guide](OPERATION_EXECUTOR.md).
