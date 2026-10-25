# QRF tactical commander and staged waves

Status: native Play verified truck/LAV spawning, boarding, factions, ACTIVE state, truck infantry delivery and continued movement after an empty truck cleared its faction. The last infantry reading remained short of the investigation point. Subsequent paid strategic selection, physical return/completion and [FIA spotting](QRF_SPOTTING.md) changes still need fresh Workbench compilation/Play. Chicken now starts at war level 2 with [money-backed strategic selection](QRF_STRATEGY_ECONOMY.md). Invader operation admission remains OFF; no USSR operation, territory/factory placement or resource production has been added.

## Responsibility and faction ownership

One server-owned `AROR_QRFTacticalCommander` belongs to each dispatched QRF operation. It coordinates the selected force after the strategic commander has requested a response. It uses the existing operation manager, shared role bindings, executor reservations, native AI groups and REAPER adapter. It does not create another faction, territory or economy authority.

| Role | Campaign faction | Current assets | Dispatch state in Chicken |
|---|---|---|---|
| Occupiers / BLUFOR | `AROR_OCCUPIERS` | US truck, LAV and UH-1 profiles | Existing Provins defender test |
| Invaders / REDFOR | `AROR_INVADERS` | USSR Ural, BTR, BRDM and Mi-8 profiles | Admission disabled; five ground tiers staged |

The same tactical code handles both roles. Asset selection stays in the existing role/faction profiles and recipes. Unit, group and vehicle faction audits still apply. The Resistance remains player-directed.

Strategic selection and tactical execution have separate responsibilities: the optional money-backed manager/executor policy selects an affordable response, while this commander executes its immutable recipe/profile and tier. The separate readiness-provider preview remains available for future physical inventory integration; stock regeneration and automatic preview callers are not connected.

## Response lifecycle

1. Validate and bind the complete response once. Reserve its entire group/personnel/vehicle budget before spawning actors.
2. Freeze the launch origin from the nearest eligible friendly outpost, or the existing configured role position when no eligible outpost exists.
3. Partition eligible large responses into a lead wave and finite reserve. Ground lead selection includes motorized, mechanized and armour elements where the recipe provides them.
4. Construct at most one ground element **or** one transport/gunship pair per executor tick. All required lead elements must finish spawning/boarding before the parent becomes ACTIVE.
5. Approach separate road points, deliver infantry, investigate reported contact and search assigned sectors. Protected vehicles cover separately; gunships retain their report-driven search/pass behavior.
6. Admit the original reserve elements after their delay, under live limits and existing aggregate reservations. Later waves join the same operation without a second ACTIVE transition.
7. Withdraw on task duration, severe casualties or ownership loss. Expire unused reserves rather than spawning them indefinitely.
8. Retain nearby physical forces/casualties after terminal handling. Release their capacity only through normal safe cleanup.

Pre-dispatch loss of a required lead element fails the whole undispatched response. Active losses degrade the response. A failed reserve construction does not cancel a healthy lead wave. Each authored element can be constructed only once; reserves are not an unlimited replacement pool.

## Two-wave policy

Defaults split responses at tier 3 or above **when there are at least four elements**. Lead size is the rounded-up half; the rest remains virtual and has no soldiers or vehicles yet. Small responses stay intact.

| Prepared ground response | Total elements | Lead | Reserve | Default live ground ceiling |
|---|---:|---:|---:|---:|
| Current US truck + LAV, tier 1 | 2 | 2 | 0 | 6 |
| Invader tier 1 | 2 | 2 | 0 | 6 |
| Invader tier 2 | 3 | 3 | 0 | 6 |
| Invader tier 3 | 5 | 3 | 2 | 6 |
| Invader tier 4 | 7 | 4 | 3 | 6 |
| Invader tier 5 | 8 | 4 | 4 | 6 |

The USSR rows describe prepared recipes, not enabled dispatch. Homogeneous US/USSR ground responses still use their selected level as total package count: level 4 splits 2/2 and level 5 splits 3/2. Recipe tiers remain distinct from element count.

