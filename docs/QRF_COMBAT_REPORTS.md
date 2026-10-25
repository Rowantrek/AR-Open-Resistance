# QRF reports from real combat

`AROR_QRFDefenseComponent` connects actual hostile damage on assigned defenders to the existing QRF scheduler. Attach it to a defending `SCR_AIGroup`, an individual character, or a vehicle. For groups, it monitors their controlled member entities and refreshes membership once per second, including members spawned after the component starts. It does not scan the map or automatically turn attacking forces into garrisons.

This is implemented source requiring Script Editor compilation and gameplay verification. No combat test has been run by Codex.

## First test in Everon_Chicken

1. Compile scripts, then open `AROR_Everon_Chicken`. On the game mode's `AROR_OperationTestComponent`, clear **both** simulated QRF target fields. Leave both automatic attack targets empty. Restart the world for a fresh cooldown/budget.
2. Chicken now includes `AROR_Provins_QRFDefender`, a US rifleman at the location centre (`5503.833 96.354 6081.837`) with `AROR_QRFDefenseComponent` enabled, role `OCCUPIERS`, and target `everon_provins`. Check his terrain/building placement visually in Workbench. For another scenario, add the same component to a defending character or group; groups must actually spawn their soldiers.
3. Keep the existing Chicken executor enabled. For a small deterministic first test, set its `m_sWorkbenchForceProfileId = us_motorized` and `m_iWorkbenchResponseLevel = 1`. Confirm the launch point is clear and reachable.
4. Play, complete campaign setup, and damage the defender using a hostile Resistance character. The scenario's diplomacy must make that faction an enemy of US. Expect `Defense under attack: role=1 target=everon_provins QRF=operation_...`, followed by the scheduler's PLANNED, SPAWNING and ACTIVE logs. A fresh accepted report uses the existing reaction delay, default 10 seconds.
5. For Invaders, repeat in a fresh session with a USSR defender, `INVADERS`, and forced profile `ussr_motorized`. Clear the forced profile and set the forced level to `0` to restore randomized supported profiles and levels 1–5.

The saved Chicken preset clears both simulated-report targets and now forces `us_mechanized` at level 3 for Jasper's three-LAV test. The level-one motorized settings in the first-test steps above remain a smaller diagnostic option. The defender references the installed base-game US rifleman, with its GUID verified read-only against the game resource index; no new prefab/resource GUID was created. Reload Chicken from disk to see these placements/settings, preserving unsaved edits first. The original Everon world is untouched by this feature.

For comparison, Jasper's manually placed rifleman is retained at `5489.166 95.901 6086.587`, named `AROR_Provins_ManualQRFDefender`, and now has the same defense component. Workbench saved both characters under the same prefab group, confirming the prefab reference is identical and neither has an instance faction override. Diagnostic messages include their owner entity to distinguish them. This comparison rules out a saved prefab/faction-override difference; runtime affiliation and damage-instigator checks still need verification.

The subsequent runtime log confirmed both defenders were unassigned (`defender faction='', expected='US'`). A US character prefab reference did not establish runtime affiliation. That test used `m_sInitialFactionKey = US`. After integration with Rowan's infrastructure, both defenders instead enable `m_bInitializeFromDefendingRole` and resolve `AROR_OCCUPIERS` through the shared role component. On the first server monitoring tick, this opt-in scenario initializer sets the existing faction affiliation component only if it is empty, then attaches damage monitoring. Expect `Scenario defender faction assigned` for each unassigned rifleman and subscription diagnostics showing `AROR_OCCUPIERS` in the current fixture. It does not replace an existing faction, continuously enforce factions, assign territory, or initialize group members. For other scenarios, configure actual defender affiliation during placement/spawning, or use this optional initializer for unassigned standalone defenders. Its default is empty.

## If spawn acknowledgement times out

The later live test reached profile selection but exposed an unassigned spawned truck, followed by `ScriptInvoker: Recursive call of Invoke!` during failure handling. The executor now initializes an empty vehicle affiliation to its owning commander's faction, still rejecting conflicting affiliations. Operation notifications queue nested state changes until the current callback returns; the Workbench scheduler smoke test includes a nested-resolution regression probe. These fixes require recompilation and another live test.

That session also logged `No AIWorld is present in the map`. Chicken's Managers layer now places the installed base-game `SCR_AIWorld_Eden.et` for Everon navigation; its resource GUID was verified against the installed read-only index. Reload the world and verify the AIWorld error disappears and vehicle/infantry navigation initializes before assessing driving. The original Everon world has not been changed.

