# Strongholds

A stronghold is a faction's home base, built and defended so the Resistance cannot take it with an early rush. Only its own army or the Resistance can ever capture it. Chicken has one at each end of Everon:

| | Fortress Vostok | Stronghold Delaware |
|---|---|---|
| Army | Invaders (USSR by default) | Occupiers (US by default) |
| Stable ID | `everon_fortress_vostok` | `everon_airport` (unchanged, so saves keep working) |
| Where | Saint-Pierre harbour, along the dry quay strip north of the basin (9990, 1538) | Everon International Airport, north coast |
| Location type | `MILITARY_BASE` | `AIRPORT` |
| Composition | `AROR_Defence_Fortress_Vostok.et` | `AROR_Defence_Stronghold_Delaware.et` |
| Generator | `tools/Set-FortressVostok.ps1` | `tools/Set-StrongholdDelaware.ps1` |
| Reinforcements launch from | Beach landing (no friendly bases nearby) | Nearest US base; beach landing as fallback |
| Reinforcement mix | `invader_ground_l5` | `occupier_ground_l5` |

Both mixes are level 5 combined arms: 3 motorised, 3 mechanised and 2 armour groups. The generators check the files without arguments and rewrite them with `-Apply`.

On the map (and in HICOMM) a stronghold is drawn as the national flag of the army holding it, instead of a military frame, so the objectives stand out: the Soviet flag on Vostok, the US flag on Delaware. If the Resistance takes one, it shows the Resistance's colours.

Related changes: the seaport at Saint-Pierre moved to Morton harbour, retaining its stable `everon_saint_pierre_port` ID and entity name for existing references. Its display name is Morton Port. Crow Hill Outpost (Durras), the old Invader foothold, now starts as an Occupier outpost with US stores. The Invader territorial campaign uses Vostok as its land base. Existing campaign ownership still comes from saved state; the new starting ownership is a fresh-campaign fixture.

## Fortress Vostok

An 80 x 46 m rectangle of high sandbags laid along the quay strip, which is only about 50 m deep (a square compound reached into the harbour basin), with:

- six gun pits facing the landward approaches (two north corner bastions, two north-wall guns and two flank guns) with low sandbags across their fields of fire;
- USSR guard towers in the two seaward corners, each manned by a marksman;
- two USSR helipads in the courtyard, each with an unmanned Mi-8 transport;
- a level-4 garrison: eight four-man infantry teams, six crewed static guns, three manned armoured vehicles, a spare parked hull and the two helicopters, each with an authored position;
- a keep against the seaward wall (command bunker between two barracks bunkers, screened from the courtyard), a north vehicle gate towards the town with guard posts and hedgehogs, and an outer belt of hedgehogs and wire on the three landward sides;
- four USSR stores (weapons, ammunition, equipment, medical) in the keep.
 on narrow sites can declare a rectangular footprint (`m_fDefenceHalfDepth`); the terrain check, placement bounds and site audit then use that depth.
-gun markers can carry an elevation (`m_fElevation`) to mount a gun on a tower or roof: the garrison places it on the first solid surface near that height. When there is no platform, or its parapet masks the gun, the gun is placed on the ground beside it instead. Helipads tagged with `AROR_LandingPadComponent` hold their parked helicopter on the deck, and their props do not count as obstructions.

## Stronghold Delaware

Built on top of the existing 88 m airbase compound, adding:

- an outer 128 m ring of high sandbags with a south gate, guard posts and hedgehogs, and a hedgehog and wire belt outside it;
- two corner gun bastions in the outer north-west and north-east corners, making six crewed static guns, with embrasures cut through the outer ring for every gun;
- three US helipads with sandbag revetments on the open south apron between the walls, clear of the gate lane, each with an **unmanned UH1H**;
- four extra infantry teams in the outer band: twelve teams of four in total, plus the airbase's three manned armoured vehicles and spare hull.

