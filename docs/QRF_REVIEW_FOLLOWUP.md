# QRF review follow-up

## Tier-one start and linear response balance — 29 October 2026

New campaigns start at Readiness 1 and 0% Crackdown, requesting tier 1. Each newly recorded capture loss adds 50 Crackdown points (five percentage points), replacing 150. With captures alone and no cooling, tiers 2/3/4/5 begin after 4/8/12/16 losses by that army. Readiness thresholds, combat/loss evidence, quiet-time cooling and existing saved progress remain independent and unchanged. Loaded campaigns retain their current tier; new capture increments use the updated rate.

Both armies' light and mechanized recipes grow from one to five complete packages. Heavy ground and joint bands grow from two to six packages. Each tier adds exactly one package and strictly increases reserved personnel and collateral within its band; differences in vehicle capacity/firepower mean this is not a promise of identical combat-strength increments. The upper ground tiers and heavy joint tier 5 are smaller than before. Full native passenger loads, assigned driver/gunners, boarding acknowledgements, landing checks, safe infantry exit and finite preparation/recovery limits remain in force.

Updated prices, force composition, lead/reserve counts and MARS previews are exported in the existing CSV evidence. Regression checks cover both armies, fresh defaults, every capture-only tier boundary, independent Readiness/army state, all six ground/joint bands and preserved full-load profiles. Mutations restore the old tier-2 start, old capture jump and a two-package tier jump to prove those regressions are detected. The installed-resource validator also follows Chicken's existing five-minute attack grace instead of its stale fifteen-minute expectation; this balance change does not alter attack timing.

Native acceptance: compile current source and reload Chicken from disk into a new campaign. Confirm both armies begin at tier 1; test successive captures while separately tracking combat/loss evidence and cooling. Exercise both armies' tiers 1–5 through MARS, including joint waves. Confirm every vehicle still boards its full planned load and retains driver/gunners when infantry unload. Check a saved campaign retains its previous Readiness/Crackdown. Native balance and combat difficulty remain unverified.

## Airborne cabin loads, preparation and USSR origin follow-up

**Compilation:** the user reports successful compilation of this revision on 28 October 2026. Native gameplay acceptance of the new cabin, boarding and origin behavior remains pending.

The 20:23 Workbench session on 28 October 2026 records three light US transports for `operation_4`, aimed at Regina Heights Outpost. Landing search completed at 20:34:00; aircraft were constructed at 20:34:01–03. Only package 3 confirmed four boarded infantry at 20:34:32. At 20:35:01 the manager failed initial preparation. There is no flight-dispatch evidence for this operation: aircraft were held during preparation, then retained after failure. The authored fireteam also explains why the cabin was not full.

All four Chicken airborne profiles now enable the native passenger-capacity policy. A transport pilot and optional escort pilot are accounted separately; the infantry queue fills usable cargo seats, excluding weapon compartments. Transport weapon crewing is unchanged. Each air package reserves 32 personnel slots, a ceiling rather than a measured seat count. Chicken's executor personnel cap increases from 128 to 160 to admit five full packages without reducing the authored intensity; group, vehicle and live-aircraft caps remain unchanged. More infantry and higher collateral require native balance/performance acceptance. Updated quotes are in [QRF prices](QRF_PRICES.csv).

Air passengers and ground retries now address distinct compartment destinations even if native reservations/occupancy have not updated yet. Previously every unseated member could choose the same first free seat during an asynchronous tick. Tests reproduce delayed reservation updates and require unique destinations. This confirms a source-level race; the existing native log cannot establish whether it caused every stalled boarding request.

The initial airborne timeout previously budgeted only about 60 seconds after the landing search, while each package had a 120-second preparation window. Initial admission now includes the finite search allowance, paced aircraft construction and the full configured preparation window. Repeated planning ticks do not refresh it; each package also keeps its frozen construction deadline. Paced per-package logs show living/planned infantry during boarding waits. No aircraft dispatches merely because a seat request was accepted.

The USSR `operation_2` selected Vostok at 20:27:39 but rejected a preliminary formation seed at 20:27:40 before trying the bounded road-pad planner. Tactical ground construction now checks the actual selected road pad rather than failing early on that seed; road, water, obstruction and player-clearance checks remain enforced. This is distinct from a MARS request refused for having no eligible origin. MARS now logs replies and explains owned origins rejected by clearance, contention, cooldown or garrison depletion. The exact original refusal text was not present in the existing log, so its cause is not asserted.

Native reproduction: compile this revision and reload Chicken. Test US and USSR light/escorted airborne at Crackdown 1 and 5, inspecting cabin counts, separate pilots, all-package readiness, takeoff, landing and infantry exit. Deliberately block one passenger and confirm a finite package/operation timeout, not an infinite hover. For USSR ground and joint responses, test Vostok launch planning toward North Ridge and inspect actual road-pad results. Test MARS again and retain its new origin-rejection message if refused. No native success for these new changes is claimed.

