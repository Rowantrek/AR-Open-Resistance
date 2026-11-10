# Town warfare (Chicken)

Status: written 2026-11-09, **not compiled or play-tested**. Chicken only; every new behaviour is off unless a
location carries the new components (base Everon is unchanged).

Reviewed and refined 2026-11-09: [town AI review](TOWN_AI_REVIEW.md) defines the role doctrines and the exact
response lifecycle; [validation](validation/TOWN_WARFARE_VALIDATION_2026-11-09.md) records the automated checks
and outstanding native tests. Individual soldier combat still uses native AI.

The fight moves from outposts in fields into towns. Each town and city has a garrison post the Resistance can take,
an occupying presence in the streets, and a fixed three-wave response when players attack its defenders.

## Map changes (Chicken)

- **Outposts cut to 3 of 14.** Kept: Wolf Wood, Beacon Point and Old Sawmill (the three furthest from any town). They
  are now `MILITARY_BASE` garrisons (level 3, plus two extra infantry teams, compound size kept at 18 m). Removed:
  Ironworks, Grey Quarry, Barley Field, Gull Point, Raven Ridge, Pinewood Lake, Stag Hill, Regina Heights, Fox Valley,
  Hawk Ridge and Crow Hill. Saved campaigns simply stop loading the removed sites.
- **The kept outposts are walled in** (`AROR_PerimeterComponent`, three extra infantry teams each): at play start the
  server raises a 44 m square of high sandbag walls around each compound with a gate front and back, barbed wire 7 m
  outside, hedgehogs across the gates and a guard tower (US or USSR by holder) on two opposite corners. Every piece
  snaps to the terrain and is skipped where trees, buildings, props, roads, water or a steep step are in the way
  (logged as `[AROR GARRISON] ... perimeter raised: N pieces, M skipped`). Chicken only; IDs hand-written.
- **The Montignac Ridge Depot moved** (it sat 186 m from Montignac, overlapping the town): it is now the Grey Quarry
  Depot on the old Grey Quarry outpost site (8606, 3389), 1.7 km from the nearest military site. Same ID, so saved
  campaigns keep its state. Base Everon keeps it at Montignac.
- **All 18 settlements** get `AROR_GarrisonComponent` (`m_bSettlementPost 1`), `AROR_TownWavesComponent`,
  `AROR_TownLifeComponent` and a capture flag that auto-places on a clear spot near the centre at play start.
  Component and entity IDs in the layer were hand-written; Workbench may rewrite them on save.
- **Military sites come alive** (`AROR_SiteLifeComponent` on every outpost, base, factory and depot): a plated yard
  inside the walls, a camp of tents, containers, nets, generators and stores round the outside on dirt and tracks
  (the same at every load), a fire team walking the perimeter and soldiers walking between the buildings.
  Stone Ridge Base is the special site: sandbags gone, plated, built up, two helipads with a gunship and a transport,
  three work details, two perimeter teams and a truck, and 1,500 extra income a payment to its holder.

## Garrison post

- **The site**: at play start the server looks for room for a building within 60 m of the town centre (6.5 m clear
  radius, at most 1.5 m height difference, nothing standing in it and clear of every road; the flag stays off roads
  too; the centre first, then four rings of eight; if none fits, five more rings between them out to 130 m, capped at
  three quarters of the town radius). It
  raises the holding army's barracks tent there (`Tent_Barracks_US_01`, `_USSR_01`, `_FIA_01`; swapped when the town
  changes hands) and stands the flag 10 m in front of it. The flag is a full flagpole whose cloth shows the holding
  faction. No room anywhere: flag only, logged as a warning. Each of these is an attribute on the flag's placement
  component (`m_fBuildingYawOffset` turns the tent 180° if its door ends up facing away from the flag).
- Raised when players first come near, sized by the holding army's **Crackdown**: level 1 (2 teams), level 2 from
  Crackdown 3 (4 teams, one gun), level 3 at Crackdown 5 (6 teams, two guns, a manned vehicle); cities one level more.
