# Chicken development review — 10 November 2026

The town warfare design has a sound foundation, but this revision is **not ready to describe as balanced or stress-tested**. The most urgent problems are AI lifecycle ownership, militia persistence, vehicle boarding, and the treatment of distant player-commanded defenders. Fix those before increasing attack sizes or adding more ambient activity.

This is a review. No gameplay scripts, configuration, world resources, or existing validation reports were changed. The accompanying review harness and evidence files are new, uncommitted files.

## Scope and evidence

- Reviewed local `chicken-dev` at `b7648abc0bdcc9469f4c84efa1a35fcbec4580c4`, committed 10 November at 18:42:22 AEDT.
- Fixed review window: 9 November 18:47:09 through 10 November 18:47:09 AEDT. Baseline immediately before the window: `e8dcf8d558712082a0e3e88681dac09d28b93fbe`.
- There are **50 commits: 46 with a Claude coauthor trailer and four others**. All 50 were included when checking interactions. Coauthor trailers establish attribution, not who typed every line.
- This is the active local branch. The GitHub branch of the same name has a divergent history and does not contain this snapshot. No merge, force push, or branch replacement was performed.
- Read the changes across town warfare, operations, garrisons, capture, HICOMM, militia, site activity, economy, presets, imports, maps, phone, leaderboard, worlds, documentation, and validation tools. The complete commit inventory is below; machine-readable metadata is in [the review evidence](CHICKEN_DEV_REVIEW_2026-11-10.json).
- Executed 43 existing automated checks: **34 pass; nine fail**. Passing JavaScript checks report 99,335 assertions. That includes mocked lifecycle and manager-soak checks, not native AI performance tests.
- Added seven counterexamples that execute extracted current method bodies against explicit mocked native conditions. All seven reproduce the reported decisions. They demonstrate script behavior under those conditions; they do not establish that every native timing condition occurs on every vehicle.
- Generated a 100-cell matrix from the current town sizing method and Chicken recipes: both roles, five Readiness levels, five Crackdown levels, towns and cities. Integer division is modeled explicitly, as specified by [Bohemia's Reforger operator documentation](https://community.bohemia.net/wiki/Arma_Reforger%3AScripting%3A_Operators).
- Reran the installed-resource/preset audit; see [WCS evidence](CHICKEN_DEV_WCS_REVIEW_2026-11-10.json).

## Findings requiring correction

### 1. P1 — Site ownership changes abandon live AI and vehicle ownership

Source: [SiteLife Tick](../../AROpenResistance/Open%20Resistance/Scripts/Game/Locations/AROR_SiteLifeComponent.c#L640), `Forget` at line 1067; introduced by `deb9a49` / `9c55902`.

When the holder changes, `Forget()` clears workers, patrols, their timers, the truck, and helicopter references. It neither retires them nor transfers them to another lifecycle owner. These groups are spawned with `SetCanDeleteIfNoPlayer(false)` and the Manual lifecycle policy in `AROR_ForceSpawner.SpawnGroup`. The helper objects have no destructor that calls `Remove_S`.

Consequently, later `Retire()` and `OnDelete()` cannot clean up the previous holder's activity. Recapturing a site can add another full activity complement. Old unclaimed helicopters can also remain on pads that the new holder considers empty. This bypasses the intended population ceiling and makes repeated ownership changes a particularly poor stress case.

Counterexample `site_handover_discards_live_ownership`: live workers, patrol, truck and helicopters were supplied to the actual `Forget` method; zero ownership references and zero cleanup calls remained.

**Correction:** keep surviving former defenders in a separately tracked retirement collection or transfer them to an existing force owner. Preserve claimed vehicles through the fleet system. Apply the same combat-aware cleanup rules after handover as before it. A capture should not instantly erase surviving enemies, but somebody must continue owning their cleanup.

### 2. P1 — Militia counting permanently converts dormant or delayed soldiers into casualties

Source: [TownMilitia CountLosses_S](../../AROpenResistance/Open%20Resistance/Scripts/Game/Towns/AROR_TownMilitiaComponent.c#L512), `RaiseInfantry_S` at line 451, and [ForceSpawner CountLiving](../../AROpenResistance/Open%20Resistance/Scripts/Game/Common/AROR_ForceSpawner.c#L309); introduced by `1cdbf47`, extended by `dd94193`.

After a 30-second form-up allowance, the saved infantry count is reduced to the number of physically materialized living agents. `CountLiving` does not include the native dormant alive count or pending soldiers. Militia therefore lose purchased, saved strength when the engine holds living soldiers dormant or materialization takes longer under load. Saving also calls this counting path.

Counterexample `militia_dormancy_erases_purchase`: a post with 12 purchased infantry, 12 dormant survivors and no materialized agents is rewritten to **zero** infantry. A failed group spawn is similarly skipped after payment and later treated as loss, without a retry/refund distinction.

**Correction:** separate purchased/pending/alive/confirmed-dead states. Reuse the dormant-survivor handling already present in HICOMM, add bounded materialization retries, and refund confirmed spawn failures. Saving must not turn absence from `GetAgents()` into proof of death.

### 3. P1 — Militia stand-down can delete a player-occupied, claimed vehicle

Source: [TownMilitia StandDown_S](../../AROpenResistance/Open%20Resistance/Scripts/Game/Towns/AROR_TownMilitiaComponent.c#L549); `1cdbf47` / `dd94193`.

The post retains its vehicle reference and unconditionally calls `DeleteEntityAndChildren` during stand-down. It does not check fleet ownership, a player occupant, or where the vehicle has been taken. A player can take a militia vehicle away, let its original town become quiet, and have that vehicle submitted for deletion while still using it elsewhere.

Counterexample `militia_retirement_deletes_claimed_player_vehicle` reproduces the deletion request with a claimed vehicle and a player aboard. The mock records the request; native effects on occupants were not executed.

**Correction:** transfer a taken vehicle out of post ownership when claimed, define how the militia book accounts for it, and protect occupied/claimed vehicles during every cleanup path.

### 4. P1 — Destroying a militia vehicle discards the cleanup reference to surviving crew

Source: [TownMilitia CountLosses_S](../../AROpenResistance/Open%20Resistance/Scripts/Game/Towns/AROR_TownMilitiaComponent.c#L529); `1cdbf47`.

When a hull is destroyed, the matching crew entry is removed along with the vehicle entry. A crew that has bailed out or survived is neither transferred to the infantry collection nor retained for stand-down. Its group has the same Manual lifecycle policy as other spawned forces.

Counterexample `destroyed_militia_vehicle_forgets_live_crew` leaves no tracked crew and requests no crew cleanup.

**Correction:** move surviving crew to the post's dismounted force collection and account for their continued presence. Retain wreck references until the appropriate wreck cleanup owner accepts them.

### 5. P1 — HICOMM can report successful embark with no driver

Source: [HighCommand UpdateEmbark_S](../../AROpenResistance/Open%20Resistance/Scripts/Game/HighCommand/AROR_HighCommandComponent.c#L651), `SeatSquad_S` at line 733 and `CountMounted_S` at line 808; `3b2bedc` / `18ca69e`.

At boarding timeout, any mounted squad member is enough for successful embark. There is no requirement that a living, usable squad driver occupies the driving seat. The actual method reports success for a six-person squad with only one cargo passenger aboard and no driver. Subsequent Move orders can therefore be presented as vehicle orders although the squad cannot drive it.

There is also an asynchronous allocation risk: each member scans the same seat list and checks occupancy, but the loop keeps no per-pass seat assignment. In a native model where an accepted boarding request has not yet changed seat accessibility/occupancy, three members request the same driver seat. Whether the native accessibility check prevents this on a particular vehicle needs Workbench confirmation; the missing-driver success condition is independent of that timing question.

**Correction:** reserve distinct actor/seat assignments, retry the same assignment until acknowledged, require a confirmed driver before declaring vehicle readiness, and report partial boarding separately. Check full cargo, blocked driver, dead driver, refused actions, and small modded vehicles.

### 6. P2 — New movement tracks bypass spotting and can label a ground vehicle as airborne

Source: [OperationAlerts PublishThreatBoard_S](../../AROpenResistance/Open%20Resistance/Scripts/Game/Operations/AROR_OperationAlerts.c#L130), [OperationExecutor GetOperationTrack_S](../../AROpenResistance/Open%20Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c#L384), and `AROR_OperationMapMarkers.c`; `545a6f2`.

The replicated threat board publishes a current lead position and selected helicopter LZ directly from executor entities. There is no spotting, line-of-sight, observer, or sighting-age condition. A general inbound warning therefore exposes exact moving enemy positions and planned landing sites even when nobody has seen them. This materially weakens reconnaissance and ambush uncertainty despite the existing spotting system.

For joint operations, `air` and `landing` are taken from any air element while `lead` is the nearest element of any kind. Counterexample `joint_track_mislabels_ground_head_as_air` produces a ground truck's position with `air=true` and a different helicopter's landing zone. The resulting air track does not describe one physical force.

**Correction:** keep strategic warnings available, but derive precise tracks from confirmed sightings. Store last-seen position/time and expire it. Publish ground and air tracks separately, or bind position, type and destination to the same selected element. Test claimed aircraft, landed troops, returning aircraft and late joiners.

### 7. P2 — Remote HICOMM warfare conflicts with human-only activation and abandonment

Source: [TownWaves IsResistanceNear](../../AROpenResistance/Open%20Resistance/Scripts/Game/Towns/AROR_TownWavesComponent.c#L1105), `UpdateResponse` at line 488 / `Abandon` at line 752; [SiteLife Tick](../../AROpenResistance/Open%20Resistance/Scripts/Game/Locations/AROR_SiteLifeComponent.c#L640). Interaction between existing town policy, `68b5a69`, `3b2bedc`, and the new militia/site systems.

HICOMM now deliberately keeps commanded forces active far from players. Town abandonment still considers only player-controlled Resistance characters: after 180 seconds without one within 1,500 metres, a response can end as abandoned and restore the army's post/support, even if a living commanded force or purchased militia is defending it.

Site activity retires after players are beyond its retirement distance for 30 seconds, without checking combat with distant allied squads. A remote gang can thus encounter activity that disappears when the human leaves. The garrison activation changes also deliberately exclude players in aircraft; a remote attack launched without a nearby ground player needs an explicit activation policy.

**Correction:** define a shared concept of an active Resistance presence that includes relevant commanded troops and combat, with a budgeted activation radius. Use that in response abandonment and site retirement. If a site intentionally requires a human observer, expose that rule clearly; it currently conflicts with the requested gang gameplay.

### 8. P2 — Kill-driven escalation undercounts commanded AI compared with player shooting

Source: [Town casualty OnDamage](../../AROpenResistance/Open%20Resistance/Scripts/Game/Towns/AROR_TownWavesComponent.c#L1317) versus commander resolution in `AROR_LeaderboardComponent`; `d947363` and the HICOMM expansion.

Town damage observation records kill eligibility only when `GetInstigatorPlayerID()` is positive. It can share AI-on-AI contact reports, but it does not resolve a HICOMM/native recruit kill to its commanding player. Consequently, two forces achieving the same result through different proportions of human and AI kills can generate different town kill progress and escalation. Taking the flag can still trigger a response; this is not a claim that gangs can never cause one.

**Correction:** explicitly decide whether hostile casualties caused by controlled allies raise town threat, then share the existing ownership/commander resolution rather than maintaining different attribution rules in score and warfare systems.

## Town response and balance assessment

### What is working coherently in the design

The finite three-wave response, road-based staging, axis separation, bounded contact reports, safe landing search, independent air executor, withdrawal, and terminal cleanup states are good foundations. USSR/Invaders have a coherent shock identity: earlier commitment, splits from the first qualifying wave, shorter retask intervals and closer dismounts. BLUFOR/Occupiers retain slower convoy attacks, wider dismounts and covering elements. Increasing USSR speed without also increasing their finite budget is the right distinction.

Several Claude fixes address real structural problems: raising the recipe validation limit, adding per-army attack slots/rest, requiring the physical post flag for settlement capture, restoring the winning army after lockdown, counting landed/escort reinforcements, expanding building-site searches away from roads, closing perimeter corners, accumulating all import-facility types, and using actual flag positions on the map. These should be preserved.

### Current force sizing

At Crackdown 1–2, ordinary towns request `[2, 2, 3]` ground packages over three waves; cities request `[3, 3, 4]`. At Crackdown 5 those become `[4, 4, 5]` and `[5, 5, 6]`. Split axes divide the wave allocation; they should not multiply it.

The actual-source recipe matrix gives the following at Readiness 2, for either army:

| Site / Crackdown | Total ground packages across three waves | Total configured ground personnel reservation |
| --- | ---: | ---: |
| Town / 2, the starting level | 7 | 126 |
| City / 2 | 10 | 164 |
| Town / 5 | 13 | 216 |
| City / 5 | 16 | 254 |

These are **cumulative reservation totals, not simultaneous live soldier counts**. Vehicles, available seats, actual casualties, releases and execution timing alter what is physically fielded. Readiness changes the profile mix, so higher Readiness can mean fewer reserved bodies but more dangerous vehicles. Raw personnel numbers alone do not measure difficulty.

The dedicated air recipe has an escorted transport and a ground support element. At the configured 100% joint-transport supplement, sufficient free capacity permits another unescorted transport: nominally **72 personnel reservation**, two transports, one gunship and a ground support vehicle. Extras are reduced when capacity does not fit, so that composition is not guaranteed in an already saturated executor. The usual airborne response share is also 100%; air pressure is not limited to the explicit town insertion.

The executor caps its own reservations at 24 groups / 220 members / 16 vehicles, with six live ground packages and six live aircraft by default. City ground totals plus the nominal air request exceed 220 over the course of the response. This is not automatically an overflow: forces may release between waves, and admission guards prevent over-reservation. It does make overlapping requests, long retention, staging retries and skipped/timed-out later waves important native cases. The current tests do not establish that the complete intended force mix consistently reaches the field under saturation.

### Why two players do not define one difficulty level

Two humans with one full native recruit squad can bring eight actors. Add two six-person HICOMM squads and that is 20. Two separate native squads plus the maximum eight HICOMM rifle/mechanized squads allow **62 allied actors**, before militia. Those are potential force sizes; funds, manpower, losses and travel constrain what is actually present.

Chicken starts with 8,000 Resistance funds and inherits 40 manpower. A six-rifleman HICOMM squad costs 2,000 and six manpower. Four such squads are financially possible at the start; native recruits are explicitly free for now. These defaults can make the same initial encounter overwhelming for a lightly equipped pair and easy for a pair that first buys an AI force. The waves currently depend on Readiness, Crackdown and site type, not nearby effective friendly strength.

Recommended first balancing pass, after lifecycle repairs:

- Keep the requested frequent transport insertions. For small early forces, use smaller passenger teams and reserve a transport opportunity rather than solving saturation by silently dropping the insertion.
- Measure nearby fighting strength, including living commanded squads and militia. Count distant reserve squads differently from troops already in the fight. Include armour, usable AT/AA, and gunship exposure as separate limits.
- Freeze each wave's admission decision when planned; use losses to influence later reserves rather than despawning enemies mid-fight. Cap opening local pressure and let finite reserves reinforce it.
- As provisional playtest envelopes, try 10–16 active hostile dismounts against two players plus six recruits; 20–32 against a force around 20; and 40–56 against a substantially larger commanded force. These are hypotheses for testing, not certified balance numbers. Vehicle/gunship support can require lower infantry caps.
- Keep USSR faster and more willing to split, but avoid delivering every high-threat element before players can recognize the attack. BLUFOR should build pressure through convoy arrival and supporting positions.
- Give the early gunship a bounded support window and clear departure, with usable counterplay. Test a 90–150 second exposure window before adopting the current longer support settings for lightly equipped players.
- Decide whether the single global town response is intentional. It limits pressure on a small group, but two independently commanded fronts can leave a second attacked town without its own response. A small per-army/site admission queue with a shared global budget is easier to explain than silently ignoring the second front.

### Campaign recovery and economy

Each observed player-caused town kill adds eight Crackdown points and four Readiness pressure. Taking a post adds 100 and 50 respectively. Chicken starts at Readiness 2 / pressure 100 and Crackdown 20% / 200 points. A post plus approximately 25 eligible kills reaches Readiness 3. Further battles can push the campaign into heavier forces quickly.

After ten quiet minutes, Readiness decays by only two pressure per minute. Recovering from pressure 700 to the starting 100 takes about **five hours of decay**, plus the initial quiet period. Crackdown 1,000 to its initial 200 takes **160 minutes** at five per minute. Individual level boundaries can cool sooner; these figures describe a full return to starting pressure. This long tail should be a deliberate campaign choice, especially for short co-op sessions. A candidate alternative is substantial recovery within 20–30 quiet minutes, while keeping strategic progression separate from immediate battle heat.

Income arrives every five minutes, with nine funds per inhabitant, resource income and site bonuses. For illustration, a population-40 town contributes 360 faction funds per payment before other multipliers. Stone Ridge adds 1,500 per payment, over four such towns' raw population income. Each player additionally receives 25% of Resistance income; it is not deducted from the faction payment. Scaling the economy to more humans therefore also scales total purchasing power.

Militia cost 300 per infantryman, 12,000 per APC and 25,000 per armour unit, plus manpower and import-facility requirements. The early militia squad is accessible, whereas heavy defence is a longer-term purchase. This is a reasonable progression, but it cannot be judged until purchase losses, crew handling and vehicle ownership are corrected. Town waves waive funding, so starving the enemy economy does not itself suppress this response stream.

## Stress and resource assessment

There is no single aggregate simulation budget covering operations, conventional garrisons, site activity, militia, HICOMM and native recruits. The 220-member executor limit is not a whole-server AI limit. Conventional garrisons have a separate 144-person cap.

Eleven Chicken military sites now have SiteLife. Ordinary defaults add four workers and four patrol soldiers per active site. Stone Ridge is configured for six workers, eight patrol soldiers and a two-person truck crew. If all eleven are active, that is up to **96 activity soldiers**, independently of the other systems. A normal two-player position will not activate all sites simultaneously; repeated handovers and absent cleanup are the more serious route to continued growth.

SiteLife also builds static camp dressing on every machine at startup, including the dedicated server. It does not proximity-stream that dressing. Its floor ceiling is 900 tiles per site, or a mathematical upper bound of 9,900 floor entities across eleven sites, before props; this is not a measured actual entity count. Work is scheduled at roughly the same startup delay. Profile startup and late joins before accepting this density, and consider reusable compositions, smaller coverage or paced/proximity construction if the measurements require it.

Under native AI-budget dormancy, the same materialized-agent-only count used by militia appears in SiteLife's `UpdateGroups`: a previously seen group with no materialized agents is removed as a loss. Extend dormant-survivor protection to site workers/patrols, not just HICOMM. Recent combat and allied AI presence should also prevent retirement in the middle of remote fights.

The site audit passes its structural checks, but reports nearby overlapping sites: Lamentin military/town 317 m; Hospital/Tyrone 595 m; Vostok/Saint-Pierre 301 m; Levie military/town 378 m. These are useful worst-case locations for simultaneous ambient, garrison and response activation.

## WCS, factions and aircraft

The fresh audit finds **2,941 exact resource references, zero unresolved references, zero missing installed dependencies and zero skipped dependency groups** for the modeled combined addon stack. This verifies resource-index consistency, not native spawnability, inventory application, AI compatibility or seat capacity.

The combined addon defaults are `RHS_USAF_FORECON` and `RHS_AFRF_MSV_VPKO3_EMR`. WCS vehicle entries have selection priority and are selected through role presets; infantry loadouts come from the RHS preset configuration. The stack is therefore a mixture of RHS equipment and WCS/base vehicles, not an all-WCS infantry/vehicle setup.

There are 13 preset vehicle entries absent from the enabled catalogue and eight type disagreements recorded in the evidence. Missing entries are filtered by `AROR_FactionPreset.IsOffered`; they are not automatically spawned as substitutes. Some catalogue category differences may be deliberate; review them against intended attack and shop categories rather than relabeling all of them blindly.

In particular, `d947363` adds a US `UH1H_base.et` transport choice at the same priority as the WCS MH-6, but the audit finds that UH-1 entry **not offered** in the combined stack. The intended transport blend therefore cannot rely on that entry. Confirm an offered, flyable UH-1 variant if the blend is required; otherwise test troop strength using the MH-6's actual usable passenger seats. The preset audit also reports missing US `RADIO_OPERATOR` and Russian `PILOT` unit types. Unit lookup can fall back to the configured default/rifleman, so this is not by itself proof that an aircraft cannot launch; verify the actual spawned crew and kit.

Native acceptance must cover WCS MH-6/AH-6 and the Russian transport/gunship choices, actual pilot/driver and turret seats, passenger offload, faction affiliation after spawning, surviving escort crew, claimed vehicle protection, and kit application at Readiness 1–5. No native aircraft compatibility certification is claimed here.

## Workshop comparison

These are five relevant author-published references, not a popularity ranking or a claim that their code/performance has been benchmarked. The conclusions in the final column are recommendations inferred for Open Resistance.

| Reference | What the author documents | Application here |
| --- | --- | --- |
| [Overthrow](https://reforger.armaplatform.com/workshop/59B657D731E2A11D-Overthrow) | Persistent solo/co-op campaign, local support/stability, civilian recruitment, threat-responsive resource-based bases and capture QRFs. | Keep support, economy and military pressure connected; a town battle needs a clear effect on control and future resistance. |
| [Freedom Fighters](https://reforger.armaplatform.com/workshop/CAFEBEEFF0CACC1A-FreedomFighters) | Designed for solo and 2–8 players, with persistent progression, supplies, population support and balancing activity against enemy threat. | Use a repeatable small-co-op benchmark and meaningful quiet recovery. Long campaign progression should remain playable in shorter sessions. |
| [Antistasi — Core — Everon](https://reforger.armaplatform.com/workshop/6A98C710D3655504-Antistasi-Core-Everon) | Separate army chaos/standing, occupier–invader fighting and escalating patrols/counterattacks, with strategic target priorities. | Preserve per-army escalation and faction rivalry; reconcile all town and army attack paths so one global response slot does not obscure strategic priorities. |
| [REAPER JustInTime](https://reforger.armaplatform.com/workshop/617B2ED33DE29156) | Co-op mission for up to ten players; enemy numbers depend on player numbers. | Explicit force scaling is useful. Here the input should also account for commanded allies because player count alone misses most potential friendly strength. |
| [RC Dynamic Counter Attacks](https://reforger.armaplatform.com/workshop/6A3AEB1AC7B02185-RCDynamicCounterAttacks) | Conflict response addon with multiple axes/waves, ground pressure before air assault, troop insertion, gunships, clear repel messages and aircraft departure/cleanup. | The combined-arms direction is appropriate. Sequence air support after a real ground commitment, retain finite budgets, and make cleanup and battle completion observable. |

## Validation results and limits

The review harness is [Review-ChickenDev20261110.js](../../tools/Review-ChickenDev20261110.js). It captures the snapshot/window, executes current source counterexamples, generates the sizing matrix, and runs existing suites while redirecting their generated documentation to temporary storage. Its successful exit means the review evidence was generated; **the JSON's failing checks and reproduced counterexamples must be read**, rather than treating that exit code as release approval.

| Failing check | Observed blocker |
| --- | --- |
| Test-OperationAlerts | Harness lacks `AROR_OperationExecutorComponent` after track publishing was added. |
| Test-TownDoctrine | Mock `Math` lacks `Round` after explicit rounding changed. |
| Test-SpottingAndAirResponse | Undefined collection used through `Contains`; new scan-reinforcement fixture is missing. |
| Test-QRFMutations | Rejects the failing SpottingAndAirResponse baseline. |
| Test-GarrisonsAndSupport | Extraction still expects the former `InfantryPrefab` method. |
| Test-PostsAndPads | Same obsolete `InfantryPrefab` extraction. |
| Test-HighCommandSimulation | New `CountMounted_S` dependency is absent from the extracted status fixture. |
| Test-PlayerSquads | Location fixture lacks `GetName` used by revised recruitment. |
| Test-PlayerSquadMutations | Rejects the PlayerSquads baseline. |

These failures mostly identify stale test adapters, not nine demonstrated engine defects. Nevertheless, they prevent a green current regression claim, and the new militia/site lifecycle paths do not have sufficient meaningful coverage. The two InfantryPrefab extraction failures were already present in the earlier validation; do not attribute their introduction to today's latest commits.

The passing suites include manager soak (10,000 mocked ticks), lifecycle, interruptions, force mix, strategy/progression, boarding/ingress/unloading, invader campaign, perimeter geometry, phone session, helicopter proximity and static site checks. They establish useful policy invariants. They cannot establish native road following, helicopter flight, replicated physics, actual kit loading, server frame time or client UI rendering.

Existing Workbench `logs_2026-11-10_18-25-51/script.log` records `Unknown operator '%'` in SiteLife and failure to compile the Game module at 18:25 and again at 18:41. HEAD's 18:42 change alters the modulo operands. There is no observed successful compile after that change. Earlier base-game-file diagnostics may be cascading or stack-specific; they are not attributed to this mod without a fresh compile. The current HEAD is **unverified**, not proven to retain the same compile error.

This environment cannot compile or run Enfusion. The required next native sequence is **Script Editor compile, then Play in World Editor, then dedicated-server acceptance with the intended addon stack**.

## Dedicated-server acceptance before calling this stress-safe

| Scenario | Required observations |
| --- | --- |
| Two humans + one six-recruit squad; then add two HICOMM squads | Test both armies and towns/cities at starting pressure. Record peak actual enemies, force arrivals, wipes, battle duration, ammunition/AT/AA demand and support gained. Use the same route/gear for comparison. |
| Two leaders with separate recruit squads and maximum HICOMM; purchased militia | Profile total live and dormant actors across every subsystem. Check that the global budget protects player-commanded forces and current defenders before ambient work details. |
| Repeat site capture/recapture at least 20 times | After retirement, old AI/vehicle/waypoint ownership must return to baseline; no stacked helis on reused pads and no growing unowned-force count. |
| Saturate operations while town waves and an ordinary/capture QRF overlap | All reservations stay bounded; deferred waves either arrive or expose an explicit finite failure. Critical air insertion retains a predictable admission path. |
| Native dormancy / delayed spawns / failed prefab load | Militia book and HICOMM strength preserve genuine survivors; pending failures resolve to retry/refund; save/load does not lose purchases. |
| Crew survives a destroyed vehicle; player takes a militia vehicle away | Crew remains owned until death/retirement; claimed or occupied vehicles survive post cleanup and persist through fleet save/load. |
| Remote HICOMM attack and defence with humans beyond 1,500–2,000 m | Defenders activate consistently, combat does not evaporate, and towns are not automatically retaken while valid friendly defenders remain. |
| Mechanized boarding: tiny/full vehicle, cargo-only occupant, dead/blocked driver, rejected entry | Distinct seat assignments, accurate readiness, usable driver, clear partial-failure result and no endless order loop. |
| At least two transport insertions and gunship support per army | Correct faction/kit, actual passengers, safe separate LZs, troops remain counted after landing, aircraft depart/clean up, stolen aircraft are protected. |
| Two simultaneous fronts around overlapping sites | No silent starvation of the second objective, no budget runaway, and clear queue/admission feedback. |
| Late join / respawn / map zoom / phone and leaderboard resolutions | Separate correct air/ground symbols; last-seen tracks respect spotting; no client/server layout disagreement or local collision surprises. |
| Save/restart with active militia, claimed vehicles, held Stone Ridge and HICOMM refunds | Ownership, money, manpower, book counts and fleet restore correctly; session-only forces do not duplicate persisted forces. |
| 45–60 minute combined soak with repeated waves and captures | Capture server frame-time percentiles, AI/dormancy counts, entity/waypoint counts, replication traffic, memory trend, routing/landing failures and player-visible delays. Compare with a warmed-up baseline on the same machine. |

Set numerical performance targets for the actual server hardware before this run. Passing means no script exceptions, no uncontrolled entity growth, stable reservation accounting, preserved owned assets, and frame time within the chosen server budget. A JavaScript assertion count cannot substitute for those measurements.

## Coverage and commit inventory

| Change family | Review disposition |
| --- | --- |
| Town capture, kill credit, lockdown and recapture | Physical post capture and winner restoration are improvements. Remote presence and AI kill attribution need reconciliation. |
| Army attacks, recipes, escalation and doctrine | Per-army pacing and recipe-cap fixes are valuable. Keep doctrine differences; tune pressure recovery and admission against measured friendly strength. |
| Militia and persistence | New functionality; fix survivor accounting, crew ownership and claimed-vehicle cleanup before relying on purchased defence. |
| SiteLife, Stone Ridge and geometry | Main stress risk is lifecycle ownership and additional uncapped activity. Validate local procedural geometry, floor density and native navigation/collision. |
| HICOMM mechanized and remote simulation | Remote simulation intent is useful. Driver/seat readiness needs correction; keep lease/dormancy handling and restore its regression fixtures. |
| Economy, shops, cars, equipment and fuel categories | Server-side purchase validation and shared display/charge price scaling were inspected. Early access and port accumulation fixes are coherent; progression still needs playtests. |
| Map silhouettes, flag placement, tracks and HUD | Correct flag position/type symbols improve clarity. Track intel/type issues need correction; rendering and collision are native acceptance items. |
| Phone recruitment/session, help and leaderboard | Recruitment retains server checks for living Resistance player, held site, squad capacity and pending reservations. Presentation changes need native input/resolution checks; no additional blocker established by this review. |
| Faction compatibility and aircraft | Resource references resolve in the installed stack. Catalogue exclusions prevent the claimed US transport blend; native seat/crew/kit acceptance remains. |
| Documentation, static audits, resource metadata and radio dumps | Reviewed as supporting changes. Lexical/static checks are not compilation; recipe/site changes affect extender configurations. Radio dumps are supporting artifacts, not evidence of gameplay behavior. |

All commits in the fixed window follow. The four non-Claude commits were reviewed as dependencies/interactions; they are not attributed to Claude.

| Commit | Coauthor | Change |
| --- | --- | --- |
| `b7648ab` | Claude | Use int variables for modulo in site life |
| `e2e7401` | Claude | Rename a site life variable that may be reserved |
| `fc62833` | Claude | Read the site's yaw into a variable before indexing it |
| `9c55902` | Claude | Bring every military site to life |
| `deb9a49` | Claude | Build Stone Ridge up and keep it busy |
| `545a6f2` | Claude | Draw tracks for inbound columns and helicopters |
| `5573b0e` | Claude | Make Stone Ridge Base a special site |
| `f3026fb` | Claude | Give Stone Ridge Base's castle walls collision |
| `dd94193` | Claude | Let the Resistance garrison outposts and other sites |
| `1ff9745` | Claude | Recruit riflemen from the phone at any held site |
| `032b33a` | Claude | Cap town patrol speed and send civilians running |
| `18ca69e` | Claude | Offer only land vehicles to Mechanized Squads |
| `1244fc4` | Claude | Sell plain cars without an import facility |
| `ec2de35` | Claude | Fix map symbol helper clashing with engine Shape |
| `94393df` | Claude | Update town warfare doc for the new map markers |
| `3b2bedc` | Claude | Add mechanized squads and Embark/Dismount orders |
| `cddbed2` | Claude | Show town flag markers where the flag stands |
| `3b05172` | Claude | Split cars from light vehicles in Imports |
| `fe1c03b` | Claude | Sell early FIA gear with no import facility |
| `b504b1d` | Claude | Draw map locations as type silhouettes |
| `c80d673` | Claude | Redraw the leaderboard as a centred A3 scoreboard |
| `ce4e8f1` | Claude | Fit HICOMM help lines on one line each |
| `81f79f1` | Claude | Open the shops without facilities; list every port |
| `7e91a43` | Claude | Document town garrisons and the Chicken economy map |
| `1cdbf47` | Claude | Add Resistance town garrisons bought in HICOMM |
| `d2e1298` | Claude | Build outpost towers for the holder, after the campaign loads |
| `dc672ef` | Claude | Move the town lockdown indicator below the headline banner |
| `10b233a` | Claude | Rebalance Chicken income around towns and vehicles |
| `8f224ef` | Claude | Sell scrappy starter weapons without import facilities |
| `e692e8f` | Claude | Tag fuel tankers as fuel trucks, not tanks |
| `5e128ec` | Other | Exclude helicopters from garrison activation |
| `91e2f91` | Other | Preserve phone boot progress and unify hotkeys |
| `68b5a69` | Other | Keep HICOMM squads active away from players |
| `3e0fbd5` | Claude | Rename a variable that used the reserved word owned |
| `ff52fcc` | Claude | Document the lockdown retake, wider site search and site audit |
| `87a3488` | Claude | Close outpost perimeter walls at the corners |
| `bbee531` | Claude | Count landed and escort troops in the town lockdown |
| `e31a8e7` | Claude | Search further out for garrison post building sites |
| `f24b406` | Claude | Hand the town back to the army that wins a lockdown |
| `5562e80` | Claude | Add a static town site audit for town warfare scenarios |
| `6ccb569` | Claude | Document town capture, perimeters, icons and depot move |
| `9e363e3` | Claude | Wall in outposts, move Montignac depot, tents off roads |
| `be59b62` | Claude | Use base-game map icons and always show post flags |
| `b9b806a` | Claude | Start town capture at the flag, count only reinforcements |
| `673c9e6` | Claude | Raise the recipe limit that disabled Chicken's executor |
| `d947363` | Claude | Fix town kill credit and blend US troop helicopters |
| `446ec78` | Other | Refine town combat and WCS air responses |
| `bfac81a` | Claude | Add radio addon folder metas and broadcast dumps |
| `d84eff5` | Claude | Pace attack orders: per-army slots and breathing room |
| `7625324` | Claude | Let the US attack towns and fight the Russians |