The helicopters are garrison "parked vehicles", like the spare armour hull. They spawn when players come near, stay destroyed once lost, and are handed over to the world (never respawned) if the Resistance takes one.

Chicken's airport keeps its location root fixed and has the airbase compound (flag, walls, markers) moved about 50 m west and 65 m north of it in the world layer. Everything Delaware adds is centred on that compound (the offset is in `Set-StrongholdDelaware.ps1`). If the compound is ever moved again, update the offset and regenerate.

To place things in the outer band, the garrison footprint is 64 m (the maximum) instead of the airbase's 44 m. The whole 132 m square must therefore pass the terrain check (up to 3 m of height difference).

## Early-game retaliation

For the first **hour of campaign time** (counted only while players are online), each stronghold counts its defenders killed by players. When players have killed **4**, its army launches a **retaliation**: its level-5 mix in **two waves**, whatever its current Readiness, Crackdown or funds. The kill count restarts when a retaliation launches, so after it ends, another 4 kills inside the first hour triggers another one.

- A kill counts when a player hit the defender in the 30 seconds before it died. Kills by AI, including High Command squads, do not count.
- Infantry and weapon/vehicle crews count; destroying an empty vehicle or helicopter does not.
- Everyone gets a headline, e.g. `FORTRESS VOSTOK RETALIATION`.
- The headline waits for confirmed deployment. A request still preparing or retrying after failure does not repeatedly announce inbound forces.

## Final defence

After the first hour, an **assault** is either a Resistance player inside the stronghold's radius or a player killing one of its defenders. An assault draws one level-5 mix, again whatever the army's Readiness, Crackdown or funds. When that response ends, another assault can draw a new one after 30 minutes. Everyone gets the headline `<NAME> UNDER ASSAULT`.

If a response ends without ever deploying (no executor capacity, no route, no launch site), the stronghold tries again 30 seconds later rather than waiting for the cooldown.

## Where reinforcements come from

