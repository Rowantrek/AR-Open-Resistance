# Truck and IFV combined QRF test

The [USSR/Invader framework](INVADER_QRF_FRAMEWORK.md) is staged with levels 1-5 and operation admission disabled; the current US/Occupier test remains selected.

This guide describes the fixed `combined_arms_l1` regression fixture. **Normal Chicken now enables strategic economy at starting war level 2**, which can select one truck plus two IFVs or the lighter truck/IFV tier. See [QRF strategy and prices](QRF_STRATEGY_ECONOMY.md).

For this exact two-vehicle test, set `m_bQRFStrategicEconomy = 0` on the operation manager and retain `m_sGroundQRFRecipeId = "combined_arms_l1"` on the executor. The helicopter override remains disabled. This authored tier 1 has **two elements**, not two copies of a profile and not a level-2 response. Re-enable strategic economy afterward to test payments and war-level selection.

| Element | Occupiers fixture | Personnel |
|---|---|---|
| Motorized | M923A1 transport truck | One driver and four-person US fireteam |
| Mechanized | LAV-25 IFV | Three crew and four-person US fireteam |
| Total | Two vehicles, four groups | Twelve actual personnel; eighteen reserved slots |

Both use the configured `AROR_OCCUPIERS` faction. An equivalent Invaders binding resolves the existing Ural/BTR profiles to `AROR_INVADERS`. Crew requirements come from those concrete profiles; the roster CSV's nominal crew estimates do not override them.

The entire response reserves four groups, eighteen personnel slots and two vehicles before any members enter the engine queue. Both elements must be ready/boarded before activation. A required element lost during spawning fails and cleans up the undispatched response. After activation, losing one package leaves the other operating. The new [shared tactical commander](QRF_TACTICAL_COMMANDER.md) retains nearby terminal forces and their capacity until age AND all-player distance permit cleanup.

Tactical coordination is now enabled: the truck delivers infantry and waits; the IFV delivers infantry and covers a separate reachable road sector. Troops exit before investigating fresh contact and searching separate town sectors. Contact changes, new waves and losses can retask surviving packages at a bounded interval. Launch seeds use 35 m lateral spacing around the selected origin, then each vehicle must obtain a clear road pad within 150 m of that origin, at least 25 m from existing operation vehicles. Up to 12 road queries are allowed per package; blocked/steep/wet candidates are rejected. All driver/crew/passenger compartment occupancy must be physically confirmed after direct seating before activation. The nearest eligible friendly outpost remains the origin when available, otherwise configured role coordinates are used. Convoy driving and matching arrival times are not guaranteed. This two-element tier-1 fixture stays in one wave; larger responses can use the new deferred reserve policy.

## Run

1. Stop the old Play session, compile Game scripts successfully, then reload `AROR_Everon_Chicken` from disk.
2. Start Play, complete campaign setup and attack the existing assigned US defender in Provins.
3. Look for `Composite QRF ... recipe=combined_arms_l1 tier=1 elements=2 reserved groups=4 personnel=18 vehicles=2`.
4. Expect one `Ground package queued` line for `us_motorized` and one for `us_mechanized` under the same operation ID. Both packages should board and the operation should reach ACTIVE (`state=2`).
5. Let both vehicles approach and infantry exit before destroying them. Check the truck and IFV use their different deployment policies and infantry search sectors. Then test one active vehicle loss: the surviving element must continue. Restart and test cancellation or a required vehicle loss before activation: no partial response should dispatch and all reservations should release.

To restore the previous helicopter experiment, clear **Ground QRF Recipe Id**, enable **Reaper QRF Test**, and set **Workbench Response Level** to the desired air level. An explicit ground recipe takes priority if both modes are configured. Clearing the ground recipe with REAPER off restores the existing homogeneous profile selector; the previous forced US-only profile is now cleared.

## Configuration and status

`AROR_GroundQRFRecipe` references ordered existing profile IDs and an authored level. Repeating an ID deliberately adds another complete element. Profiles must be enabled, QRF-eligible, ground-supported and match the recipe's role/faction. Recipe IDs may be reused across roles, but duplicate ID/role pairs are rejected. One to eight elements and tiers 1-5 are supported; the selected list is deployed once.

The operation records recipe identity, response tier and expected package count separately. Recipe/profile bindings are mutually exclusive and cannot reroll. Shared budgets include other active/reserved/retained forces. Unsupported airborne elements fail validation; mixed ground/air recipes and resource-aware selection remain future work. There is no campaign spending or stock reservation in this prototype.

Earlier logs verified mixed spawning, boarding, correct campaign factions and activation. The new tactical/wave source and updated Workbench smoke tests are uncompiled/unrun in the engine. Follow the acceptance checks above and the tactical guide before treating revised delivery, search, withdrawal, retention or performance as verified.
## LAV passenger-exit regression

After compiling and reloading Chicken, trigger a ground response containing a LAV. All four infantry passengers should exit while the driver and turret crew remain aboard. The log should reach `delivered infantry; separate infantry search and vehicle support` for the LAV package, without an exit-deadline withdrawal. Initial seating remains direct placement.

Exit requests are staggered, try alternate usable doors and leave doors open. `m_iQRFExitRecoveryDelay` on the executor defaults to 10 seconds (0 disables recovery). After that delay, a still-seated AI passenger whose normal exit is rejected may use the native cleared exit transform at a stopped ground vehicle. The candidate must be within 12 metres of the hull and within 2 metres of terrain height; native clearance failure leaves the passenger aboard for later retries and the existing timeout. Aircraft, crew, players and active exit/seat-switch animations are excluded. This is a local dismount fallback, not a teleport to the town.

Offline actual-source regression: `node tools/Test-QRFUnloading.js` (40 mocked assertions, including nearby AI driver recovery). Native animations, exit clearance and multiplayer replication require Workbench/Play verification.

Airport ingress regression: capture the airport in normal strategic mode. Expect three road-launch lines, followed by confirmed seating and ACTIVE. Travel distances must decrease from Farm toward the airport. Check all packages reach their road delivery points, unload and investigate; an ACTIVE acknowledgement alone is insufficient. Missing pads or incomplete seating should fail/wait explicitly rather than claim delivery. Offline checks: node tools/Test-QRFIngress.js (21 mocked assertions).