## Full ground transport loads

Chicken's eight ground profiles now enable `m_bFillPassengerSeats`. Before requesting any character spawns, the executor inspects the actual vehicle compartments, reserves driver/turret seats for crew, and repeats the profile's authored infantry mix to fill usable passenger seats. Infantry and vehicle crew remain separate groups. The delivery pipeline now also covers light-support and armoured profiles carrying passengers: drive to the planned safe road stop, request infantry exits, verify completion, then assign infantry ground orders while the driver/gunners retain the vehicle. Existing bounded exit retries, native exit clearance and player protections remain enforced.

The personnel ceilings are 22 per truck, 12 per LAV/BTR, and 8 per Humvee/UAZ/BRDM. These are conservative reservation limits, not measured occupancy counts. Actual capacity determines spawning. A hull whose required crew, passenger capacity or configuration cannot fit its frozen reservation fails preparation before character spawning; this must be checked on the installed native vehicles, including the BRDM's compartment layout. Crew-only extender profiles retain their existing behavior by leaving the new attribute off. Airborne passenger loads are covered by the later follow-up above.

Passenger-carrying armour/light profiles reserve two groups. Updated collateral and slot ceilings appear in [QRF prices](QRF_PRICES.csv); exact physical totals remain `NOT_RUN` until native observation. No actor is added beyond its profile allocation. The 62 recipes and all 50 army/Readiness/Crackdown combinations pass installed-resource/configuration checks against the then-current 128-person/20-group/10-vehicle executor caps; the later airborne follow-up raises only the personnel cap to 160. Full-load and reservation mutations exercise the new roster construction, and animated-exit tests cover a 13-soldier load while leaving driver/gunner exit queues untouched.

**Native status:** the gunner-only version compiled successfully in the observed 19:26 Workbench run on 28 October 2026. That run subsequently showed rejected boarding retries for a truck and Humvee, ending in a spawn-acknowledgement timeout at 19:29:33. The existing logs do not establish the native rejection cause. This full-load revision needs a fresh compile and Play test; neither the boarding timeout nor sustained native crew occupancy is claimed resolved. Repeat ordinary QRFs and joint reserve waves for both armies, counting passengers at departure and after unloading. Confirm all infantry exit only after arrival, every required crew seat remains occupied, and no player is moved or ejected.

## Armed vehicle crew follow-up

The user observed an M2 Humvee with its weapon present but its gunner seat empty. Source inspection confirmed that dispatch checked the driver and whether crew were aboard, without requiring turret occupancy. Seating recognised only `TurretCompartmentSlot`; the installed native API also exposes attached turrets on other compartment types. The exact native seat type and the moment the Humvee lost its gunner remain unverified.

Ground preparation and recovery now prioritise turret seats and compartments with `GetAttachedTurret()`. Occupied or inaccessible required seats are not silently omitted. Immediately before dispatch, every such seat must contain a living AI member of the assigned crew group; an empty seat, foreign occupant, player or dead gunner blocks dispatch. Direct seating also respects accessibility and other actors' reservations. No extra personnel are spawned and funding/capacity budgets are unchanged. Existing preparation deadlines and package failure policy still apply.

Behavioral regressions cover attached weapons enumerated after passenger seats, crew aboard in cargo while the gunner seat is empty, insufficient crew, multiple turrets, blocked/reserved seats and gunner loss before dispatch. Two mutations remove attached-weapon recognition and the gunner readiness gate to establish that the tests detect either regression. Retry logs now report occupied/required gunner seats.

Native acceptance: compile and reload Chicken; dispatch US M2 Humvee, USSR PKM UAZ, LAV, BTR and BRDM packages, including joint reserves. Inspect the driver and every gunner before movement and at contact. Kill or remove a gunner before dispatch and verify that the package does not receive orders with an empty turret. Repeat with a player or foreign AI occupying the seat, checking that nobody is displaced. Also watch for native AI abandoning a turret after dispatch: this change gates departure; it does not replace combat casualties or prove native AI remains mounted. Compile and Play are not available to this audit.

## Original six-issue review

Reviewed the six reported issues against `f6277ac`. The changes below need a fresh Workbench compile and native acceptance. Current automated evidence: 28 suites, 97,342 mocked assertions and 35 detected mutations. See [release validation](QRF_RELEASE_VALIDATION.md) for source hashes and broader limits.

