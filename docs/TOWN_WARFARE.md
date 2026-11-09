# Town warfare (Chicken)

Status: written 2026-11-09, **not compiled or play-tested**. Chicken only; every new behaviour is off unless a
location carries the new components (base Everon is unchanged).

The fight moves from outposts in fields into towns. Each town and city has a garrison post the Resistance can take,
an occupying presence in the streets, and a fixed three-wave response when players attack its defenders.

## Map changes (Chicken)

- **Outposts cut to 3 of 14.** Kept: Wolf Wood, Beacon Point and Old Sawmill (the three furthest from any town). They
  are now `MILITARY_BASE` garrisons (level 3, plus two extra infantry teams, compound size kept at 18 m). Removed:
  Ironworks, Grey Quarry, Barley Field, Gull Point, Raven Ridge, Pinewood Lake, Stag Hill, Regina Heights, Fox Valley,
  Hawk Ridge and Crow Hill. Saved campaigns simply stop loading the removed sites.
- **All 18 settlements** get `AROR_GarrisonComponent` (`m_bSettlementPost 1`), `AROR_TownWavesComponent`,
  `AROR_TownLifeComponent` and a capture flag that auto-places on a clear spot near the centre at play start.
  Component and entity IDs in the layer were hand-written; Workbench may rewrite them on save.

## Garrison post

- **The site**: at play start the server looks for room for a building within 60 m of the town centre (6.5 m clear
  radius, at most 1.5 m height difference, nothing standing in it; the centre first, then four rings of eight). It
  raises the holding army's barracks tent there (`Tent_Barracks_US_01`, `_USSR_01`, `_FIA_01`; swapped when the town
  changes hands) and stands the flag 10 m in front of it. The flag is a full flagpole whose cloth shows the holding
  faction. No room anywhere: flag only, logged as a warning. Each of these is an attribute on the flag's placement
  component (`m_fBuildingYawOffset` turns the tent 180° if its door ends up facing away from the flag).
- Raised when players first come near, sized by the holding army's **Crackdown**: level 1 (2 teams), level 2 from
  Crackdown 3 (4 teams, one gun), level 3 at Crackdown 5 (6 teams, two guns, a manned vehicle); cities one level more.
- Infantry stands in a tight ring (20 m half-size) around the flag, so in front of and beside the tent. Town ground is
  rarely flat, so posts always use the infantry-only fallback when the compound check fails.
- Chicken's garrison personnel cap is 144 (was 96, which the extra town posts filled near Meaux).
- **Map**: a pennant in the holding faction's colour marks each post on its flag once zoomed in, drawn as an outline
  once the post is taken.
- **Taking the flag** (all hostiles within ~60 m of it dead): +30% Resistance support in that town. This may flip the
  town through the normal support rule, but the flag never changes control itself. The post is then broken: nothing
  in its roster deploys again. It re-forms 45 minutes later, once nobody is near and the army still holds the town.

## Wave response

Trigger: players kill 25% of the defenders seen (post, checkpoint guards, patrol crews), at least 3. The response
then runs in phases, after the pattern of RC Dynamic Counter Attacks (ideas from its workshop page only; no code):

1. **Alert**: a contact report that names who hit the garrison, "CONTACT REPORT · MEAUX / Garrison engaged by
   Jasper and Rowan. Reinforcements requested". The first wave forms up 30–90 s later.
2. **Waves**: three, 150 s apart (±30 s). Each stages on a road 1–2 km out and drives in as a column. The lead
   vehicle stops 220 m short of the post, later vehicles 25 m behind each other, and the infantry walk in.
   - **Axes**: wave 1 comes from the side of the army's nearest base; each later wave uses a road at least 500 m from
     the previous wave's (preferred bearings rotate 0°, 90° one way, 90° the other, then the diagonals). If no such
     road is safe, a repeated road beats no wave.
   - **Split last wave**: with 3+ elements, the last wave is two columns on two roads. The lead column takes the
     recipe's first (heaviest) half, the second column the rest.
