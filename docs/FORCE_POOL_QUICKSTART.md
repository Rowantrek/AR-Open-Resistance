# Prepared Chicken force-pool test

**Current Chicken fixture:** [paid strategic selection](QRF_STRATEGY_ECONOMY.md), war level 2, tactical coordination and [FIA spotting](QRF_SPOTTING.md) are enabled. Responses come from five authored ground tiers with finite waves; REAPER dispatch and Invader admission remain off. This supersedes older forced-profile/level-5 instructions below. The exact [truck/IFV regression test](TRUCK_IFV_QRF_TEST.md) requires strategic economy off. Compile and reload before testing these additions.

The legacy homogeneous pool supports randomized strength levels 1-5 and supported profiles; see [RESPONSE_SCALE.md](RESPONSE_SCALE.md). Current strategic recipes use an explicit element list rather than multiplying one profile. The [REAPER test](AIRBORNE_QRF_PROTOTYPE.md) is a separate explicit configuration, currently disabled. Aircraft research remains in [AIR_OPERATIONS_RESEARCH.md](AIR_OPERATIONS_RESEARCH.md).

For real combat instead of a simulated report, follow [QRF_COMBAT_REPORTS.md](QRF_COMBAT_REPORTS.md): assign `AROR_QRFDefenseComponent` to a defender and clear the simulated target fields first.

Reload `AROR_Everon_Chicken` from disk before saving in Workbench: an already-open world can overwrite the newly added components and profile configuration.

The saved Chicken game-mode instance is configured with eight force profiles, all resource references, attack/defend/move waypoints, launch coordinates and an enabled executor. Simulated reports are currently disabled; shoot the prepared US defender in Provins for the combat test. The original Everon world and shared game-mode prefab have not been given this force-pool setup.

| Profile | Assets | Status |
|---|---|---|
| `us_motorized` | US fireteam, M923A1, US crew | Ground executor implemented; needs runtime test |
| `us_mechanized` | US fireteam, LAV-25, US crew | Ground executor implemented; needs runtime test |
| `us_armour` | Crewed LAV-25, no passenger infantry | Ground executor implemented; needs runtime test |
| `us_airborne` | US fireteam, UH-1H, pilot resources, HE gunship escort | Configured, disabled; flight/landing/escort adapter missing |
| `ussr_motorized` | USSR light fireteam, Ural-4320, USSR crew | Ground executor implemented; needs runtime test |
| `ussr_mechanized` | USSR light fireteam, BTR-70, USSR crew | Ground executor implemented; needs runtime test |
| `ussr_armour` | Crewed BRDM-2, no passenger infantry | Ground executor implemented; needs runtime test |
| `ussr_airborne` | USSR light fireteam, Mi-8 transport, pilot resources, HE gunship escort | Configured, disabled; flight/landing/escort adapter missing |

Armour currently uses base-game wheeled armoured vehicles, not tanks. Vehicle GUIDs were read from the installed game's resource index and cross-checked against the addon's known GameMode_Plain GUID. `FORCE_ASSETS.json` records the references. The index was only read, never edited; no new resource GUIDs were invented.

## First run

1. Compile in Script Editor. Reload Chicken, then inspect `AROR_GameMode_Base1` and confirm its executor and test components are present.
2. Inspect launch points `5100 0 6080` (Occupiers) and `5950 0 6080` (Invaders) around Provins. They are initial test coordinates, not verified navigable road positions. Move them onto clear, usable ground if necessary. Terrain height is applied at runtime; obstacles and route accessibility are not automatically checked.
3. Play and complete setup, then shoot the assigned US defender as hostile FIA; see [QRF_COMBAT_REPORTS.md](QRF_COMBAT_REPORTS.md). The preset forces `us_mechanized` at level `3`, spawning three LAV-25s, each with three crew slots and a fireteam. The complete response reserves six groups, 30 personnel slots and three vehicles, within Chicken's configured capacity. To test dispatch without shooting, set the Occupier simulated QRF target to `everon_provins`; it reports once after 15 seconds. Automatic attack targets are blank. The spawn deadline is 90 seconds for the complete response.
4. Expect a selected-profile log, `Ground package queued`, then ACTIVE once crews and passengers are seated and vehicle orders are issued. Observe the truck approach the dismount position, infantry exit, then infantry receive the objective waypoint. The truck crew waits at the dismount point. Initial boarding uses teleport seating at the launch point; dismounting uses normal exit animations.
5. Arrival only logs arrival; capture and successful defence require the future territory/combat integration. The existing active timeout still applies. Kill/cancel the force and verify crews, passengers, vehicle, bodies and waypoints are all cleaned up. Player-controlled entities and player-occupied vehicles are preserved.

