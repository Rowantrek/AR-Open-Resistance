# Infantry executor and QRF testing in Everon_Chicken

**Current Chicken fixture:** [strategic selection](QRF_STRATEGY_ECONOMY.md) starts at readiness 1 and crackdown 20%. Readiness gates equipment; Crackdown requests intensity 1–5. Ground/joint capture responses and explicit airborne selections are enabled for both AI roles, with tactical waves and spotting; combat-triggered dispatch and the blanket airborne override remain off. [Strongholds](STRONGHOLDS.md) use trusted scenario-funded responses through the same executor. The exact [truck/APC regression fixture](TRUCK_IFV_QRF_TEST.md) requires strategic economy off. Compile and reload before native acceptance.

Chicken has preconfigured force pools and ground execution paths. The infantry-only instructions below apply when `m_aForceProfiles` is empty and strategic economy is disabled. The [REAPER airborne experiment](AIRBORNE_QRF_PROTOTYPE.md) is a separate explicit fixture; normal weighted airborne selection remains excluded.

Use `Worlds/AROR_Everon/AROR_Everon_Chicken.ent`. Its `AROR_GameMode_Base1` instance now contains `AROR_OperationExecutorComponent` and `AROR_OperationTestComponent`. These additions are in Chicken's Managers layer; the original Everon world and shared game-mode prefab do not receive the executor or test component. Existing shared commander/scheduler scripts still apply to both worlds.

The script default is disabled, but the prepared Chicken override now enables the executor and supplies verified base-game profile/waypoint references. No new assets or `.meta` GUIDs were fabricated. Disable the executor explicitly when running scheduler-only smoke tests.

## Test QRF scheduling first

1. Compile in Script Editor, then select `AROR_GameMode_Base1` in Chicken's Managers layer.
2. Keep `m_bExecuteOperations` off on the executor and disable `m_bQRFStrategicEconomy` for this isolated immediate-scheduler test. Clear both automatic attack target IDs.
3. On `AROR_OperationTestComponent`, set `m_sOccupiersQRFTestTargetId` to `everon_provins`. Keep the Invader target empty and its admission gate off. Leave `m_bRunSchedulerSmokeTests` off. The default report delay is 15 seconds after campaign readiness.
4. Play and complete setup. Expect one Occupier `[AROR TEST] Simulated attack` ID, then SPAWNING and an eventual timeout with no executor. A dual-role scheduler test requires a separate explicit disposable configuration with valid Invader targets; normal Chicken now admits Invader capture responses. Keep its Invader gate off only for this isolated Occupier-only scheduler test.

These are simulated reports for isolated testing, not a claim that both factions own Provins. Firing without damaging an assigned defender does not generate a defense report. The component sends each configured report once per world run; clear the fields afterward. It runs only on the Workbench server and is inert in packaged builds.

For the broader request/state-machine tests instead, start a fresh world with automatic targets and QRF test targets empty, executor disabled, strategic economy disabled, and default scheduler limits/cooldowns. Set `m_bRunSchedulerSmokeTests` on. The test component calls the existing helper after readiness using `everon_provins` and `resistance_hq`; both must be registered. Expect `Operation smoke tests passed=1`. Restart before rerunning because cooldowns are deliberately retained.

## Configure physical forces

On Chicken's executor component, select:

| Attribute | Required value |
|---|---|
| `m_sOccupiersGroupPrefab` | Fallback infantry group matching the shared Occupier campaign faction; native US assets should use a profile declaring their asset faction |
| `m_sInvadersGroupPrefab` | Fallback infantry group matching the shared Invader campaign faction; native USSR assets should use a profile declaring their asset faction |
| `m_sAttackWaypointPrefab` | A base-game AI attack/search-and-destroy waypoint prefab |
| `m_sQRFWaypointPrefab` | A base-game AI defend waypoint prefab |
| `m_vOccupiersSpawnPosition` | A clear, navigable land position for the Occupier force |
| `m_vInvadersSpawnPosition` | A separate clear, navigable land position for the Invader force |
| `m_bExecuteOperations` | Enable after filling the fields above |