| Issue | Finding and action | Remaining limit |
|---|---|---|
| One dispatch failure ends the operation | Confirmed for active reinforcement. `TryActivateOperation` now calls `FailPackage`. First/middle/last failure tests preserve healthy packages and prevent duplicate orders during nested activation. | Required composite lead loss during SPAWNING intentionally still aborts preparation. Native orders already issued cannot be atomically undone; terminal retention applies. |
| Boarding timeout | Initial preparation already had a manager deadline: default 30 seconds, Chicken 90 seconds, or the larger finite landing-search allowance (121 seconds with current candidates). Active reserves lacked their own short preparation limit. | New `m_iReaperPreparationTimeout` defaults to 120 seconds, validated at 30–600. Construction freezes the deadline; dormant groups, seat requests and the ready barrier cannot reset it. The initial global deadline may expire earlier. Physical actors retain capacity/collateral until safe cleanup. |
| Last `GetInVehicle` argument | Installed Workbench `CompartmentAccessComponent` API declares the last parameter as `bool performWhenPaused`. Added an inline explanation for collision-only initial boarding and continued actual occupancy checks. | Native paused boarding remains to be tested. |
| Settings bounds | Both actual-source validators pass their attribute defaults and every serialized executor in shipped `.et`, `.ent` and `.layer` resources. Only Chicken `Managers.layer` contains an executor instance. Exact tested values are in the interruption suite's automated output. | Extender overrides outside the repository need their own validation. No bounds were silently loosened. |
| Headline sizing and missing helper | Confirmed inconsistent factors. Both width and row estimates now use one UTF-8-aware 0.75 helper. `AROR_TextUtils.CharCount` exists in `Scripts/Game/Common`; actual-source tests cover signed/unsigned UTF-8 bytes, Cyrillic and supplementary characters. | **Visual check NOT_RUN.** This remains a conservative estimate, not proportional-font measurement. The previous `MeasureText` helper reads laid-out widget size, which becomes the assigned frame after explicit placement. Tests cannot establish glyph widths or rendered wrapping. |
| Null asset classified recovered | Cleanup now logs an **unobserved removal** and releases its collateral without incrementing recovered value. Confirmed deaths still debit Economy; a missing reference alone cannot establish death. Tests distinguish both cases and preserve later-wave collateral. | If an external deletion erases a destroyed asset before any observation, the bill cannot be reconstructed from null. Owned cleanup observes losses before deletion. Native/external deletion ordering remains a risk to verify. |

Four added mutations prove the tests reject operation-wide active dispatch failure, missing reserve preparation deadlines, recovered classification for unknown removals and divergent banner estimates.

## Shipped settings

Both validators run against all validated fields, not only the selected values below. Defaults and Chicken pass unchanged.

| Setting | Default / Chicken | Valid range |
|---|---:|---:|
| Attack staging distance | 500 / 500 m | 200–1500 |
| Attack staging timeout | 180 / 180 s | 30–600 |
| Origin cooldown | 600 / 600 s | 0–3600 |
| Preferred minimum origin distance | 600 / 600 m | 0–3000 |
| Origin water penalty | 2 / 2 | 1–10 |
| Maximum origin garrison loss | 0.5 / 0.5 | 0–1 |
| Town support per minute | 6 / 6 | 0–30 |
| Reserve wait | 300 / 450 s | 60–480; greater than reinforcement delay |
| Road launch search radius | 150 / 300 m | 75–300 |
| Air package preparation timeout | 120 / 120 s | 30–600 |

## Native checks

1. Compile current source, reload Chicken and test both armies. Block pilot/passenger boarding during an ACTIVE reserve. It must time out at its frozen construction deadline, degrade only that package and use safe retention. Check initial boarding against the global timeout separately.
2. Invalidate a ready ACTIVE reserve's crew or waypoint immediately before dispatch. Healthy earlier/later packages must receive orders once. During SPAWNING, losing a required lead element must abort preparation without a false ACTIVE event.
3. Repeat paused initial pilot boarding, verifying the pilot actually occupies the correct compartment before flight.
4. Exercise long title/source/detail strings, including `ТЕРРИТОРИЯ ПОД АТАКОЙ / СЕВЕРНЫЙ ХРЕБЕТ` and `ГОТОВНОСТЬ / СОВЕТСКАЯ АРМИЯ`, at multiple resolutions and UI scales. Inspect clipping, early wrapping, unused height and the timer. No visual success is claimed by this audit.
5. Compare a confirmed tracked destruction with an external deletion before observation. The former must charge once; the latter must log unknown removal and never claim recovery or fabricate income. Inspect reserved amounts for surviving assets and future waves.

Existing [native acceptance steps](QRF_LIGHT_RESPONSE_ACCEPTANCE.md) still apply. The review introduces no save-format or replication change. Extenders may configure `m_iReaperPreparationTimeout` within its validated range; required-lead and active-package failure semantics are documented above.