- **Friendly base** (Delaware): the army's best safe military site, picked exactly as for any other QRF.
- **Coastal landing** (Vostok, and Delaware's fallback): any authored **landing points** on the component, in order. Otherwise the server searches rings around the stronghold for low dry ground with open sea within 100 m and a road nearby. Vostok searches 1.2–4.5 km across an arc centred on north; Delaware searches 0.8–4.5 km all round. The point must also be safe to launch from (on land, away from players and the target).

If neither is available, the stronghold logs a warning and tries again in 30 seconds.

## Tuning

Everything is an attribute on `AROR_StrongholdComponent`, set on the composition and overridable per placed site:

| Attribute | Default | Meaning |
|---|---|---|
| `m_eHomeRole` | Invaders | Whose stronghold it is |
| `m_iProtectionSeconds` | 3600 | Length of the early-game window |
| `m_iRetaliationKills` | 4 | Player kills that trigger a retaliation |
| `m_sRetaliationRecipeId` | `invader_ground_l5` | Retaliation mix (always two waves) |
| `m_sFinalDefenceRecipeId` | `invader_ground_l5` | Final-defence mix |
| `m_iFinalDefenceRearm` | 1800 | Seconds before another assault can draw a final defence |
| `m_iResponseLevel` | 5 | Readiness and Crackdown both responses are frozen at |
| `m_bLaunchFromFriendlyBase` | off | Launch from a friendly base when possible |
| `m_aLandingPoints` | none | Authored landing points, tried first |
| `m_fLandingBearing` / `m_fLandingArc` | 0 / 180 | Coastal search arc (compass bearing, degrees) |
| `m_fLandingMinDistance` / `m_fLandingMaxDistance` | 1200 / 4500 | Coastal search distances |

Two new `AROR_GarrisonComponent` attributes build the bigger rosters and work on any military site: `m_iExtraInfantryTeams` (0–8) and `m_aExtraParkedVehicles` (extra unmanned vehicles, not for outposts). Each extra team or vehicle needs an authored position marker.

## How it is built

- `AROR_StrongholdComponent` (`Scripts/Game/Strongholds/`) runs on the server once a second. It watches the garrison's living defenders through their damage managers, counts player kills, detects assaults and requests the responses.
- Responses use `AROR_OperationManagerComponent.RequestScriptedQRF_S` (`Scripts/Game/Operations/AROR_ScriptedResponses.c`). It creates an ordinary QRF with a named mix, Readiness and Crackdown frozen at the given level, funding waived and, optionally, a fixed launch point. The executor then plans, spawns, runs the waves, withdraws and cleans up as for any other QRF.
  - Still applied: Invader operation admission (`m_bInvaderOperationsEnabled`), executor actor capacity and launch-safety checks.
  - Skipped: scheduler cooldowns and the per-role QRF cap. Existing nonterminal operations for the same role and target still block a scripted request.
- `AROR_LocationComponent.CanBeTakenBy` asks the stronghold component which roles may take the site.
- Extra parked vehicles are saved as variants of the parked-vehicle slot, so a reload re-binds the helicopters rather than turning them into the faction's spare armour.
- Expanded defence recipes apply when a new roster is generated. Existing saved rosters and casualties are not refilled to add the new teams or helicopters.
- Saved (`AROR_StrongholdComponentSerializer`): early-window time used and the player kills counted towards the next retaliation. Live responses and the final-defence cooldown are session-only.
- Reconciliation: an unowned coastal landing is not a safe friendly recovery base. A tactical dispatch receipt survives completion between stronghold polls, and an externally removed/retired defender is not counted as a player kill merely because its entity disappeared.

## Known limits

- Taking a stronghold does not end the campaign yet.
- Near the airport, Delaware and its neighbours can ask for up to 112 garrison personnel against the shared cap of 96, so their deployment will be staged. Raise the garrison manager's personnel cap in Chicken if that feels too slow, at some performance cost.
- The UH1H must accept the US faction when it spawns. If the log shows `asset faction binding failed` for it, the helicopter prefab's default faction needs checking.

## Checking it in Workbench

Nothing here has been compiled or played yet.

1. Compile Game scripts and let Workbench discover both new compositions and their `.meta` files. Their GUIDs (`7C1E5A0D93B4F268`, `5D2A8E41C7B03F96`) are hand-authored. Reload Chicken from disk.
2. **Vostok**: run **Ground fortified outposts**, inspect and save, then **Audit Fortress Vostok clearing** to confirm the 80 x 46 m footprint passes (`current ... suitable=1`).
3. **Delaware**: the airport is not covered by **Ground fortified outposts**; use **Ground military preset walls** for its new props. Check the outer wall, pads and revetments sit on open ground around the airport compound and don't cut across runways, taxiways or hangars. The airport root is fixed. If the 132 m footprint fails the terrain check, the log says `perimeter terrain rejected`. Ground the new props with the wall grounding command, then save.
4. Check Morton Port sits on the Morton quay (4957, 3876) and Crow Hill shows as an Occupier outpost.
5. Start a fresh campaign. The map should show **FORTRESS VOSTOK** (red) and **STRONGHOLD DELAWARE** (blue), with every non-city name to the right of its icon.
6. Approach Delaware: three UH1Hs should appear on their helipads with no crew, alongside the larger garrison. At Vostok, check the two Mi-8s sit on their helipads clear of the walls and towers, and a marksman stands on each guard tower.
7. In the first hour, kill four defenders at either stronghold as a player. Look for `[AROR STRONGHOLD] ... retaliation launched` and `[AROR] Scripted QRF ...` in the log, the headline, and the second wave following the first. Vostok's should land on a beach; Delaware's should come from a nearby US base.
8. To test the final defence without waiting an hour, set `m_iProtectionSeconds` low on a placed stronghold (and restore it afterwards). Then walk into its radius and check the response and headline.
9. Save and load during the first hour and check the window and kill count carry over. After loading, Delaware's helicopters should still be helicopters.