- Infantry stands in a tight ring (20 m half-size) around the flag, so in front of and beside the tent. Town ground is
  rarely flat, so posts always use the infantry-only fallback when the compound check fails.
- Chicken's garrison personnel cap is 144 (was 96, which the extra town posts filled near Meaux).
- **Map**: a pennant in the holding faction's colour marks each post where its flag actually stands, separate from the
  town marker at the town centre. Drawn as an outline once the post is taken.
- **Taking the flag** (all hostiles within ~60 m of it dead): +30% Resistance support in that town. This may flip the
  town through the normal support rule, but the flag never changes control itself. The post is then broken: nothing
  in its roster deploys again. It re-forms 45 minutes later, once nobody is near and the army still holds the town.

## Wave response

Trigger: players kill 25% of the defenders seen (post, checkpoint guards, patrol crews), at least 3, or take the
garrison post. The post preserves its relief request if the support reward immediately flips control. The response
then runs in phases, after the pattern of RC Dynamic Counter Attacks (ideas from its workshop page only; no code):

1. **Alert**: a contact report that names who hit the garrison, "CONTACT REPORT · MEAUX / Garrison engaged by
   Jasper and Rowan. Reinforcements requested". The first wave forms up 30–90 s later for Occupiers; Invaders use
   0.65 times that delay (approximately 20–60 s).
2. **Waves**: three, 150 s apart (±30 s) for Occupiers; Invaders use 0.65 times that interval (78–117 s). Each stages
   on a road 1–2 km out and drives in as a column. The lead vehicle prefers a stop 220 m short of the post for
   Occupiers or 160 m for Invaders, later vehicles 25 m behind each other, and the infantry walk in.
   - **Axes**: wave 1 comes from the side of the army's nearest base; each later wave uses a road at least 500 m from
     the previous wave's (preferred bearings rotate 0°, 90° one way, 90° the other, then the diagonals). If no such
     road is safe, a repeated road beats no wave.
   - **Split attacks**: with 3+ elements, Occupiers split the last wave and Invaders can split from wave 1. The
     lead column takes the recipe's first (heaviest) half, the second the rest. A second safe road is required;
     otherwise one column carries the complete wave. The split never doubles its element budget.
   - **Infantry doctrine**: both armies share attributed hostile-damage reports, accepted for 45 s and expired at
     the next allowed retask. Invaders push via lateral ingress points, then close; Occupiers retain covering
     elements when enough packages are present. Teams retaking a lost town return to its objective after their
     contact task. Native AI handles perception, firing, cover and movement. See the review for tuning attributes.
3. **Helicopter assault** (Readiness 1+): when the first ground wave is committed, the army requests one joint
   insertion from its own bases: normally two troop transports, one gunship and one light ground-support element.
   Capacity can reduce extra transports to one primary transport plus gunship. The existing recipe IDs
   (`<army>_townair_r4`) are retained for compatibility, with their equipment window expanded to Readiness 1–5.
   It waits if live aircraft reserve the airspace; unsafe launches or landing sites remain subject to normal failure
   and cleanup. `m_bAirOnFirstWave = 0` restores the option to wait until a wave presses within 350 m of the post;
   `m_iAirAssaultReadiness = 0` disables the insertion. See [air response](SPOTTING_AND_AIR_RESPONSE.md).
   - **Response reports**: once a wave's first column (or the helicopters) is on the road, one headline answers the
     contact report with where it comes from and when it arrives: "RESPONSE · MEAUX / Convoy 2 of 3 inbound from the
     south-west · ETA ~3 MIN", "Two columns inbound from the north and east, wave 3 of 3", "Helicopters inbound".
   - None of this uses the QRF wording, which stays for outposts, bases and the other campaign responses: town waves
     raise no "QRF DISPATCHED" headline, and on the map the town gets one red ring (however many columns are out)
     captioned "CONTACT · NATO REINFORCEMENTS FORMING", then "NATO REINFORCEMENTS INBOUND · ETA 1:20", then
     "NATO REINFORCEMENTS IN TOWN". Each column and the helicopter flight also draw their own arrow from where they are
     to the town (dashed to a circled-X landing zone for helicopters), refreshed every 4 s; attack orders and QRFs get
     the same arrows.