For a tier-5 eight-element ground recipe, the lead contains at least one truck, IFV and armour element. At 60 seconds after first dispatch, reserve elements may construct until six live packages are present. The remainder waits for capacity. Loss of a package shortens the release delay to 15 seconds after first dispatch; it does not reset the timer or create a new allocation.

An air level-5 response remains five complete transport/fireteam/gunship packages. Three pairs dispatch first (six aircraft); two pairs remain virtual. A later pair requires **two** free live-aircraft slots. A successfully departed and removed transport can free one slot, but that alone cannot admit a pair. Healthy retained or disabled aircraft still occupy physical slots; a loss flag alone does not admit another pair. Delivered infantry remains owned and globally budgeted after its transport departs.

Waves may overlap under the live ceiling. Unspawned reserves expire 300 seconds after first dispatch. If survivors continue to occupy all slots, some authorized reserves can expire unused; this deliberately favors the cap over forcing every element onto the map. Expired reserves release only their unspawned allocation. They do not delete the lead force.

Disabling waves does not bypass live limits. A lead wave that cannot fit its configured domain ceiling is rejected before actors spawn, with a configuration error. Other operations can also consume the available slots; existing spawn acknowledgement timeouts remain the bound while waiting for capacity.

## Tactical behavior

### Evidence and search

Assigned defenders and operation-owned units already publish validated hostile-damage snapshots to the role-scoped report registry. The director checks for fresh reports within the defended town radius plus its configured search margin. It stores positions, not an enemy entity or a continuously updated player position.

Fresh ground reports can retask at most every 15 seconds. A first contact, expiry or cumulative movement beyond 75 metres from the last commanded contact changes the search version. Small incremental reports cannot prevent eventual retasking once their cumulative displacement exceeds that threshold. Stale/missing contact returns search to the town.

Every delivered fireteam first investigates the reported contact or town center, with separate nearby points, then moves through four vicinity sectors and holds a sector. This includes IFV-delivered infantry; its supporting vehicle receives separate cover orders. A leg advances on arrival or its bounded timeout. Walking deadlines scale with distance and configured planning speed, from 60 seconds to a configurable ceiling (180 seconds in Chicken), rather than abandoning a long investigation walk after 60 seconds. Progress diagnostics distinguish physical arrival from timeout. Points remain inside the defended vicinity. New waves and lost packages redistribute the surviving packages across available sectors, subject to the same retask interval.

This assigns investigation and containment orders to native AI. It does not implement room-by-room building clearance, omniscient detection, coordinated suppression or guaranteed road encirclement. Native navigation/perception/combat still determine how units move and fight.

### Ground delivery and support

Launch offsets run laterally around the selected origin. Road queries first project the origin and goal onto terrain height, including configured origins authored with Y=0. A bounded search tries the preferred approach wedge, adjacent wedges and direct origin-side lanes at nearby radii (12 queries by default). Results must stay within the defended vicinity, preserve a nominal standoff with at most 75 m road snapping, remain at least 40 m from launch and keep configured spacing from other ground approach points. Only exhausting these candidates rejects the package; no teleport or unvalidated direct route is substituted.

Ground QRF delivery prioritises the town objective. Profile/contact standoff is capped to 60% of `m_fQRFObjectiveDeliveryRadius` (60 m in Chicken); road candidates must be within that radius (100 m), inside the defended vicinity and at least 15 m from the centre. A stopped vehicle must physically reach within 20 m of its selected road point and remain within the objective delivery radius before unloading. Native waypoint completion does not relax these gates. The driver receives a hold order before staggered animated exits. Infantry tasks start only after living passengers are physically out. Ingress allows 600 seconds with up to four road retries after 30 seconds without 15 metres of movement. Unloading has a 30-second retry window; fixed security mode renews that window while holding the delivery point, bounded by the town task and outer operation timeout; delayed native cleared-position recovery is available for stuck AI ground passengers. Travel and recovery diagnostics distinguish failure to reach the delivery point from rejected passenger exits. These revised objective/exit rules require native verification.

The truck waits at the drop point. An IFV or armour package receives its own bounded reachable road cover point, normally on the origin side of reported contact. Infantry and vehicle task timers are independent, so passenger searching cannot starve vehicle support orders. Cover is reconsidered every 45 seconds. This is not a convoy driving controller and does not guarantee matching arrival times or terrain line of sight.

