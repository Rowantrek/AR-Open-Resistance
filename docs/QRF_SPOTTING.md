# Resistance spotting of QRFs

Status: implemented in source; **native visibility, client rendering and multiplayer acceptance are pending**. Chicken enables it on `AROR_OperationManagerComponent` with `m_bQRFSpotting = 1`. Other scenarios default off.

## Player behavior

Temporary Chicken debug mode: `m_bQRFDebugMarkers = 1` shows all spawned QRF vehicle contacts on living Resistance players' maps, updating positions every five seconds without spotting, range or visibility checks. Blue/red colouring and vehicle designations remain. Contacts stay refreshed while the vehicle is tracked, including retained hulls; cleanup removes them from the next snapshot. This performs no visibility traces. Set the flag to `0` to restore observation-based spotting; other scenarios default to `0`.

A living Resistance player looks toward a QRF vehicle. A second valid observation by that player confirms the sighting. The server then shares a last-seen marker with living Resistance players. Once confirmed, any valid Resistance observation can refresh it.

Markers distinguish **TRANSPORT VEHICLE, IFV, ARMOURED VEHICLE, TRANSPORT HELICOPTER and GUNSHIP**, with a 40-pixel contact icon, an 18-pixel type label and last-seen age. Occupier/US contacts are blue; Invader/USSR contacts are red. Server snapshots include the operation role, preserving side identification for empty transports. They use the existing map icon layout/coordinate conversion, so map pan and zoom continue to work. The icon is currently a shared military-contact image plus a type label, not five bespoke vehicle silhouettes. Foot squads are not marked by this first vehicle-intelligence implementation.

Positions are snapshots. When observers turn away, lose line of sight, die or leave range, the marker stays at the last report, fades and expires. No unseen entity is followed on the client. A destroyed/disappeared vehicle's last report can remain until expiry; removal does not give the player an unseen kill confirmation.

## Server rules and limits

- Only operation-owned QRF vehicles are candidates. The server never sends that full candidate list to clients.
- Observers and recipients must be living characters in the shared Resistance role faction, rather than a hard-coded FIA asset faction key.
- Sightings require range, a head-view cone and a native world/entity/visibility-occluder trace from the character's actual eye position. A clear trace or first hit on the target/root permits an observation. Friendly-affiliated captured vehicles are excluded from new observations.
- Pending confirmation needs two observations by the same player within a 15-second continuity window. Another observer cannot complete that first glance. Reports cap at 32.
- Round-robin work checks at most 64 player/vehicle pairs and performs at most eight native traces per server second in Chicken. Cheap range/view failures do not consume trace budget. At large player/vehicle counts, confirmation/refresh can take longer.
- Filtered snapshots are delivered every five seconds using reliable owner-targeted player-controller RPCs. There is no client request RPC accepting arbitrary target coordinates or entity IDs. New recipients receive current shared intelligence on the next snapshot.
- Opening a map suppresses that player's observations until it closes. An owned-controller RPC carries only this suppression bit; the server derives the player ID from the controller and still owns all sighting/visibility decisions. Disconnected observation flags are pruned. Reading the map does not provide a live target feed, although other FIA players can still refresh shared reports.
- Client caches store IDs, kinds, ages and positions only; they clear across world initialization and when rendering detects a non-Resistance/dead viewer. Map close removes widgets. Labels/images reuse the existing map layout; the new optional label child is blank for normal location icons.

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
6. Air contacts require a separate explicit airborne fixture: clear the fixed ground recipe override and enable REAPER. Spot transport and gunship separately. Invader admission stays disabled for the normal test.

Source-adapted checks cover view geometry, confirmation/refresh, packet bounds and expiry filtering. They do not replace those multiplayer and native trace tests.