4. **Outcome**: nothing is declared while any column, armour or the assault is still out there.
   - **Repelled**: every operation has ended (destroyed or withdrawn) with the Resistance still near: +20% Resistance
     support in the town and an HQ headline from a pool ("X HOLDS / …").
   - **Retaken (lockdown)**: see below.
   - **Abandoned**: no Resistance player within 1.5 km for 3 minutes. If the Resistance had gained anything (support
     or the post), the army retakes the town as for the lockdown ("X RETAKEN / The Resistance pulled out").
   - Unresolved 15 minutes after the last wave: ends quietly. All end paths cancel outstanding child operations
     through Operations' existing retention rules, preventing later launches after the town response slot frees.

## Lockdown

The town capture (lockdown) starts when the Resistance takes the post's flag during a response (immediately, when
taking the flag is what brought the response). From then on the town counts, within its radius (120–300 m) every
5 s, the Resistance's fighters (players and recruits) against **only the response's own reinforcements** (wave and
helicopter troops): the garrison, checkpoints and patrol that were already in town never count, so leftover
defenders cannot retake it. While the reinforcements outnumber the Resistance the lockdown clock runs up; otherwise
it runs back down.

A town held by an army with a standing garrison post never passes to the Resistance on support alone (a repelled
response, regional support, missions): its flag must be taken first. Taking it breaks the post and adds the support
swing, which flips the town then. At 10 minutes in total the army
retakes the town: the Resistance support gained since the contact report is lost, the army is given a bare majority
and control even inside the Resistance's flip cooldown (so the post can re-form), and the headline reads "X RETAKEN". A response keeps running if the
town's support flips it to the Resistance mid-fight: the lockdown decides who keeps it.

Players within 700 m see an indicator at the top centre of the HUD: "MEAUX · ENEMY 14 vs 6 · TOWN LOST IN 4:10" (red,
bar filling), "RECOVERING 2:30" (amber, bar draining) or "HOLDING" (green). `m_fResistanceWeight` makes each
Resistance fighter count as more than one enemy if small squads lose towns too easily (default 1: plain headcount).

## One response at a time

Only one town response runs on the map. A second town hit meanwhile sends a contact report ending "No reinforcements
available" and its garrison fights alone; once the first response ends, it gets its own if the attackers are still
near and the kill threshold is met.

## Escalation

Town fighting moves the army's two scales directly (`RecordTownEscalation_S`):

- Every enemy the players kill in or around the town (garrison, checkpoints, patrol, wave troops): **+8 Crackdown**
  (0–1000; a level per 200) and **+4 Readiness pressure** (levels at 100, 250, 450, 700). About 25 kills raise
  Crackdown a level. Both are per-town attributes.
- Taking a garrison post counts as a capture: the scenario's capture escalation (Chicken: +100 Crackdown, +50
  Readiness).
- Each of these counts as fresh evidence, so neither scale cools while a fight goes on. After 10 quiet minutes Chicken
  cools Crackdown by 5 and Readiness pressure by 2 per minute (Readiness never below the starting level). Crackdown
  cooling was off in Chicken before; Readiness cooling is new (`m_iReadinessDecay`, off by default elsewhere).

## Invader attack orders and town occupation

The Invaders (USSR) now take towns as well as military sites (Chicken: `m_bOccupyTowns`, `m_bAttackOrders` on the
Invader campaign):

- Their periodic push (every 15 minutes, on the southernmost target within reach of their front) includes towns and
  cities. Far from players the battle stays simulated and a win occupies the town through support (60% Invader
  support, which flips control).
- Near players or an active garrison, instead of waiting, they issue an **attack order**: a physical attack launched
  from an Invader base, sized to take the target (at least one tier above its garrison level, a city one more, never
  below their Crackdown level). Resistance-held targets always get one.