Player-commanded groups are not retasked. Personnel affiliation and vehicle-destruction validation remain in the native monitor. A native vehicle affiliation component may return an empty key or its declared asset key after occupants exit: this is tolerated for proven-empty passenger vehicles when their living infantry have left, including early native dismounts, never armour. Early dismounted infantry continue toward town instead of losing their package. A corresponding occupied-seat transition during unloading gets at most five seconds to settle. Missing components, unrelated keys and persistent occupied-vehicle mismatches still fail validation; no runtime faction repainting is performed. A vehicle audit failure degrades its own active package, preserving the other packages; required pre-dispatch package failures still cancel the complete recipe. Detailed audit diagnostics identify the package, profile, expected/actual key, occupancy and position.

### Air insertion and gunships

The existing incremental LZ planner, separate pads, direct landing, verified touchdown, animated exits, departure and predictive traffic yielding remain in use. Later-wave aircraft keep their boarding hold until they receive flight orders, even though the parent operation is already ACTIVE.

Landed air infantry uses the same rate-limited tactical search/sector logic as ground infantry. Aircraft departures are not counted as infantry casualties. Gunships continue to investigate recent allied reports and vary passes through the existing engagement and traffic policies; firing still requires native perceived hostile targets and friendly-clearance checks. The tactical commander adds wave admission, task duration and withdrawal, rather than replacing REAPER flight control. Collision-free flight and FPS are not certified by these source changes.

Ground recipes still cannot contain air profiles. Ground combined arms and complete air packages share the tactical policy, but a truck + helicopter + gunship recipe under one operation is a remaining executor extension.

## Withdrawal and persistence

The intended sequence is enter town, unload, investigate and hold town sectors. Fixed security mode now lasts 900 seconds from the first physically dismounted infantry entry into the 100-metre objective area (armour-only entry uses the hull). Later arrivals do not reset it. Capture does not end or restart this temporary timer; a post-recapture timer is not yet wired. In fixed mode, casualty-based whole-response and local-package tactical retreats are suppressed, while total-force losses, missing targets, lost origins and the outer timeout remain authoritative. The manager allows at least 2,700 active seconds for travel, holding and return. Set m_iQRFTownHoldDuration to 0 to restore the legacy 600-second mission/casualty policy.

`m_bTownCaptureQRFs` on the operation manager opts into town/city capture responses (off by default, on in Chicken). After campaign readiness, the server observes canonical location events and requests a response for the previous AI owner when a hostile role takes its settlement. Registration/restoration provides a baseline and does not spawn a historical response. Existing role gates, reaction delay, cooldown, capacity and same-target deduplication still apply; an existing defense QRF is reused. Invader admission remains off, so captured USSR territory cannot dispatch an Invader response until that separate gate is enabled. The separate m_bOutpostCaptureQRFs switch enables ordinary/military outpost control events in Chicken; it also covers airport/factory/port/resource control events. With strategic economy enabled, the shared admission path applies QRF prices and payments. The QRF investigates and holds; settlement support/capture mechanics still decide ownership.

Vehicles receive reachable return orders and dismounted troops move toward the safe origin. There is no teleport/remount shortcut. If an origin outpost changes hands, the commander seeks another eligible same-role outpost. If none exists, it holds troops/vehicles rather than ordering them back to the captured base. A missing return road also produces a hold. Configured fallback origins have no ownership entity to monitor.

Ground terminal retention requires **both** at least 120 seconds of age and at least 1,200 m separation between every owned member/vehicle and every controlled player. Stationary grounded passenger survivors may exit normally. Nearby wrecks and survivors remain, with their aggregate capacity reservation. Retained living packages also occupy the live-ground limit; corpse-only packages do not occupy that live limit but still hold aggregate budget until cleanup. Air retains its existing separate retention policy (600 seconds and 1,500 m by the current fixture). World unload releases the session records.

This is session retention, not saved campaign persistence. A force is not declared victorious, ownership is not changed and resources are not refunded by tactical arrival or withdrawal.