An invalid vehicle faction, insufficient seats, unsuitable driver/vehicle, resource-load failure or missing approach route may fail or time out the request. These must be checked in Workbench; source/API checks are not a substitute for runtime testing.

## Other profiles and random selection

- Set `m_sWorkbenchForceProfileId` to `us_mechanized` or `us_armour` and restart for each test. Mechanized infantry dismounts while the crew receives its own objective support order; armour stays mounted.
- For Invaders, clear the Occupier QRF test target, set the Invader QRF test target to `everon_provins`, and set the forced profile to `ussr_motorized`, `ussr_mechanized` or `ussr_armour`.
- To use weighted selection, clear `m_sWorkbenchForceProfileId` and set `m_iWorkbenchResponseLevel` to `0`. Each role selects a feasible strength level, then its own enabled matching profile. Ground type weights are motorized 40, mechanized 30, armour 10. The configured airborne weight is 20 but excluded until flight support is implemented, so the enabled ground type weights are normalized over 80.
- Selected profile and level stay bound to the operation; capacity retries cannot reroll them. The complete response reserves capacity before member spawning, with each package releasing its share on loss or terminal cleanup. Surviving packages continue after individual losses. If no eligible choice fits, the request waits until capacity is available or its spawn deadline expires. There is no silent fallback to infantry when a configured pool has no eligible entry.
- Scheduled attacks use the same profiles, with separate attack weights. Disable simulated QRF reports when restoring automatic attack targets for the same role/location to avoid the existing conflict rule.

The pool is a designer-editable `m_aForceProfiles` array on the executor. Change or extend entries in an inherited scenario/game-mode setup for different factions. Roles and profile campaign faction keys must match `AROR_FactionRolesComponent`; changing Occupiers to another faction without supplying matching profiles intentionally leaves no eligible entry. Chicken uses `AROR_OCCUPIERS` with declared native asset faction `US`, and `AROR_INVADERS` with native asset faction `USSR`. Its defenders and player spawning use Rowan's shared role mapping and Resistance loadout. The first-spawn asset conversion and native group `SetFaction` integration need a fresh Workbench compile and test.

## Validation

Run scheduler/selection smoke tests in a fresh Chicken session with executor disabled and automatic targets blank by enabling `m_bRunSchedulerSmokeTests` on the test component. It runs both `AROR_ForcePoolSmokeTests` and `AROR_OperationSmokeTests`. The pool tests cover weighted-ticket boundaries, role/faction isolation, disabled/unsupported entries, separate attack/QRF weights and each budget filter. The operation tests also verify one-time profile binding. These Enforce tests have not been run by Codex.

The read-only asset check can be run independently:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./tools/Test-ForcePoolAssets.ps1 -ResourceDatabasePath 'D:/SteamLibrary/steamapps/common/Arma Reforger/addons/data/resourceDatabase.rdb'
```

Ground acceptance still needs vehicle seating, actual driving, route failure, all passenger exits, cancellation during materialisation/transit/dismount, vehicle loss before delivery, vehicle loss after delivery, simultaneous roles, engine dormancy and dedicated-server replication checks. Airborne packages stay disabled even if their enabled checkbox is changed: the selector explicitly excludes unsupported execution policies. Gunship/transport resource availability does not establish working AI flight.