- The attack wins by holding the force advantage on the objective (defenders count double): 60 s for a military site
  (AI recapture), 3 minutes for a town (occupation contest, only while the attack order is out, so a passing patrol
  never flips a town). Players defending a Resistance town see "TERRITORY UNDER ATTACK"; a lost town reads
  "X LOST / USSR forces have occupied the town", a town taken from the Occupiers "X OCCUPIED".
- An occupied town gets the Invaders' garrison post (USSR tent and flag), checkpoints and patrols, and their wave
  recipes when players attack it. A town fighting off a garrison response is not attacked until that ends.

## US attack orders

The Occupiers fight for towns too (Chicken: `m_bTownAttacks` on the operation manager):

- Their territory attacks (one at a time, the front site nearest an Occupier base first, 5 minutes after the capture,
  15 minutes rest after each attack) now include Resistance-held towns and cities, with the same sizing as the
  Invaders' orders and the same occupation contest. "NATO ATTACK INCOMING / Meaux · ETA ~4 MIN" announces them.
- When the Invaders hold more than their goal share, the Occupiers win ground back from them. Far from players that
  stays simulated; near players it is now a physical US attack order, so US and USSR forces fight over the front where
  the players are.

## Holding a town

When either army's attack order hits a Resistance-held town, its defenders see the lockdown indicator: "MEAUX · ENEMY
12 vs 5 · TOWN LOST IN 1:40" while the attackers have the advantage (defenders count double), "HOLDING" otherwise. The
attackers must keep the advantage for 3 minutes without a break; losing it resets the clock. Both orders and
occupations are logged as `[AROR BALANCE] attack_order` (role, target, tier, `source=front|territory`) and
`town_occupied`.

Pacing rules for both armies' attack orders (Chicken):

- **One attack per army**: the US and the Russians can each have one attack out at the same time
  (`m_bOneAttackPerArmy`), never on the same target.
- **No attacks during a town assault**: while any town garrison response is running, no attack order launches.
- **Breathing room**: a town or city is not targeted by any attack, simulated battles included, for 30 minutes after
  it changes hands (`m_iTownAttackGrace`; also after the campaign loads). The Russian push picks another target
  meanwhile.

The intended loop: take towns from the US garrison posts and hold them against the counterattacks, while the Russians
push up from the south and take towns too, so the Resistance eventually meets them on the front.

## Economy (Chicken)

The aim: one held town buys rifles, a couple of towns buy vehicles. Income is paid every 5 minutes (`AROR_IncomeComponent`
on the Chicken game mode); prices are faction funds unless noted.

| Held | Faction funds per payment | Per hour | Each player's share per hour |
|---|---|---|---|
| 1 town (~37 people) | ~330 | ~4,000 | ~1,000 |
| 3 towns | ~1,000 | ~12,000 | ~3,000 |
| 1 city (100-160 people) | 900-1,400 | 11,000-17,000 | 2,700-4,300 |

| Purchase | Price | What it takes |
|---|---|---|
| AKS-74U / AK-74 (no facility needed) | 350 / 400 | a few minutes of one town; a new player starts with 600 |
| RPG-7, PG-7VL / PG-7VM rockets (no facility) | 900, 150 / 200 | |
| Early FIA kit (no facility): Sa-58P/V, SVD, UK-59, RPG-75, PM, RGD-5 | 150-2,200 | the FIA's own arsenal, available from the start |
| Light car (UAZ, Imports, x1.5; no facility) | ~2,700 | 3 towns for 15 minutes; two in half an hour |
| Armed car | ~7,200 | 3 towns for 35 minutes |
| APC (Imports) | ~21,750 | 3 towns for ~2 hours, or a city |
| Town garrison: infantry | 300 + 1 MP a soldier | a full team (4) is ~1,200 |
| Town garrison: APC / armour | 12,000 / 25,000 + 3 MP | needs 1 / 2 held seaports |

