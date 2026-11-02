# MARS and territory attacks

Status: implemented for testing; Workbench compilation and native acceptance remain pending.

## MARS

MARS replaces the temporary **Test** phone tile. It uses a separate dark-red landscape layout, rose-red controls and cached circular instrument backdrop, with small hooks in `AROR_PhoneMenu`; REBINTEL, notifications and HICOMM keep their own screens.

In packaged multiplayer, sign in as a server administrator or session administrator, then open MARS and authenticate, then select **US / Occupiers** or **USSR / Invaders**. Pick a type in the paged 3×2 grid: **Motorized, Mechanized, Armour, Airborne, Combined Arms or Light Support**. Use the native **Readiness** and **Crackdown** sliders (1–5); cards show availability and resolved counts, and the preview shows the exact configured roster and budgets. Select **SUMMON RESPONSE**. **USE CAMPAIGN LEVELS** restores the current army's real metrics to the preview; the sliders do not change campaign state. Extra response families page through the same grid.

A seventh **AIR + GROUND** family is available on the next page when joint execution is enabled. Both armies have light recipes at Readiness 2, mechanized options at 3–4 and escorted/heavier recipes at 5. Standalone airborne responses use unescorted transport/fireteam packages from Readiness 1. See [release validation](QRF_RELEASE_VALIDATION.md) for exact tiers and native acceptance limits.

- Recipes deploy their authored element list once. Their tier does not multiply the list.
- Readiness unlocks profiles and recipe equipment pools. Crackdown chooses the highest eligible authored recipe tier at or below the requested level, or repeats a homogeneous profile 1–5 times. Airborne repetition means complete transport/fireteam packages with their configured zero or one escort. With the current assets, mechanized equipment unlocks at Readiness 3 and armour/escorted airborne at 5; light airborne unlocks at 1 and joint responses at 2; high Crackdown cannot bypass that gate.
- The target is the closest registered **Rebel-owned territory to the requesting player**. HQ does not count as territory. With no eligible Rebel holding, no operation is created.
- Launch uses the responding army's closest safe owned **outpost, military base or airport**. Losing all suitable origins blocks dispatch; configured fallback coordinates cannot bypass this requirement.
- Requests use the existing server operation/executor/tactical system. The server rechecks access, role, catalogue selection, player, target and origin. Each player's requests are limited to one every five scheduler seconds.
- Debug requests use the selected simulated Readiness and Crackdown, bypass campaign funds and ordinary QRF request cooldowns, and leave the actual campaign metrics and balances unchanged. The server independently resolves and freezes the selected metrics; it does not trust a client-supplied roster. Faction checks, scheduler slots, actor reservations, spawn safety and cleanup still apply. A request never changes an existing operation's selection. The older explicit catalogue request API remains available for existing debug callers.
- Server hints report admission/rejection; the bottom status line shows the latest debug operation state and terminal reason. Console logs retain the normal operation/tactical diagnostics.
- An explicit airborne selection goes through REAPER even while the ground recipe fixture is configured. Selecting a ground profile or recipe retains native ground dispatch even if the legacy blanket REAPER override is enabled. Debug operations do not unlock automatic attacks.

Chicken enables `m_bMarsEnabled` and `m_bAllowAirborneResponses`, with light and escorted airborne profiles for both armies. The blanket `m_bReaperQRFTest` override remains off. Other scenarios must opt in and supply their own profiles/recipes and military origins. Normal capture QRFs continue using the Readiness/Crackdown strategy; MARS does not turn every ordinary response into an airborne force.

The new `UI/Layouts/Phone/AROR_Mars.layout.meta` resource GUID is hand-authored. Let Workbench discover the layout and verify its resource reference before Play. Existing asset GUIDs were checked against the installed base-game and REAPER resource databases.

## Shared response scaling

