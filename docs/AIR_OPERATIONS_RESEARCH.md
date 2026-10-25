# Flight, landing and escort waypoint investigation

Investigated on 2026-10-23 using the installed Workbench API documentation/resource index and primary public sources. This is a capability assessment and proposed implementation, not a working airborne executor. The core still depends only on the base game, and no external flight mod has been installed or referenced by the addon.

## Verified evidence

| Evidence | What it supports | What it does not establish |
|---|---|---|
| Installed `AIHelicopterMovementComponent` and its inherited movement interface | Path inspection, following another entity's path, completion checks | A complete pilot, terrain-aware route builder, landing command or gunship engagement controller |
| Installed `HelicopterControllerComponent` | Simulation access, collective mode, autohover, brakes and engine/controller interfaces | A mission-level FlyTo/Land/Escort API |
| Installed `SCR_TransportWaypoint` | Driver reference and boarding-wait timing | Helicopter flight or landing by itself |
| Base-game waypoint resources | Attack, defend, get-in/get-out variants and ground movement resources exist | A verified native aircraft landing/escort waypoint resource; none was identified in the installed waypoint resource scan |
| Cinematic helicopter helpers | Cinematic tracks expose forces/throttle or periodic force pulses | A reusable combat transport pilot; cinematic movement is not evidence of safe multiplayer mission execution |

The public [AIHelicopterMovementComponent reference](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceAIHelicopterMovementComponent.html) also lists inherited path functions, without a landing/escort mission interface. Absence from these inspected APIs is not proof that no internal engine behaviour exists; it means a native end-to-end implementation has not been verified.

