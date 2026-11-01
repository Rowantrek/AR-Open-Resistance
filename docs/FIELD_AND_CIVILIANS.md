# FIELD tools and ambient civilians

Status: implemented in source and wired to `AROR_GameMode_Base.et`; native acceptance is pending. There is no command-line Enforce compiler or game runtime in this workspace. Mocked checks do not establish that Workbench compiles the scripts or that native AI drives correctly.

## Player controls

Open the phone with **Ctrl+Y**, then **FIELD** (first tile on the third row).

### Map markers

- Select **MAP MARKERS**. Enter a caption and cycle sharing, type and expiry.
- **NEW HERE** creates an annotation at your position; **NEW ON MAP** closes the phone and opens the map for one placement click. Closing the map cancels placement.
- Sharing: **PRIVATE**, **SQUAD** (your native player group), or **RESISTANCE**. Native group membership is required for squad sharing. The server filters each recipient's snapshot; hiding a marker is not merely a client-side UI rule.
- Types: note, danger, rally, supplies and numbered route points. Route points are independent annotations numbered in creation order, not an AI path or an automatically drawn route line.
- Select an authored marker to edit its caption, sharing or type with **UPDATE SELECTED**, reposition it with **MOVE ON MAP**, or delete it with two clicks. Other players' shared annotations are read-only.
- Expiry: persistent, 10, 30 or 60 minutes. Updating a marker starts its chosen expiry again. Private and Resistance annotations are saved with the campaign; remaining expiry time resumes on load. Squad annotations last only this server session.
- Defaults: 32 markers per author and 256 per campaign. Coordinates follow HICOMM's configured map bounds.

### Personal squad

- Select **PERSONAL SQUAD**. Recruit riflemen while within 75 m of the Resistance HQ; each costs **$200 Resistance funds and 1 manpower**. The default cap is eight per player and 32 across the server. No leader delegation is required.
- Click a recruit to select it; **SELECT ALL** applies orders to your entire personal roster. This roster is separate from HICOMM squads and does not alter the player's native squad membership.
- **FOLLOW**, **HOLD** and **REGROUP** manage foot movement. Regroup reissues follow; it does not teleport companions to the player. Native navigation and combat awareness govern actual movement.
- **PASSENGER**, **DRIVER** and **GUNNER** board the player's current vehicle or the vehicle the player is looking at. Assign driver/gunner to individual recruits. Vehicles must be friendly/civilian, stationary, within 25 m of the player and within 30 m of the recruit. Occupied seats are never evicted. Boarding uses the existing native compartment insertion API and has a 20-second timeout; order acceptance is separate from actual seat occupancy.
- **DISMOUNT** requires a stopped vehicle and a clear native exit near ground level. Retry after stopping if no safe exit is available. Foot orders require dismounting first; this pass does not add a player-directed vehicle driving waypoint interface.
- Cycle the transfer recipient, then **TRANSFER**. Both players must be living Resistance members within 30 m, and the recruit must be within 50 m of the recipient. Ownership and identity change together; the former owner loses command.
- **DISMISS AT HQ** needs two clicks, with the player and recruit both near HQ and the recruit out of a vehicle. Eligible recruits return their purchase value to the Resistance.
- These companions are session-only, following HICOMM's existing economy policy: saved funds/manpower include the value of living and pending recruits. Loading returns that value without recreating the personnel. Failed spawning, disconnect and campaign end return eligible value; dead recruits do not receive refunds. A dead player's surviving recruits wait until their owner respawns or disconnects.

## Civilians

The game-mode template now carries `AROR_CivilianManagerComponent`, enabled by default. It populates registered **Town** and **City** locations while at least one player is within **1 km of the settlement centre**. This is shared town activity, not a new population for each player.

Defaults:

| Setting | Default |
|---|---:|
| Pedestrians per active settlement | 8 |
| Occupied cars per active settlement | 2 |
| Global people cap, including reserved drivers/passengers | 48 |
| Global ambient vehicle cap | 10 |
| Minimum pedestrian spawn distance from every player | 125 m |
| Minimum vehicle spawn distance from every player | 250 m |
| Town retention radius | 1,300 m |
| Unobserved cleanup delay | 60 s |
| Spawn/seating timeout | 90 s |

At most one activity is constructed per five-second update. Each pedestrian has an individual roaming group. Cars receive a civilian driver and may have one passenger, using the Civilians faction preset's **MOTOR_CAR** catalogue. They receive road destinations and use native AI driving, rather than scripted vehicle movement. Spawn and destination points stay within the settlement's activity radius; the engine may choose a road detour beyond it. Empty roads, unavailable prefabs or blocked spawn positions cause a bounded attempt to fail and retry on a later update.

Successful arrivals generate another destination, keeping occupied towns active. Failed routing or three consecutive stalled movement checks retire an activity until players leave. A stalled check first attempts destination replacement. Dead bodies, failed constructions and retiring groups remain in the activity budget until distant cleanup, preventing infinite replenishment beside a player. Spawn timeouts cancel pending native queue requests. Cleanup retains a town while players are nearby and also checks individual members and vehicles, so an activity cannot disappear simply because its first member moved away.

