# Add paid QRF strategy, vehicle intel and Everon capture locations

## Summary

Enemy commanders can select and fund threat responses using the existing campaign economy, war levels and location importance. Ground QRFs gain more reliable launch, seating, delivery and town-security behavior. Everon_Chicken gains 41 flag-bearing campaign locations, aligned with Rowan's existing airport, factory and resource anchors.

This extends the tactical commanders and staged waves already integrated through PR #11. It does not add another bank, income authority or territory-ownership system. Native testing confirms road ingress, partial objective arrival/recapture and loss debits; complete multi-package delivery and the latest corrections still need acceptance testing.

## Changes

### QRF economy and loss accounting

- Add designer-authored personnel, vehicle and escort replacement prices and a response-price catalogue.
- Reserve complete-response collateral before spawning. Charge a small deployment overhead once when the operation activates, covering later waves under the same deployment.
- Debit the existing faction bank for individually confirmed deaths, destruction or capture, once per tracked asset. Normal recovery, retirement, aircraft egress and unused virtual reserves do not manufacture replacement losses.
- Retain unpaid liabilities when external spending consumes reserved funds; service debt from future funds before new paid responses. Do not allow negative balances or mint refunds.
- Add versioned serialization for faction war pressure and QRF debt. Bank balances remain owned and saved by the existing Economy system.
- Integrate the existing REAPER execution path with affordability, reservations and normal asset retirement. Flight dispatch remains disabled in Chicken.

### War levels and threat prioritisation

- Maintain independent Occupier and Invader war pressure. Validated combat, captures and confirmed losses can escalate it; quiet time reduces it.
- Select a response level from 1–5 using war level, bounded variation and threat context, then downgrade to an affordable complete response when necessary.
- Coalesce role/location reports into a bounded queue, prioritising strategic importance, capture, sustained reports and waiting age. Expire stale threats and defer blocked candidates.
- Author five ground response tiers for each opposing role, using the existing faction/profile bindings and staged-wave policy. Later waves retain their original recipe and allocation.
- Keep canonical admission checks, cooldowns, capacity, target conflicts and role gates authoritative. Invader dispatch remains disabled in the current fixture.
- Treat current difficulty as provisional balancing for 3–4 capable players; no combat-difficulty or performance acceptance is claimed.

### Ground execution, investigation and town security

- Consolidate tactical executor members/helpers into the original executor component to resolve Workbench visibility failures from the separate modded-class implementation. Remove the obsolete tactical-executor script and correct Enforce reference-array/type issues.
- Resolve clear, separated road launch pads near friendly strategic origins instead of spawning vehicles at unchecked lateral outpost coordinates. Bound road searches and reject obstructed, steep or submerged pads.
- Place crew/passengers directly into seats without boarding pathfinding. Confirm actual driver, crew and passenger occupancy before declaring readiness and dispatching.
- Register transports with both crew and passenger group utilities. Gate infantry orders during mounted ingress so one early exit cannot pull the remaining passengers out prematurely.
- Require physical arrival within the objective delivery area before the commanded road-stop unload. Keep road alternatives bounded and allow safe compact spacing when preferred separation cannot fit.
- Retry passenger exits through usable doors, stagger accepted requests and leave doors open. After a configurable delay, stopped ground vehicles can use native cleared-position recovery for stuck AI passengers; players, crew and aircraft are excluded.
- Allow delivered infantry and fully dismounted survivors to investigate recent reported contact, then search separate town sectors. Preserve surviving infantry after hull loss.
- Add bounded stalled-route retries and nearby AI driver recovery. Do not teleport vehicles to the objective or declare successful arrival from waypoint completion alone.
- Start a configurable 15-minute security hold on physical objective entry. Keep later arrivals from restarting it; fixed security mode suppresses casualty-driven withdrawal while safety checks and outer deadlines remain authoritative.
- Require surviving personnel/vehicles to return near a friendly origin for recovery. Retain nearby terminal actors under the existing age and all-player-distance cleanup rules.
- Handle native blank vehicle affiliation during unloading/combat without repainting the vehicle: tolerate the fallback only after an empty/own-AI seat audit, while preserving rejection of foreign occupants and explicit faction mismatches. Active package failures preserve healthy elements.

### Capture-driven responses and origin integration

- Observe canonical location-control events and queue/reuse the displaced hostile AI owner's QRF after an enabled capture. Initial registration/restoration establishes a baseline without generating historical responses.
- Support enabled settlement and military-location captures, including outposts, bases, airports, factories, ports and resource sites, under the existing request/cooldown/funding gates.
- Accept friendly ordinary outposts, military bases and airports as strategic dispatch/return origins. Preserve configured role coordinates as the existing fallback.
- Keep captured objectives as investigation/security tasks rather than treating a control change as an automatic end to the response.

### Resistance map intelligence

- Add bounded server observation of tracked QRF vehicles using player view direction, range, confirmation and native visibility traces.
- Send filtered last-seen role/type/position/age snapshots through owner-targeted player-controller RPCs to Resistance recipients. Unseen contacts stop moving, fade and expire.
- Display larger map icons and captions for transport vehicles, IFVs, armour, transport helicopters and gunships; Occupier contacts are blue and Invader contacts red.
- Enable a temporary Chicken debug mode that continuously exposes tracked QRF vehicle positions to Resistance maps. Normal observation-based spotting remains available when debug visibility is disabled.

