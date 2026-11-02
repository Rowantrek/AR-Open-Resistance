# Missions (OPS)

The Resistance runs side missions from **OPS**, a landscape phone app styled as a rugged field tablet: a login with a boot log, then an operations board with filters, a mission list and a dossier for the selected mission. Every Resistance player can browse the board. Only Resistance Leaders (see `docs/HIGH_COMMAND.md`) accept or abort operations. Rewards are Resistance funds and manpower for now.

Code: `Scripts/Game/Missions/` (server/shared), `Scripts/Game/Phone/AROR_OpsApp.c`, `AROR_OpsCrest.c` and `AROR_PhonePooledApp.c` (client). Component: `AROR_MissionBoardComponent` on `AROR_GameMode_Base.et`.

## Mission types

Each mission picks its target when it is offered, and spawns its enemies and props when a Resistance player comes within the deploy range (1.2 km). Convoys are the exception: they set off as soon as they are accepted. Enemy units and vehicles come from the enemy role's faction preset (riflemen, leader, automatic rifleman, machine gunner, crew, officer, supply trucks, armed cars). Civilian prisoners come from the Civilians preset.

| Type | Category | Target | Objective | Fails if | Default reward | Time |
|---|---|---|---|---|---|---|
| Supply Theft | Logistics | Enemy-held military site | Seize the guarded supply truck and drive it into any Resistance-held location. The truck is kept. | Truck destroyed | $1,200, 3 MP | 45 min |
| Convoy Ambush | Assault | Road route between two sites held by the same enemy, 1.5–5 km apart | Destroy the vehicles (armed escort and cargo truck) or kill their crews | Convoy reaches its destination | $1,800, 2 MP | 40 min |
| Prisoner Rescue | Rescue | Enemy-held settlement or military site | Clear the guards and reach the three prisoners to free them; they then walk to the nearest friendly location | All prisoners killed | $600, up to 6 MP (paid per prisoner who arrives) | 50 min |
| Supply Sabotage | Sabotage | Enemy-held military site | Destroy both parked supply trucks | — | $1,500, 1 MP | 45 min |
| Officer Assassination | Assault | Enemy-held settlement or military site | Kill the visiting officer (two bodyguards and a security detail) | The officer leaves when time runs out | $2,000, 1 MP | 35 min |
| Arms Cache Recovery | Logistics | A 150 m search area in the countryside 300–700 m from a town | Find the crate and stay beside it for 10 s; 60% of caches have an enemy patrol | — | $900, 2 MP | 60 min |

All missions also fail when their time limit runs out. **Threat** comes from the target's type: towns are low; cities, outposts, factories and resources moderate; ports and military bases high; airports extreme. Each threat level adds 25% to the funds reward and one guard; high and extreme threats add 1 MP.

Prisoners count as civilians while detained, so the guards leave them alone. Once freed they become Resistance and can be shot on the way out. Rescue completes when every surviving prisoner reaches the destination; manpower scales with the number actually delivered. No reward is paid while survivors are still on the way. A truck counts as seized once it has moved 40 m from where it was parked.

## Board and lifecycle

- The board holds 6 offers. Each new offer is the mission type least represented on the board and among active missions, so the board stays varied. Two missions never share a location.
- Offers expire after 30 minutes. A taken or expired slot is refilled after 90 seconds.
- At most 3 missions are active at once. Accepting is refused if the offer's site has changed hands since it was offered. Active missions waiting to deploy also fail if their target becomes invalid; cache recovery is independent of the nearby town's owner.
- An active mission's status line (for example "Truck seized: drive it into a Resistance-held location") shows in OPS, on the game map and in the notifications.
- Success pays Resistance funds and manpower and shows a headline. Failure (including abort) shows a notification.
- The leader who accepted a mission, or a host, can abort it (two clicks). After reload, any current Resistance Leader can abort it and receives its progress hints. Nothing is lost but the reward.
- Leftovers (guards, vehicles, wrecks, unsecured crate) are removed once no Resistance player is within 400 m, or after 15 minutes regardless. A delivered truck and a successfully secured cache stay. Vehicles with a player aboard are left in the world and released from cleanup tracking.
- Failed deployment immediately deletes partial spawns before retrying, including pending groups; it does not wait for proximity cleanup. Extender missions must track provisional entities/groups in the shared arrays. `Release_S` handles mission completion, not deployment rollback.
- Offers are not saved; the board rerolls on load. Active missions are saved with their identity, target, rewards and time left. A loaded mission deploys again from the start the next time the Resistance comes near.
- A loaded convoy waits for players near its origin, then starts over there. New convoys still deploy on acceptance. Crew members must materialize before crew casualties can complete the mission, and dropped boarding requests are retried until surviving crew members are seated. Boarding cannot advance just because 120 seconds elapsed.

## OPS app