Use the resource pickers to select group entities, not individual soldier prefabs. Start with small infantry groups without vehicles, nested subgroups or attached static waypoints. Their member-slot arrays define the reservation cost. Campaign faction keys come from `AROR_FactionRolesComponent`. A force profile can explicitly declare a native `m_sAssetFactionKey` for first-spawn conversion to its campaign key; unrelated factions and subsequent changes are rejected. The legacy fallback fields have no source-faction declaration and require matching or unassigned entities. See [force pools](FORCE_POOLS.md).

Choose launch points at least 150 m from the objective centre and 100 m from player-controlled characters under default checks. Heights are snapped to terrain. Checks only cover terrain height above zero, objective distance and controlled-player distance; they do not prove water absence at nonzero water levels, obstacle clearance, navmesh coverage or a usable route. Inspect the positions and route in World Editor. The player-distance check does not use the Game Master camera position.

The shared executor budget defaults to four groups and 32 reserved infantry member slots across both factions, including materialising and dormant forces. A full group budget leaves dispatches waiting within their spawn deadline; an invalid/oversized group fails its request. If a chosen full squad exceeds the member budget across simultaneous operations, raise the budget deliberately or select smaller group prefabs.

Keep enough spawn timeout for the engine queue: use the normal 30 seconds initially, rather than the shortened ten-second scheduler test setting. The world also needs the usual AI world/navigation setup for infantry simulation; script compilation alone cannot verify it.

## Physical-force acceptance checks

Codex has checked APIs against the installed Workbench documentation but cannot compile or run the addon here. Run these checks in Workbench, then on a dedicated server with clients:

1. Enable one role's scheduled attack only. Expect `Infantry operation executor enabled`, `Force queued for operation_...`, and an operation change to `state=2` (ACTIVE) once living members have materialised and the queue has filled the group. Observe the correct faction moving toward the target waypoint.
2. Test the other role alone, then both together. Each operation must own a different group and correct faction. Repeated scheduler ticks must not create extra groups. Private groups discourage player joining, but this is not a player recruitment implementation.
3. Clear automatic attacks and enable both simulated QRF reports. Expect `type=1` and correct defensive waypoint orders. QRFs use HIGH engine spawn importance; attacks use NORMAL. Both still respect the engine's global AI queue and limits. These orders do not dynamically detect enemy strength or choose transport.
4. On reaching the configured arrival radius, expect `reached objective; awaiting territory/combat outcome`. Arrival does not mark success. Without territory/combat resolution, operations remain active until elimination, explicit cancellation/result or the configured active timeout.
5. Kill the operation's squad. Expect FAILED with `Operation infantry eliminated`. Cancel a separate operation while materialising and again while active using the manager's `ResolveOperation_S` integration; verify its pending group, members and waypoint disappear and no late queue expansion produces stragglers. Terminal cleanup currently includes bodies/loot attached to those bodies; durable battlefield loot is not implemented.
6. Test incorrect group faction, incorrect resource type, exhausted budget and unsafe launch position. Expect bounded failure or waiting followed by spawn timeout, with no leftover empty groups or waypoints. A group evicted into engine dormancy retains its operation/reservations and gets periodic wake requests; it must not count as killed merely because it has no physical members.
7. Stop/replay, test target deletion, and join another client. Confirm cleanup, no duplicate subscriptions and server-only spawning. If a player takes over a group during testing, cleanup preserves their controlled entity and group entity; it still retires the operation's AI members and waypoint.

Do not run simulated QRF reports against the same role/location as an outstanding attack: the manager intentionally rejects that conflict. Use separate test runs, or a second registered objective for simultaneous attack-plus-defence tests.

## API basis and limits

The installed `Arma Reforger Tools/Workbench/docs/ArmaReforgerScriptAPIPublic/html/` documentation was used for `SCR_AIGroup.RequestSpawn`, `IsExpandComplete`, lifecycle policy, member events, faction affiliation, character life state and entity cleanup. In this installed API, `IsInitializing()` always returns false for compatibility, so the executor does not use it to decide that a queued group has finished spawning.

The engine queue/lifecycle approach is also described in [Bohemia's 1.9 modding update](https://reforger.armaplatform.com/news/modding-update-sept-24-2026). Engine group deletion is intended to retire its queue requests; cancellation under load remains a required runtime acceptance check. This prototype does not implement save/restore, safe route selection, territorial capture, a military economy or force transfer into a persistent garrison. A future territory/combat system must report success and real attack events through the operation manager.
