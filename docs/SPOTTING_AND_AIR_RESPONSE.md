# Spotting, map contacts and helicopter responses

Implemented source, 2026-11-09. **Workbench compilation, native visibility, map rendering and REAPER flight
acceptance are not verified.** This extends the existing spotting and airborne executors; it adds no Workshop
dependency or resource GUID.

## Spotting and map contacts

Both shipped Everon scenarios enable real spotting and disable `m_bQRFDebugMarkers`. Precise positions become
visible only after a living Resistance observer obtains two valid observations within 15 seconds. Each observation
needs range, head direction, hostile affiliation and a clear native visibility trace. Opening a map suspends that
player's observations. Other living Resistance players can still refresh shared reports.

There are seven separate vector badges, drawn through the existing `Frame` canvas in `AROR_MapIcon.layout`:

| Kind | Contact | Symbol |
|---|---|---|
| 0 | Transport vehicle | Truck cargo body, cab and wheels |
| 1 | IFV | Tracked hull and infantry cross |
| 2 | Armoured vehicle | Track outline, turret and long gun |
| 3 | Transport helicopter | Rotor, cabin and landing skid |
| 4 | Gunship | Rotor silhouette with weapon stroke and attack chevron |
| 5 | Light armed vehicle | Wheeled body with mounted weapon |
| 6 | Infantry squad | Infantry cross |

Each deployed dismounted group contributes its observed leader as a possible squad contact. Seated passengers
do not create squad markers. Infantry traces use the character's stance-aware eye position, so a prone soldier
does not become visible because a ray passes through empty space above their cover. Vehicle candidates include
both troop transports and gunship escorts, with their campaign role preserved. Destroyed and claimed assets
cannot create new sightings. Previously observed wrecks can retain stale reports until normal expiry; the map
does not supply an unseen kill confirmation.

Markers use the US/USSR UI colour tokens, show type and last-seen age, freeze when observations stop, fade and
expire after 90 seconds by default. Ground/infantry range is 800 m; helicopter range is 1,500 m. Client caches
contain snapshots rather than entity references. A reused widget immediately refreshes its class, faction and
label when packets reorder. Map close releases widgets and their owned canvas commands. Packet bounds,
duplicate IDs, valid kinds, report ages and lifetime are validated before rendering.

Work stays bounded: at most 64 candidate/observer pairs and eight native traces per server second in the shipped
scenarios, 32 pending/confirmed reports, five-second shared snapshots. Unconfirmed reports expire after their
15-second continuity window instead of occupying the report table for a full confirmed lifetime. No client RPC
accepts enemy coordinates or requests arbitrary sightings.

Scope: operation-owned QRF/attack assets, including their delivered infantry. This does not scan the entire island
for every unrelated garrison, civilian or Game Master unit. Native visibility, smoke, prone stance and vehicle
observations need Play verification. The phone's separate HICOMM renderer is not extended by these map widgets.

## Common transport insertions

Airborne and joint backends are enabled in both Everon scenarios. The legacy `m_bReaperQRFTest` diagnostic remains
off: turning it on would replace normal response selection, rather than enable the production airborne path.

- Chicken keeps its 100% target airborne share for ordinary campaign QRFs; base Everon increases its target from
  30% to 70%. These are running deployment-share targets, not a promise that every request can fly.
- Joint responses request one extra unescorted transport for each ground element (`m_iJointTransportPercent = 100`).
  Aircraft, actor, finance and launch limits can reduce the extras; there is no unlimited spawning.
- Every Chicken town response requests a dedicated insertion once its first ground wave is committed. The
  default town air threshold is Readiness 1. Both existing `_townair_r4` recipe IDs now cover Readiness 1–5;
  the IDs stay unchanged for extenders.
- The town recipe has one escorted airborne package and one light ground element. With normal spare capacity,
  that yields **two troop transports, one gunship and one light ground-support vehicle**. If extras cannot fit,
  the primary transport plus gunship remains the minimum selected air package.
