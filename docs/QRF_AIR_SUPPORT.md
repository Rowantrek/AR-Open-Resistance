# Gunship reports, search, traffic and outpost origins

This extends the existing Chicken REAPER QRF. Jasper reported the separate-pad landing revision working. These new changes still need Game-script compilation and a fresh Play test; offline checks do not validate backend flight behavior.

## North Ridge flight feedback, 27 October 2026

The session `logs_2026-10-27_20-03-54` records three airborne packages activating at 20:27:45, actual passenger delivery from 20:29:01, and Occupier recapture of North Ridge Works at 20:29:28. Jasper confirmed the airborne response worked, but observed gunships remaining too high to contribute. Orders used cruise bands of 210/245/280 m AGL and repeatedly yielded to predicted traffic; actual continuous altitude was not logged, so the log does not prove which maneuver caused the observed height.

Support now uses a run-in point followed by an opposite endpoint, crossing the reported search area rather than repeatedly changing perimeter destinations. Each complete run changes its observation axis; three packages command 110/130/150 m AGL at 130 km/h maximum. The configurable run radius defaults to 450 m. Reordering waits until within 80 m of an endpoint or the existing finite leg deadline. Fresh substantial contact changes and completed traffic yields restart the run-in. Cruise/egress bands remain higher; repeated yields are capped at the package's cruise band instead of climbing toward 600 m. A run log includes package, phase, search centre, endpoint, commanded altitude and actual altitude at order time.

`Reaper Gunship Run Height`, `Reaper Gunship Run Band` and `Reaper Gunship Run Radius` are executor attributes, explicitly set in Chicken. Native perception, hostile-target selection, nearby-non-hostile checks, finite ammunition, traffic priority, support duration and player-distance cleanup remain in force. A crossing is an observation/engagement opportunity, not an instruction to fire at an unseen entity. These lower runs and traffic interactions still need native Play verification, especially over hills and with three simultaneous transports.

The original ground response launched at 20:22:11 from a site about 1 km away. Its travel logs show vehicles moving without closing on the objective; package 3 went from approximately 486 m remaining at 20:23:57 to 876 m at 20:25:58. The old stalled-route check treated any movement as progress. The new check also detects 90 seconds without 20 m of net progress toward the objective, then tries a bounded road dismount on the current approach side before inner-objective roads. Physical arrival, safe foot placement and the original capture objective still apply. The 30-second check interval, four-retry ceiling and ingress deadline are retained. `QRF Detour Timeout` is tunable (90 s in Chicken). This does not replace native road pathfinding or guarantee the shortest driving route.

Run `tools/Test-QRFSupportAndHeadlines.js` and `tools/Test-QRFIngress.js` for mocked regressions. In Workbench, compile and reload Chicken, repeat the North Ridge ground/air responses, inspect the new route/pass diagnostics, and verify actual lower crossing runs, hostile spotting/firing, friendly clearance, traffic yielding and eventual departure.

## Reports and search

- Assigned defenders publish validated hostile-damage snapshots through the existing defense component. Reports reach air support even when QRF cooldown/capacity rejects another deployment.
- Operation-owned ground/air personnel and vehicles also report attributed hostile damage. This works with friendly-damage logging disabled. Unassigned world AI need an explicit defense assignment or a trusted server combat integration; there is no global player-position scanner.
- The operation manager retains at most 32 role/location reports, throttled to one update per location/role per five seconds. Reports contain a position/time/sequence, not a live attacker reference. Healing, Game Master events and non-hostile damage do not produce these operation combat reports.
- Active gunships poll every five seconds for the newest same-role report within 2,000 m of the defended town. Reports expire after 90 seconds without fresh combat evidence. Fresh first contacts or movements over 100 m can retask a pass; small updates refresh intel without continually replacing orders. A stale contact returns support to town search.
- Search passes use the run-in/crossing pattern above, with different package phases. REAPER must acquire targets through its native perception. The hostility and nearby-non-hostile weapon filters remain; a report does not force firing at an unseen player.
- Reports do not extend the configured 300-second gunship support window. Infantry keeps its existing frozen investigation/patrol plan. Contacts and routes are session-local and are not saved.

Executor attributes: `Reaper Support Radius`, `Reaper Contact Age`, `Reaper Gunship Height`, `Reaper Gunship Band`, `Reaper Gunship Run Height`, `Reaper Gunship Run Band`, `Reaper Gunship Run Radius`, `Reaper Traffic Clearance`.

## Air traffic

Gunships use commanded cruise altitude bands starting at 210 m AGL, separated by 35 m per package, through insertion, ingress and egress. Search/crossing runs use the lower support bands above. Search headings also differ by package. The native controller can deviate, especially during attack runs; altitude above terrain alone does not guarantee separation over different ground elevations.

Every escort monitor tick checks actual world position and velocity against tracked transports and lower-index gunships. A closest-approach prediction looks five seconds ahead. Within the default 100 m clearance, the gunship temporarily disables explosive engagement and receives an outward Move order, with altitude capped at its cruise band, for six seconds, then reevaluates traffic and resumes support. Transports have priority; gunships have stable package priority. Maneuvers do not teleport aircraft, replace pilots, extend support deadlines or disable collisions. Departure cleanup remains subject to distance from every player. This is advisory avoidance layered on REAPER, not a guarantee against collisions, terrain or untracked aircraft.

## Outpost origins

`OUTPOST` is appended as location type **8**, preserving all previous enum values. It follows existing military flag-capture and AI-recapture rules. No outposts or new resource GUIDs are invented or automatically placed by this change.

To use an outpost later, place an existing military location prefab in Workbench, set its location Type to **OUTPOST**, give it a unique stable ID and the correct initial controlling role, and save. Its inherited flag, icon and replication setup remain available. A friendly outpost's location centre is the origin for spawn offsets; dedicated road/helipad spawn-point authoring remains future work.

Ground and airborne QRFs select the nearest outpost to their defended target that is currently controlled by their role and passes the existing launch distance/land checks. The target itself is excluded. Full package offsets and air insertion volumes still have their normal validation. Captured enemy outposts are excluded. With no eligible outpost, existing configured role coordinates remain the fallback; the console reports that fallback. Automatic attacks retain their configured origins.

## Acceptance

1. Stop Play, compile Game scripts successfully, reload Chicken, start fresh and trigger the defender QRF.
2. With the gunships active, attack another assigned same-role defender or a deployed QRF soldier nearby. Expect `investigating allied report` and subsequent search passes around that report. No further hits for 90 seconds should return them to town search. Test the other role and an out-of-range report: neither should divert these gunships.
3. Verify same-faction/GM damage does not attract support. Turn off friendly-damage logging and confirm real hostile hits still report. Confirm support time stays bounded and firing still respects non-hostiles near the target.
4. Observe multiple flights and `yielding to predicted aircraft conflict` diagnostics. Verify yield maneuvers resume support and egress still occurs after the support deadline. Record actual collisions/near misses for tuning; avoidance has not yet been engine-validated.
5. When outposts are placed, test two friendly origins, a closer enemy-controlled origin, an unsafe origin and no outposts. Expect the nearest eligible friendly origin or the explicit configured-coordinate fallback. This revision leaves existing world locations untouched.

Offline validation ran source-adapted production method bodies with mocked engine/managers: five closest-approach scenarios, eight contact publication/throttle/expiry/role/range/capacity cases, and four origin-selection/fallback cases. Workbench-only traffic regression helpers are wired to the existing optional smoke-test runner but remain unrun. Dedicated-server and multiple-client acceptance remain outstanding.