Manpower: 1 per town and 2 per city each payment. Resources (170), factories, ports and the armies' external support
(170 each) are the old 15-minute rates rescaled. Plain cars (unarmed, base cost up to 1,800: UAZ, S105, M151, M998, MRZR) and starter weapons need no import facility; every other vehicle keeps its requirement.

## Town garrisons (Resistance)

HICOMM → **Garrisons** lists every town, city and military site (outposts, bases, ports, airfields and the rest; not the HQ) the Resistance holds. Buy infantry (a team of up to four), APCs or
armour; the cost is paid once and the units stay in the town, saved with the campaign, until they die or the town is
lost. Seaports bring the heavy kit: one held seaport unlocks APCs, two unlock armour (armour only exists with RHS or
WCS loaded; the base game has none for the Resistance). Towns and small sites hold 12 infantry and 2 vehicles; cities, bases and airfields 20 and 3.

The garrison turns out while a Resistance player is within 1,200 m or an enemy soldier within 700 m (an attack order
arriving), holding the town on a defend waypoint; vehicles park at the roadside with a crew of two. After 90 quiet
seconds it stands down and its survivors are written back; dead soldiers and destroyed vehicles must be bought again.
Logged as `[AROR MILITIA]`.

## Vehicle presets

Troop transports (the motorized profiles) now request `MOTOR_TRUCK`, which every enemy preset fills with plain
transport trucks, following Rowan's FORECON preset (a truck listed once per job). Before, they drew from
`SUPPLY_TRUCK`, which also holds arsenal, ammo, repair, command and engineer trucks, so a wave could arrive in an
arsenal truck. The USSR and civilian fuel tankers were typed `TANK` and are now `FUEL_TRUCK`. With WCS+RHS, the
armies use Rowan's presets in `OpenResistanceWCSRHSCompat` (WCS vehicles: KBA-105 BTR, NSV and Kornet UAZ,
Ural ZU-23, Mi-24, Ka-52, Strykers, JLTVs, AH-6); the RHS-only presets carry no vehicles of their own.

## Balance logging

Every response writes `[AROR BALANCE]` lines to the console log: `town_response_start`, `town_wave`,
`town_contest` (every 30 s), `town_escalation` (every kill or post), `town_response_end` (outcome, duration, waves,
kills, peak forces), `town_response_effect` (support, Crackdown and Readiness before and after),
`town_response_deferred` and `post_taken`. `tools/Export-BalanceLog.ps1` turns them into one CSV per event (default:
the newest Workbench log folder).
- **Readiness picks the kit** (wave recipes `<army>_townwave<N>_r1|r3|r5`):

| Readiness | Wave 1 | Wave 2 | Wave 3 |
|---|---|---|---|
| 1–2 | trucks, armed car | trucks, armed cars | armed car leading trucks |
| 3–4 | trucks, then IFV | truck + IFV mixes | IFV-led, armed car |
| 5 | truck + IFV | IFVs, armour | armour-led, IFVs |

- **Crackdown picks the size**: 2 vehicles at levels 1–2, 3 at 3–4, 4 at 5. The third wave and cities each add one.
  Recipes list elements in priority order and the response takes the first N.
- Funded by the scenario, not the army's bank. Executor capacity still applies (Chicken's live ground cap is raised
  from 6 to 8 packages, the validator's maximum, and its vehicle reservation cap from 12 to 16 so waves can overlap).
  `tools/Test-ExecutorSettings.ps1` checks every scenario's executor values against the validator before play.
- Re-arms 30 minutes after it ends.
- **No capture QRF** for these towns: the waves are the whole response.

## Town life

Spawned while a player is within 900 m; removed half a minute after everyone is beyond 1150 m.

- **Civilians**: 15% of population (3–10), one per group, walking between street road points. A player killing one
  costs 3% Resistance support there and gets a hint. When anyone in or near town is hit (soldier, crew or civilian),
  every civilian runs 60–110 m away from that spot and no new ones walk out; 90 s after the last hit they slow to a
  walk and go back to the streets (`[AROR TOWN] ... gunfire: civilians take cover`).