- One accepted insertion request is tracked per town response, including its aircraft and infantry lifetime.
  Rejected requests can retry every five seconds. It uses owned launch sites, player clearance, real boarding,
  safe landing pads, native passenger exits and existing departure/retention rules. An accepted request does not
  prove the engine successfully flew or landed it; unsafe or unavailable aviation can still wait or fail.

`m_bAirOnFirstWave = 0` restores ground-pressure timing. `m_iAirAssaultReadiness = 0` disables the town insertion.
The town's force budget and ground-wave split rules remain separate from this tracked air-support operation.

## Gunship response and repeated flights

The escorted package provides a gunship response alongside the troop insertion. Existing flight logic escorts
the approach, then performs support passes around the town or fresh allied damage reports. The gunship still
needs native perception to engage a target; report coordinates are investigation areas. Existing faction,
civilian/friendly-clearance, ammunition, traffic and departure checks remain in force.

The default 300-second support window starts when the gunship reaches the town's support area. Its finite ingress
allowance includes the calculated flight deadline for distant origins. The window starts once, cannot be extended by repeated arrival checks,
and cannot restart after expiry or a withdrawal order. Town gunships use the town doctrine's shorter contact-age
limit where applicable.

Live aircraft and construction queues reserve airspace. Delivered infantry and retained wreck records no longer
reserve it after the aircraft have departed. The temporary landing-plan workspace is released after its pad
coordinates have been copied into queued packages. This allows later insertions without deleting the previously
delivered tactical units. Existing personnel reservations, aircraft limits and physical landing reservations still
apply. This is not a standalone gunship-only operation type.

## Automated evidence and native acceptance

`tools/Test-SpottingAndAirResponse.js` currently passes **89 actual-source policy/widget checks**, with engine
services mocked. It tests visual confirmation and occlusion, stale snapshots, map suppression, recipient filtering,
range and trace budgets, seven unique glyphs, marker reuse and expiry, candidate classification, early town
insertion/deduplication, gunship timers and airspace/planner release. The response-mix routing suite also passes
1,842 checks; the earlier town doctrine suite passes 496. The new suite is included in `Validate-QRFRelease.js`.

The broad runner passes **33/35 suites** after reconciling older harnesses with the revised APIs. Two garrison
harnesses still call the removed `InfantryPrefab` helper and remain failed. This is not release approval.
See the [current audit and native checks](validation/TOWN_GAMEPLAY_VALIDATION_2026-11-09.md). Detailed output is in
[QRF_AUTOMATED_RESULTS.json](validation/QRF_AUTOMATED_RESULTS.json).

Additional read-only checks passed: balanced source structure across 277 scripts, executor settings/recipe coverage
for both Everon scenarios, and all five pilot/flight-waypoint GUIDs against the installed REAPER addon's own resource
index. These checks do not establish native compilation or successful flight.

Compile Game scripts in Script Editor, then Play `AROR_Everon_Chicken` in World Editor:

1. At Readiness 1, attack a town and confirm a transport/gunship insertion request after wave one. Check passenger
   occupancy, safe touchdown, actual disembarkation and infantry advancing into town for both armies.
2. Before seeing aircraft, check there are no precise map contacts. Observe each aircraft, open the map and verify
   separate transport and gunship silhouettes. After troops dismount, spot the squad leader and check its infantry
   badge. Pan, zoom, close/reopen the map and change factions in successive snapshots.
3. Break line of sight. Positions must freeze, age and disappear; map viewing alone must not refresh them. Check
   low cover, prone soldiers, smoke and observation from vehicles separately.
4. Confirm the gunship gets its support interval near town, obeys native target perception and friendly clearance,
   then leaves. Once aircraft depart, request another insertion while the first infantry remain deployed.
5. Repeat on a dedicated server with two Resistance clients and a late joiner. Compare markers and timestamps;
   test death/respawn, claimed aircraft, failed landing, exhausted capacity and a blocked launch site. Record server
   frame time with town life, transports, gunship support and overlapping ground waves.

Extenders: no RPC signature changed; kind 6 extends the spotting packet's class range. Custom icon layouts need
the `Frame` canvas and `QRFLabel` text child already present in the shipped layout. Both server and client scripts
must be updated. Shipped scenario air frequency and the default town insertion threshold have changed.
