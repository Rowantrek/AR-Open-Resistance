# QRF response roster

The [tactical commander](QRF_TACTICAL_COMMANDER.md) now coordinates supported ground recipes and complete air packages, including finite two-wave deployment. It does not turn every catalogue entry into an executable recipe or connect campaign stock spending. Chicken enables the authored Invader ground capture responses; the shared admission default remains off and native acceptance is pending.

Proposed commander catalogue: **eight response families, five levels each, 40 authored responses**. The catalogue is design data. The ground-only `combined_arms_l1` recipe now has a Chicken prototype (truck plus IFV); see [the test guide](TRUCK_IFV_QRF_TEST.md). Chicken now also provides 30 joint recipes across Readiness 2, 3–4 and 5, combining one airborne package with ground elements; their source implementation is covered by [release validation](QRF_RELEASE_VALIDATION.md), with native acceptance pending. The full table below, independent H/G counts and specialist families remain design proposals. Homogeneous profiles retain their existing level multiplier. A CSV copy lives in [QRF_RESPONSE_ROSTER.csv](QRF_RESPONSE_ROSTER.csv).

For composite responses, level becomes an authored threat/commitment tier. It does not multiply this table again. A level-3 combined-arms response deploys the listed mix once. Existing homogeneous profiles keep their current semantics until explicitly migrated. All templates resolve assets through the responding role's configured faction.

## Elements

| Code | Complete element | Tactical responsibility |
|---|---|---|
| M | One motorized vehicle, required crew and one four-person infantry team | Fast reinforcement, dismount and town search |
| I | One APC/IFV, required crew and one four-person infantry team | Protected insertion, infantry support and road control |
| A | One armoured fighting vehicle and its complete crew | Counter vehicles and support from suitable firing positions; no passenger team |
| AT | One M element whose four-person team has an approved anti-armour loadout | Dismounted anti-armour response; replaces that M team, not extra infantry |
| H | One transport helicopter, full flight crew and one four-person infantry team | Insert at a clear nearby LZ, unload, depart |
| G | One gunship with its complete required flight/weapon crew | Escort, respond to allied reports and bounded search/support |
| F | One four-person infantry team deploying on foot from a nearby outpost | Reinforce buildings and hold ground; requires a viable short walking route |

Numbers count complete elements. Crews are additional to the stated infantry. Passenger capacity, specialist loadouts and actual crew requirements must be verified for each faction's concrete profile. A is a role, so a faction can supply a verified wheeled armoured asset; this table does not assert that a tank prefab exists. H does not implicitly include G: aircraft are counted separately to avoid multiplying escorts unintentionally.

## Response table

| Family / stable ID | Level 1 | Level 2 | Level 3 | Level 4 | Level 5 |
|---|---|---|---|---|---|
| Motorized reinforcement / `motorized` | M | 2M | 2M + I | 3M + I + A | 4M + 2I + 2A |
| Mechanized response / `mechanized` | I | 2I | 2I + M | 3I + M + A | 3I + 2M + 2A |
| Armour counterattack / `armour_counter` | A + F | A + I | 2A + I | 2A + 2I + M | 3A + 2I + 2M |
| Air assault / `air_assault` | H + G | 2H + G | 2H + 2G | 3H + 2G | 4H + 2G |
| Combined arms / `combined_arms` | M + I | M + I + G | 2M + I + H + G | 2M + 2I + A + H + G | 3M + 2I + 2A + H + G |
| Anti-armour response / `anti_armour` | AT | 2AT | 2AT + I | 2AT + 2I + A | 3AT + 2I + 2A |
| Reinforce and hold / `reinforce_hold` | 2F + M | 2F + 2M | 2F + 2M + I | 3F + 2M + 2I | 4F + 3M + 2I + A |
| Search and containment / `search_contain` | M | M + H | M + H + G | 2M + 2H + G | 2M + I + 2H + 2G |

These are distinct responses with overlapping equipment, rather than eight names for a uniform strength roll. At the same level, different families have different cost, mobility and tactical value. Every listed element is required for that recipe. If a recipe is unaffordable, select another eligible recipe or explicitly choose a lower permitted tier; do not quietly omit its gunship, crew or infantry.

Air-heavy high levels deliberately add fewer gunships than the current five-transports/five-gunships test. Extra infantry delivery does not require a matching increase in escorts. Unescorted H entries in the containment family are intentional lower-cost variants and must be independently enabled after acceptance, rather than created by silently losing a required G.

## Choosing the response level

| Level | Intended commitment | Evidence that can justify it |
|---|---|---|
| 1 | Local reaction | First isolated hostile report or limited defender losses |
| 2 | Reinforced reaction | Repeated valid reports, several casualties or the first response under pressure |
| 3 | Deliberate counterattack | Sustained fighting, credible vehicle contact or a threatened important location |
| 4 | Major response | Substantial confirmed threat or heavy defender losses at a valuable location |
| 5 | Strategic commitment | Confirmed major attack on a critical location and enough reserves to support it |

These are policy examples, not automatic triggers already implemented. A player count alone must not reveal unseen players or determine a response. Use reported threat categories, defender losses, report freshness and strategic location value. Unknown enemy strength remains unknown; it does not default to armour or level 5.

Location importance can raise the desired tier or its permitted ceiling; it must not manufacture resources. A low-priority incident can resolve with a smaller affordable response even when a level-5 force exists elsewhere. Repeated reports update the same incident; escalation, if permitted, adds an explicitly budgeted reinforcement operation rather than rerolling and respawning the current force.