`AROR_ResponsePolicy` is a side-effect-free selector shared by the MARS preview and server dispatch. Its equipment/intensity eligibility helpers are also used by normal ground strategy and airborne affordability checks. It reads existing profile `m_iMinimumReadinessLevel`, recipe minimum/maximum Readiness, recipe tier and element list. It returns the resolved recipe/profile, actual elements, group/vehicle/personnel budgets and a reason when equipment is locked.

Recipes now have `m_eResponseType`, defaulting to `COMBINED_ARMS` for existing authored mixes. Authors can add recipe tiers, alternate profiles and new families rather than adding dispatch branches to the UI. Homogeneous selection prefers the highest unlocked equipment profile within the selected family; Light Support remains separate from armoured vehicles. Extenders can append `AROR_EResponseType` values, supply recipes with that family and extend `AROR_ResponsePolicy.GetTypes`/`TypeName` to expose them. New response families page through the same app controls.

Readiness and Crackdown are frozen independently on the operation before execution. MARS equipment revalidation uses that selected Readiness, including the REAPER path, and the selected recipe is deployed once without a second intensity multiplier. The shared finance/capacity checks remain the final admission boundary for ordinary paid responses.

## Lamentin airborne planning correction

The Workbench log recorded `operation_2` targeting `everon_lamentin_port`, entering the flight adapter at 18:52:52 and failing at 18:53:28 because all 73 near-target landing candidates were rejected. No aircraft had been constructed; this was a landing-site planning failure. The original log did not identify which safety check rejected each point.

The search now retains the original 73 candidates and adds 48 staggered outer candidates, up to `m_fReaperLZFallbackRadius` (default 1,200 m). Two radial seeds are processed per executor tick; all spacing, water, slope and rotor-corridor checks remain. The complete pad must be dry. Diagnostics count water, slope, obstacle and spacing rejection reasons on both initial and replacement searches. A bounded per-operation preparation allowance covers incremental planning plus boarding without increasing ordinary ground spawn timeouts. Native Lamentin success remains to be retested.

The later session `logs_2026-10-27_19-38-09` confirmed that the wider radial search alone was insufficient. Airborne `operation_3` entered planning at 19:54:05 and failed at 19:55:05: all 121 seeds were rejected, with 69 water, 43 slope and 9 obstacle failures. No aircraft spawned. Flat, dry seeds rejected for obstacles now get a native empty-terrain search within `m_fReaperLZAdjustmentRadius` (60 m in Chicken), bounded to three native attempts. Suggested positions must remain inside both search bounds and pass the full helicopter safety and pad-spacing checks. Diagnostics append successful adjustments and adjustment queries. This improves coverage between radial seeds; a safe native Lamentin pad is still unverified.

## Lamentin combined-arms road launch correction

In the same session, combined-arms `operation_2` dispatched at 19:51:43 from Old Wood. Three lead-wave vehicle packages spawned, but the fourth exhausted all 36 road queries within 150 m. Rejections were 7 missing roads, 2 outside the search radius, 9 insufficient separation and 18 obstructions. The operation failed at 19:51:47 and cleaned up its records, before boarding or movement could complete.

Chicken now sets `m_fQRFRoadLaunchRadius` to 300 m, within the executor's existing supported range. The query limit, vehicle separation, collision, water, slope and player-distance guards are retained. Native acceptance requires all four lead packages to obtain separate safe launch positions, seat their occupants and reach `ACTIVE`; widening the search is not proof that Old Wood has enough usable road space.

## Lamentin ground boarding correction

The later Workbench session `logs_2026-10-27_19-38-09` recorded a capture QRF for `everon_lamentin_port`: admitted at 19:43:09, dispatched at 19:43:19 from Sawmill (about 2.2 km away), then failed at 19:44:49 with `Spawn acknowledgement timed out`. All three truck/IFV packages had spawned but never confirmed driver/passenger occupancy. Every retry reported zero accepted reseat requests. This operation's blocker is initial ground boarding.