- **Parked cars**: 8% of population (2–6) from the Civilians preset; players can take them. Each is parked on the
  roadside of the nearest road, lined up with it and facing the traffic direction for its side, at least 12 m from
  the next. Moved cars are left alone.
- **Checkpoints** (AI-held towns): 1, 2 from Crackdown 3, 3 at Crackdown 5, one more in a city, on roads where they
  cross the town edge. Two sandbag walls and a guard team holding the spot.
- **Patrol vehicle** (AI-held towns): one (two from Crackdown 4), Readiness picks armed car, then APC, then IFV. It
  spawns on the road, lined up with it. The crew loops the town's road points; if someone cannot board within 90 s
  it drives with whoever got in, as long as the driver did. It keeps to 30 km/h on the streets, 55 km/h while
  civilians are sheltering from gunfire (AI cruise speed; `m_fPatrolSpeed`, `m_fPatrolAlertSpeed`).
- Wiped-out checkpoints and patrols stay gone for 45 minutes.

## Also changed

- Early QRFs: the level 1–2 light and joint recipes (both armies) each carry one more truck of infantry.
- `AROR_ForceSpawner` now holds the faction-preset vehicle lookup and crew seating that missions used; missions call it.

## Other mods

RC Dynamic Counter Attacks (RadarController, APL) was used the same way: its source is not public, and only the
behaviour its workshop page describes (alert delay, several axes, air committing once the ground force presses,
"repelled" only when nothing is left inbound, an HQ line pool) shaped the response phases.

Freedom Fighters (Johnny Kerner) is APL-ND: none of its code is used. Ideas taken from its design: civilians each in
their own group, spawned away from players; convoys formed up along one road; checkpoints at town approaches.

## Map markers

Each location is drawn as a silhouette of its type, filled in the holder's faction colour inside a dark keyline, so
the type reads at a glance (`AROR_MapSiteSymbol`): a fort for bases, a bunker for outposts, a plane for airfields, an
H pad for heliports, an anchor for ports, a sawtooth roof for factories, crates for resources, a flag for the HQ, a
house for towns and a skyline for cities. Size follows importance: key sites (bases, outposts, airfields, heliports,
ports, the HQ, strongholds) are largest with their name in bold capitals on a plate; other sites carry a smaller
plate. Towns and cities also show their garrison post's pennant where the flag stands. HICOMM's map shares the same
drawing.

## To check in Workbench

1. Script Editor compile. Run `tools/Test-ExecutorSettings.ps1` and `tools/Test-TownSites.ps1` first: the first fails if
   an executor value would disable
   Operations (this is what silenced the first Meaux test: the console showed `Invalid tactical commander settings`,
   and now also names the setting); the second checks every town has its components and flag, kept outposts their
   perimeter, and reports military sites close to a town.
2. Open `AROR_Everon_Chicken.ent` and save once so Workbench rewrites the edited layers. Check a town shows the three
   components and a flag child.
3. In play, look for `[AROR] Garrison post site:` lines (or the "no room" warning) and check a town has the tent with
   the faction flag in front and defenders around it. Open the map: new markers, bold key-site names, post pennants.
4. Play near Meaux: civilians, cars parked along the road, a checkpoint and the patrol should appear (`[AROR TOWN]` log lines). Kill three
   defenders: expect `response triggered`, the alert headline, then `[AROR] Town wave` lines 30–90 s later and a
   column arriving from a road 1–2 km away. Check waves 2 and 3 come from different roads and wave 3 logs two
   columns. With Readiness 4+ (debug), expect `helicopter assault requested` once a column reaches the town. Kill
   everything: expect `response repelled` and an "X HOLDS" headline. Run off 1.5 km instead: expect `abandoned`.
5. Clear the post and take the flag: expect the headline and a support jump, and no QRF.
6. Watch server FPS with a wave, a QRF and town life at once.

Known gaps: checkpoint placement is a road-snap estimate; civilians run away from a hit but do not enter buildings;
kill progress and town life are not saved.
