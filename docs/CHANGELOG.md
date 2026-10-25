# Changelog

## Unreleased - Measured flag-model base correction (2026-10-24)

### Native editor audit

- The 23:50:47 read-only editor pass inspected all 41 loaded poles with zero clearance warnings. Actual pivot coordinates matched the saved placements; this rules out a simple missing world reload or child-offset arithmetic error for that run.
- Every pole reported local model minimum Y = -0.805946 m. The previous placement grounded the pivot at terrain +0.1 m, leaving the model bottom approximately 0.706 m underground. The initial audit was compiled before the subsequent model-base-aware calculation; its raw terrain, actual positions and bounds provide the measurements used for this correction.

### Saved placement and interaction

- Bake all 41 pole heights from measured editor terrain minus the model minimum Y plus 0.1 m. Preserve pole X/Z, location root transforms, IDs, roles, populations and Rowan's scene. Store the full native audit and corrected world positions in EVERON_FLAG_EDITOR_AUDIT.json and update the CSV/placement evidence.
- Override the inherited capture-context offset per pole to 0.694054 m, preserving its world position at terrain +1.6 m after raising the mesh. The shared capture prefab and runtime HQ behavior remain unchanged by this correction.
- Catalogue checks now verify both actual model-base clearance and capture-context world height for every site, alongside hierarchy/source/GUID/anchor checks. These pass. Reload Chicken from disk; no Ground plugin pass is necessary to apply this bake. A fresh visual inspection and Play capture check remain required.

## Unreleased - Recheck editor flag placement across Everon (2026-10-24)

### Report and verification limits

- Record continued underground flags at Chotain, Durras, Entre-Deux, Camurac, Levie and Montignac. Jasper clarified those named observations are in World Editor, with only a couple checked in Play. The previous saved-coordinate bake and runtime messages did not validate the visible editor poles. No full-island visual pass is claimed.
- Recheck all 41 saved roots, inherited flag IDs and coordinate reconstruction. Prepare a native editor audit of every actual pole transform, collision-traced terrain height, model bounds/base and clearance, instead of another unmeasured height offset.

### Editor tools

- Add read-only Audit all campaign flags and undoable Ground all campaign flags under the World Editor Open Resistance plugin category. Follow Bohemia's official WorkbenchGame plugin layout and the installed editor SDK APIs.
- Ground operates on the actual flag's editor source/local transform and model base, preserves location anchors/X/Z, and marks changes for the normal world Save. Block Play, prefab editing, other worlds and incomplete catalogues. Warn for blocked terrain/clearance; no automatic horizontal relocation or access guarantee.
- 14 actual-source mocked editor checks cover all 41 reports/edits, read-only behavior, Undo grouping, rotated-parent conversion, mesh-pivot bounds, world/mode/catalogue restrictions and invalid terrain. Catalogue validation passes. Native plugin compile and the requested editor audit are pending; editor changes must be reconciled into the CSV before regeneration.

## Unreleased - Airport road test and combat affiliation follow-up (2026-10-24)

### Native evidence

- The 23:29 run confirms cleared road launches, physical seating and vehicle ingress from Farm toward the airport. War level 2 selected response tier 3: two motorized trucks, two mechanized LAVs and one crew-only armour LAV, with three first-wave elements and two reserve elements. Each truck has one driver and four infantry; the light passenger load is the authored fireteam size.
- Armour entered the airport at 23:33:26, starting the 900-second hold, and Occupiers recaptured it at 23:33:48. This establishes arrival/recapture for part of the response, not successful unloading/hold completion for every vehicle. Mechanized packages were still outside the objective and combat interrupted the run.
- Finance recorded 80 overhead, six personnel losses at 25 each, and one vehicle loss at 1000, all paid by role 1 with zero debt. These are debits, not a claimed net account balance after periodic income. The current loss log does not name its entity, so it cannot independently identify the user's specific destroyed LAV.

### Combat affiliation correction

- The test exposed two packages incorrectly retired when native hull affiliation became blank during combat. Accept this fallback only after checking every seat: hull must be empty or occupied exclusively by this package's AI with the expected campaign faction. This covers both transports and armour before/after delivery.
- Unknown seat state, foreign/human occupants and explicit hostile hull affiliation still fail. Do not re-faction vehicles, declare capture, debit a loss from a blank key or suppress genuine destruction/death accounting.
- 14 new actual-source mocked faction checks pass alongside 21 ingress and 40 unloading checks. Native compilation and another combat run remain required; the full response lifecycle is not yet certified.

## Unreleased - Ground QRF launch and dispatch correction (2026-10-24)

### Ingress and physical placement

- Replace raw lateral outpost vehicle spawns for tactical ground QRFs with a bounded road-pad search near the selected friendly origin. Require separation from existing operation vehicles, above-water terrain, acceptable slope and a clear hull-sized volume. Reject missing pads instead of spawning into an unchecked field/compound. The search performs at most 12 road queries per package.
- Spawn ground vehicles facing the objective with a small terrain clearance. The strategic origin remains the selected outpost/airport; no teleport recovery during travel is added. Trucks, LAVs and USSR equivalents share this path.

### Boarding and order ownership

- Treat native direct-seat calls as requests. Verify actual driver occupancy, every crew member and every passenger in a matching compartment before declaring the package ready; incomplete native seating remains in SPAWNING under the existing timeout.
- Register the transport as usable by both its crew and passenger AI groups. Block premature infantry-group investigation orders while a non-delivering group still has seated passengers, so one early exit cannot pull the rest out at the origin. Authorized unloading and wholly dismounted survivors retain investigation.

### Validation

- 21 actual-source mocked ingress checks pass: asynchronous seating, missing driver/passengers, wrong vehicle/stale occupant, obstructed/steep/underwater pads, origin bounds, vehicle separation, finite query budget and mounted-order gating. Existing 40 unloading regression checks pass; installed force-pool asset/config validation passes.
- Native compile and the repeat airport capture test remain required. The prior log showed all three packages stalled near Farm; these source corrections address concrete launch/readiness/order gaps but do not certify native pathfinding or successful arrival.

