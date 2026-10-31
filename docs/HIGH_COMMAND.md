# High Command (HICOMM)

Resistance Leaders raise AI squads and order them to attack or defend locations through **HICOMM**, a landscape phone app in a phosphor / Carrier Command terminal style. This first pass covers structure and functionality only; features, visuals and balance are added afterwards.

Code: `Scripts/Game/HighCommand/` (server/shared), `Scripts/Game/Phone/AROR_HiCommApp.c` and `AROR_HiCommMap.c` (client). Component: `AROR_HighCommandComponent` on `AROR_GameMode_Base.et`.

## Screens

| Screen | Who | Purpose |
|---|---|---|
| Login | Anyone who opens the app | A "global network uplink" screen: a rotating wireframe globe with relay nodes and data arcs, and a connection log that types out (relay, encryption, operator, clearance). Only Resistance Leaders get the LOG IN button; others see ACCESS DENIED. |
| Command | Leaders | Resistance name (host renames it), campaign summary, and the list of players with their role. The host delegates or revokes leaders here. |
| Strategic Command | Leaders | Orders only, RTS-style. The zoomable phosphor map fills the screen: terrain-traced coastline, smooth shallow-water and high-ground shading, contours, the road network (from the AI road network), adaptive grid with grid references, and every location shown with the in-game map's icon for its type, tinted with the controlling faction's colour. The order of battle floats over the left (collapsible); the orders panel (MOVE / ATTACK / DEFEND / HOLD, target, EXECUTE) appears on the right while squads are selected. Legend, scale bar, cursor grid reference, hover tooltips, controls help (?). |
| Unit Management | Leaders | Squad catalogue with funds and manpower costs and a RAISE button, plus the full order of battle (strength, order, commander). |

The session's login lasts until the phone closes. Losing leader status drops the app back to the login screen.

Map controls: left-click a squad (or its order-of-battle row) to select it; shift adds to the selection; left-drag box-selects your squads. Left-click a location to target it, then EXECUTE; right-click a location to attack or defend it at once, or right-click open ground to move there. With MOVE chosen, left-click open ground picks the move point. HOLD stands the selection by immediately. The order defaults to ATTACK for enemy-held targets and DEFEND for Resistance-held ones. Zoom is smooth and continuous: the zoom slider on the map's right edge, the mouse wheel (around the cursor), Q/E or the + / - buttons from the whole island down to about 1 km across; the grid refines from 2 km to 50 m lines with matching grid references. Right-drag or arrow keys/WASD pan; FIT, HQ and SEL recentre the view. Hovering shows squad or location details and the cursor grid reference. Squads that overlap on screen are drawn as a stack: a dot at each squad's real position and their symbols in a column above on a leader line. Active OPS missions (see `docs/MISSIONS.md`) are drawn as amber diamonds with their codename, and a dashed line to any destination.

## Rules

- **Leaders**: the host is always a leader. The host is the listen-server or Workbench player; on a dedicated server, every connected admin counts as host. The host can delegate any connected player and revoke them again. Each leader has their own command: they see all squads but can only order their own. Squads of a revoked or disconnected leader keep their orders and become *unassigned*; the host can take one over by ordering it.
- **Raising**: costs Resistance funds **and** manpower. Each leader can command a capped number of squads (default 4). Squads form up at the Resistance HQ; members spawn through the engine's AI queue and get the Resistance faction.
- **Orders**: Attack an enemy-held location, Defend a Resistance-held one, Move to any land point (squads stand by there on arrival, so they can be pre-positioned near an objective), or Hold where they are. Several squads moved together spread out around the point. A new order replaces the previous waypoint. The server re-checks every request.
- **Losses**: a squad that never spawns within the form-up timeout is refunded. A wiped-out squad is removed without a refund.
- **Refunds and saving**: squads are not saved. When the campaign ends, every living squad is refunded at 100% of its cost. Every save also stores Resistance funds and manpower as if the living squads had been refunded, so loading any save keeps their value.
- **Debug starting values**: the Resistance currently starts with $20,000 and 40 manpower so squads can be tested. Rebalance later.

## Configuration (on `AROR_HighCommandComponent`)

- **Squad catalogue**: callsign prefix, name, member character prefabs, funds cost, manpower cost. The default catalogue uses the base-game FIA rifleman only: Rifle Squad (6 men, $2,000, 6 MP) and Fire Team (4 men, $1,200, 4 MP). Add MG/AT/medic prefabs here in Workbench.
- **Limits**: per-leader squad cap, form-up radius and timeout, status update interval, and the longest allowed resistance name.
- **Prefabs**: the empty group prefab plus the attack and defend waypoint prefabs (base-game defaults).
- **Map bounds**: the area HICOMM's map shows. The default is Everon's 12.8 km square. Extender maps set their own; the coastline is sampled from the terrain automatically.

## Networking

Clients send `AROR_RequestHiCommRaise / Order / SetLeader / Rename` through their player controller. Results come back as hints. Clients draw HICOMM from a replicated string snapshot on the game-mode component: host IDs, delegate IDs, name, and one row per squad. The server refreshes it every few seconds and only replicates when something changed.

## Workbench test checklist (not yet run)

1. Compile scripts and reload `AROR_PhoneMenu.layout` and the game-mode prefab. Watch for errors about `OnGameModeEnd`, `CanvasWidget` draw commands or `EditBoxWidget`; these APIs are used here for the first time in this project.
2. Play Chicken and create a new campaign. Open the phone (Ctrl+Y): there should be an HC tile on the second row. Open it: the phone turns to landscape and shows the login screen with clearance `RESISTANCE LEADER (HOST)`.
3. Log in. On the Command screen, rename the resistance and check the name updates.
4. Strategic Command: the coastline and 1 km grid should line up with Everon, and locations should be coloured by owner. If the map is offset or scaled on a high-DPI screen, the canvas coordinate units need adjusting (see `AROR_HiCommApp.UpdateMap`).
5. Unit Management: raise a Rifle Squad. Funds should drop by $2,000 and manpower by 6, and six FIA soldiers should appear at the HQ on the Resistance side (not hostile to the player).
6. Strategic Command: select the squad, click an Occupier location, and EXECUTE. The squad should move to attack. Then order DEFEND on the HQ.
7. Save, then load. Funds and manpower should include the squad's refund, and the squad should be gone.
8. Multiplayer: as host, delegate a second player. They log in and raise their own squad. Each can see the other's squad but can't order it. Revoke the second player: they drop to login and their squad shows as unassigned.

## Next steps

More order types (move, recon, ambush, hold, waypoint chains), platoons, live enemy intel and threats on the map (the REBINTEL panel), squad specialisations, map pan/zoom, a saved resistance name, styling to the mockups, and balance.
