# Session deployment fixes

The 29 October console log (game 1.8.0.13) recorded repeated native compartment rejections, failed default-occupant replacements, joint-response timeouts and faction failures after pilot loss. This change addresses the source paths behind those symptoms. It does not establish that native activation resolves every rejected seat request, or that the WCS magazine exceptions are eliminated.

## Changes

- Operations activate the existing living AI members with the native world-limit check enabled and a short, expiring LOD lease. Initial activation yields one executor tick before boarding. Paced retries renew the lease; player-controlled actors/groups are excluded.
- Ground mounting establishes the whole passenger roster even when individual native requests fail. Failed requests wait for bounded recovery instead of immediately abandoning a partially seated roster. The shared readiness audit checks living assigned crew/passengers, driver ownership, every required gunner and physical compartment identity; retries report the first blocking condition.
- Ground boarding stops at `m_iQRFBoardingTimeout` (default 90 seconds). Existing operation admission/preparation deadlines still bound materialisation and joint activation. Failure uses the normal package/operation cleanup and finance lifecycle.
- Default-occupant replacement is disabled. No soldiers are deleted or spawned as a seating workaround; prefab choice, faction and personnel accounts stay attached to the original actors. `ReplaceUnseated_S` is retained as a compatibility no-op; its protected replacement helpers were removed. Extenders using those helpers need to migrate.
- Aircraft pilots use the same seat-owner/no-door teleport convention as ground troops. Readiness diagnostics distinguish pilots, passengers, capacity and the shared dispatch barrier.
- Aircraft accept native blank/source affiliation only after a complete empty/owned-AI occupant audit. Explicit hostile affiliation, human takeover, foreign occupants and missing components remain failures. Owned casualties remain in retention/accounting but do not cause a living-personnel faction mismatch. The first failed audit logs the entity and expected/actual faction.
- Shop metadata searches loaded faction catalogues directly with optional-catalogue diagnostics disabled, preserving content-addon entries. Preset augmentation also probes optional catalogues quietly. Other callers outside these paths may still produce catalogue warnings.
- Enemy soldier rearm is idempotent per character and excludes player-controlled characters. The native inventory replacement transaction retains the equipped weapon until replacement executes. Its completion callback changes magazines and selects the new gun; rejection removes only an unattached candidate. Detached old weapons are removed only after success. This removes our delete-first sequence; base/WCS AI exceptions still require reproduction.

The retry field previously labelled `paused` was the initial-deployment flag, not measured simulation state. It is now labelled `initialDeployment`.

## Workbench acceptance

Compile in Script Editor, then Play in World Editor. These checks are required before calling the changes verified:

1. Deploy one truck and one armed vehicle near players, then from an origin more than a kilometre from every player. Confirm the same original soldiers board, all required gunners are present, and each vehicle receives orders only after physical occupancy. Check native AI capacity prevents excess activation.
2. Reject/block a required seat. Confirm paced diagnostics name the blocking condition, actor counts do not increase, no default-character deletion loop occurs, and timeout releases/retains the package through normal accounting. Repeat with a player occupying the seat or controlling either group.
3. Kill a driver/gunner during preparation. An occupied corpse must not satisfy readiness. Restore a clean test package for each dispatch; no automatic refill is expected.
4. Deploy one transport-only, one escorted and one joint response. Verify pilot/passenger seating and flight. A ready aircraft waiting on a blocked ground package must report the shared barrier when its preparation deadline expires.
5. Kill a transport pilot during an active insertion and observe surviving infantry recovery. Test a confirmed empty aircraft whose native affiliation clears, a retained own casualty, and Resistance takeover. The first two must not masquerade as capture; takeover must still fail affiliation validation.
6. Open Black Market/Imports/Garage with base and WCS assets. Check names/images still resolve and these lookups no longer emit missing `ITEM` catalogue warnings for every role.
7. Spawn enemy squads with and without WCS. Verify one weapon swap per soldier, appropriate spare magazines, reload and combat. Reject a replacement and confirm the original gun remains. Retest the magazine-switch null-pointer and suppression-volume exceptions; those are not proven fixed by these source changes.

## Automated scope

`tools/Test-QRFBoarding.js` executes the boarding/readiness methods against mocked native services. `tools/Test-QRFReleaseLifecycle.js`, `tools/Test-QRFInterruptions.js`, `tools/Test-QRFIngress.js` and `tools/Test-QRFVehicleFaction.js` cover lifecycle and affiliation regressions. `tools/Test-SessionFixes.js` exercises pending/completed/rejected inventory replacements and optional catalogue lookup. None compiles Enforce or certifies native AI, inventory, replication, driving or flight.

The wider release runner also has pre-existing failures reproduced from an untouched HEAD archive: Mars/air readiness expectations, campaign progression defaults, and a garrison mock missing `ParkedPrefab`. Its mutation suite stops at the failing progression baseline. These are separate from this change; consult the generated validation report for current results.