## Unreleased - Bake flag placements and review airport QRF (2026-10-24)

### Flag placement

- Follow up continued underground reports: 34 catalogue roots were still authored at Y=0 and relied on Play-only grounding. Save nonzero heights and explicit inherited-child flag offsets for all 41 sites using accepted terrain/clearance/navmesh candidate positions from the latest Workbench log. Disable runtime relocation for these baked Chicken poles so saved scene geometry owns placement in editor and Play.
- Preserve all location IDs, X/Z anchors, the seven established root transforms and both Provins defenders. The 34 new root elevations use nearby candidate terrain, rather than claiming exact centre-height samples. Add FlagX/FlagY/FlagZ CSV fields and a placement evidence snapshot; validate parent-plus-child coordinate reconstruction. Rowan scene files are unchanged.
- This bake still needs a World Editor reload and visual inspection. Previous runtime placement messages alone did not prove that the user could see/reach the saved poles.

### Latest QRF run

- Airport capture queued and admitted a war-level-2 response from Farm Outpost. One motorized and two mechanized packages spawned, with direct seating and verified Occupier affiliation. The operation became active and charged 70 overhead.
- Delivery planning now succeeded for all three packages, but vehicles subsequently stalled near the origin, roughly 1.8 km from the airport. Four route retries did not resolve it by 23:18:45. Arrival, unloading and the 15-minute hold were not demonstrated. This is an unresolved native ingress issue, not a successful QRF test.

## Unreleased - Capture response and pole placement fixes (2026-10-24)

### Native findings and QRF delivery

- Fresh Workbench logs confirm 38 sites registered, Provins flag capture set Resistance support to 100%, and the capture adapter reused the defense QRF. That response selected Pinewood Lake but failed during second-package planning, rather than failing to notice the capture.
- Expand bounded road alternatives from origin-side wedges to a whole-town ring. Keep the 100-metre objective bound and preferred delivery spacing; when preferred spacing cannot fit, select the best existing candidate with at least 18 metres clearance (or the tighter authored spacing). No extra road-query budget is added. Genuine lack of a safe/reachable delivery point still fails explicitly.

### Capture poles

- Add server-owned, bounded placement to the shared capture pole: move/update the actual child pole world transform and static collision after the location hierarchy initializes. Check terrain slope, nearby structure/tree clearance, water and soldier-navmesh reachability; exclude the pole itself from clearance traces.
- Replicate the selected position to clients and late joiners. Search at most 33 nearby candidates over startup timers; on navmesh failure prefer a physically clear grounded candidate and log that placement needs verification. Never silently leave the pole at authored Y=0. Location centres/IDs remain unchanged. Editor-only authored heights still need a Workbench bake for permanent editor positioning.

### Rowan location alignment

- Replace the overlapping Airport Military Outpost with Rowan's `everon_airport`, preserving its name, AIRPORT type, 500-metre radius and coordinates. Bring across his factory and both resource anchors, assigning explicit stable IDs to the previously unnamed sites in Chicken. Rowan's own scene remains unchanged.
- Chicken now contains 41 locations: 7 cities, 11 towns, 14 outposts, 5 military outposts, 1 airport, 1 factory and 2 resource sites. All have capture flags and use the existing ownership/income rules. Airports can serve as friendly QRF origins; the military capture opt-in also covers factory/port/resource capture events. USSR dispatch remains disabled.

### Validation

- Catalogue/layer/GUID checks pass for all 41 sites. Actual-source mocked tests pass 38 capture/origin assertions and 14 road-spacing/flag-transform assertions. Native compilation, clear pole placement, replication and successful multi-package dispatch from the new origins require a fresh Play test.

## Unreleased - Everon location prefabs and flags (2026-10-24)

### Campaign map

- Populate Everon_Chicken with 38 campaign locations: seven cities, eleven towns, fourteen ordinary outposts and six military outposts. Preserve the established Provins, Morton and Lamentin IDs/coordinates and both Provins test defenders. All sites initially belong to Occupiers/US; Invader admission remains disabled. Shared Everon and Rowan's separate scene are unchanged.
- Add a CSV catalogue and deterministic layer generator with read-only verification by default. New X/Z anchors adapt EnfusionMapMaker's extracted Everon data with source attribution and APL-SA terms; optional terrain grounding resolves their heights before registration. Terrain accessibility and installed-version placement need native review.

### Reusable prefabs and capture

- Add City_Flag, Town_Flag, Outpost and MilitaryOutpost templates, each with one attached/inherited capturable flagpole. New resource/entity GUIDs are hand-authored in prefab/meta pairs and need Workbench registration. Existing prefab GUIDs and original settlement templates remain unchanged.
- Opt flag-bearing settlements into the existing capture action and weighted AI recapture. Successful captures set support through the existing settlement authority, avoiding contradictory control/support. Preserve hostile-clearance checks, server authority, fixed control and flip cooldown. Ordinary/military outposts retain military flag behavior. No garrison or scenery population is introduced.

### QRF and economy integration

- Accept ordinary outposts and military bases as eligible friendly QRF origins in readiness validation, dispatch, origin binding and withdrawal. Add separately configurable outpost capture responses via canonical control events; keep the settlement API as a compatibility wrapper. Factory/port/resource capture hooks remain outside this scope, and no USSR dispatch is enabled.
- Author provisional settlement populations totalling 1,230. The existing income system accounts for these populations; no balance, multiplier, resource stock or income authority is replaced. At default rates, initial Occupier settlement income is $6,150 plus existing external support per cycle.

### Validation