3. **Helicopter assault** (Readiness 4+): once any wave package is within 350 m of the post, the army commits one
   joint recipe (`<army>_townair_r4`: airborne pair plus light support) from its own bases. It waits its turn if
   another airborne response holds the airspace, and falls back to ground if no air launch is clear.
   - **Response reports**: once a wave's first column (or the helicopters) is on the road, one headline answers the
     contact report with where it comes from and when it arrives: "RESPONSE · MEAUX / Convoy 2 of 3 inbound from the
     south-west · ETA ~3 MIN", "Two columns inbound from the north and east, wave 3 of 3", "Helicopters inbound".
   - None of this uses the QRF wording, which stays for outposts, bases and the other campaign responses: town waves
     raise no "QRF DISPATCHED" headline, and on the map the town gets one red ring (however many columns are out)
     captioned "CONTACT · NATO REINFORCEMENTS FORMING", then "NATO REINFORCEMENTS INBOUND · ETA 1:20", then
     "NATO REINFORCEMENTS IN TOWN".
4. **Outcome**: nothing is declared while any column, armour or the assault is still out there.
   - **Repelled**: every operation has ended (destroyed or withdrawn) with the Resistance still near: +20% Resistance
     support in the town and an HQ headline from a pool ("X HOLDS / …").
   - **Retaken (lockdown)**: see below.
   - **Abandoned**: no Resistance player within 1.5 km for 3 minutes. If the Resistance had gained anything (support
     or the post), the army retakes the town as for the lockdown ("X RETAKEN / The Resistance pulled out").
   - Unresolved 15 minutes after the last wave: ends quietly.

## Lockdown

While a response runs, the town counts the responding army's living fighters (garrison, checkpoints, patrol, wave
troops) and the Resistance's (players and recruits) within its radius (120–300 m) every 5 s. While the army
outnumbers the Resistance the lockdown clock runs up; otherwise it runs back down. At 10 minutes in total the army
retakes the town: the Resistance support gained since the contact report is lost (which flips the town back if the
gain had flipped it), the post is restored, and the headline reads "X RETAKEN". A response keeps running if the
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
  costs 3% Resistance support there and gets a hint.
- **Parked cars**: 8% of population (2–6) from the Civilians preset; players can take them. Each is parked on the
  roadside of the nearest road, lined up with it and facing the traffic direction for its side, at least 12 m from
  the next. Moved cars are left alone.
- **Checkpoints** (AI-held towns): 1, 2 from Crackdown 3, 3 at Crackdown 5, one more in a city, on roads where they
  cross the town edge. Two sandbag walls and a guard team holding the spot.
- **Patrol vehicle** (AI-held towns): one (two from Crackdown 4), Readiness picks armed car, then APC, then IFV. It
  spawns on the road, lined up with it. The crew loops the town's road points; if someone cannot board within 90 s
  it drives with whoever got in, as long as the driver did.
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

Location markers are kept simple: a solid shape in the holder's faction colour inside a dark keyline, with one thick
symbol (white, or near-black on a light colour) for the type. A square for the Resistance, a diamond for the enemy,
a circle for neutral sites. Towns and cities have no symbol. Size follows importance: key sites (bases, outposts,
airfields, heliports, ports, the HQ, strongholds) are largest and carry their name in bold capitals on a plate to
their right; factories and resources are smaller; cities and towns smallest (the map prints their names). A post's
flag marker appears on the flag once zoomed in. HICOMM's map shares the same badge drawing.

## To check in Workbench

1. Script Editor compile. Run `tools/Test-ExecutorSettings.ps1` first: it fails if an executor value would disable
   Operations (this is what silenced the first Meaux test: the console showed `Invalid tactical commander settings`,
   and now also names the setting).
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

Known gaps: the patrol has no speed cap (no `SetCruiseSpeed` call); checkpoint placement is a road-snap estimate;
civilians do not react to combat beyond native AI; kill progress and town life are not saved.
