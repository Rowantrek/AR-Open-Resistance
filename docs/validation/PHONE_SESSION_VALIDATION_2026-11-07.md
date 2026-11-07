# Phone session validation — 7 November 2026

Closing the phone now remembers the last app and its presentation state for the current world session. Reopening returns directly in the appropriate orientation. The Phone component owns plain client data independently of the menu's widgets, so menu destruction and player respawn do not discard it. World teardown clears it. No profile or campaign save data is added.

Remembered state includes HICOMM's signed-in screen, selected squads, planned order and target, map mode/centre/zoom and panel toggles; OPS sign-in, category and selected mission; REBINTEL's army tab; Squad group/page and name draft; Black Market tab/search/scroll/payment choice and unsubmitted cart; Imports tab/scroll; MARS sign-in and simulation choices; and the Bank transfer draft. No action is automatically submitted on restore.

HICOMM still checks current leadership in its layout and all gameplay requests retain their existing server validation. Explicit logout persists. Losing leadership returns the app to login. Squad already has no separate login and still checks native leadership for its controls. Dismiss/abort confirmation and pointer/drag state are not restored. A new native squad resets the old group's roster page, and a shrinking roster clamps it.

Shop browsing no longer resets its scroll on every app reveal. Showing Black Market again also retains its in-flight checkout guard. A checkout already submitted is not preserved as a draft when the phone closes. A successful Bank transfer reply clears the saved amount even if it arrives while the phone is closed. Existing receipt/pop-up history and purchases are not replayed.

HICOMM captures a real terrain map before releasing it, restores the procedural view after bounds are initialized, and transfers that view when the asynchronous native map opens. Terrain zoom is not truncated to the procedural map's lower maximum. The existing map cleanup and marker suppression release remain in place.

## Executed checks

- `Test-PhoneSession.js`: **90 assertions pass**, executing the actual session capture/restore, app state methods, HICOMM layout/leadership guard, map view methods, successful/failed late transfer reply and Phone component initialization/deletion against mocked native services. Covers every phone app, portrait/landscape return, explicit logout, missing optional apps/search, failed initialization, dismissal confirmation clearing, submitted checkout handling, asynchronous terrain attachment, native zoom and fresh-world/dedicated-server guards.
- Full JavaScript source/mock run: **25/38 pass**, with the same thirteen previous first failure signatures and no new failures. This is not an all-green suite; see [individual outcomes and normalized source hashes](PHONE_SESSION_SUITE_2026-11-07.json).
- Existing squad checks: 408 assertions and all ten mutation controls pass. Existing WCS integration: 774 assertions pass.
- `Test-OperationUIResources.ps1`: metadata, script bindings and local registration pass. No layouts, resource GUIDs, generated databases or user Radio artifacts were changed.
- `git diff --check`: passes.

The previous WCS changes were already committed and pushed as `63e4392`. These changes affect Phone presentation and a client-side Bank reply, without changing RPC signatures, server authority or campaign serialization. Extenders inheriting the normal phone component/menu gain session restoration; new custom apps can add their own presentation fields and capture/restore methods.

## Native acceptance still required

AI cannot compile or run Enfusion here. Compile in Workbench Script Editor, then Play the Chicken world in World Editor:

1. Sign into HICOMM, select a squad, open Strategic and pan/zoom the terrain map. Close with Esc and reopen with the phone key. Check the same screen/view and no repeated login. Repeat through Home, another app, and the tactical-map toggle.
2. Sign out explicitly; close/reopen and confirm login remains required. Revoke leadership while HICOMM is open and while it is closed; verify current access and server request checks still apply.
3. Recruit riflemen, open Squad, change page or edit the name, close/reopen, and check the roster state. Arm a dismissal first and confirm that reopening requires a new confirmation. Change native squads or lose members and check page/group refresh.
4. Repeat close/reopen from each app and from Home, including shop searches/cart, OPS selection and a transfer draft. Close while rotating or holding the opening key, and check that the phone opens once in the correct orientation.
5. Close after checkout/transfer submission. Verify no purchase/transfer is repeated and a late successful transfer clears its saved amount.
6. Verify respawn retains this client session, reconnect/world change starts fresh, native game-map callbacks and location markers clean up, and dedicated-server clients retain independent presentation state.

Native rendering, menu/input lifetime, actual map initialization, RPC timing, respawn and multiplayer execution remain unverified.