- Catalogue/layer checks pass for all 38 IDs, preserved anchors, source coordinates, prefab GUIDs, flag hierarchy and QRF origin gates. Thirty-eight actual-source mocked integration checks pass for capture routing, opt-ins, support/control coherence and server guards. Existing force-pool asset checks remain green. Workbench compilation, new resource registration, terrain/flag access, multiplayer/save-load and native vehicle routes remain pending.

## Unreleased - Paid, war-level QRFs and FIA spotting (2026-10-24)

### QRF contact readability

- Enable a temporary Chicken-only debug-marker switch: show every spawned/tracked QRF vehicle on Resistance maps and refresh positions every five seconds without observation or visibility traces. Keep normal spotting available by disabling the switch. The latest run confirms truck delivery and two LAV passengers exiting; two remaining LAV passengers reported blocked/rejected exits, so complete mechanized unloading remains unresolved.
- Enlarge spotted-vehicle icons from 26 to 40 pixels and captions from 14 to 18 pixels. Colour Occupier/US contacts blue and Invader/USSR contacts red using server-supplied operation roles. Use explicit Transport Vehicle, IFV, Armoured Vehicle, Transport Helicopter and Gunship designations. Preserve last-seen positions and expiry. The QRF snapshot RPC now includes role metadata; server and clients require the same updated build. Visual changes require Workbench verification.

### Response lifecycle

- Add a fixed 15-minute town-security hold starting on the first dismounted infantry entry into the objective area (armour-only forces start on hull entry). Later arrivals do not restart the clock. Keep infantry/support sectors inside the town objective area. Fixed mode suppresses casualty-based tactical retreats, continues unloading retries at the town delivery point, and preserves safety withdrawals and the outer operation timeout. The manager allows 45 minutes total for travel, the 15-minute hold and return; ground ingress allows ten minutes. This is a temporary arrival-based timer, not a post-recapture ownership timer.
- Retry stalled ground routes after 30 seconds without 15 metres of movement, with four bounded alternate road-point searches per package. Infantry already out investigate independently of remaining seated passengers; an early-empty truck no longer discards its surviving squad, and surviving dismounted infantry can continue after hull loss. All-element arrival, native exit recovery and the full hold need a fresh Play test.
- During those ingress retries only, a living AI crew member within 15 metres of a stationary abandoned vehicle can be placed directly back into its empty driver seat. Players, player-commanded groups, distant crew and occupied seats are excluded. Offline checks pass 16 hold/route assertions and 40 unloading/driver assertions; native navigation, delivery and the full hold remain unverified.
- Correct premature hillside delivery: ground QRF road candidates must now be within 100 metres of the town objective, with preferred approach distance capped to 60 metres. Physical unloading requires the vehicle within 20 metres of its assigned road stop and inside the objective delivery radius; native waypoint completion cannot widen that gate. Fix the empty-truck affiliation race when native exit completes before the delivery flag updates. Add bounded diagnostics for native exit-recovery clearance/position failures. These changes require a fresh compile/reload and native route/unloading test.
- Harden ground passenger unloading after the native LAV test left two passengers seated: try alternate usable doors, leave doors open and stagger accepted exit requests across executor ticks. After a configurable ten seconds, stuck AI passengers in stopped motorized/mechanized vehicles may use the native cleared-exit transform; reject distant/elevated candidates and moving vehicles. Aircraft, crew and player characters are excluded from recovery. Physical passenger-exit confirmation and the existing withdrawal deadline remain required. Native compilation and a new LAV delivery test are pending.
- Latest Chicken playtest selected the tier-2 truck/two-LAV response, verified all deployment factions and charged the $70 dispatch fee. Jasper subsequently observed infantry deployed in town. Both LAV packages logged passenger-exit timeouts; full mechanized delivery is not verified. Initial seating already uses direct placement with no boarding pathfinding. Harden unloading so a rejected passenger exit does not block other passengers, retry seated passengers until the existing deadline, and add bounded exit diagnostics. These unloading changes require recompilation and another native test.
- Preserve the verified empty-delivered-truck faction handling, distance-scaled infantry investigation and existing retention protections. The latest native run confirms truck delivery and continuing infantry/LAV movement; it stops before a physical investigation-arrival message.
- Add explicit ground recovery: surviving personnel and vehicles must physically return near a friendly origin before leaving the tactical roster. Recovery does not count as a combat loss or accelerate casualty-triggered reserves. A completed response with recovered survivors can resolve without implying town ownership or territorial victory.
- Keep virtual reserve deadlines, actual boarding/delivery gates, live-force ceilings and player-preservation rules. New lifecycle work still needs a full native return/cleanup test.

### Prices, loss payments and faction accounts

- Add designer-authored personnel, hull and gunship replacement values on force profiles. Price the same US LAV equally in its mechanized and crew-only armour policies; use separate BTR/BRDM and helicopter values for Invaders. Export the ten authored role/tier quotes in `docs/QRF_PRICES.csv`.
- Integrate with Rowan's existing `AROR_EconomyComponent` read/spend API. Reserve complete-response collateral before spawning and charge deployment overhead once at ACTIVE, including one fee across all later waves. Surviving assets incur no replacement debit merely because an operation ends.
- Register physical personnel/vehicles once, charge confirmed death/destruction/capture once, and observe before cleanup. Empty/native vehicle faction changes and absent/virtualized entities are not fabricated deaths. Normal actor retirement and successful transport egress release collateral without loss charges; unspawned ground/air reserves release their quotes.
- If another system spends reserved funds, record an unpaid QRF liability after consuming available funds. Persist that debt and service it from future bank funds before admitting new paid responses. Keep balances nonnegative and avoid refund/income minting. The Resistance's funds, player money, manpower, income and economic ownership rules are unchanged.

### War level, threat prioritisation and difficulty