The author of [REAPER_AiHelicopters](https://reforger.armaplatform.com/workshop/69A4D664A6284E06) documents custom movement/landing waypoints, an AI controller, troop transport and gunship attack features. Its [changelog](https://reforger.armaplatform.com/workshop/69A4D664A6284E06/changelog) records landing, takeoff and save/load fixes. This demonstrates a mod-level solution exists; those custom APIs are not part of the base game. The revised adapter now uses setter/target-filter signatures evidenced by the independent integration source linked in the test guide; installed-version compilation remains required. The listed game version must not be taken as confirmation of compatibility with this local project.

## Conclusion

Waypoints are a suitable mission-order format, but our existing `group.AddWaypoint(groundWaypoint)` path does not establish helicopter piloting. There are two separate layers to build/verify: a flight controller that can physically execute aircraft orders, and a mission coordinator that sequences transport, unloading, support and return. The recommendation is to preserve the core's flight-adapter boundary, with an independent native controller implementation or an optional extender integration once its real API is available. The original architecture kept flight dependencies optional. Jasper subsequently requested direct loading through the current project for this experiment; that branch exception is recorded in ARCHITECTURE.md.

## Proposed transport waypoint sequence

| Stage | Order | Required completion evidence |
|---|---|---|
| Prepare | Reserve infantry, pilots, transports, gunships and an LZ | Every required element fits budgets, resources load, and seats are assigned |
| Takeoff | Climb toward a safe departure point | Aircraft airborne with stable clearance; pilot alive; time limit respected |
| Transit | Follow approach waypoints | Horizontal distance, altitude band and route progress; no completion based on proximity alone |
| Approach | Reduce speed and align with the LZ approach | Acceptable speed/descent profile and LZ remains usable |
| Land | Execute landing at a reserved LZ | Ground contact or a verified landed state plus low horizontal/vertical speed for a settling interval |
| Unload | Release passenger infantry through valid exits | Living passengers have actually left; touching the LZ does not count as delivering them |
| Depart | Take off and leave the landing lane | Aircraft clears ground troops and other approaches |
| Return | Follow return route and land at its home/holding location | Flight controller confirms return or a bounded failure/abort |

Each leg needs a deadline, failure reason and abort/alternate route. Proposed order data includes world position, altitude above terrain, speed, acceptance radius and stage-specific completion tests. Terrain height must be sampled along the route, not only at the destination. Slopes, obstacles, rotor clearance, approach/departure corridors and occupied LZs require explicit validation. A ground waypoint's completion radius alone cannot certify a safe landing.

The controller must handle pilot death, engine damage, blocked landing, threatened LZ, aircraft collision risk and cancellation. Do not teleport an aircraft into a landing state or report passenger delivery based only on elapsed time. Player occupation must remain protected during cleanup. Saving aircraft mid-flight will need native persistence integration and reapplication of non-saved controller policy; that is outside the current session-local operation model.

## Proposed gunship escort logic

Each gunship needs its own group, route and controller state; it must not receive the transport's landing order.

1. Join an offset escort lane before the transports commit. Required escorts must be ready before dispatch; optional escorts require an explicit different profile policy.
2. Cover the transit/approach with spacing and altitude separation. Multiple gunships need distinct lanes or staggered support slots, not identical coordinates.
3. At the LZ, hold an overwatch route outside the transport's approach/departure corridor. Engage only valid hostile targets through a verified weapon/targeting interface; ordinary Attack waypoint assignment is not proof that an aircraft can aim and fire rockets.
4. Keep support bounded by time, ammunition, fuel and aircraft condition. Pursuit must not pull required escorts indefinitely away from the transport mission.
5. On infantry delivery and transport departure, transition to a configured support or return route. Escort loss, transport loss and delivered-infantry survival are distinct outcomes.
6. Cancellation retires the whole flight package through the same operation IDs and reservation accounting. Losing one aircraft should degrade the response according to remaining capability, rather than deleting healthy delivered infantry automatically.

The level-1–5 design scales complete air packages in the same way as ground packages. Before enabling it, implement aircraft-specific budget/LZ concurrency limits and verify that the chosen number of escorts can be supported; the current ground vehicle budget is not an adequate airspace policy.

## Current direct QRF experiment

Jasper selected direct REAPER QRF testing with occupied midair insertion. The normal Open Resistance project now directly depends on REAPER and can override new QRF dispatches with one to five transport/fireteam/gunship packages, honoring the Workbench response-level override, sharing campaign factions and normal operation ownership/reservations. The transport must physically land and passengers must exit before receiving ground objective orders. After unloading, transport takes off on an outward egress leg; gunship support uses moving passes rather than a stationary Hover order. Pilot/aircraft losses retain casualties, and terminal cleanup waits for a minimum age plus distance from every player. Chicken explicitly enables the experiment; the switch defaults off elsewhere. Random airborne selection remains excluded. Disabling dispatch still leaves the project dependency loaded.

Aircraft now spawn in checked air volumes and are held against falling while personnel board; construction is limited to one pair per executor tick. A cached multi-pad search checks at most two candidates per tick and stops after 73 or enough separated landing sites. Distinct sites land concurrently; partial results queue only shared-pad users. Blocked-site replacement searches are package-local, exclude other reservations and share a fleet-wide budget. This replaces repeated full landing/ground-assembly scans. This revision needs engine and performance verification. Installed SDK signatures confirm `BaseVehicleControllerComponent.ForceStartEngine(ForceStartEngineParams)` with `m_bAirborne`, `IsEngineOn()`, `IEntity.SetOrigin`, `Physics.SetVelocity/GetVelocity` and helicopter simulation `HasAnyGroundContact()`. These native interfaces support the experiment; they do not prove REAPER order consumption, stable flight or gunship targeting. Custom pilot/Move/Land/Hover resources were verified in the installed REAPER index; packed controller method signatures were not guessed.

The first run confirmed basic flight, landing and unloading but exposed friendly explosive damage and immediate package cleanup. The revised LZ/formation/engagement/egress/retention implementation is uncompiled and untested. Reopen the normal project and use the existing Chicken world; the tutorial and separate diagnostic probe are optional. See [the direct test guide and acceptance checklist](AIRBORNE_QRF_PROTOTYPE.md). Bounded sampled LZ clearance checks and relative ingress guidance are implemented, pending engine verification. Full route certification, rigid formation control, durable persistence remain future work. Multi-package dispatch is now implemented for the level-5 fixture but still needs compilation/flight verification. REAPER setter and target-filter signatures are evidenced by the linked integration source in the test guide, pending compilation against installed 1.0.8.