## Family suitability and resource constraints

| Family | Prefer when | Main gates / reasons to exclude |
|---|---|---|
| Motorized | Road-accessible town, resources site or distant outpost; light/unknown threat | Available transport, crew, fuel, infantry and a viable dismount route; avoid known heavy armour without support |
| Mechanized | Factory, port, military location or town with usable vehicle approaches | IFVs/APCs, crew and infantry; vehicle route and turning space; avoid forcing vehicles into inaccessible streets |
| Armour counterattack | Credible hostile vehicle reports; open approaches to airport or military site | Available armour/crew, suitable ground and ammunition; F variants also require nearby foot access; do not select solely because a town is important |
| Air assault | Road response would arrive too late or route is unsuitable; insertion near a threatened location | Flight-ready aircraft, crews, infantry, fuel, LZs and safe launch plan; exclude under a configured unacceptable air threat |
| Combined arms | Sustained fighting at towns, airports, factories, ports or military bases | All listed land/air assets, both routes and their combined runtime budget; no air capability means an authored ground variant must be selected |
| Anti-armour | Valid reports identify hostile vehicles; constrained armour stock | Verified AT-equipped infantry, transport and ammunition; additional I/A assets at higher levels; no unverified loadout substitution |
| Reinforce and hold | Hold an outpost, town buildings or a factory perimeter | Nearby outpost, infantry, walking route and listed transport/IFV assets; reject F if walking cannot meet the deadline |
| Search and containment | Mobile attacker, recent contact lost, several approaches need watching | Recent bounded contact information, vehicle/air readiness and search area; no live player tracking and no indefinite pursuit |

Terrain and access matter more than the location label alone. A port with narrow streets may prefer dismounted reinforcement; an airport without an eligible friendly outpost cannot magically produce an armoured column.

## Resource and reservation contract

Availability should come from the future military-resource provider, separate from runtime entity capacity:

| Resource dimension | Required check |
|---|---|
| Origin and control | Nearest eligible friendly outpost with stock/readiness for the entire recipe; capture invalidates an unlaunched reservation |
| Infantry | Available teams, including AT specialists; leave a configurable minimum garrison behind |
| Vehicles and aircraft | Exact required types, condition/readiness and usable passenger/crew seats |
| Personnel | Every dismount, driver, pilot and weapon crew member; no free replacement crew |
| Supplies | Fuel, ammunition and any future campaign supply cost; prices remain configurable and are not defined here |
| Arrival | Ground/air/foot ETA and an incident-specific response deadline |
| Runtime capacity | Shared group/personnel/vehicle budgets, counting escorts, pending forces and retained wrecks |

The current nearest-outpost resolver chooses an origin by friendly control and launch checks only; stock-aware origin selection is planned. Do not drain Rowan's economy, garrison or persistence systems directly. Integrate through an agreed availability/reserve/commit/release interface when those providers exist. Configured test coordinates remain a development fallback; production policy should explicitly decide whether no usable outpost means defer, use another base type, or fail.

Reserve every required component atomically under one parent operation before queuing members. Air and ground legs receive separate tasks and readiness states, while the parent owns total cost, outcome and cleanup. Dispatch timings can differ to coordinate arrival; a shared parent does not mean trucks must wait for a helicopter to land. One lost element degrades the remaining response rather than deleting healthy forces. Define stock consumption on dispatch and salvage/return policy explicitly; cleanup must not refund destroyed assets.

The CSV provides nominal accounting using four infantry per team, one crew per M/H/G and two crew per I/A, plus separate crew groups. Those assumptions help review the roster; they are not final spawn reservations. Actual profile slot counts and vehicle crew needs override the estimates, including a larger flight/weapon crew where required. Retained forces still consume runtime capacity.

## Selector sequence

1. Build or update one role-owned incident from trusted reports and the attacked location. Keep role, location ID, report time and known threat categories.
2. Determine desired and permitted response tiers from threat evidence, importance, commander policy and available reserves.
3. For each candidate recipe, resolve concrete faction profiles, an eligible origin, stock, crew, routes/LZs, ETA and shared runtime capacity. Exclude unsupported execution policies.
4. Choose the highest permitted affordable tier according to policy, then select a family using tactical suitability and configurable weights. Weight families first so adding more recipe variants does not inflate a family's probability. Do not fall through every lower tier unless policy permits downgrading.
5. Reserve the chosen recipe once. Record stable recipe ID (`family_l1` through `family_l5`), resolved elements, origin and reservation IDs on the operation. Keep the choice stable through spawn retries.
6. If no recipe qualifies, return a bounded defer/failure reason. If availability changes before dispatch, release reservations and explicitly replan. In production, no unlimited rerolls, silent free assets or automatically replacing destroyed units.

## Next implementation

The explicit `AIR_GROUND` family now owns one airborne package with an optional gunship and authored ground elements under one operation, using existing profiles as element definitions. This does not implement the entire table above: independent H/G counts, shared gunships and specialist AT variants still need separate execution and acceptance.

Validate the executable ground and joint recipes in Chicken before adding more families. Cover low-funds fallback, no eligible origin, captured origin, impossible route/LZ, exhausted runtime capacity, partial combat losses and terminal cleanup. Campaign stock spending remains a separate unfinished resource seam; the proposed design catalogue must not be treated as native release evidence.
