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

- Raised when players first come near, sized by the holding army's **Crackdown**: level 1 (2 teams), level 2 from
  Crackdown 3 (4 teams, one gun), level 3 at Crackdown 5 (6 teams, two guns, a manned vehicle); cities one level more.
- Infantry stands in a tight ring (20 m half-size) around the flag. Town ground is rarely flat, so posts always use the
  infantry-only fallback when the compound check fails.
- **Taking the flag** (all hostiles within ~60 m of it dead): +30% Resistance support in that town. This may flip the
  town through the normal support rule, but the flag never changes control itself. The post is then broken: nothing
  in its roster deploys again. It re-forms 45 minutes later, once nobody is near and the army still holds the town.

## Wave response

Trigger: players kill 25% of the defenders seen (post, checkpoint guards, patrol crews), at least 3. Then:

- Three waves, one every 150 s. Each stages on a road 1–2 km out, on the side of the army's nearest base, and drives
  in as a column. The lead vehicle stops 220 m short of the post, later vehicles 25 m behind each other, and the
  infantry walk in.
- **Readiness picks the kit** (wave recipes `<army>_townwave<N>_r1|r3|r5`):

| Readiness | Wave 1 | Wave 2 | Wave 3 |
|---|---|---|---|
| 1–2 | trucks, armed car | trucks, armed cars | armed car leading trucks |
| 3–4 | trucks, then IFV | truck + IFV mixes | IFV-led, armed car |
| 5 | truck + IFV | IFVs, armour | armour-led, IFVs |

- **Crackdown picks the size**: 2 vehicles at levels 1–2, 3 at 3–4, 4 at 5. The third wave and cities each add one.
  Recipes list elements in priority order and the response takes the first N.
- Funded by the scenario, not the army's bank. Executor capacity still applies (Chicken's live ground cap is raised
  from 6 to 10 packages and its vehicle reservation cap from 12 to 16 so waves can overlap).
- Stops sending waves after 3 minutes with no Resistance player within 1.5 km. Re-arms 30 minutes after it ends.
- **No capture QRF** for these towns: the waves are the whole response.

## Town life

Spawned while a player is within 900 m; removed half a minute after everyone is beyond 1150 m.

- **Civilians**: 15% of population (3–10), one per group, walking between street road points. A player killing one
  costs 3% Resistance support there and gets a hint.
- **Parked cars**: 8% of population (2–6) from the Civilians preset; players can take them. Moved cars are left alone.
- **Checkpoints** (AI-held towns): 1, 2 from Crackdown 3, 3 at Crackdown 5, one more in a city, on roads where they
  cross the town edge. Two sandbag walls and a guard team holding the spot.
- **Patrol vehicle** (AI-held towns): one (two from Crackdown 4), Readiness picks armed car, then APC, then IFV. The
  crew loops the town's road points.
- Wiped-out checkpoints and patrols stay gone for 45 minutes.

## Also changed

- Early QRFs: the level 1–2 light and joint recipes (both armies) each carry one more truck of infantry.
- `AROR_ForceSpawner` now holds the faction-preset vehicle lookup and crew seating that missions used; missions call it.

## Freedom Fighters

Freedom Fighters (Johnny Kerner) is APL-ND: none of its code is used. Ideas taken from its design: civilians each in
their own group, spawned away from players; convoys formed up along one road; checkpoints at town approaches.

## To check in Workbench

1. Script Editor compile. The new files are `Operations/AROR_TownWaveResponses.c` and `Towns/*.c`.
2. Open `AROR_Everon_Chicken.ent` and save once so Workbench rewrites the edited layers. Check a town shows the three
   components and a flag child.
3. Play near Meaux: civilians, cars, a checkpoint and the patrol should appear (`[AROR TOWN]` log lines). Kill three
   defenders: expect `response triggered`, a headline and `[AROR] Town wave` lines, then a column arriving from a road
   1–2 km away.
4. Clear the post and take the flag: expect the headline and a support jump, and no QRF.
5. Watch server FPS with a wave, a QRF and town life at once.

Known gaps: the patrol has no speed cap (no `SetCruiseSpeed` call); checkpoint placement is a road-snap estimate;
civilians do not react to combat beyond native AI; kill progress and town life are not saved.