### Everon locations, flags and Rowan alignment

- Populate Chicken with 41 stable campaign locations: 7 cities, 11 towns, 14 ordinary outposts, 5 military outposts, 1 airport, 1 factory and 2 resource sites.
- Add reusable Town_Flag, City_Flag, Outpost and MilitaryOutpost prefabs with matching resource metadata. Preserve existing location enum values and original support-only settlement templates.
- Opt flag-bearing settlements into the existing capture action and weighted AI recapture. Update support through the existing settlement authority so support and control remain coherent; retain hostile-clearance, fixed-control and flip-cooldown checks.
- Preserve established location anchors and the two assigned Provins defenders. Align Rowan's airport, factory and resource placements in Chicken, replacing the duplicate airport outpost without rewriting Rowan's scene files.
- Add an editable CSV catalogue, deterministic layer generation, validation and coordinate-source attribution/license files. Provisional settlement population totals 1,230; existing income rules apply without a new income implementation.
- Bake pole heights from a native 41-site editor terrain/model-bounds audit. Account for the model extending 0.805946 m below its pivot, place the model base 0.1 m above measured terrain and preserve capture contexts at terrain +1.6 m.
- Add a shared server placement component for non-baked capture poles. Chicken's baked poles disable automatic runtime search.
- Add read-only editor auditing and undoable grounding tools for Chicken. Grounding edits the real child source transform, supports parent rotation/model bounds and participates in normal world Save/Undo.
- Do not spawn garrisons or scenery compounds and do not claim that a placed flag establishes viable vehicle access.

### Documentation and developer tooling

- Update architecture and synchronize its full AGENTS.md copy; leave CLAUDE.md unchanged.
- Add economy/strategy, pricing, spotting and Everon placement documentation, native audit evidence and regression tools.
- Update force-pool, executor, tactical-commander and truck/IFV test guidance, distinguishing implementation from native acceptance.

## Testing

### Checked on the current working tree

- Everon catalogue checks pass for all 41 locations: unique IDs, original/Rowan anchors, prefab GUIDs, source-coordinate provenance, hierarchy offsets, model-base clearance, capture-context height and QRF origin gates.
- Installed-asset/configuration checks pass: 25 GUIDs, navigation resources, eight force profiles, fixed recipes, five capacity-checked tiers per opposing role and fixture opt-ins.
- 89 actual-source mocked regression assertions pass: 21 ingress, 40 unloading, 14 combat affiliation and 14 editor-tool checks. These use mocked native services and are not an Enforce compile or gameplay test.
- `git diff --check` passes.

### Observed in Workbench

- Compilation/startup, capture-driven admission, direct seating and Occupier affiliations were verified in earlier runs.
- The latest airport road run selected a tier-3 response at war level 2 and deployed three first-wave elements plus two reserves. Vehicles drove from Farm; armour entered the airport, began the 900-second hold and Occupiers recaptured the location.
- Finance logged 80 overhead, six personnel debits of 25 and a vehicle debit of 1,000, all paid with zero outstanding debt. The log does not identify the specific lost hull; these are recorded debits, not a net balance after income.
- The native read-only editor audit inspected all 41 poles with zero clearance warnings and supplied the terrain/model measurements used for the final bake.

### Still required

- Compile/reload the final source and retest the combat-affiliation correction.
- Visually inspect the corrected poles after reloading Chicken, then verify capture interactions. The final model-base/context bake has passed data checks but has not received a complete visual/Play pass.
- Verify every package arrives, unloads, investigates, remains for the full hold, returns and cleans up appropriately. Prior tests establish partial success, not the entire lifecycle.
- Validate routes and practical launch/dismount/return points across the island, adding authored overrides where automatic planning is inadequate.
- Test dedicated-server/multiclient replication, marker recipient filtering, non-debug spotting, save/load debt/war state and sustained performance.
- Run a separate explicitly configured USSR test. Invader gameplay dispatch is intentionally disabled.

## Notes

- Suitable for an integration/draft PR with the above acceptance work visible; this is not a release-ready declaration.
- Balances, income, support and location control continue to use Rowan's existing authorities. Physical vehicle stock, factories supplying assets, garrison planning and resource-backed readiness remain future work.
- Durable restoration of deployed operations, queues, collateral claims and spotting contacts is not implemented; these remain session-local. Saving war pressure/debt does not restore a deployment.
- Extenders must review new force-profile price attributes, strategy/capture/spotting opt-ins, the operation-manager serializer, role/type-bearing spotting RPCs, map captions, location flag templates and the shared capture-pole placement component. Server and clients need matching updated scripts.
- Tactical members now live on the original executor component; extenders referencing the removed AROR_QRFTacticalExecutor script must update their integration.
- New prefab GUIDs are hand-authored and checked against their sibling metadata. Workbench resource discovery and native registration must be retained in validation.
- The normal addon still has the existing experimental REAPER load-time dependency. Disabling flight dispatch does not remove it; a base-game-only release still needs that branch exception resolved.
- The new WorkbenchGame/Plugins sources are intentional editor tools. The separate gitignored EnfusionMCP handler folder must remain excluded and be removed before packing, per project guidance.
- Keep the Everon coordinate-source attribution/license files. Reconcile future manual editor placement changes into the CSV before regenerating the location layers.
