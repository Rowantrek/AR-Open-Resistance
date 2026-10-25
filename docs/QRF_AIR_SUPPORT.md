# Gunship reports, search, traffic and outpost origins

This extends the existing Chicken REAPER QRF. Jasper reported the separate-pad landing revision working. These new changes still need Game-script compilation and a fresh Play test; offline checks do not validate backend flight behavior.

## Reports and search

- Assigned defenders publish validated hostile-damage snapshots through the existing defense component. Reports reach air support even when QRF cooldown/capacity rejects another deployment.
- Operation-owned ground/air personnel and vehicles also report attributed hostile damage. This works with friendly-damage logging disabled. Unassigned world AI need an explicit defense assignment or a trusted server combat integration; there is no global player-position scanner.
- The operation manager retains at most 32 role/location reports, throttled to one update per location/role per five seconds. Reports contain a position/time/sequence, not a live attacker reference. Healing, Game Master events and non-hostile damage do not produce these operation combat reports.
- Active gunships poll every five seconds for the newest same-role report within 2,000 m of the defended town. Reports expire after 90 seconds without fresh combat evidence. Fresh first contacts or movements over 100 m can retask a pass; small updates refresh intel without continually replacing orders. A stale contact returns support to town search.
- Search passes change heading and alternate 320/460/600 m radii around the reported area, with different package phases. REAPER must acquire targets through its native perception. The hostility and nearby-non-hostile weapon filters remain; a report does not force firing at an unseen player.
- Reports do not extend the configured 300-second gunship support window. Infantry keeps its existing frozen investigation/patrol plan. Contacts and routes are session-local and are not saved.

Executor attributes: `Reaper Support Radius`, `Reaper Contact Age`, `Reaper Gunship Height`, `Reaper Gunship Band`, `Reaper Traffic Clearance`.

## Air traffic

Gunships now use commanded altitude bands starting at 210 m AGL, separated by 35 m per package, through insertion, ingress, search and egress. These sit above the default transport route bands. Search headings also differ by package. The native controller can deviate, especially during attack runs; altitude above terrain alone does not guarantee separation over different ground elevations.

Every escort monitor tick checks actual world position and velocity against tracked transports and lower-index gunships. A closest-approach prediction looks five seconds ahead. Within the default 100 m clearance, the gunship temporarily disables explosive engagement and receives an outward/climbing Move order for six seconds, then reevaluates traffic and resumes support. Transports have priority; gunships have stable package priority. Maneuvers do not teleport aircraft, replace pilots, extend support deadlines or disable collisions. Departure cleanup remains subject to distance from every player. This is advisory avoidance layered on REAPER, not a guarantee against collisions, terrain or untracked aircraft.

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