- Maintain independent saved war pressure for Occupiers and Invaders, initially war level 2 in Chicken. Validated fighting, captured settlements and confirmed asset losses raise pressure; quiet time decays it. Select a desired tier once per queued threat, with bounded variation and an extra ceiling for sustained/capture threats.
- Coalesce trusted reports per role/location into a bounded queue. Rank strategic location importance, capped report counts, capture and bounded waiting age; expire stale reports and defer blocked candidates. Reuse existing responses and keep role gates, cooldowns, capacity and target conflict checks authoritative.
- Preflight complete affordable ground recipes or airborne pairs before request admission; downgrade within desired intent and available money/actor capacity. Reserve again atomically before construction to handle subsequent budget changes. Existing force recipes are not rerolled after binding.
- Author five matching ground tiers per AI role: truck/IFV at tier 1 through finite multi-vehicle tier-5 responses. The starting tier-2 mix is one truck plus two IFVs, with the lighter tier-1 variation. These are initial balance settings for 3–4 capable players, not measured combat acceptance.
- Enable strategic selection only in Chicken. Disabling it restores the fixed recipe fixture. Invader dispatch and the REAPER override remain disabled; physical stock, factories, garrisons and durable deployed-operation restoration remain future integration.

### Resistance visual intelligence

- Add server observation of known QRF vehicles using actual eye position, head direction, range and native world/entity/visibility traces. Require a confirming observation before classifying a contact; bound pair scans, traces and retained reports.
- Share filtered last-seen type/position/age snapshots only through living Resistance players' owner-targeted RPCs. There is no arbitrary client spotting request and no replication of the unobserved candidate list.
- Draw labeled truck, IFV, armour, transport-helicopter and gunship map contacts through the existing map widget lifecycle. Unseen reports stop moving, fade and expire; client state contains positions rather than target entities. Reuse the existing military icon and add an optional caption to its layout. Foot squad markers and dedicated vehicle silhouettes are not included.

### Persistence, extension points and validation

- Add versioned operation-manager serialization for both roles' war pressure and debt. Economy continues to serialize bank debits. Queues, collateral claims, deployed-force ownership and spotting reports remain session-local; full save/load deployment recovery remains unimplemented.
- Keep authoritative guidance in `CLAUDE.md` unchanged, update high-level `ARCHITECTURE.md`, and refresh its full `AGENTS.md` copy. Add strategy/payment and spotting guides with configuration, price tables and explicit acceptance steps.
- Add source-adapted regression coverage and pure Workbench smoke checks without bank mutations or actor spawning. All 6,177 offline assertions pass for pricing, exact-once payments/losses, debt repayment, affordability downgrade, war bounds, queue priority/expiry, role gates, returned-package completion, pending-bill save snapshots, versioned restoration and spotting confirmation/packet filtering. Installed GUID/configuration checks pass for all eight profiles, ten strategic tiers and their documented prices.
- **Not compiled or play-tested after these additions.** Native navigation/return, perception/head replication, marker rendering, owner RPC delivery, saving and performance need Workbench/multiplayer verification. Extenders should review the new profile price attributes, opt-in manager strategy/spotting settings, serializer and the optional map-layout caption. No new hard dependency or resource GUID is introduced by this work.

## Unreleased - Keep delivered infantry investigating the town (2026-10-24)

- The latest native run verifies successful truck unloading, then exposes a whole-response cancellation three seconds later from a vehicle affiliation audit. Infantry had been ordered toward the town but had insufficient time to reach it. The previous diagnostic does not establish which vehicle changed affiliation or its actual key.
- Tolerate an empty or declared asset key only on a native affiliation component of a proven empty, delivered passenger vehicle. Bound native occupied-seat updates during unloading to five seconds; preserve strict rejection of missing components, unrelated keys and persistent occupied-vehicle mismatches. Do not repaint vehicle factions during monitoring. Log the exact package, profile, keys, occupancy and position on failure.
- Route vehicle audit failures through package loss handling: preserve healthy active packages, while required pre-dispatch failures still cancel the complete recipe.
- Send every delivered fireteam, including IFV passengers, to its initial contact/town investigation point before sector patrol. Scale walking deadlines by distance and planning speed, with a configurable 180-second ceiling in Chicken, and log progress, actual arrival and leg timeout separately.
- Thirty-three source-adapted checks pass for validation, seat occupancy, walking deadlines, arrival progression and player-command safeguards with native dependencies mocked. Installed resource/configuration checks pass. These latest changes still require Workbench compilation and Play; reaching the town is not yet verified in-engine.

## Unreleased - Road-stop unloading and town security responses (2026-10-24)

- A fresh Play run verifies distinct truck/LAV road selection, complete boarding, correct `AROR_OCCUPIERS` vehicle/group/member factions and ACTIVE state. Truck infantry unloads and begins tactical search. The LAV is observed parked down the road and withdraws exactly at the old 120-second ingress deadline without logging delivery.
- Accept a stopped vehicle within a configurable 75 m arrival tolerance, or up to twice that distance when native AI has completed its move waypoint, only within the defended vicinity. Hold the driver at its actual road stop before animated exits, give ingress and passenger exits separate configurable 300/30-second deadlines, and log travel/drop state plus timeout details. This revised behavior needs fresh Workbench compilation/Play.
- Keep a captured town as the active QRF's investigation/security objective; refresh bounded search sectors on control change, then patrol/hold until the task-duration/casualty limits. Missing targets and lost origins still have safety handling. No automatic settlement ownership flip is introduced.
- Add an opt-in server town/city capture observer using canonical location events, enabled only in Chicken. Request a response for a displaced hostile AI owner through existing reaction delay, cooldowns, deduplication, capacity and role gates. Baseline initial/restored ownership, detach subscriptions on shutdown and do not manufacture attacker-position reports. Invader admission remains disabled; factory/military capture and stock/funds spending remain disconnected.
- Offline source-adapted checks passed for 22 ground delivery/capture-admission/control cases and seven capture-observer baseline/change/teardown cases, with native AI, invokers and scheduler dependencies mocked. Installed resource/configuration and context/whitespace checks pass. New Workbench policy assertions are added; the revised delivery/capture behavior remains uncompiled and unverified in-engine.

