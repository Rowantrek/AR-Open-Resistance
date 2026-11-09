# HICOMM and phone bug fixes — 10 November 2026

Branch: `chicken-dev`. Related issues: [#67](https://github.com/Rowantrek/AR-Open-Resistance/issues/67), [#37](https://github.com/Rowantrek/AR-Open-Resistance/issues/37), [#36](https://github.com/Rowantrek/AR-Open-Resistance/issues/36).

## Changes

- HICOMM renews finite simulation leases for living AI on its existing server status timer. The lease spans the configured update interval. Player-controlled characters are excluded. Native budget-dormant survivors keep their strength and receive paced spawn requests instead of being reported as wiped out. Actual elimination and form-up refunds retain their existing behavior.
- Phone gameplay and menu updates poll the same input latch, including the rebindable actions and Ctrl+Y/F9 keyboard fallbacks. One held press produces one toggle, including when native menus stop delivering the action. Other menus and dead players still block opening.
- HICOMM and OPS preserve their first boot timestamps in the client world session. Closing during boot resumes progress; reopening or logging out does not restart the loading animation. A new world starts fresh. Live HICOMM leadership checks remain active.

No RPC, campaign save format, resource GUID, prefab or extender configuration changes. Extenders using the inherited phone behavior gain shared input polling; custom code directly calling the former protected `OnPhoneAction` callback must use the shared `PollInput` entry point instead.

## Executed checks

- `node tools/Test-HighCommandSimulation.js`: 16 assertions pass, executing actual status/activation bodies against mocked native AI services. Includes lease renewal, long update intervals, dead/player exclusion, client guard, dormant strength, paced wake-up, reactivation, elimination and form-up timeout/refund.
- `node tools/Test-PhoneSession.js`: 111 assertions pass. Includes existing session/access checks, first boot at tick zero, menu recreation during boot, logout, both Ctrl keys, action-only/rebound input, F9, held presses and simultaneous menu/gameplay polling.
- `git diff --check`: passes.
- Broader player squad checks are blocked by an existing JavaScript harness error: `Test-PlayerSquads.js` throws `ReferenceError: Print is not defined` in `Hint`; `Test-PlayerSquadMutations.js` consequently rejects its baseline. The same direct test failure was reproduced from untouched commit `3e0fbd5a40b7d821abc68999cad557fe118483e6` in a separate baseline directory. These checks are not claimed to pass.

## Native acceptance required

Enfusion scripts cannot be compiled or run in this environment. In Workbench, compile in Script Editor, then Play the Chicken world in World Editor:

1. Raise HICOMM squads, move every player beyond normal AI simulation range and order a distant Move/Attack/Defend. Check that squads advance and fight without a nearby player, retain accurate living counts, and survive an AI-budget dormancy/reactivation cycle. Check true casualties and form-up refunds. Repeat on a dedicated server with two leaders.
2. Open and close the phone with left Ctrl+Y, right Ctrl+Y, F9 and a rebound action. Hold each press through the opening/closing animation; it should toggle once. Repeat inside HICOMM's native map, after Esc/button close, and after respawn. Verify other menus and death still prevent opening.
3. Close midway through the HICOMM/OPS boot animation and reopen. Switch apps, log in/out and reopen again. Boot progress should persist. Revoke HICOMM leadership and verify access is denied. Change world/reconnect and verify first boot starts again.

Issue #68 remains open and unchanged in this patch. Native early dismount behavior and the specific withdrawal trigger need reproduction/logs before changing QRF policy. No in-game fix for #68 is claimed.