The 19:24–19:26 follow-up run successfully spawned the truck and five personnel, boarded them and became ACTIVE without the earlier recursion/faction failure. It revealed two further issues: the group-added event passes only `AIAgent` (handler corrected), and the AIWorld prefab initializes empty navigation worlds without explicit files. Chicken's saved Managers layer now binds `Soldiers` to `worlds/GameMaster/Navmeshes/GM_Eden.nmn`, `BTRlike` to `worlds/GameMaster/Navmeshes/GM_Eden_vehicles.nmn`, and `LowRes` to `worlds/MP/Navmeshes/LowResEden.nmn`. All resource GUIDs were verified against the installed index. Component identities and native serialization follow [Bohemia's Everon sample](https://github.com/BohemiaInteractive/Arma-Reforger-Samples/blob/main/SampleMod_NewFaction/Worlds/MP/CTI_Campaign_Eden_base_Layers/AI.layer) and its [three-component showcase sample](https://github.com/BohemiaInteractive/Arma-Reforger-Samples/blob/main/SampleMod_Main/Worlds/Modding/Assets_Showcase_Base_Layers/default.layer). Reload Chicken from disk before saving; if navigation remains cached, close and reopen World Editor. Confirm the `No navmesh file specified` warnings disappear. Navigation-file configuration is required by the [BI navmesh tutorial](https://community.bohemia.net/wiki/Arma_Reforger%3A_World_Editor%3A_Navmesh_Tool_Tutorial?useskin=darkvector). These runtime milestones do not yet prove driving or delivery.

The corrected scheduler prints `Operation notifications: nested changes queued` at startup; its absence means that run cannot verify the recursion fix. Dispatch now fails explicitly if no AIWorld exists. Ground packages log changing spawn/boarding/transit/dismount stages and terminal force-record cleanup, so an ACTIVE operation alone is not treated as evidence of successful driving or delivery. After arrival, defense forces retain their orders until a trusted combat/territory outcome or timeout; there is no fabricated victory based solely on arrival.

`Defense under attack` confirms the damage report worked. `Executor requested` confirms dispatch, but not that an executor exists or has spawned anything. Stop Play and check the actual `AROR_GameMode_Base1` instance in Chicken has `AROR_OperationExecutorComponent` with `m_bEnabled` checked. Keep your unsaved defender placements/settings when saving the world.

After recompilation and a fresh Play session, capture the startup log as well as the attack:

| Log | What it establishes |
|---|---|
| `Operation executor disabled` | Component exists but its enabled setting is off |
| `Invalid or duplicate operation executor` | Startup configuration validation failed |
| `Operation force executor enabled` and `Executor configuration` | Native executor started; configuration counts and budgets are printed |
| `Operation executor connected to scheduler` | Executor subscribed to operation dispatch |
| `Executor waiting for ...` | Selection/dependency/budget blocker; repeated identical blockers are suppressed |
| `selected force profile` then `Ground package queued` | Selection succeeded and member spawning was requested |

An absent enabled/disabled/validation message requires checking that the component exists on the game mode actually running, and reviewing earlier script errors. If packages are queued but never become ACTIVE, include all subsequent script/runtime errors and whether the truck, crew and infantry appeared. Increasing the timeout does not identify the underlying blocker.

The October 23 startup log exposed two corrected setup defects: script declaration initializers emptied the designer-authored profile/level arrays, and Chicken's commander/scheduler component IDs were absent from the shared base prefab, creating duplicate managers. Attribute arrays now initialize only when missing after deserialization, and the base prefab uses the existing component IDs from Chicken. Recompile and reload the world/prefab after these changes, preserving any unsaved defender placements first. The next startup should retain all eight configured profiles and five levels, without duplicate-manager errors. Runtime spawning remains to be verified.

## Level 3 LAV faction check (October 23)

The 19:49 Play session selected `us_mechanized`, level 3: three LAV packages. All three boarded and the operation became ACTIVE at 19:52. The existing log contains no faction-mismatch failure, but does not record attacker/victim pairs, so it cannot establish whether apparent fire between vehicles was deliberate friendly targeting, crossfire, or another event.

That run still reported `OnAgentAdded` expecting `AIGroup` while the event supplied `AIAgent`. The last recorded Game script compilation was 19:24, before the corrected single-argument handler and town-search changes. Stop Play and compile Game scripts in Script Editor before replaying; restarting Play alone has continued to use the older compiled code.

Current spawning initializes empty or explicitly declared native asset affiliations to the commander's configured campaign key and assigns crew and infantry groups before requesting members. Mismatched personnel, groups or vehicles fail validation. Vehicle affiliation is also checked during monitoring. Before each package deploys, `Deployment faction verified` prints the expected key and actual vehicle, crew-group and infantry-group keys; the level 3 US fixture should print AROR_OCCUPIERS for every faction field, three times.

Chicken enables the executor's optional `m_bLogFriendlyDamage` diagnostic. It watches operation-owned characters and vehicles and logs the first attributed same-faction damage event per victim as `Same-faction damage`, including operation, package, faction, victim, attacker and instigator player ID. This records damage rather than intent; it does not suppress friendly damage or change diplomacy. Missing instigators/affiliations cannot be classified. Subscriptions detach during force cleanup. Disable the diagnostic after verifying the test.

These added checks and diagnostics still require Workbench compilation and a fresh Play test.

## Report rules

The prepared defender has `m_bDebugReports` enabled while combat reporting is being verified. After the first membership tick, look for `QRF defense diagnostic` with `Damage subscriptions=1`. A hit that is ignored logs its filter reason, including faction keys when diplomacy rejects it. Identical reasons are suppressed for five seconds. This distinguishes a missing subscription, missing instigator attribution, faction mismatch/non-hostility and scheduler rejection without bypassing any validation. Set this option off when testing is complete.

- Server only, after campaign readiness, for an explicitly assigned Occupier or Invader defense.
- The victim must currently have the configured commander's faction and be inside the assigned registered location's radius. The attacker may fire from outside it.
- Damage must be positive and have an identifiable hostile instigator with a faction affiliation. Healing, regeneration, self damage, friendly/neutral damage, missing instigators and Game Master actions are ignored. A lethal hostile hit can still request a QRF.
- There is no fallback to the last remembered killer, projectile faction, or nearest town. Unresolved instigators are ignored; artillery/vehicle weapons need runtime verification of their instigator attribution.
- One group-wide attempt at most every five seconds by default. Scheduler cooldowns, deduplication, capacity and conflicting-operation rules still apply. Duplicate reports do not reset preparation or reroll the selected force.
- A rejected event is discarded. There is no delayed queue of old attacks. New hostile damage after the report interval may try again; continued fighting can request a later QRF once the previous operation is terminal and cooldown/capacity permit.
- Accepted damage now snapshots the attributed attacker's position through `ReportAttackAt_S`. This is event-time damage-source intelligence, not a continuous player lookup or verified visual sighting. The shared tactical plan uses recent pre-search reports for bounded investigation, freezes at search start, then patrols separate marker sectors. See [QRF search](QRF_TOWN_SEARCH.md). A stale snapshot cannot bypass the normal fresh-damage rules to create another QRF.
- Subscriptions detach when group members leave or are deleted, when the defense is cleared, or when the component is deleted. A member newly joining a group may take up to one second to acquire its subscription. Attach directly to a vehicle to monitor vehicle damage; attaching to a group monitors its controlled members, not every vehicle they use.

## Garrison and territory integration

The assignment is a trusted declaration by the scenario or garrison owner. The component verifies faction and position, but cannot establish territorial ownership because that system is not implemented here.

```c
AROR_QRFDefenseComponent defense = AROR_QRFDefenseComponent.Cast(defendingGroup.FindComponent(AROR_QRFDefenseComponent));
if (defense)
    defense.BindDefense_S(AROR_EFactionRole.OCCUPIERS, "everon_provins");

// On capture, retreat, or reassignment, stop reporting for the old defense.
if (defense)
    defense.ClearDefense_S();
```

The component must already exist on the group/prefab. Bindings, observers and throttles are session-local. After a future save restore, the garrison system must recreate the assignment. Clearing one defender does not cancel a shared QRF: other defenders may still need it. The authoritative territory system must separately cancel obsolete operations through `ResolveOperation_S` when control changes. Arrival still does not prove a successful defense or capture.

## Workbench acceptance checks

Run each role in a fresh session, then check:

| Case | Expected result |
|---|---|
| Hostile hit inside the assigned location | One QRF, using that defending role's pool |
| Several members hit rapidly | One outstanding request; preparation and chosen force stay unchanged |
| Healing, friendly fire, self damage, environmental fall, GM kill | No QRF |
| Disabled component, empty/missing location, wrong victim faction, outside radius | No QRF |
| Damage before campaign readiness | No QRF, and no queued report after readiness |
| More hits while role cooldown/capacity blocks a request | No extra force; a subsequent fresh hit may retry later |
| Defender group receives/relinquishes a member | Subscription added/removed on the next membership tick |
| Clear assignment, delete defender/group, stop and replay | No stale reports, duplicate subscriptions or orphan timers |
| Dedicated server with two clients | Only server creates requests and forces |

The event adapter subscribes to `SCR_DamageManagerComponent.GetOnDamage()` with one `BaseDamageContext` argument. Installed official API docs describe the damage manager, instigator and faction interfaces; the callback argument was cross-checked against the [published base-game source listing](https://arexplorer.zeroy.com/_s_c_r___damage_manager_component_8c_source.html), which invokes the event with `damageContext`. Confirm compatibility with your installed scripts during Workbench compilation.