Civilians use the existing non-military Civilians role, already friendly to all campaign armies. They neither capture locations nor report military casualties to Operations. They are not recruitable as soldiers in this pass. No civilian support penalty or other new economy is introduced.

A car observed with a player aboard is marked claimed and retained when ambient activity is cleaned up, even after the player gets out. Civilian people and unclaimed cars are session-only. Cars stored in the Resistance Garage follow its existing persistence rules. A claimed car alone is not made persistent by this feature.

## Extension and resource notes

The inherited game-mode template gains `AROR_PlayerMarkersComponent`, `AROR_RecruitmentComponent` and `AROR_CivilianManagerComponent`. Extender scenarios inheriting that template receive the features automatically; they can disable civilians or tune their budgets in their prefab. Personal recruitment and civilian assets use the selected faction presets rather than fixed FIA/CIV asset references. Existing classes and configurations are not renamed.

`UI/Layouts/Phone/AROR_Field.layout.meta` has a **hand-authored new resource GUID**, `{7D0A31C4B95E6201}`. The three new prefab component instance IDs are also hand-authored. Open/import the layout and game-mode prefab in Workbench and verify their identifiers before release. Existing asset GUIDs were reused from repository sources, not invented. No resource database or AGENTS.md was modified.

The new squad-sharing API was checked against [Bohemia's GroupsManager API](https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__GroupsManagerComponent.html); edit-box calls follow the [native EditBoxWidget API](https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfaceEditBoxWidget.html). Spawning, waypoint, faction and compartment calls follow existing project implementations.

## Verification

`tools/Test-FieldSystems.js` executes extracted Enforce method bodies with mocked native services, plus resource wiring checks. Run it with `node tools/Test-FieldSystems.js`. It checks annotation ownership/privacy/squad changes, invalid inputs and caps; recruit ownership, transfer and refunds; civilian retention, timeout cancellation and claimed vehicles; and phone/prefab/widget integration. It does not simulate native replication, collision detection or pathfinding.

### Required Workbench acceptance (not run)

1. Open the addon and **compile all scripts in Script Editor**. Open the FIELD layout and game-mode prefab; check missing-resource, unknown property and component errors. Confirm the new layout GUID resolves.
2. **Play Chicken in World Editor**, create a campaign and open FIELD. Verify portrait/home and landscape sizing, caption input, tabs, paging, selection and confirmation buttons. Reopen the phone repeatedly and confirm no stale widgets or handlers remain.
3. Create each marker kind and scope. Place through the map, cancel placement and reopen normally; location info cards must work again. Edit, move and delete a selected marker. Try a long caption containing delimiters and markup. Confirm route points show their numbers and text has a dark backing plate.
4. Host plus at least two clients: place private, same-squad and Resistance markers; check both displayed markers and recipient snapshots. Change native groups and confirm access updates within two seconds. Another client must be unable to edit/delete the author's marker. Join late and verify the allowed snapshot arrives. Verify a non-Resistance character cannot receive or change field state.
5. Save and load persistent/expiring markers. Confirm author permissions survive reconnect and expiry resumes with the saved remaining time. Squad markers must not return after load. Check old saves created before these components were added.
6. Recruit at HQ, verify exact fund/manpower debits and preset rifleman assets, then select one/all and test follow, hold and regroup over terrain. Try another player's recruit ID, caps, insufficient funds and recruiting away from HQ. Confirm failed queued formation refunds once and cancels its group.
7. Test passenger, driver and gunner roles on supported cars/trucks. Check full seats, moving vehicles, hostile vehicles, out-of-range references, destroyed cars and failed seating. A successfully seated recruit must remain boarded after driving away. Stop and dismount on clear ground; dismount must fail safely while moving or without an exit. Check weapon crew behaviours separately in native AI.
8. Transfer one recruit, then all, to a nearby player; verify recipient caps and former-owner rejection. Kill one recruit, dismiss another at HQ, disconnect with survivors and save/load with survivors/pending formation. Check funds/manpower include only eligible value and never double-refund at game end.
9. Visit a town and city. Allow the paced population to build. Verify civilians walk and civilian cars have living drivers, start engines and actually drive through road destinations. Check curved roads, bridges, narrow streets and settlements by the coast. Confirm all three armies remain neutral to civilians.
10. Put two clients in one town, then in different towns. Verify shared per-town counts and global reservations (including cars still seating), no spawning inside exclusion distances, and no new AI during setup. Approach and leave repeatedly to check cleanup and replenishment.
11. Kill civilians, block a car and fail a spawn/seat. Confirm finite retries, queue cancellation and no deletion near players. Take a car, get out, leave the town and return; it must remain. Garage a taken car, save/load and confirm existing garage state still works.
12. Repeat core cases on a dedicated server, including joining late, owner death/respawn, disconnect and shutdown. Check no UI runs there, no orphaned groups remain, and AI/performance budgets hold with the configured player count. Record actual results before marking these systems implemented in architecture.