| Screen | What it shows |
|---|---|
| Login | The OPS crest (sight ring and amber pointer), a typed boot log (uplink, cell roster, intel feed with the number of offers, enemy crackdown from Operations), the player's callsign, clearance (LEADER or OPERATIVE) and the resistance name, and AUTHENTICATE. Non-leaders sign in view-only. |
| Board | Top bar with Resistance funds, manpower and active operations (x/3). Filter chips by category with counts. The list shows active missions first, each with a threat stripe, name, ID, category, grid, and an OPEN / ACTIVE / URGENT tag with its time limit or countdown. The network log is below the list. |
| Dossier | Kicker, mission name and codename, then facts (threat, time limit or time remaining, squad size, grid). Active missions show a status banner. Then situation, objectives, failure conditions and the outcome on success (funds, manpower, opposition). On the right: the area map (the HICOMM terrain map, zoomed to the target, with its area circle, codename, any destination and nearby locations in their controller's colour; + / - zoom) and recommended kit. ACCEPT OPERATION or ABORT OPERATION at the bottom, with a note explaining when it is disabled. |

Active missions are also drawn on the in-game map: a pulsing amber diamond with codename, type, time left and search area, plus a green square at a destination. They appear on HICOMM's Strategic Command map as amber diamonds with a dashed line to any destination.

## Configuration (on `AROR_MissionBoardComponent`)

- **Mission types**: per type, enabled, funds, manpower, time limit (minutes) and guards. Types not listed use the defaults in the table above.
- **Board**: size, most active missions, offer lifetime, refill delay.
- **Ranges**: deploy range, cleanup range and cleanup timeout; update and resync intervals.
- **Prefabs**: empty group prefab, defend and move waypoint prefabs, cache crate prefab (base-game US wooden weapons box).

Extender mods add mission types through `modded enum AROR_EMissionType` and an `AROR_Mission` subclass returned from a modded `AROR_MissionBoardComponent.CreateMission`. Its text goes in `AROR_MissionTexts`.

## Networking

Clients send `AROR_RequestMissionAccept / Abort` through their player controller; results come back as hints and notifications. Every client parses a replicated string snapshot on the game-mode component: one row per offer and active mission, plus the network log. The server republishes it when anything other than timers changes, and every 30 seconds to resync countdowns, which clients run locally between updates.

The snapshot includes the accepting leader's session ID; a restored mission uses 0 for shared leader management. Server and clients need matching scripts (22 fields per mission row). Existing version-1 saves remain compatible; session IDs are not persisted.

## Workbench test checklist (not yet run)

1. Script Editor compile.
2. Play the Chicken world and start a new campaign. Open the phone (Ctrl+Y). An OPS tile should be on the second row; OPS turns the phone to landscape and shows the login boot log.
3. AUTHENTICATE as the host (LEADER). The board should list 6 offers of different types with grid references, and the dossier's area map should zoom to the selected target.
4. Accept one mission of each type in turn and check:
   - it moves to the top of the list as ACTIVE with a countdown;
   - its marker appears on the in-game map (M) and in HICOMM Strategic Command;
   - a notification is shown.
5. Teleport (Ctrl+T admin menu) within 1 km of the target and check that guards, vehicles, prisoners or the crate spawn. Complete the objective and check the headline, reward in the top bar and network log.
6. Convoy: accept it and watch the convoy board its vehicles and leave (log line "convoy has left"). Check that the marker follows it every 30 s.
7. Abort an active mission (two clicks), let one time out, and destroy a supply-theft truck to check the failure paths.
8. Save with a mission active, reload, and check that it is still active with its time left and deploys again when you approach.
9. Join as a second player who is not a leader. OPS should sign in as OPERATIVE, and ACCEPT should be disabled with an explanation.
10. On a dedicated server, accept a distant convoy and leave it beyond AI activation range for over 120 seconds. It must not pay out or log departure with missing/unseated crews. Approach and verify seating retries, departure, crew casualties, hull destruction and destination failure.
11. Save an en-route convoy, reload with nobody connected, then approach its origin. It must restart only on approach. As a non-host Resistance Leader, abort a restored mission and verify progress hints reach current leaders; a non-leader must remain blocked.
12. Capture an accepted mission's target before approaching it. It must cancel without spawning its objective. Cache recovery must remain valid regardless of the nearby town's owner.
13. Force the second sabotage truck placement to fail. The first truck and any partial groups must disappear immediately, without accumulating across retries. Check occupied vehicles remain exempt.
14. Deliver only one prisoner while the others remain alive outside the destination: no success or reward yet. Deliver the remaining survivors (including a run with one casualty) and verify the manpower reward. Secure a cache, leave for more than 15 minutes, return and verify it remains lootable.

Mocked regression checks: `node tools/Test-MissionLifecycle.js`. These execute selected script method bodies with native services mocked; they do not compile Enforce Script or validate native AI/replication behavior.
