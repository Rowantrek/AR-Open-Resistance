# Four-type attack and QRF force pools

Ordinary campaign QRFs target a **30% airborne share per army**, measured over successful dispatches. Standalone airborne and joint air/ground responses count in the same bucket. The remaining share uses ground recipes; an eligible joint recipe no longer automatically wins every ordinary response. Selection follows the nearest attainable running proportion, not an independent random roll. The first response is ground; ten feasible successful dispatches contain three airborne/joint responses.

Readiness, funding, capacity, launch origins, airspace and landing safety still apply. Air requests can fall back to ground before funding or construction; unavailable aircraft do not cause a free deployment or override limits. Failed preparation does not consume a dispatch. MARS, explicit-profile/recipe requests, attacks and scenario-funded stronghold responses are excluded. Aggregate per-army totals survive save/load through operation policy serializer version 4; versions 1–3 load with empty mix history and retain their existing campaign progress. `m_iAirborneQRFShare` exposes the percentage to scenarios/extenders. See [strategy and economy](QRF_STRATEGY_ECONOMY.md#airborne-response-share).

See [QRF tactical commander and staged waves](QRF_TACTICAL_COMMANDER.md) for the new shared BLUFOR/USSR execution policy, finite reserves, live limits, report-driven sectors and withdrawal/retention. It is enabled for Chicken's ground fixture; Chicken now enables capture-driven Invader ground responses; native acceptance is pending. New tactical behavior is not yet verified in Workbench.

The [USSR/Invader framework](INVADER_QRF_FRAMEWORK.md) has levels 1-5 and ground capture-response admission enabled in Chicken; other scenarios default off.

**Current Chicken fixture:** [strategic selection](QRF_STRATEGY_ECONOMY.md) starts at readiness 1 and crackdown 20%. Readiness gates equipment; Crackdown requests response intensity 1–5. Capture responses are enabled for both AI roles, with tactical waves, spotting, Readiness-1 light airborne selections and joint recipes from Readiness 2. Combat-triggered dispatch and the blanket REAPER override remain off. The exact [truck/APC regression fixture](TRUCK_IFV_QRF_TEST.md) requires strategic economy off. Compile and reload before native acceptance.

The proposed [QRF response roster](QRF_RESPONSE_ROSTER.md) adds 40 authored responses across eight families and levels 1-5, including combined ground/air compositions. It defines future commander/resource selection; it is separate from the executable Chicken catalogue. Chicken now has explicit one-air-package/ground joint recipes with optional escorts; see [release validation](QRF_RELEASE_VALIDATION.md). The broader design table is not fully executable. A [CSV catalogue](QRF_RESPONSE_ROSTER.csv) provides stable recipe IDs and nominal accounting.

Response strength now has five independently weighted levels. Each deploys one to five complete packages of the chosen type; see [RESPONSE_SCALE.md](RESPONSE_SCALE.md). Flight/landing/escort findings and the proposed waypoint sequence are in [AIR_OPERATIONS_RESEARCH.md](AIR_OPERATIONS_RESEARCH.md).

Force-profile configuration, bounded execution and campaign selection are implemented for native verification. Chicken has 12 prepared profiles and 62 recipes using verified existing resource references. The native weighted ground pool excludes airborne profiles; explicit airborne and joint selections use the REAPER adapter. Light transport-only profiles unlock at Readiness 1, while escorted packages unlock at 5. An empty pool preserves the earlier infantry-only executor. See [release validation](QRF_RELEASE_VALIDATION.md) for current evidence and limitations.

Ground QRFs now have a configurable [mounted town-search phase](QRF_TOWN_SEARCH.md): visit reachable road points in the attacked location, then resume deployment. This new phase still requires Workbench compilation and runtime verification.

The force types are motorized, mechanized, armour and airborne. Airborne packages contain one transport, a fireteam and its pilot, with zero or one gunship escort. Actual aircraft resources and AI behavior require native verification; prefab availability does not establish reliable flight or landing.

## Force packages

Campaign faction and native asset identity are separate. Set `m_sFactionKey` to the role component's faction (for example `AROR_OCCUPIERS`) and `m_sAssetFactionKey` to the source assets' faction (for example `US`). The latter authorizes first-spawn conversion only for that source faction; empty, already-matching and declared-source affiliations are accepted, while unrelated factions remain errors. Groups are assigned before requesting members, using `SetFaction` when an inherited group already has a faction. Periodic validation never resets an affiliation that changes later. Chicken now inherits Rowan's core role mappings and Resistance loadout without legacy faction overrides.

Each pool entry should describe a complete force package, rather than just one group prefab.

| Type | Initial test package | Behaviour to implement |
|---|---|---|
| Motorized | One infantry element, one truck/light transport, required crew | Board, drive to a dismount point, unload, infantry attacks or reinforces; transport waits/withdraws |
| Mechanized | One infantry element, one APC/IFV, required crew | Mounted approach, dismount short of contact, infantry advances while vehicle supports |
| Armour | One armoured fighting vehicle and required crew | Vehicle advances to a suitable fighting position and engages; no passenger infantry required |
| Airborne | One infantry element, one transport/pilot and zero or one gunship/pilot | Transport approaches a safe LZ, lands, unloads and departs; an authored gunship provides bounded support and withdraws |

These are package designs; they do not require a combined base-game group prefab. The executor assembles ground vehicles, separate crew groups and passenger infantry. Actual seating, driving and dismounting still need Workbench tests. Start with infantry that fits the transport's usable passenger seats after crew positions are filled.

Homogeneous airborne responses repeat their complete package 1–5 times. Joint recipes currently contain one airborne package plus ground elements. Independent transport/gunship counts and shared escorts remain planned; a transport weapon does not silently satisfy an authored escort requirement.

## Preparing additional profiles in Workbench

The initial US/USSR assets and profiles are already selected in Chicken. The steps below are for replacing or extending them.

Work in Everon_Chicken. Create reusable assets inside the addon, but place test entities/positions in Chicken's layers. Create and save resources through Workbench so it generates GUIDs and metadata. Keep base-game resources unchanged.

1. In Resource Browser, identify faction-matching infantry groups, crew/pilot resources and vehicle/aircraft prefabs. Inspect their inherited configuration before copying anything. Use the current US selection for Occupiers and USSR selection for Invaders; these faction choices remain replaceable.
2. Create inherited infantry/crew group prefabs only where customization is needed. Suggested organization is `Prefabs/AI/Groups/US/` and `Prefabs/AI/Groups/USSR/`. These folders and filenames are organizational suggestions, not implemented lookup rules.
3. Select existing base-game vehicle resources or make inherited variants under `Prefabs/AI/Vehicles/`. Record infantry, crew, transport and escort resources separately. The ground executor assembles and links its package; putting a truck next to soldiers in a saved composition does not establish boarding or command relationships. Airborne assembly/execution still requires a flight adapter.
4. Inspect each vehicle's crew positions, passenger capacity, faction, weapons/ammunition and replication setup. Aircraft need suitable pilot resources; a gunship also needs whatever crew its weapons require. Do not assume every armed helicopter has a separate gunner seat.
5. Prepare one motorized, mechanized, armour and airborne package for each current faction: eight starting profiles. A package may reference existing resources without needing a new combined prefab. If a faction has no suitable working aircraft, leave that profile unavailable rather than substituting another faction's aircraft silently.
6. Mark clear ground launch areas and an air launch/landing setup in Chicken. These are test placements for now; the current executor has only one ground launch coordinate per role and cannot consume a larger launch-point registry yet. Keep armour off cramped roads and helicopters away from buildings, trees and power lines. An LZ should be a separate position from the objective centre.

For airborne, test transport flight and landing independently before adding escorts. Then test one gunship with an appropriate pilot/crew and combat task. Add a second aircraft only after the single-aircraft behaviour and operation cleanup work. If AI flight/landing is unsupported for a chosen asset, disable that profile while its adapter is investigated.

## Profile data the code needs

`AROR_ForceProfile` and the executor's `m_aForceProfiles` array now provide role/faction-specific configuration. The implemented fields cover identity, enabled state, weights, primary infantry/crew/vehicle resources, escort resource/count, personnel reservation and dismount distance. The full design also needs:

- Stable profile ID, force type, enabled flag, attack weight and QRF weight.
- Infantry and crew group resources, with explicit counts.
- Vehicle resources/counts; airborne profiles separate transports from gunship escorts.
- Execution policy: motorized, mechanized, armour or air assault.
- Allowed launch-point types, approach/dismount/LZ requirements and relevant timeouts.
- Reserved personnel, vehicle and aircraft budgets; future military-resource cost and availability requirements.
- Whether escorts are required for dispatch, their number, support duration and withdrawal policy.

The earlier infantry-group fields are used only when the profile array is empty. Entries are editable on the executor in Workbench. Airborne launch/LZ planning, staged packages and Economy collateral/loss accounting are implemented in source; physical military stock, broader independent H/G compositions and native acceptance remain unfinished.

Changing a role's faction must select a matching pool. Resolve by campaign role, then faction configuration; never code OCCUPIERS as synonymous with US or INVADERS as synonymous with USSR. Validate infantry, crews, vehicles and aircraft together.

## Selecting an attack or QRF

For each request, filter the relevant role's pool before choosing:

1. Is the profile enabled for this operation type and faction?
2. Are all required resources available and validated?
3. Is there a suitable launch area and route? For helicopters, is there a usable insertion plan/LZ?
4. Can the package fit the shared personnel, vehicle and aircraft budgets? Include crews and escorts.
5. Does it meet the response deadline? A tank column far away may be an unsuitable QRF even when affordable.

Then make a weighted random choice from eligible profiles. Suggested initial QRF weights are motorized 40, mechanized 30, airborne 20 and armour 10. These are tuning suggestions, not current code values; weights need not total 100. Attack weights should be configured separately. A zero weight disables that profile for the operation type.

Choose once, reserve the entire package atomically, and record its profile ID on the operation. Do not reroll every commander evaluation or every spawn retry. If eligibility changes before dispatch, release reservations and explicitly replan or fail. An empty eligible pool should cause a bounded defer/failure with a reason, never an unlimited stream of spawn attempts.

The attack detector supplies a report; the role's commander/pool selects its response. No separate randomized QRF spawner should bypass operation deduplication, cooldowns or budgets.

## Airborne with gunships

Treat insertion and escort as coordinated parts of one operation:

1. Reserve transport, infantry, flight crews and gunships together.
2. Spawn and verify each required element; establish boarding and seat assignments.
3. Dispatch when the minimum required package is ready. If required gunships fail to spawn, do not quietly send an unescorted transport.
4. Fly the transport to an LZ outside the main fight; escorts cover the approach without sharing the same flight position.
5. Confirm landing and actual passenger exit before sending infantry toward the objective.
6. Send transport home or to a holding area. Escorts support for a configured time/threat policy, then withdraw.
7. Track losses separately: transport lost, infantry delivered, escort lost and ground objective resolved are different facts. A destroyed escort should not automatically erase surviving delivered infantry.

Use zero escorts for a light transport-only profile or one required escort for a supported escorted profile. Two escorts per package and arbitrary transport/escort ratios are not implemented. Match reservations and replacement prices to the actual package, then verify native boarding, flight, landing and cleanup.

## Implementation and acceptance order

1. Force-profile selection and reservations: test deterministic eligibility and logged selections for all four types, including light and escorted airborne variants.
2. Motorized: crew, board, drive, unload, ground orders and cleanup.
3. Mechanized: dismount plus separate crew/vehicle support orders.
4. Armour: vehicle-aware damage, mobility and combat outcomes; crew survival alone must not mean the armoured attack remains viable.
5. Helicopter insertion: pilots, boarding, flight, LZ, unloading and withdrawal.
6. Gunship escorts, then multi-aircraft packages.

Enable each profile for random QRF selection only after its physical acceptance test passes for that faction. Keep the same profiles usable for scheduled attacks with separate selection weights.

For every package, verify correct factions, unique package indices, one activation per operation, all crew/passenger seats assigned, objective approach, vehicle/crew losses, cancellation during spawning/transit/unloading, and complete tracked-entity/reservation cleanup. Current force records track hulls, groups, crews, passengers, damage observers and package-attributed funding. Multiple packages can share one operation; native acceptance must verify the complete lead wave and staged reserves.

Implementation status and first-run instructions are in [FORCE_POOL_QUICKSTART.md](FORCE_POOL_QUICKSTART.md). The earlier infantry setup remains documented in [OPERATION_EXECUTOR.md](OPERATION_EXECUTOR.md).