## Settings and performance

All settings are attributes on the existing operation executor. Tactical coordination defaults OFF for other scenarios and is explicitly ON in Chicken.

| Attribute | Chicken value | Purpose |
|---|---:|---|
| `m_bQRFTacticalCommander` | 1 | Shared ground/air tactical policy |
| `m_bQRFTwoWaves` | 1 | Keep the reserve unspawned |
| `m_iQRFSplitTier` | 3 | Minimum tier; also requires at least four elements |
| `m_iQRFReinforcementDelay` | 60 s | Normal reserve release |
| `m_iQRFReserveWait` | 300 s | Finite reserve deadline after first dispatch |
| `m_iQRFMaxLiveGround` | 6 | Live ground packages across the executor |
| `m_iQRFMaxLiveAircraft` | 6 | Live transport/gunship airframes across the executor |
| `m_iQRFTacticalDuration` | 600 s (legacy mode) | Active task deadline |
| `m_iQRFTownHoldDuration` | 900 s | Shared security hold from first physical town entry; 0 restores legacy withdrawal |
| `m_iQRFOrderInterval` | 15 s | Contact/roster/infantry retask interval |
| `m_iQRFApproachAttempts` | 12 | Maximum road queries for a vehicle's initial approach |
| `m_fQRFVehicleArrivalTolerance` | 20 m | Physical stopped-vehicle tolerance around the delivery road point |
| `m_fQRFObjectiveDeliveryRadius` | 100 m | Maximum distance from town centre for road-point selection and unloading |
| `m_iQRFExitRecoveryDelay` | 10 s | Delay before native cleared-position recovery for stuck ground AI passengers; 0 disables |
| `m_iQRFIngressTimeout` | 600 s | Ground travel deadline including bounded route recovery |
| `m_iQRFExitTimeout` | 30 s | Separate passenger-exit deadline |
| `m_iQRFInfantryLegTimeout` | 180 s | Maximum distance-scaled infantry leg duration |
| `m_fQRFInfantryPlanningSpeed` | 1.5 m/s | Walking-speed estimate used to budget a leg |
| `m_fQRFThreatDismount` | 200 m | Legacy threat standoff preference, capped by objective delivery policy |
| `m_fQRFWithdrawFraction` | 0.4 | Whole-force strength threshold without reserve |
| `m_fQRFLocalWithdrawFraction` | 0.5 | Local package strength threshold |
| `m_iQRFGroundRetention` | 120 s | Minimum ground retention age |
| `m_fQRFGroundCleanupDistance` | 1,200 m | Ground all-player cleanup distance |

The director evaluates each operation every five seconds on the existing server timer. Construction is limited to one ground element or air pair per tick; no per-frame search loop is added. Routine orders and road lookups are throttled. Full allocations, including virtual reserves and retained forces, still count against the original aggregate group/personnel/vehicle budgets.

The six-package ceiling does **not** mean six soldiers. Each package can include infantry and crew, and multiple retained forces may continue to use aggregate budget. Waves reduce simultaneous construction and constrain live deployment; they do not guarantee a particular server FPS. Measure spawn-time spikes, server frame time and AI counts before increasing limits. Retention intentionally trades available capacity for persistence near players.

## Verification and next Play test

The latest native run reached ACTIVE with both vehicles correctly affiliated and verified truck unloading. Three seconds after its infantry received an investigation order, a vehicle affiliation audit cancelled the entire operation; the old diagnostic did not identify the vehicle or actual key. The empty-vehicle exception, package-local audit handling and revised infantry walking behavior address that failure path, but require fresh Workbench compilation/Play. Thirty-three source-adapted checks pass for the revised validation, occupancy, walking deadlines and task progression with native dependencies mocked; installed configuration checks also pass.

Offline checks exercise source-adapted policy/lifecycle bodies with mocked engine operations: 582 policy assertions across five tiers and eight sizes, 25 ground lifecycle assertions, 29 air lifecycle assertions, 11 contact/roster assertions and three retained-aircraft counter cases passed. Installed-resource/configuration checks also passed for 25 GUIDs, navigation, eight profiles, all prepared levels/recipes, tactical settings and the disabled Invader gate. Workbench smoke tests are connected to the existing optional test runner and do not create QRFs or enable Invaders. These checks cannot establish Enforce compilation, native navigation, boarding, flight or multiplayer behavior.