## Unreleased - Workbench compilation fixes (2026-10-24)

- A fresh Play run confirms compilation, Chicken startup, both defender affiliations/subscriptions and hostile-hit QRF admission. It exposes a new pre-dispatch failure: the LAV's single tactical road query rejects its approach and cancels the mixed response before activation. Project native road-query inputs to terrain height (configured launch positions use Y=0), try a bounded set of alternate approach wedges/radii, enforce vicinity/standoff/spacing checks and log selection or exhausted search. Expose the query budget as `m_iQRFApproachAttempts` (12 in Chicken). The revised route search needs fresh Workbench compilation/Play; it does not claim native route success yet.

- Fix tactical executor visibility errors reported by Workbench: move shared tactical attributes/state and helpers into the original `AROR_OperationExecutorComponent`. Base-class methods cannot reference members introduced in a later `modded` layer. Remove the separate tactical modded layer and move backend-only air withdrawal into the existing REAPER extension. Preserve the relocated behavior and serialized attribute names.
- Replace the readiness snapshot's incompatible owning-array `Copy` call with explicit cloned stock entries. Runtime-capacity previews retain their entries without sharing mutable stock objects with the provider. Extend the Workbench selection smoke tests to cover this ownership boundary.
- Verify all 21 relocated method bodies are unchanged, no base executor calls backend-only methods, and installed-asset/configuration and whitespace checks pass. These source checks do not replace a fresh Workbench compile; base-game missing-class messages in the failed compile still need reassessment after the project errors are cleared.

## Unreleased - Rowan integration and combined development build (2026-10-24)

### Branch reconciliation

- Commit the accumulated QRF, REAPER and tactical work on `chicken-dev` as `7a3b340` (`Add QRF tactical command and staged waves`).
- Merge Rowan's `master-dev` work through `99ad920`, including PRs #8, #9 and #10. Preserve both development histories without rebasing or force-pushing the shared branch.
- Rebuild the conflicted `AGENTS.md` architecture copy from current authoritative `CLAUDE.md` and merged `ARCHITECTURE.md`. Keep Economy, Phone and Admin Tools architecture alongside the QRF design. No project-guidance policy is rewritten.
- Keep the shared role authority: Resistance uses FIA assets, Occupiers/BLUFOR use US assets and Invaders/REDFOR use USSR assets. QRF code does not introduce another faction or location ownership registry.

### Rowan: campaign economy and persistence

- Bring in role-based faction funds, Resistance manpower and personal player money through `AROR_EconomyComponent` and its serializer. Faction balances are replicated; personal balances are sent through the player's controller and indexed by persistent identity.
- Preserve server-owned add/spend APIs and nonnegative balance checks. Save economy state through the existing game-mode persistence path and retain setup/new-campaign initialization.
- Bring in `AROR_IncomeComponent` and Rowan's latest 15-minute default payment interval. Held resource locations produce $500 each, multiplied by factory/port bonuses; held town/city population adds $5 per inhabitant outside that multiplier.
- Include the newer fixed external-support payment for Occupiers and Invaders ($500 per role per payment), independent of their held locations.
- Include the newer personal share for every online Resistance player: 10% of Resistance income, paid in addition to faction funds. Keep Resistance manpower income, setup-phase gating and income notifications.
- Rates remain designer attributes. These are campaign money/manpower systems; they do not yet supply military vehicle stock, crew availability, ammunition, route readiness or QRF purchase reservations.

### Rowan: phone, bank and administration

- Bring in the client phone menu, frame/app/status assets, menu registration and rebindable input context. Default opening action is Ctrl+Y for a living player when no other menu is open.
- Include the Bank app displaying personal money and Resistance funds, portrait/landscape transitions, home navigation, status bar and Escape handling. Money transfers and general app registration remain planned.
- Preserve the expanded Ctrl+T admin sub-menus, economy balance display, money-grant buttons and immediate-income test action alongside teleport, Game Master, support and notification tools.
- Keep the upstream setup-dialog and player-controller changes needed by these systems. Admin access restrictions remain future work; this merge does not claim to add them.

### QRFs: shared tactical command

- Add one server tactical commander per QRF operation for either opposing role. Existing faction strategic commanders still own scheduling; the tactical commander coordinates the already selected immutable response.
- Use recent validated allied combat reports to investigate last-known enemy positions, then search bounded town sectors. No live enemy entity tracking or omniscient player-position feed is added.
- Give dismounted infantry separate sectors, redistribute after losses/new waves and rate-limit retasking. Retain native AI perception, navigation and weapon use.
- Coordinate truck delivery and waiting, separate IFV/armour road cover, reported-threat dismount standoff and bounded ingress attempts. Air-inserted infantry uses the same tactical search controller.
- Add local/whole-force casualty withdrawal, target/origin ownership checks, safe return/hold behavior and bounded task duration. Building-by-building clearance, guaranteed convoy movement and coordinated suppression remain outside this implementation.

### QRFs: reinforcement waves and performance limits

- Split eligible tier-3+ responses with at least four elements into a type-balanced lead and finite reserve. Keep later elements as actor-free reservations rather than spawning them into a holding area.
- Preserve the full original group/personnel/vehicle allocation; later waves do not reroll the recipe, create extra replacements or consume untracked capacity.
- Limit construction to one ground element or transport/gunship pair per executor tick. Default live ceilings are six ground packages and six healthy aircraft, including retained survivors/healthy airframes.
- Normally release reserves after 60 seconds from lead dispatch, shorten eligibility to 15 seconds following package loss, and expire unspawned reserves after 300 seconds. Later elements join the existing ACTIVE operation.
- Prepared Invader ground tiers 3/4/5 divide into 3+2, 4+3 and 4+4 elements. Air level 5 divides into three pairs plus two reserved pairs; a pair requires two free aircraft slots.
- Hold capacity for retained forces near players. Waves bound deployment/construction work; they do not certify FPS or guarantee every reserve deploys before its deadline.

