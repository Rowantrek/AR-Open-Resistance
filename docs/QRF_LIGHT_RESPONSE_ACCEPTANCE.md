# Light airborne and joint response acceptance

Status: source implementation and mocked regressions only. The user's successful compile covered the preceding version. Compile these changes again before Play. Use a disposable Chicken campaign and keep the complete log.

## Setup and smoke checks

1. Compile in Script Editor and reload Chicken. Record whether Workbench and packaged build variants were both compiled.
2. Run the existing memory-only smoke checks, including `AROR_JointResponseSmokeTests.Run_S()`. These do not spawn actors or establish route success.
3. Confirm airborne execution, joint responses and tactical coordination are enabled. Keep the blanket REAPER override off.
4. Test both army tabs in MARS. Its simulated Readiness/Crackdown values do not change campaign metrics or balances. Its debug deployments bypass campaign funding and do not establish automatic-attack eligibility.
5. Give the selected army a safe, uncontested military origin. Initial Invader holdings can restrict origin choice; test prospective ownership with a disposable fixture rather than treating lack of an owned launch site as a route defect.

## Nominal deployments

| Fixture | Expected package structure | Key observation |
|---|---|---|
| AIRBORNE, R1 C1 | One unescorted transport, one fireteam, one pilot | No gunship, escort pilot or missing-escort failure. Reservation: two groups, eight personnel slots, one aircraft. |
| AIRBORNE, R1 C5 | Five transport/fireteam packages | With current split-wave settings: three lead packages, two reserves; shared aircraft cap and construction pacing still apply. |
| AIR + GROUND, R1 | Unavailable | A forged/legacy request must not bypass the joint recipe's equipment gate through the current type/metric path. |
| AIR + GROUND, R2 C1 | Unescorted air package + one truck package | Both lead packages ready before dispatch; actual assigned driver and all transport passengers seated. |
| AIR + GROUND, R2 C5 | Unescorted air + three trucks + two light-support vehicles | No APC/IFV, tank or gunship; three lead packages and three reserves. |
| AIR + GROUND, R3 C2 | Unescorted air + truck + APC/IFV | Mechanized equipment is available; armour/gunship remain absent. |
| AIR + GROUND, R3 C5 | Unescorted air + two trucks + two APCs/IFVs + light support | Three lead packages and three reserves; separate infantry/support orders after delivery. |
| AIRBORNE, R5 C1 | One transport/fireteam/gunship package | Stronger profile selected; three groups and two aircraft reserved. |
| AIR + GROUND, R5 | Existing escorted/heavier recipes | Existing IDs, authored tiers, armour and reserve behavior remain available. |

Repeat each relevant fixture for Occupiers and Invaders. Use Lamentin, North Ridge and the three ports as priorities; expand to other endpoints after diagnosing failures. A camera view alone does not move the player's body for clearance checks.

Readiness and Crackdown are separate: Readiness unlocks equipment; Crackdown selects an authored tier or the number of standalone packages. At R3 C1 a light truck/air mix is valid even though mechanized equipment is unlocked.

## Capture QRFs and attacks

MARS exercises debug QRF execution. It does not exercise campaign funding, canonical capture triggers or automatic attack eligibility.

For paid acceptance, start a fresh disposable campaign at the required Readiness using the operation-manager fixture settings, provide known army funds and cause an actual Resistance capture from that army. Verify the admitted recipe and finance log rather than assuming the highest available force can be funded.

For attacks, use the existing trusted server attack interface or a genuinely eligible automatic Occupier attack. Invader automatic territory attacks are not introduced by this change. Verify:

- A joint attack releases air insertion with the ground assault; a joint QRF retains immediate insertion.
- One operation owns all packages and funds. Arrival alone cannot report successful capture.
- Ground and air reserve indices remain unique. No unbuilt or unready package receives orders.
- Loss of one active package does not erase every healthy package; loss of a required lead element during preparation cannot fabricate ACTIVE eligibility.

## Interrupt preparation and reserve waves