Initial seat teleports now set the native `performWhenPaused` argument so they can execute outside the player simulation bubble. After `m_iQRFSeatRecoveryDelay` (10 seconds in Chicken), retries can interrupt a stuck get-in action for an unseated AI owned by that package, release only that actor's empty seat reservation, then request a seat again. Recovery requires initial deployment, a stationary ground vehicle and no player control; it never ejects occupants, clears another actor's reservation or interrupts active mounted/dismounting forces. Actual seat occupancy still gates activation, and the ordinary spawn timeout remains in force.

Retry diagnostics now distinguish accepted requests, attempted native calls, queue resets and busy actors. The log alone did not identify which native flags caused zero requests; the new diagnostics make that visible on the next Play run. `node tools/Test-QRFBoarding.js` passed 49 actual-source mocked assertions, including paused execution, seven-member IFV boarding, rejected requests, and player/occupant/reservation guards. Native boarding and replication still require Script Editor compile, reload Chicken and Play. At Lamentin, verify `Deployment faction verified` followed by operation state `ACTIVE`, then actual departure from Sawmill.

## Automatic attacks

Chicken enables `m_bTerritoryAttacksEnabled`. Occupiers attack eligible Resistance-held **military** sites; USSR choices are available in MARS, while autonomous USSR attack selection remains outside this policy.

1. A Resistance military capture establishes a fresh timer. A town/city is support-only; HQ is fixed. Neither enters this attack policy.
2. The capture QRF from either enemy role must physically reach **ACTIVE** and then end. A planned request, failed spawn or MARS debug response does not satisfy this condition.
3. Wait at least **900 seconds from capture** and at least **300 seconds after that QRF ends**. Both conditions must hold.
4. Wait for all outstanding operations and queued QRF threats to clear. Check for a safe Occupier origin and an affordable force within Readiness, Crackdown and shared actor limits.
5. Plan one attack with **60 seconds preparation**. Only one attack can be planned, spawning or active globally, including trusted requests from the other army. A headline names the threatened location.
6. Use the same force selection, finance, tactical commander, wave reservations, delivery, search, withdrawal and retention code as a QRF. The regular capture system decides when troops have actually retaken the site. Arrival alone does not count as capture; withdrawing without taking the target fails the attack.
7. After an attack ends, wait **900 seconds globally** before another automatic attack. Eligible targets rotate by their next-attempt time. Lost targets are removed; a fresh Rebel recapture resets their grace and prior-QRF requirement.

The five-minute post-QRF recovery and fifteen-minute post-attack rest prevent an immediate replacement wave. QRF requests retain their separate admission path and dispatch priority.

These timers and prior-QRF records are **session-only**, matching existing live-operation persistence. Loading owned territory establishes a fresh baseline; it does not fabricate a completed capture QRF. That holding requires a capture QRF in the current session before becoming eligible. Saving live attacks or their eligibility is follow-up persistence work.

Tuning lives on the operation manager:

| Attribute | Chicken value | Purpose |
|---|---:|---|
| `m_bTerritoryAttacksEnabled` | 1 | Automatic Occupier counterattacks |
| `m_iTerritoryAttackGrace` | 900 s | Minimum capture age |
| `m_iTerritoryAttackAfterQRF` | 300 s | Recovery after the capture QRF ends |
| `m_iTerritoryAttackRest` | 900 s | Global rest after an attack |
| `m_iAttackPreparation` | 60 s | Preparation before dispatch |

## Extension and multiplayer impact