### QRFs: air operations, origins and retention

- Bring the direct REAPER experiment into the normal project, retaining separate diagnostic files and launcher for optional troubleshooting. The current normal fixture leaves flight dispatch disabled, but REAPER remains a load-time dependency on this experimental branch.
- Preserve incremental separated-pad landing searches, direct transport landing, stable-touchdown and actual passenger-exit gates, departure, report-driven gunship passes and predictive traffic yielding.
- Keep boarding holds for later-wave helicopters until flight dispatch. Successful aircraft departures do not count as inserted-infantry casualties, and disabled/retained healthy airframes do not silently free live slots.
- Select eligible nearest friendly outposts for ground/air QRF origins, with configured role coordinates as fallback. Append OUTPOST without changing existing location enum values and retain military capture classification.
- Keep nearby ground survivors/wrecks at least 120 seconds and until every player is 1,200 m away. Preserve the separate air retention policy and aggregate reservations. These are session policies, not durable saved operations.

### Worlds, fixtures and resource planning boundary

- Bring in Rowan's separate Everon test world, income-test location data and original Everon updates. Preserve the separate `AROR_Everon_Chicken` QRF fixture and its inherited shared game-mode systems.
- Keep Chicken on the authored US truck/LAV `combined_arms_l1` response: two vehicles, four groups, twelve actual personnel and eighteen reserved slots, with tactical coordination enabled. This small tier-1 response stays in one wave.
- Keep the REAPER override off, Invader operation admission off and Invader automatic/simulated targets empty. Stage USSR Ural/BTR/BRDM tiers and Mi-8 profiles without adding an Invader gameplay trigger.
- Keep the resource-aware QRF readiness-provider preview read-only and disconnected from dispatch/spending. Rowan's balances and income are preserved as the economy authority; no parallel economy, stock generator or automatic QRF funds deduction is introduced.
- Maintain the 40-response design roster and separate authored recipe tier from element count. Mixed ground/air recipes, stock-backed selection and campaign factory/territory dispatch wiring remain future work.

### Validation, extender impact and next acceptance

- Re-run installed-asset/configuration validation after integration: verified GUIDs, navigation, force profiles, prepared tiers/recipes, tactical fixture settings and disabled Invader admission. Check copied context synchronization, merge markers and whitespace.
- Earlier development included source-adapted offline tactical/lifecycle checks with mocked native dependencies and Workbench-only smoke-test helpers. These do not establish native compilation or gameplay correctness.
- Preserve Rowan's imported Economy/Phone implementation and verify shared prefab/component coexistence at source level. The combined build still needs a successful Workbench compile and fresh Chicken/Rowantrek Play tests, including setup, Bank balances, income, QRF delivery/search/waves and multiplayer behavior.
- Extenders must account for the experimental direct REAPER dependency, opt-in tactical attributes, default-disabled Invader admission, immutable recipe tier/package semantics and deferred/retained capacity. Operation state, tactical contacts and wave timers remain session-local.
- Detailed operational settings and acceptance steps are in [QRF_TACTICAL_COMMANDER.md](QRF_TACTICAL_COMMANDER.md); ground and air fixtures have their own guides. No merge into `master-dev` or `master` is performed by publishing the development branch/PR.

## Unreleased - QRF tactical command and finite waves (2026-10-24)

- Add a shared server tactical commander for US-backed Occupiers and USSR-backed Invaders, using existing Operations, factions, location ownership, native group AI and REAPER. Coordinate report-driven investigation, bounded infantry sectors, survivor/new-wave redistribution, ground delivery, separate IFV/armour road support and landed air infantry. Retask from time-limited position snapshots; no live player tracking, building-clearance controller or new territory authority is introduced.
- Add optional type-balanced lead waves and finite actor-free reserves for larger responses. Reserve the complete original allocation, construct at most one ground element or air pair per executor tick, admit later elements under delay/live caps and expire unused reserves. Preserve healthy active elements after reinforcement failure and prevent repeat construction/duplicate activation. Air pairs require two free aircraft slots; undersized lead-wave limits reject configuration before spawning.
- Add ownership/task-duration/casualty withdrawal and ground retention requiring minimum age AND distance from every player. Retained surviving troops still occupy the live ground ceiling and all retained forces retain aggregate capacity. Keep air boarding holds until flight dispatch, share infantry search orders, and exclude successful aircraft departures from troop-casualty accounting. Player-control checks remain in force.
- Enable tactical coordination on Chicken's existing tier-1 truck/LAV fixture, retaining one wave for its two elements. Leave Invader admission disabled, Invader targets empty and the REAPER override off. No economy provider, stock spending, production, territory/factory setup or automatic USSR caller is added.
- Add Workbench tactical smoke tests, offline source-adapted policy/lifecycle checks and tactical fixture validation; document behavior, wave tables, settings, remaining mixed-domain/persistence limitations and Play acceptance in `QRF_TACTICAL_COMMANDER.md`. Native compilation, movement/flight, FPS and dedicated/multiclient acceptance remain outstanding.
- Extender impact: tactical execution defaults OFF outside the explicit Chicken override. Opt-in scenarios gain deferred reservations, bounded live deployment and terminal retention; aggregate budget may stay occupied near players. Recipe tier and element count remain separate, and runtime/session state is not saved. Existing non-tactical paths remain available.

## Unreleased — Commanders and QRF foundation (2026-10-23)

### Integration with master-dev