1. Stop Play, compile Game scripts successfully, then reload `AROR_Everon_Chicken` from disk.
2. For the exact fixed regression test, disable `m_bQRFStrategicEconomy`, then run the existing US defender test in Provins. Expect `[AROR TACTICAL]` with tier 1, first wave 2, reserve 0, followed by one truck and one LAV under one ACTIVE operation. Both must remain `AROR_OCCUPIERS`. Normal Chicken leaves strategic economy enabled and can select a larger affordable tier; use its strategy/payment guide for that acceptance run.
3. Let them arrive without shooting the response. Verify passenger exit, then both fireteams walking toward their initial town/contact investigation points before sector patrol. Check infantry progress logs and the explicit physical-arrival message, truck holding and IFV road cover. An empty delivered transport must not cancel the healthy response; any actual mismatch must identify the vehicle/key. Attack another assigned defender away from the first contact to check bounded retasking. Then stop firing and check return to town search after report expiry.
4. Test the road stop: the LAV should hold its driver, unload infantry, then cover while infantry investigates and patrols. Check travel logs if it remains mounted. Test an authoritative town control change both with and without an existing defense QRF: the previous hostile Occupier owner can request one response; an existing QRF continues investigating rather than withdrawing because of capture. It must not flip settlement ownership directly or dispatch Invaders while disabled. Then test active losses, nearby retention, withdrawal and cancellation with a second player near the force.
5. In a separate explicit test configuration, use a tier-4/5 ground response with at least four elements. Verify first-wave size, 60-second reserve delay, live cap, finite unused-reserve expiry and no duplicate ACTIVE acknowledgement. The normal truck/LAV fixture has no second wave by design.
6. For flight testing, clear the ground override, explicitly select the existing REAPER test and level 5. Expect three pairs initially and at most six live aircraft; later pairs need two free slots. Check boarding holds, independent pads, actual exits, gunship reports, traffic yielding and departure before judging performance.
7. Keep Invader admission OFF during these tests. Enable a separate USSR test only when explicitly ready to establish valid Invader-owned targets/origins. The optional readiness preview remains disconnected from dispatch and stock spending.

Extenders: opt in on the executor, preserve immutable tier/package semantics and account for virtual/retained capacity. Existing non-tactical scenarios retain their execution path. Resource owners can later supply readiness/reservation data through the existing provider seam; no factory, garrison or territory implementation is duplicated here.

Town-hold regression: node tools/Test-QRFTownHold.js checks actual-source timer boundaries, independent operation timeout and bounded stalled-route recovery with mocked native services. Compile/reload Chicken and verify each package reaches town, infantry investigate, survivors remain for the full 15-minute arrival-based hold, then withdraw without nearby deletion. Native driving, unloading and the complete hold are not certified by the offline tests.

A later native capture test confirmed the capture event but failed second-package road planning from Pinewood Lake. The approach search now tries a whole-town ring after its preferred ingress wedges and can use the safest compact candidate with at least 18 metres separation when preferred spacing cannot fit. Objective-distance limits and the query budget remain unchanged. Airport origins are eligible alongside ordinary/military outposts. These changes need native validation.

Ground launch/readiness correction: strategic outpost coordinates are now search anchors for clear nearby road pads rather than direct vehicle positions. Native seating is verified before readiness; premature foot orders are suppressed while the group remains partly seated before authorized delivery. This applies to both role bindings. See tools/Test-QRFIngress.js and the airport regression in TRUCK_IFV_QRF_TEST.md; native acceptance remains pending.

Combat affiliation follow-up: the next airport run verified road ingress, armour arrival and recapture but exposed native blank hull keys during combat. A complete empty/own-AI seat audit now permits that native fallback without rewriting vehicle factions. Explicit foreign occupants/affiliations still reject the package. Test-QRFVehicleFaction.js covers 14 mocked cases; native combat and complete delivery/hold verification remain pending.