1. Destroy an aircraft or its pilot while another lead package is still preparing. Verify no stale readiness dispatch and no passenger teleport into a destroyed hull.
2. Remove a ground driver or put an unrelated actor in the driver seat while the assigned crew sit elsewhere. The occupied slot must not count as the assigned AI driver.
3. Capture, contest or unregister the committed launch site before a reserve wave constructs. Verify withdrawal/cancellation and no further construction at that site.
4. Move a player's body into the launch clearance area after planning but before construction. Repeat with a newly obstructed air insertion volume. Refusal must occur before creating that package's actors.
5. Cancel while only the transport, only a ground hull, or some groups have materialized. Surviving actors remain owned and consume capacity until safe removal; pure virtual reservations release once.
6. Check late group members during retention and native queue cancellation during final cleanup. Mocked tests do not establish native queue behavior.
7. Block pilot/passenger boarding during an ACTIVE reserve. At 120 executor seconds from package construction, it must fail that package and preserve healthy peers. Dormant wakeups and repeated seat requests must not refresh the deadline. Initial preparation can hit the manager deadline sooner.
8. Invalidate a ready ACTIVE reserve's crew or waypoint immediately before dispatch. Verify healthy earlier/later packages continue and receive orders once. A required composite lead failure during SPAWNING must still abort preparation.

## Retention, takeover and finances

An aborted, held aircraft without orders must not be released into an artificial uncrewed crash merely because preparation failed. Its retained ownership uses the existing age and all-player-distance gates. If a player directly occupies a retained aircraft, its held simulation must release; the retirement timer checks takeover every five seconds.

Inspect escort waypoints when cancelling before dispatch: the retirement queue must not issue egress orders to the unissued, held package. Already dispatched aircraft retain their normal retirement behavior.

Verify that player-controlled entities survive final cleanup, AI callbacks are detached, and nearby casualties/loot are preserved until the configured retention conditions permit removal. World unload uses immediate cleanup and must cancel timers.

For paid responses, record balances and reserved amounts before admission, after activation, after losses and after final removal:

- Healthy cleanup releases collateral; it does not add money.
- Activation overhead and confirmed losses are charged once, including repeated notifications.
- Cancelling unbuilt elements releases their quotes once.
- Fail a partially built reserve while a healthy lead package continues. After its final cleanup, only its unused margin releases; the future wave's full quote and any player-preserved physical assets remain reserved. A player-owned group that cannot be removed keeps its pending margin until operation closure.
- External spending can create debt; balances must not become negative.
- Closed, fully settled accounts are removed; retained physical assets remain accounted for.
- External deletion before loss observation must log an unobserved removal, not recovery. This cannot reconstruct erased death evidence. Separately verify a tracked destruction charges once before owned deletion.
- No funds must still produce the configured ground-only minimum capture response or a finite refusal/wait, never an unsecured airborne/joint response.

## Bounded route and exit recovery

Keep the existing four route-recovery attempts and 30-second check pacing under observation. A moving vehicle that makes no net objective progress should eventually consume the bounded detour path; its ingress deadline remains in force. No recovery may weaken water, slope, obstacle, player-distance or foot-path checks.

At a validated road stop, block all usable passenger exits. With current defaults, the package has an initial 30-second exit window plus two additional windows, then withdraws. Setting `m_iQRFExitRetryLimit` to zero disables the additional windows. Check partial exits too: already deployed infantry must not make the remaining passengers retry forever. Normal native exit/recovery safety checks remain enforced.

A withdrawal counts as recovery only after living personnel and surviving hulls physically return within the configured friendly-origin radii. A contested or enemy origin must not count as a safe return.

For R5 C5 standalone airborne, observe the fifth heavy package under the six-live-aircraft cap. Chicken now permits 450 seconds for reserves, beyond the configured 300-second support cycle. Confirm actual egress frees capacity in time; the mocked support-plus-30-second egress case does not establish native flight duration. Reserve expiry must still cancel an unbuilt package without spending its replacement quote.

## Planning sweep

On a separate idle run, invoke `AROR_QRFRouteAudit.Start_S()` and import the native log with `tools/Import-QRFRouteAudit.ps1`.

Each of the 924 pairs now records ground planning, escorted-air planning and light-air planning separately. An unsafe gunship corridor can fail the escorted mode while the transport-only mode passes. Older logs leave the new light-air field `NOT_RUN`. Missing safe pads block both air modes.

Planning results do not establish actual driving, landing, full-convoy space or native infantry navigation. Preserve every unrun gameplay/dedicated-server field as `NOT_RUN`.