- `AROR_Operation.UsesTacticalResponse()` includes attacks and QRFs; both have search plans. Existing tactical budgets and retention are shared across roles and operation types.
- The manager now enforces **one global attack**, even if an extender sets a larger per-role attack count.
- MARS selections live on the operation record, not mutable global fixture settings. Ordinary recipes remain ground-only; explicit `AIR_GROUND` recipes combine one airborne package with ground elements through the installed REAPER adapter. Requests validate the configured role/faction rather than borrowing the other army's assets.
- The player-controller bridge lives separately in `AROR_MarsPlayerController.c`. The manager replicates a small latest-debug-status string; requests/results and every gameplay decision remain server-owned. Matching client/server scripts and the new UI resource are required.
- No new ownership or economy authority, continuous per-frame server work, or external persistence files are introduced.

## Validation and Workbench acceptance

Automated checks run selected actual script method bodies with native services mocked. They cover entry validation, both armies' catalogues, explicit flight/ground routing, territory/origin rejection, request deduplication, the global attack slot, grace/recovery boundaries, capture-QRF activation, funding and resource pool structure. Run `node tools/Validate-QRFRelease.js`, including lifecycle, scheduler soak and mutation regressions, plus `Test-ForcePoolAssets.ps1` against the installed base-game resource database.

Current automated results, source hashes, the 350-cell response matrix, all 860 Chicken origin/target pairs and the remaining release gates are recorded in [QRF release validation](QRF_RELEASE_VALIDATION.md). Previously failing strategy, progression, town-hold and REBINTEL harnesses have been repaired. These checks still mock native services; current-source compile, driving, flight and multiplayer acceptance are pending.

Native acceptance still requires:

1. Script Editor compile, discover the new MARS layout, then Play in the Chicken world. Confirm existing phone apps still work and MARS opens in landscape. Reject incorrect entry, accept correct entry, lock again, and verify closing/reopening clears entry state.
2. As host and a remote dedicated-server client, check both army tabs and both native sliders. Readiness 1 / Crackdown 5 permits light unescorted airborne but must not unlock mechanized, armour, gunships or joint recipes. Readiness 2 unlocks light joint recipes; Readiness 5 / Crackdown 1 must give a small unlocked response. Check all response types and levels, army switching and USE CAMPAIGN LEVELS. Campaign metrics/balances must remain unchanged. No Rebel holdings except HQ must disable launch; missing owned origins must be rejected by the server.
3. For each army, launch motorized, mechanized, armour, light support, the two-element combined-arms fixture, each authored recipe tier and airborne packages 1–5. Verify exact profile/recipe retention, army affiliation, crew seating, road launch, unloading, sector orders and cleanup. Expect a capacity rejection/wait while previous forces still reserve actors.
4. For airborne responses, verify both native transport and gunship profiles use their corresponding REAPER pilot and faction. Check boarding, takeoff, ingress, LZ planning, landings, unloading, gunship support, withdrawal and player-distance retention. Test simultaneous ground/air reservations and the live-aircraft ceiling.
   Retest Lamentin Port at Readiness 5. When nearby pads fail, the log must show the outer search and rejection totals, followed by a selected inland pad or a finite diagnostic failure. Check troop navigation from the outer pad to the original target.
5. Capture a military site from each enemy army. No attack may plan before 15 minutes, before the capture QRF actually activates and ends, or during the five-minute post-QRF recovery. A QRF that fails during spawning must not unlock attacks. For a disposable fast test, temporarily shorten grace/recovery/rest attributes, then restore the documented values.
6. Hold multiple eligible sites: only one global attack may exist, and a new attack must wait fifteen minutes after the previous one ends. Check insufficient funds, shared capacity exhaustion, pending QRFs and loss of all US launch sites.
7. Observe an attack arrive without an ownership change: it must not immediately succeed. Verify normal AI recapture marks success; Resistance retention followed by withdrawal marks failure. Capture the target with another role during preparation to check cancellation.
8. Save/reload and verify live operations and attack eligibility reset as documented. Recapture a site in the new session and repeat the capture-QRF/grace path. Confirm towns/cities still change only through support and HQ is never selected by automatic attacks.

Mocked checks cannot verify Enforce compilation, native widget loading, driving, flight, engine AI activation or replication.
