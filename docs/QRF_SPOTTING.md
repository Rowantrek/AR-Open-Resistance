# Resistance spotting of QRFs

Status: implemented in source; **native visibility, client rendering and multiplayer acceptance are pending**. Both shipped Everon scenarios enable it on `AROR_OperationManagerComponent` with `m_bQRFSpotting = 1`; debug reveal is disabled. See [spotting and air response](SPOTTING_AND_AIR_RESPONSE.md) for the current implementation and checks.

## Player behavior

`m_bQRFDebugMarkers = 0` in both shipped scenarios. Setting it to `1` is an explicit debugging override that reveals tracked contacts without visibility checks; it is not the gameplay spotting mechanic.

A living Resistance player looks toward a hostile response vehicle or a dismounted squad leader. A second valid observation by that player confirms the sighting. The server then shares a last-seen marker with living Resistance players. Once confirmed, any valid Resistance observation can refresh it.

Markers distinguish **TRANSPORT VEHICLE, LIGHT ARMED VEHICLE, IFV, ARMOURED VEHICLE, TRANSPORT HELICOPTER, GUNSHIP and INFANTRY SQUAD** with seven distinct vector silhouettes, a 40-pixel badge, an 18-pixel type label and last-seen age. Occupier/US contacts are blue; Invader/USSR contacts are red, using the UI colour tokens. Server snapshots include the operation role, preserving side identification for empty transports. The existing map layout and coordinate conversion handle pan and zoom. Reused widgets redraw both their silhouette and faction when snapshot ordering changes, and refresh labels immediately. Profile `m_bLightVehicle` classifies light armed vehicles as kind 5; kind 6 is a deployed dismounted squad leader. Kinds 0–4 retain their previous meanings. Server and clients must run matching scripts. Squad markers follow the observed leader's last-seen position, not an omniscient group centroid.

Positions are snapshots. When observers turn away, lose line of sight, die or leave range, the marker stays at the last report, fades and expires. No unseen entity is followed on the client. A destroyed/disappeared vehicle's last report can remain until expiry; removal does not give the player an unseen kill confirmation.

## Server rules and limits

- Only operation-owned QRF/attack vehicles and deployed dismounted group leaders are candidates. Seated passengers do not create infantry markers. The server never sends that full candidate list to clients.
- Observers and recipients must be living characters in the shared Resistance role faction, rather than a hard-coded FIA asset faction key.
- Sightings require range, a head-view cone and a native world/entity/visibility-occluder trace from the character's actual eye position. A clear trace or first hit on the target/root permits an observation. Friendly-affiliated captured vehicles are excluded from new observations.
- Pending confirmation needs two observations by the same player within a 15-second continuity window. Another observer cannot complete that first glance. Unconfirmed contacts expire after that window; confirmed reports cap at 32 and retain their normal lifetime.
- Round-robin work checks at most 64 player/vehicle pairs and performs at most eight native traces per server second in Chicken. Cheap range/view failures do not consume trace budget. At large player/vehicle counts, confirmation/refresh can take longer.
- Filtered snapshots are delivered every five seconds using reliable owner-targeted player-controller RPCs. There is no client request RPC accepting arbitrary target coordinates or entity IDs. New recipients receive current shared intelligence on the next snapshot.
- Opening a map suppresses that player's observations until it closes. An owned-controller RPC carries only this suppression bit; the server derives the player ID from the controller and still owns all sighting/visibility decisions. Disconnected observation flags are pruned. Reading the map does not provide a live target feed, although other FIA players can still refresh shared reports.
- Client caches store IDs, kinds, ages and positions only; they clear across world initialization and when rendering detects a non-Resistance/dead viewer. Map close removes widgets and their owned canvas commands. The shared layout contains `Icon`, `Frame` and `QRFLabel`; custom layouts need the `Frame` canvas for contact silhouettes. Duplicate IDs, unsupported kinds and invalid lifetimes are rejected.

| Manager attribute | Chicken value |
|---|---:|
| `m_bQRFSpotting` | 1 |
| `m_fQRFSpotGroundRange` | 800 m |
| `m_fQRFSpotAirRange` | 1,500 m |
| `m_fQRFSpotMinimumDot` | 0.9, about a 26° half cone |
| `m_iQRFSpotTraceLimit` | 8 per second, globally |
| `m_iQRFSpotLifetime` | 90 seconds |

The visibility trace is a conservative prototype of player observation. It does not model illumination, eyesight, magnification or identification training. Server head direction, prone/vehicle observations, smoke/vegetation occlusion and perceived target classification need native verification. No change to native AI perception or weapon engagement is made by spotting.

## Play acceptance

1. Stop Play, compile Game scripts, reload Chicken and trigger a US QRF. Before seeing it, open the map: there should be no QRF marker.
2. Look at the approaching truck/LAV in clear view for several seconds. Open the map and verify a correctly labeled contact at its observed position. A second FIA player should receive the same report.
3. Turn away or go behind a building. Keep watching the map: the marker must stop moving, age, fade and disappear after 90 seconds without fresh observations by any FIA player. Watching the map alone must not refresh it.
4. Verify no new sighting through terrain/buildings, outside range or behind the observer. Check prone stance and smoke separately.
5. Verify a non-Resistance client receives no intelligence packet, and a dead/non-Resistance local viewer cannot render cached contacts. Test late join and map close/reopen.
6. Trigger a Chicken town response at Readiness 1. Spot its transport helicopters and gunship separately, then spot the delivered squad after it unloads. Before spotting, no precise aircraft or infantry marker should appear. Repeat for Invaders and test a late joiner.

Source-adapted checks cover view geometry, confirmation/refresh, packet bounds and expiry filtering. They do not replace those multiplayer and native trace tests.