- Preserve Rowan's core factions, forced Resistance spawning and setup-flow updates while retaining the commander/scheduler components.
- Use the shared `AROR_FactionRolesComponent` as the authority for commander faction bindings. Remove the duplicate commander-folder role enum; the canonical definition lives under `Scripts/Game/Factions/`.
- Retrofit Chicken onto the same core AROR faction keys and Resistance loadout as original Everon. Its defenders resolve the shared defending role; profiles declare campaign faction separately from native US/USSR assets.
- Initialize operation-owned vehicle/member affiliations once from an empty, matching, or explicitly declared native asset faction. Initialize empty groups or use the native server `SetFaction` API for assigned groups before requesting members. Unexpected affiliations and later faction changes still fail validation.
- Extenders must move commander-manager faction attributes to `AROR_FactionRolesComponent`; evaluation interval remains on the commander manager. Recompile and reload prefabs/worlds before testing this integration.

### Added

- Add a non-dispatching Invader ground QRF planner with authored tier limits/downgrade policy, expiring faction/role readiness snapshots, repeated-profile stock accounting and aggregate runtime capacity checks. Expose a server readiness-provider registration/preview seam that validates target ownership, friendly outpost origin and route/arrival constraints without mutating provider data. No default resource provider, economy spending, automatic caller or Invader trigger is added. Fifteen source-adapted offline scenarios passed with mocked dependencies; Workbench smoke tests are added but unrun, and native compilation/provider integration remain outstanding.

- Stage the USSR-backed Invader QRF framework with five authored Ural/BTR/BRDM ground tiers, role-specific ground recipe selection and the existing shared Mi-8/REAPER support paths. Default Invader operation admission OFF and explicitly disable it in Chicken, rejecting Invader attack/QRF requests before operation creation; leave automatic/test targets empty. Preserve the Occupier truck/LAV fixture. Extend capacity/configuration and disabled-gate checks. No territories, factories, Invader defenders, production or triggers are added. Extender impact: Invader operations now require explicit opt-in; existing role mappings and profile IDs are unchanged. Compilation and Invader runtime acceptance remain outstanding.

- Add authored ground QRF recipes resolving ordered existing profiles under one operation, with atomic aggregate runtime reservations, separate tier/element count, complete pre-dispatch readiness and survivor behavior after active losses. Configure Chicken for combined_arms_l1: US M923 truck plus LAV-25 with eight infantry/four crew, four groups/eighteen reserved slots/two vehicles; provide the Ural/BTR role equivalent. Disable the air override and clear the old forced US profile. Add recipe validation/regressions and extend installed-asset fixture checks. Extenders can opt into recipe configuration; homogeneous behavior is retained. Workbench compilation and mixed-force Play acceptance remain outstanding.

- Design a 40-entry QRF roster: motorized, mechanized, armour counterattack, air assault, combined arms, anti-armour, reinforce-and-hold, and search/containment, each with authored levels 1-5. Include a Markdown table and CSV with stable IDs/nominal accounting, location/threat suitability, required asset/crew/supply/ETA gates, and a future resource-reservation/selection contract. Mixed execution, stock-aware selection and composite tier semantics remain planned; no runtime scripts, spawn counts or economy balances change.

- Add role-scoped, expiring allied combat reports for gunship support, fed by validated assigned-defender damage and operation-owned units even when QRF admission is on cooldown. Gunships investigate recent nearby snapshots and vary search passes without live player tracking; native hostile-target and friendly-clearance filters remain. Separate gunship altitude bands from transports and add predictive proximity yielding with stable gunship priority. Ground and airborne QRFs select the nearest eligible friendly OUTPOST, falling back to configured role origins. Append OUTPOST to the enum and existing military capture/recapture classification; existing enum values stay stable. Offline source-adapted checks passed for five traffic predictions, eight contact-policy cases and four origin-policy cases with engine dependencies mocked. New integration still requires Workbench compile/Play and dedicated/multiclient verification.

- Set Chicken's Workbench response level to 5 and make REAPER dispatch honor levels 1–5: five transports/fireteams plus five gunships for this test. Reserve the complete fleet and select a shared LZ, gate activation on every package being boarded, and retain healthy packages after individual active losses. Add package-specific launch lanes, altitude bands, gunship pass phases and infantry sectors; extend configuration validation to cover airborne fleet budgets. Requires fresh Workbench compilation and testing.

- Extend REAPER QRFs with closest sampled clear-town LZ selection, offset ingress guidance, last-contact infantry investigation/patrol, transport takeoff/egress with all-player distance checks, and moving gunship passes. Use documented REAPER controller/waypoint interfaces and a perceived-target filter for campaign hostility and nearby non-hostiles. New backend signatures and behavior require fresh Workbench compilation/testing.
- Retain active air-package casualties after pilot/aircraft loss; allow surviving grounded passengers and delivered infantry to continue. Terminal air forces keep ownership/capacity until a minimum retention period and all-player distance checks permit removal; world unload flushes retained records. This is session retention, not saved persistence.

- Add a direct REAPER QRF experiment on the existing Chicken defender/scheduler path: one transport/fireteam and one gunship, faction checks, three-group/two-aircraft reservations, occupied midair insertion with native airborne engine startup, Move/Land orders and landing/exit-gated infantry orders. An off switch restores native ground dispatch. The separate disabled flight probe remains diagnostic; the first run established basic flight/landing/unloading, while the revised engagement/formation/egress/retention behavior and weapon filtering remain unverified.

- Limit airborne concurrency to one operation with one to five complete packages and a shared reserved LZ; transport now departs after unloading and gunship flies moving passes before egress. Bounded candidate LZ checks are included; full route certification and persistence are deferred. Normal airborne selection remains unchanged. At Jasper's request this experimental branch now adds REAPER to the normal project dependencies and moves the adapter into core AROR scripts; Chicken enables the switch explicitly, while other executors default off. Ground dispatch can be restored without removing the dependency.

- Shared QRF tactical search plans: snapshot validated attacker reports, investigate recent contact, then assign separate marker patrol sectors and spaced road stops to each vehicle package. No live attacker tracking; stale/missing reports fall back to town patrol. These latest changes require Workbench compilation and testing.

- Three configurable role-based commander contexts: player-directed Resistance and AI-directed Occupiers and Invaders. Bindings come from Rowan's shared role component, defaulting to `AROR_RESISTANCE`, `AROR_OCCUPIERS` and `AROR_INVADERS`, with FIA/US/USSR assets respectively.
- Server-owned attack and QRF scheduling, independent role cooldowns/capacity, target deduplication, bounded operation history and terminal cleanup.
- Weighted force pools and response levels 1–5. Each level deploys that many complete packages, with the whole response reserved before spawning.
- Ground execution for motorized, mechanized and wheeled-armour packages, including queued crew/infantry spawning, boarding, deployment waypoints and individual package-loss handling.
- Hostile-damage reporting from explicitly assigned defenders, with faction, instigator and location validation.
- Optional bounded mounted QRF town searches using reachable road points and native AI perception.
- Deployment faction audits, continuing vehicle-faction validation and optional same-faction damage diagnostics.
- Separate Everon_Chicken test world with Provins defenders, navigation bindings, perception manager and configurable US/USSR force pools. The current fixture enables the REAPER test at level 5 (five transport/fireteam/gunship packages); disabling it falls back to the forced US mechanized profile at level 5.
- Workbench smoke-test helpers, installed-resource/configuration validation and setup/testing guides.

### Fixed

- Plan individual REAPER transport landing sites across the town vicinity (400 m default search, expanding to town radius plus 150 m, capped at 800 m), with 90 m configurable pad/corridor separation. Distinct pads can land concurrently; if fewer clear pads exist, only transports sharing a pad queue, preserving the full response. Search remains cached at two candidates per tick and 73 total. Blocked pads receive package-local alternate searches with a shared fleet-wide budget and stage timeout. Eleven source-adapted offline planner scenarios passed with mocked geometry/native acceptance; extended Workbench smoke tests, actual flight and FPS remain unverified.

- Reduce transport exposure in the REAPER QRF: grant direct landing clearance before the approach/hover gate; move queued holding points outside town (minimum 700 m behind the LZ); expose 160 km/h ingress, 60 m AGL, two stable touchdown ticks and 180 km/h departure as designer attributes, explicitly set in Chicken. Request animated passenger exits on the stable-touchdown tick and retain the actual-exit gate before takeoff. Match escort ingress commands to transport speed/height. The previous run logged about 68 seconds from Land order to stable touchdown; actual speed/descent remain REAPER-controlled and the new tuning needs Workbench compilation and timing verification.

- Replace repeated level-5 full landing/assembly scans with a cached shared-LZ search: at most two candidates per executor tick, 73 total, then stop on success or exhaustion. Remove ground assembly-pad searches; validate air insertion volumes, hold aircraft while boarding and construct at most one transport/gunship pair per tick. Transports hold separately and take turns through the landing/departure corridor, with incremental alternate-LZ searches if blocked. Level 5 remains five complete packages. Seven source-adapted offline planner scenarios passed with native terrain/trace acceptance mocked; Workbench regression helpers are added but unrun. Compilation, actual flight and FPS remain unverified.

- Fix REAPER executor compilation: split oversized validation/cleanup/faction formulas into independent checks and replace the reserved `owned` loop identifier. Fresh Workbench compilation is still required.

- Make REAPER discoverable through Workbench's standard addon folder using a local junction to its installed files. The normal project can now open without the CMD launcher or separate project. The old launcher remains available only for standalone diagnostics.

- Replace an unsupported remainder operator in the coordinated search plan that blocked Game-script compilation and left Play running the older patrol implementation. A fresh successful compile is still required.

- Designer force-profile/response-level arrays being overwritten during initialization.
- Duplicate inherited commander/scheduler components caused by inconsistent component IDs.
- Recursive operation-event invocation, empty standalone defender affiliations, and the agent-added callback signature in source.
- Premature loss detection for groups still materializing through the engine queue.

### Validation and remaining work

- Configuration validation passes for 25 installed resource GUIDs, three navigation bindings, eight profiles and five response levels; staged whitespace checks pass.
- Observed Play tests confirm hostile-hit QRF dispatch, motorized spawning/boarding/activation, and all three level-3 LAV packages boarding and becoming ACTIVE. Jasper observed the infantry truck arrive; the latest logs also show vehicle movement.
- The latest recorded run still used older compiled scripts and reported the previous agent-added callback error. Current callback, town-search and faction-diagnostic changes require a fresh Workbench compile and Play test. Apparent LAV friendly fire is not yet confirmed or excluded.
- Airborne transport/gunship profiles remain disabled. The first direct-dispatch run confirmed boarding, flight, landing and unloading, but showed friendly explosive damage and immediate terminal cleanup. New engagement, LZ, formation, egress and retention changes require fresh compilation and runtime verification.
- Dedicated-server/multiclient acceptance, full deployment/dismount/search behavior, durable persistence, territory outcomes and economy integration remain outstanding.

### Extender impact

- This development branch now requires REAPER (`69A4D664A6284E06`) at project load. This is an explicit experimental exception to the base-game-only architecture; turning off Reaper QRF Test restores ground behavior but does not unload the dependency.

- The shared `AROR_GameMode_Base.et` now inherits commander and operation-manager components. Extenders should override these inherited components rather than add duplicates, and keep role bindings consistent with their scenario factions and loadouts.
- Force profiles must match configured commander factions. Ground profiles require matching crew, infantry and vehicle resources; unsupported airborne profiles are excluded.
- Executor pools and test fixtures are configured in Everon_Chicken; inheriting the shared game-mode template alone does not enable this test executor.
- Operation state is server-owned and session-local. No persistent save schema or client operation UI is introduced.

Architecture and project guidance are recorded in `ARCHITECTURE.md` and `CLAUDE.md`, with synchronized copies in `AGENTS.md`.
