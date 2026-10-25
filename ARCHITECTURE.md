# Architecture

Tracks the architecture of Open Resistance: its systems, sub-systems, and how they fit together. Update this file whenever a system is added, removed, or changes shape.

Status tags: **Planned** (designed, not built) · **In progress** · **Implemented**.

## Mod structure

### AR-Open-Resistance (CORE MOD)

All the scripts and functionality, plus templates for base-game assets.

- Lives in this repo: `AROpenResistance/Open Resistance/` (addon ID `OpenResistance`, GUID `6AA282336705EED2`).
- Class prefix: `AROR_` for all core classes, enums, and globals (e.g. `AROR_LocationComponent`). Reserved for the core mod.
- The intended core baseline depends only on the base game (`ArmaReforger`, `58D0FB3206B6F859`). **Experimental branch exception requested by Jasper:** the normal project currently also depends on REAPER (`69A4D664A6284E06`) for direct QRF testing. Disabling flight dispatch restores ground behavior but does not remove that load-time dependency. This exception must be resolved before a base-game-only release.
- Provides:
  - every game mode system (see [Systems](#systems))
  - template prefabs and configs built from base-game assets (locations, spawn points, factions, loadouts, AI groups) that extender mods can inherit from or replace
  - the extension points that extender mods use

### Extender Mods (made by others)

Separate addons, maintained by others, that add extra content such as maps, weapons, vehicles, scripts, functions, missions etc.

- Each one lists the core mod (`6AA282336705EED2`) as a dependency in its `addon.gproj`.
- Extends the core by:
  - inheriting from core template prefabs and configs
  - supplying their own configs (factions, loadouts, location data for new maps, missions)
  - subclassing core `AROR_` classes, or using `modded class` where needed
- Uses its own class prefix, never `AROR_`, so names don't collide with the core or with other extenders.
- The core must never depend on, or reference, an extender mod.

```
                 ┌─────────────────────────┐
                 │  ArmaReforger (base)    │
                 └────────────▲────────────┘
                              │ depends on
                 ┌────────────┴────────────┐
                 │ AR-Open-Resistance      │
                 │ (CORE MOD)              │
                 │ systems + templates     │
                 └────────────▲────────────┘
                              │ depends on
        ┌─────────────────────┼─────────────────────┐
 ┌──────┴──────┐       ┌──────┴──────┐       ┌──────┴──────┐
 │ Extender:   │       │ Extender:   │       │ Extender:   │
 │ maps        │       │ weapons /   │       │ scripts /   │
 │             │       │ vehicles    │       │ missions    │
 └─────────────┘       └─────────────┘       └─────────────┘
```

## Systems

Add each system here as it is designed or built, using the template at the end of this section.

### Game Mode — In progress
Purpose: the game mode entity every map places; all core systems hang off it as components.

Key classes/prefabs/configs:
- `Prefabs/GameModes/AROR_GameMode_Base.et`: inherits the base-game `GameMode_Plain.et` (`SCR_BaseGameMode`). Core system components are added here.
- A custom game mode script class will be added when game mode logic is needed.

Server/client: standard `SCR_BaseGameMode` replication.

Extension points: extender mods can inherit `AROR_GameMode_Base.et` to add or swap components.

Sub-systems (Planned):
- Game mode rules for the Resistance gameplay loop (to be defined).
- A playable world also needs faction manager, loadout manager and spawn point entities; these will be part of the map template.

### Locations — In progress
Purpose: named areas on the map that other systems use to decide where things happen (start location, AI, objectives). World-placed types: **Town, City, Airport, Factory, Military Base, Port, Resources, Outpost**; spawned at runtime: **HQ**.

Key classes/prefabs/configs:
- `AROR_LocationComponent`: on each placed location. Name, type and a per-location radius set in the World Editor.
- `AROR_LocationManagerComponent`: on the game mode. Registry of all locations; other systems query locations through it.
- `AROR_ELocationType`: location types (`TOWN`, `HQ`, `AIRPORT`, `CITY`, `FACTORY`, `MILITARY_BASE`, `PORT`, `RESOURCES`, `OUTPOST`); new values are appended so existing ones stay stable.
- `Prefabs/Locations/AROR_Location_Base.et` → one template per world-placed type (`AROR_Location_Town.et`, `_City`, `_Airport`, `_Factory`, `_MilitaryBase`, `_Port`, `_Resources`), each setting only its type and map icon. `AROR_Location_HQ.et`: a base-game spawn point entity with the location component (type HQ) and Persistence.
- Map icons (planned): each location prefab names its own map icon (image in our addon); `AROR_LocationMapIconsComponent` on the game mode draws them (Map Markers layer 1).

Server/client: towns are static world entities and HQs are replicated dynamic entities; both register on server and clients.

**Control**: every location is always controlled by a role (Resistance, Occupiers or Invaders). Each placed location sets its initial controlling role; a loaded campaign restores the saved one. Control is server-owned, replicated to all clients (location prefabs carry an `RplComponent`), raised as a "control changed" event, saved per location, and shown on the map by tinting the location icon with the controlling faction's colour. The Resistance HQ is fixed to the Resistance.

**Settlements**: towns and cities additionally carry population and support (see Settlements).

Everon campaign placement (In progress): Chicken supplies an island-wide catalogue of settlements and ordinary/military outposts, using stable location IDs and inherited replication/persistence. Dedicated `City_Flag`, `Town_Flag`, `Outpost` and `MilitaryOutpost` location templates carry a capture flag. Military outposts use the existing `MILITARY_BASE` type; ordinary ones use `OUTPOST`. Chicken stores explicit root heights and per-site flag child offsets sourced from engine placement logs; these baked sites disable runtime relocation. Other catalogue locations can opt into deterministic terrain grounding before registration. Chicken aligns the airport, factory and resource anchors already authored in Rowan's scene without rewriting that scene. The shared capture pole independently finds a bounded clear/walkable terrain position on the server after hierarchy initialization, commits its child world transform/collision and replicates the result; location centres remain stable. Failed placement validation is logged. Shared Everon and Rowan's scenes retain their own placement data. This is location/scenario data, not automatic garrison or compound construction. Editor placement tools (In progress) audit actual pole transforms, terrain collision and model-base bounds across Chicken; an explicit undoable editor command can ground and serialize child positions without changing campaign location centres. These tools run only in the editor; native validation and reconciliation into the catalogue are required. See `docs/EVERON_CAMPAIGN_LOCATIONS.md`.

Depends on: Map Templates (for placement).

Extension points:
- New location types via `modded enum AROR_ELocationType` plus a prefab inheriting `AROR_Location_Base.et`.
- Extender maps place `AROR_Location_*` prefabs in their own sub-scene.

Sub-systems (Planned):
- Optional hand-placed child points (spawn, garrison, objective); fall back to scanning buildings inside the radius.
- Default name from the nearest base-game map descriptor.
- Workbench plugin to auto-place towns on a terrain.

### Location Capture — In progress
Purpose: how a location's controlling role changes. Rules depend on the location type; all changes are made by the server through the location's control (see Locations → Control). The HQ cannot be captured.

- **Towns & Cities**: held by the role with **more than 50% support** in that location (see Settlements).
- **Flag settlement opt-in**: the new flag-bearing settlement templates additionally allow hostile-cleared player flag capture and weighted AI recapture. These captures update the existing settlement support authority, keeping support and control coherent. Original settlement templates remain support-only; fixed control and flip cooldown still apply.
- **Military locations** (airport, factory, military base, port, resources, outpost): captured through an **interaction on the location's flag** (the capture flag prop gets collision and an interaction), only when **all hostiles within the location's radius have been eliminated**.
- **Flip cooldown**: after the Resistance captures a location it cannot flip back to the Occupiers or Invaders for **5 minutes** (fixed).
- **Resistance captures** of military locations require **every hostile** in the radius to be dead.
- **AI recapture** (`AROR_RecaptureComponent`, server, on the game mode): every few seconds, forces inside each military location's radius are weighed. **Defenders** (the controlling role's players and AI) count **×2**, **attacking AI** count **×1**. An attacking AI role (Occupiers or Invaders) with more weight than the defenders takes the location, subject to the flip cooldown. Applies AI vs AI too.

Military capture (in progress): `AROR_CaptureLocationAction` is a user action on the capture flag prop (`AROR_Prop_CaptureFlag.et`, a child of the location). Clients check the rules for feedback ("Hostiles in the area", "Recently captured"); the server re-checks and changes control. The flip-cooldown lock is replicated but not saved.

Depends on: Locations, Factions, Settlements.

QRF capture integration observes canonical ownership events without changing their rules. Settlement and outpost responses have separate scenario opt-ins. Military capture sites can queue the previous AI owner's response; ordinary/military outposts and airports can serve as friendly dispatch/return origins. Role admission, money and operation limits remain authoritative; registration and load provide a baseline rather than triggering a response.

### Settlements — In progress
Purpose: towns and cities as places with people: population, and how much each role is backed there (support), which decides who holds them (see Location Capture).

- Towns and cities only: `AROR_SettlementComponent` on the Town and City prefabs holds the **population** (set per placed location) and the **support**.
- Support is a percentage per role that **always sums to 100**. Raising one role lowers the others in proportion.
- A new campaign starts at **100% for the location's initial controlling role**.
- When a role other than the controller goes **above 50%**, control flips to it; otherwise the current controller keeps it.
- Server-owned, replicated, raised as a "support changed" event, and saved per location (only when it differs from the starting state).
- What changes support in play is still to be designed; for now the framework (server API) plus admin-menu buttons for testing.

Depends on: Locations, Factions, Persistence.

### Game Flow — In progress
Purpose: the lifecycle from world boot to play, separating **scenario data** (from the world, never saved) from **playthrough data** (from a save).

Phases (mapped onto the base-game game mode states PREGAME → GAME → POSTGAME):
1. **Scenario load**: world boots, locations register, base location markers appear. No save involved.
2. **Setup** (PREGAME): the **server admin** sees the setup screen. Options: create a new playthrough (custom settings + pick the start location on the map) or load an existing one. Non-admin players see a "waiting for admin" screen. A server config fallback (auto-load latest / named save / new with defaults) lets dedicated servers start unattended.
   - **Setup UI** is built from base-game **configurable dialogs** (`SCR_ConfigurableDialogUi`) using our own dialog presets config, content layouts and handler classes. No base-game config (e.g. the menu registry `chimeraMenus.conf`) is overridden, so it cannot conflict with game updates or other mods. `AROR_SetupUIComponent` (client, on the game mode) drives it: "New" opens the base-game map menu, the admin clicks the HQ position and a dialog confirms it (validity checked client-side, re-checked by the server); "Load" steps through the saves sent by the server. Presets live in `Configs/UI/AROR_SetupDialogs.conf`.
3. **Playthrough load**: new → generate fresh state from settings + start location; load → restore state from the save. State markers are drawn. Transition to GAME.
4. **Running**: state changes are autosaved (see Persistence).

Pre-game is held by `AROR_GameFlowComponent`, our pre-game state component on the game mode (the base game skips PREGAME unless such a component exists); it allows the move to GAME only once a playthrough has been created or loaded, and keeps player controls disabled until then.

**Staging spawn** (Antistasi-style): each map places one `AROR_StagingSpawn.et` (spawn point + `AROR_StagingSpawnComponent`). While no playthrough exists, players spawn there as the Resistance and can move freely; the admin picks the HQ from there. Once a playthrough exists (new or loaded) the staging spawn is disabled; on a new playthrough every player is teleported to the HQ (keeping their gear).

**Resistance HQ**: the start choice. Placement rules (checked on the client for feedback, enforced by the server): on land, and at least a configurable distance (default 100 m) outside the radius of every location. On "New", the admin picks a spot on the map and the server spawns the Resistance HQ there. Players always spawn at the HQ. The HQ is a **dynamic location** (location type `HQ`): a spawned entity that carries the location component and the Resistance spawn point, so it shows up through the location registry and markers like any town, and is saved through entity persistence (*Register Dynamic*).

Settings:
- Chosen at setup, stored in the save, and **editable mid-playthrough** by the admin.
- Each setting is flagged *applies immediately* or *applies at the next cycle*; systems listen for a settings-changed event.
- Defaults and presets come from a core config that server admins and extender mods can override.

Server/client: the server owns phase transitions, settings and all generated state. Admin actions are client → server RPCs checked against base-game admin roles.

Depends on: Game Mode, Locations, Persistence.

Extension points: systems hook into lifecycle events (scenario ready, playthrough created, playthrough loaded, settings changed).

### Persistence — Planned
Purpose: unlimited, persistent playthrough saves.

- Built on the **base-game save system** (`SaveGameManager` plus entity persistence), not the community Enfusion Database/Persistence Framework mods, so the core mod depends only on the base game.
- Base-game model: a **playthrough** (one campaign) contains numbered **save points** (manual, auto, scripted, shutdown). Saves belong to a **mission**, so each map needs a mission header and its saves are kept separate automatically. Version compatibility checks come built in.
- **Limits**: the engine caps playthroughs and save points at about 101 each per mission, so old auto-saves need pruning. Saves carry no custom metadata, so a settings summary in the save list would need our own small index.
- **Boot rule**: if a save is active when the world loads, restore it and go straight to GAME (to verify); otherwise enter setup. Starting a new playthrough happens in the running world, with no reload (verified). It writes an initial save point straight away, so new and loaded games share one restore path.
- **Saving must be enabled for the scenario**: running the bare world in Workbench reports saving disabled. It is expected to come from the mission header (Save Types) and/or the world systems config (under investigation).
- **Where our data lives: inside the base-game save, via entity persistence. No separate save files.** Entities with the engine `Persistence` component are saved; the game mode already has it.
  - **Playthrough-wide state** (settings, start location, etc.) lives in our components on the game mode.
  - **Per-location state** (controlling role) lives on the location entities, which carry a `Persistence` component; `AROR_LocationComponentSerializer` saves it only when it differs from the initial role (first serializer).
  - Each `AROR_` component with saved state has a matching serializer (`ScriptedComponentSerializer`, as base-game components do). Serializers write a version number first, so older saves keep loading after the data changes.
- Locations are referenced by a **stable location ID**, never by list position or entity reference. Saved data for a location that no longer exists is skipped with a warning.
- **Each system owns its save section** via three hooks: initialise fresh / restore from save / write to save. Extender mods add saved data the same way.
- Save **generated results**, not random seeds, so saves survive map and code changes.
- Server-only writes: periodic autosave, on key events (e.g. town captured, settings changed), and on shutdown.

### Factions — In progress
Purpose: the sides in play, referenced by **role** so the actual factions are configurable.

Two layers:
- **Roles** (gameplay): `AROR_EFactionRole` = `RESISTANCE`, `OCCUPIERS`, `INVADERS`. Code only ever refers to roles, never to faction keys.
- **Factions** (data): identity (key, name, colours, side), relationships and entity catalogs (which characters, groups and vehicles belong to it). The side and the models are independent, so any side can use any army's assets.
- Players always join the Resistance role's faction; there is no faction choice.
- `AROR_FactionRolesComponent` (on the game mode) maps role → faction key. Defaults: Resistance → **`AROR_RESISTANCE`**, Occupiers → **`AROR_OCCUPIERS`**, Invaders → **`AROR_INVADERS`**. Extender mods can point a role at their own faction without touching the core factions or code.

Core factions (in `AROR_FactionManager.et`), all mutually hostile, only the Resistance playable (base-game US, USSR, FIA, CIV stay present but not playable):

| Faction | Side | Default assets |
|---|---|---|
| `AROR_RESISTANCE` | INDFOR | FIA catalogs (FIA as faction ancestor) |
| `AROR_OCCUPIERS` | BLUFOR | US catalogs (US as faction ancestor) |
| `AROR_INVADERS` | REDFOR | USSR catalogs (USSR as faction ancestor) |

- `Prefabs/Managers/AROR_FactionManager.et` (inherits the base-game `FactionManager_Editor.et`: US, USSR, FIA, CIV) and `AROR_LoadoutManager.et` (inherits the empty `LoadoutManager_Base.et`; holds only the Resistance loadout) are placed in every map as part of the map template.
- `Prefabs/Characters/AROR_Character_Resistance_Base.et`: the Resistance player character (inherits the base-game FIA rifleman, which carries a map and compass).
- Resistance loadouts: the Resistance role's loadouts are the only ones offered; each includes a map and compass.
- Extension points: extender mods or server admins swap factions by overriding the config.

### Commanders — In progress
Purpose: one server-owned command context for each campaign role. Resistance is player-directed; Occupiers and Invaders are independently AI-directed, using the same evaluation interface.

Key classes/prefabs/configs:
- `AROR_Commander`: role identity, configured faction key and player/AI control mode.
- `AROR_CommanderManagerComponent`: on `AROR_GameMode_Base.et`; resolves faction bindings from the same entity's `AROR_FactionRolesComponent`, owns the three contexts and periodically offers the two enemy commanders for strategic evaluation after campaign setup permits play.
- `AROR_EFactionRole`: shared role identity for future military and territory systems.

Server/client: contexts and evaluation run only on the server. No commander UI, player-order RPCs or replicated strategic state yet. Runtime contexts are rebuilt from scenario configuration and contain no mutable campaign state to save at this stage.

Depends on: Game Mode, Factions and Game Flow. Readiness temporarily uses Game Flow's existing transition gate; campaign lifecycle events should replace this adapter when available. This inherits Game Flow's current save/restore readiness limitations.

Extension points: inherited game-mode prefabs configure faction bindings on `AROR_FactionRolesComponent` and evaluation cadence on the commander manager. Server systems subscribe to the manager's typed evaluation event and receive the commander whose turn is being evaluated. The Operations system consumes this event for optional configured attack targets; autonomous target selection remains planned. Each role is evaluated independently; Resistance never receives automatic evaluations.

Sub-systems (Planned):
- Player-directed Resistance orders with server validation.
- Enemy target selection and distinct Occupier/Invader strategy policies.
- Garrisons and military resources; Operations provides attack/QRF scheduling, weighted force selection and optional ground execution requiring runtime verification.
- Versioned persistence and client summaries when mutable strategic state is introduced.

### Operations — In progress
Purpose: schedule enemy attacks and defensive QRF responses independently for Occupiers and Invaders, tracking each request through execution and a terminal outcome.

Key classes/prefabs/configs:
- `AROR_OperationManagerComponent`: on `AROR_GameMode_Base.et`; subscribes to commander evaluations, schedules optional configured location targets, and accepts trusted server attack reports for QRFs.
- `AROR_QRFDefenseComponent`, `AROR_QRFDamageObserver`: optional server damage monitoring on explicitly assigned defending groups, characters or vehicles. Validates defender faction, hostile instigator and location bounds, then reports to the scheduler. Scenario/garrison code supplies and clears assignments; this does not establish territory ownership. An opt-in scenario initializer can assign an initial faction once to an unassigned standalone defender before monitoring starts; it does not override existing affiliations or initialize groups.
- `AROR_Operation`, `AROR_EOperationType`, `AROR_EOperationState`: role-owned requests referencing stable location IDs; lifecycle is planned → spawning → active → succeeded/failed/cancelled.
- `AROR_ForceProfile`, `AROR_ForcePool`, `AROR_EForceType`: designer-authored role/faction pools with separate attack/QRF weights, eligibility/capacity filtering and one-time profile binding per operation. Unsupported flight policies are excluded from selection.
- `AROR_ResponseLevel`, `AROR_ResponseScale`: configurable response levels 1–5, each deploying that many complete force packages. Level weights are separate for attacks and QRFs; feasible level selection precedes weighted force-type selection so additional profile variants do not bias strength probabilities. Profile and level remain bound to the operation.
- `AROR_QRFSearchPlan`, `AROR_VehicleTownSearch`: a shared server tactical plan per QRF records time-limited attacker-position snapshots from validated damage reports, then assigns package-specific investigation/patrol sectors and spaced reachable road stops around the defended location. Legacy mounted searches freeze contact when searching starts; opt-in tactical execution creates fresh bounded plans from validated reports. No live attacker tracking or entity reference is retained. Searches are bounded by age, area, stop count and time, and resume normal deployment afterward. Native AI owns spotting/combat. Attacks retain their deployment policy; tactical QRF infantry uses shared investigation and sector orders. Coordinated search changes require runtime verification.

Server/client: all requests, scheduling and outcomes are server-only. No player RPCs or replicated operation UI. Each role has independent attack and QRF cooldowns and capacity; planned requests reserve capacity, with QRFs dispatched before attacks. Outstanding requests against the same target for one role are deduplicated. Timeouts and missing locations terminate requests, and retained history is bounded.

Depends on: Commanders, Game Flow readiness and the existing Locations registry. Territory/combat callers must establish target hostility or defensive ownership before submitting requests; Operations does not create a second ownership registry. Configured test attack targets bypass automatic target selection and must be appropriate for the attacking role.

Extension points: scenario prefabs configure schedules, limits, optional attack targets and force-profile arrays. Trusted server callers schedule attacks and report attacks against a defending role. `AROR_OperationExecutorComponent` provides optional server infantry and ground-vehicle execution, shared group/personnel/vehicle reservations, engine-queued members, activation acknowledgement and terminal cleanup. `AROR_GroundOperationForce` tracks separate crew/passenger groups, vehicles and their orders: motorized deliveries leave the vehicle at the dismount point, mechanized deliveries issue separate vehicle support orders, and armour remains mounted. Arrival does not change ownership or declare success. An empty pool retains the earlier infantry resource fields for compatibility.

Everon_Chicken inherits the same shared AROR role mappings and Resistance loadout as original Everon. Profiles distinguish the campaign faction from a declared native asset faction, allowing newly owned US/USSR assets to adopt their role faction once during spawning; unexpected occupied-vehicle and personnel affiliations remain validation errors; a proven empty delivered transport may retain its native empty affiliation. Empty standalone defenders can resolve their initial faction from the shared defending role. It contains prepared US/USSR motorized, mechanized, wheeled-armour and airborne profiles using verified installed base-game resources. Airborne profiles include transport and gunship resources but stay disabled/excluded from native selection; the REAPER experiment explicitly selects airborne packages, using paid war intent when strategic selection is enabled and the Workbench response override otherwise. The original Everon world and shared game-mode prefab do not receive this test pool. Changing a role's faction requires matching profile configuration.

Multi-package execution reserves the complete response before member spawning. Packages deploy from separated launch positions and receive orders once the required initial wave or constructed reinforcement batch is ready; individual combat losses degrade the response without replacing packages or failing surviving elements. Terminal cancellation, failure or timeout releases virtual reservations and retires operation-owned actors under the applicable retention/player-preservation policy. Ground route/convoy coordination is not automatic.

Composite QRF execution (In progress): `AROR_GroundQRFRecipe` defines an authored tier and ordered ground profile IDs resolved for a role/faction. One parent operation binds the recipe once, records tier separately from element count and reserves the complete mixed ground response. Required lead elements must be ready before activation; later tactical waves join the same active operation. Pre-dispatch losses fail the complete recipe; active losses leave survivors operating. Existing homogeneous profiles retain their multiplier semantics. Chicken now opts into strategic selection from five authored ground tiers per AI role; disabling that opt-in restores its fixed truck/IFV fixture. REAPER dispatch and Invader admission remain disabled. Native logs verify mixed spawning, boarding, factions, activation and truck delivery; subsequent source changes still require compilation and runtime acceptance. The eight-family roster in `docs/QRF_RESPONSE_ROSTER.md` remains largely planned; mixed ground/air children, stock-aware origins and the physical inventory reserve/commit/release contract remain future work. The money-backed selector below does not introduce a garrison or military stock provider. See `docs/QRF_STRATEGY_ECONOMY.md`.

QRF tactical command and staged waves (In progress): `AROR_QRFTacticalCommander` is per-operation server/session state shared by both opposing roles. It coordinates bounded report-driven investigation, survivor search sectors, ground delivery and separate protected vehicle support, air-inserted infantry tasking, finite reinforcements and ownership/casualty/task-duration withdrawal through the existing executor. Native group AI and REAPER retain navigation, perception, combat and flight. Optional large-response wave partitioning holds later elements as actor-free reservations against the complete original budget; type-balanced lead waves dispatch first, with later construction gated by time, losses and executor-wide live limits. No element respawns beyond the bound response. Terminal ground forces retain nearby casualties/survivors and budget until age and all-player distance conditions allow cleanup. The feature defaults off for other scenarios and is enabled in Chicken; Invader admission remains disabled. Resource-provider integration, durable persistence, mixed-domain recipes and native performance/multiplayer acceptance remain outstanding. See `docs/QRF_TACTICAL_COMMANDER.md`.

Invader framework (In progress, dispatch disabled): USSR-backed `AROR_INVADERS` uses the same ground/air profile, faction, reservation, search, outpost-origin and cleanup paths as Occupiers. Chicken stages five authored ground tiers using Ural/BTR/BRDM elements and a role-specific recipe override without changing the Occupier test. Mi-8 transport/gunship profiles remain prepared but unverified and excluded from native random selection. The operation manager now defaults Invader operation admission off, explicitly off in Chicken: scheduled attack, trusted QRF and damage-report request paths cannot create Invader operations while disabled. No territory/factory trigger, Invader defender placement or resource production is added. Territory ownership and campaign resource-provider integration remain future work. See `docs/INVADER_QRF_FRAMEWORK.md`.

Resource-aware QRF preview (In progress, not connected to dispatch): `AROR_QRFSelectionPolicy` evaluates complete ground recipes against commander-supplied desired/minimum tiers, a location ceiling, downgrade permission, route/ETA and expiring role/faction readiness snapshots. It checks repeated profile quantities and total personnel/group/vehicle capacity. `AROR_QRFReadinessProvider` is a registration seam for the military-resource owner, with no default stock or economy implementation. The executor preview validates an Invader-controlled target and friendly outpost origin, caps copied availability against current runtime reservations and returns a feasibility decision with a reason. No preview caller, automatic threat assessment, stock reservation/commit, operation binding or spawning is installed; Invader dispatch remains disabled. See `docs/QRF_SELECTION_PLANNER.md`.

QRF origins: ground and airborne execution select the nearest same-role-controlled OUTPOST, MILITARY_BASE or AIRPORT location to the defended target that passes existing launch-distance checks. The same type policy applies to readiness preview, origin binding and return retargeting. Configured role origins remain the fallback when no eligible site exists. Chicken now places ordinary/military outposts through its authored catalogue; garrison, physical stock and dedicated launch-point authoring remain separate work.

Location capture/defense response (In progress): assigned defender damage requests defense QRFs. The server capture observer subscribes to canonical ownership events after campaign readiness and can request a response for a displaced hostile AI owner. Settlement and military-site capture responses have separate scenario opt-ins, enabled in Chicken. Initial/restored ownership establishes a baseline rather than a historical attack. Role hostility, money, cooldowns, deduplication, capacity and the disabled Invader gate remain authoritative. The executor investigates/holds without directly assigning ownership; existing capture/support rules handle actual control, including opt-in settlement flag/weighted recapture. The military-site adapter covers outposts, bases, airports, factories, ports and resources. This is session-local and requires native verification.

QRF strategy and money (In progress, runtime acceptance pending): the operation manager optionally coalesces trusted threats per role/location, ranks location importance, sustained reports, capture and bounded waiting age, and admits responses through existing cooldown/capacity/role gates. Independent saved war pressure selects a desired tier, with bounded variation/escalation and affordable downward selection before binding. The executor checks complete authored recipes or airborne pairs against both money and actor budgets. `AROR_QRFFinance`/`AROR_QRFFunding` reserve session collateral against the existing Economy balance, debit one activation overhead and charge individually observed deaths, destruction or faction capture once. Normal retirement releases collateral without a loss debit; virtual reserves never count as physical losses. External spending shortfalls become saved QRF debt, serviced from future funds before new responses. Economy remains the only bank and income authority. The manager serializer saves war pressure/debt; operations, actor claims, queues and contacts remain session-local, so durable deployment restoration is not implemented. Normal ground recovery requires a physical return to a friendly origin; nearby actors still follow retention/cleanup rules. Extenders tune profile prices, manager policy and recipes. See `docs/QRF_STRATEGY_ECONOMY.md`.

Persistence: operation IDs, physical deployments, queues, cooldowns and outcomes remain session-local. Only war pressure and outstanding loss debt are restored by the operation-manager serializer; bank debits use Economy persistence. Native versioned deployment persistence must be added before active operations are treated as durable campaign state.

Sub-systems (Planned):
- Infantry and ground-vehicle execution are in progress and require Workbench verification. Launch positions/routes are designer-selected; automatic route/LZ validation, airborne flight and force handoff after success remain planned. Terminal cleanup retires operation-owned soldiers, corpses, vehicles and waypoints, preserving player-controlled entities/occupied vehicles. Engine-dormant groups retain their operation and reserved budget rather than counting as defeated.
- Hostile-damage reporting on explicitly assigned defenders is implemented pending Workbench verification. Authoritative ownership checks, capture invalidation, wider threat detection and restored garrison assignments remain responsibilities of combat/territory integration.
- Autonomous target selection and military-resource spending; executor capacity reservations exist, while economy costs and strategic availability remain planned.
- Save/restore and client summaries.

Development validation: Everon_Chicken has an `AROR_OperationTestComponent` for one-shot simulated QRF reports or scheduler/force-pool/response-scale smoke tests in Workbench. The current combat fixture places explicitly assigned US defenders in Provins, disables simulated reports and uses explicit profile/strength overrides for repeatable tests; clearing the executor overrides restores randomized supported profiles and levels. Chicken places the base-game Everon AIWorld prefab with explicit soldier, vehicle and low-resolution navigation bindings to installed Everon resources. The bindings use Bohemia's sample-world structure and require a fresh runtime check, because the prefab alone initialized empty navigation worlds in the earlier test. General test-component defaults remain inert; its simulated/smoke-test execution cannot run in packaged builds. Live logs verify hostile-damage reporting, QRF dispatch, motorized profile selection, spawning five personnel, boarding and activation. Driving, dismount and complete response execution remain unverified. The placed defense component does not establish territory ownership. No test executor is added to the original Everon world or shared game-mode prefab.

Airborne execution experiment: the normal project directly loads REAPER on this experimental branch; Chicken currently disables the adapter for its truck/LAV fixture, while other executors default to ground behavior. Shared Operations/factions/reservations own one to five complete transport/fireteam/gunship packages, honoring the Workbench response-level override (one package by default outside that override). The entire fleet is reserved before entity spawning; cached incremental planning selects separated clear landing sites across the town vicinity for individual transports. Aircraft construction is spread across executor ticks, with airframes held against falling during asynchronous boarding; all required lead packages must be boarded before activation; tactical reserves remain unspawned until admitted and later batches board before receiving orders. Transports queue outside the town vicinity, commit directly to a cleared landing corridor without a mandatory approach-hover stop, unload after verified stable touchdown, and depart as soon as living passengers have exited. Separated pads permit concurrent landings; only transports assigned overlapping corridors queue. A partial landing-site result is reused locally without reducing response strength or rejecting the entire fleet. Blocked pads receive package-local incremental replacement searches that respect other reserved sites and share a bounded fleet-wide search budget. Active package losses preserve other packages. Launch lanes, altitude bands, gunship pass phases and infantry patrol sectors are separated by package. Only one airborne operation can dispatch at a time. Server coordination searches bounded nearby clear landing candidates, guides an offset ingress, verifies physical touchdown/passenger exit, then sends infantry to shared reported-contact/town search and aircraft to support/egress. REAPER owns flight and perceived-target acquisition; an additional engagement policy requires campaign hostility, confines attacks to the defended vicinity and excludes nearby non-hostiles. Pilot/aircraft losses degrade individual elements without immediate package deletion. Departing aircraft are removed only away from every player; terminal forces retain casualties/survivors and their capacity until minimum age and player-distance conditions permit cleanup. This retention is session-local, with world-unload cleanup, not saved persistence. The first run verified basic flight, landing and unloading and exposed friendly damage/immediate cleanup; the revised LZ, formation, engagement, departure and retention behavior requires fresh Workbench verification. An off switch restores ground dispatch but does not remove the REAPER dependency. A subsequent level-5 run logged fleet activation and stable touchdown, but Jasper observed excessive transport hovering. Separate landing sites, concurrent clearance, remote holding and faster insertion/departure tuning now require fresh compilation and flight verification; overall fleet performance still needs measurement. Full flight-path validation and durable saves remain planned. The optional project retains only a disabled standalone diagnostic. See `docs/AIRBORNE_QRF_PROTOTYPE.md`.

Air support reporting and traffic coordination (in progress): validated hostile damage from assigned defenders and operation-owned units feeds a bounded, role-scoped contact registry, independent of QRF admission/cooldowns. Active gunships investigate recent nearby report snapshots and fly changing search passes; expired reports return them to town search. Native perception and campaign hostility still govern weapons. Legacy infantry retains its frozen ground search plan; opt-in tactical infantry can retask from fresh local snapshots and redistributes survivor sectors. Aircraft route bands separate gunship and transport traffic; gunships predict short-term proximity and yield to transports or higher-priority gunships through bounded waypoint maneuvers. This reduces conflict risk but does not replace native flight control or certify collision avoidance. Contacts and flight coordination are server/session-local and are not saved. Support deadlines and all-player cleanup checks remain in force. Separate-pad landings were reported working by Jasper; the new report/search/traffic/outpost integration requires compilation and runtime verification. See `docs/QRF_AIR_SUPPORT.md`.

Town security lifecycle (In progress): tactical QRF survivors investigate and hold bounded town sectors for a configurable period starting on physical entry, independently of travel time and location ownership. Ground packages resolve clear nearby road launch pads from their strategic origin and verify native seat occupancy before dispatch. Infantry orders remain gated during mounted ingress; early fully dismounted survivors retain their investigation task. Stalled ground vehicles receive bounded road-route retries. Fixed security mode suppresses casualty-driven tactical withdrawal and keeps unloading retries local to the delivery point, while target/origin safety, total-force losses and an operation-wide timeout remain authoritative. This is server/session-local and does not implement a post-recapture ownership timer or saved occupation. Native route, unloading and full-duration hold acceptance remain pending.

### Map Markers — In progress
Three parts:
1. **Location markers** (scenario load): **our own icon widgets** drawn on any open map (player map, deploy screen, setup screen) by a client-only component that follows the base-game map events (open/close/pan/zoom) and positions icons from world coordinates. Each location prefab chooses its icon image (white on transparent, so it can be tinted). No base-game config overrides, not draggable, no replication or saving. The HQ also keeps its spawn point map descriptor for the deploy screen.
2. **State** (playthrough load): location state is shown on the **same icon**, e.g. a town icon tinted by its controlling role (the faction's own colour, so it follows the Factions config). The control state is server-owned, replicated and saved on the location; each client re-tints its icon when it changes.
3. **Info popup and selector**: hovering a location icon shows pulsing corner brackets (steady on the selected icon); clicking it opens an info card next to it with name, type and controlling faction; settlements also show population and support per faction. It follows the icon, updates live when control or support changes, and is suppressed while a map click is used for something else (HQ pick, teleport). Client-only.

QRF spotting markers (In progress): optional server visibility checks against operation-owned vehicles create bounded last-seen reports only after living Resistance players observe them. Reports are delivered through owner-targeted player-controller RPCs exclusively to Resistance recipients. Client map widgets show type, reported position and age, then fade/expire without following unseen entities. QRF execution, the shared role mapping and existing map events provide the integration points. This intelligence is session-local and separate from location ownership; native visibility, head-direction replication and multiplayer acceptance remain unverified. See docs/QRF_SPOTTING.md.

### Economy — In progress
Purpose: money and manpower for the campaign: balances, and periodic income from held locations. Optional QRF strategy consumes the existing server spending API for deployment overhead and observed asset losses; general purchasing rules remain future work.
Key classes: `AROR_EconomyComponent`, `AROR_EconomyComponentSerializer`, `AROR_IncomeComponent` (all on the game mode).

- **Faction funds**: one bank balance per faction role (Resistance, Occupiers, Invaders), starting from per-role base values set on the component. Server-owned, replicated to every client, with a funds-changed event.
- **Manpower**: a Resistance-only balance with a starting value. Server-owned, replicated to every client, with a changed event.
- **Player money**: personal money per player, starting from a base value the first time a player joins the campaign. Server-owned; each player is sent only their own balance (through the player controller). Keyed by the player's identity ID so it survives reconnects and restarts.
- **Income** (server, every 15 minutes, not during setup): each faction role is paid for the locations it controls.
  - Money = held Resources × $500 × (1 + 0.3 per held Factory + 0.5 per held Port), plus $5 per inhabitant of held towns and cities (not multiplied).
  - The Occupiers and Invaders also receive fixed **external support** each payment ($500 each), regardless of what they hold.
  - Every online Resistance player is paid 10% of the Resistance's money income as personal money, on top of (not taken from) the Resistance funds.
  - The Resistance also gains manpower per held town and per held city.
  - Resistance players get a dispatch card notification with the amounts.
  - Rates and the interval are attributes on `AROR_IncomeComponent`.
- **Persistence**: funds, manpower and player money are saved with the game mode (nothing is written while everything is still at its starting values). Income timing is not saved.
- **Rules**: balances never go below zero; spending only succeeds if the balance covers it. All changes happen on the server.
- Extension points: other systems and extender mods call the server API to add, spend and read balances (`AddFactionFunds_S`, `SpendFactionFunds_S`, `AddPlayerMoney_S`, `SpendPlayerMoney_S`, `AddManpower_S`, `SpendManpower_S`) and can trigger a payment (`AROR_IncomeComponent.PayIncome_S`).

### Phone — In progress
Purpose: an in-game phone that hosts player-facing apps. Client only.
Key classes: `AROR_PhoneMenu` (menu, preset `AROR_PhoneMenu`), `AROR_PhoneComponent` (game mode; keybind), `UI/Layouts/Phone/AROR_PhoneMenu.layout`, textures in `UI/Textures/Phone/`.

- **Opening**: a rebindable input action (`AROR_OpenPhone` in its own `AROR_PhoneContext`) opens and closes it (Ctrl+Y by default), for every living player, when no other menu is open.
- **Menu**: registered through `chimeraMenus.conf` and the input config via **Override in addon**, which layers our entries on top of the base configs rather than replacing them.
- **Look**: a phone frame image over a wallpaper, with a status bar (in-game clock, signal, battery). It slides up from the bottom when opened.
- **Orientation**: apps open in portrait or landscape. Landscape apps turn the whole phone 90 degrees and scale it up, with the frame staying on; app content is hidden while it turns and laid out for the new size afterwards.
- **Apps**: launched from tiles on the home screen; the frame's home button returns to the home screen. **Bank** (portrait) shows the account holder, the player's personal money and the Resistance funds live from the Economy; transfers are planned. A landscape test app exercises the turn.
- **Esc** closes the phone from any screen.
- Extension points: planned app registration so extender mods can add apps.

### Notifications — In progress
Purpose: tell players what is happening in the campaign, plus quiet personal feedback. Anything can send: core systems, extender mods, admin tools.
Key classes: `AROR_NotificationManagerComponent` (on the game mode; static sending API), `AROR_Notification`, `AROR_NotificationDisplay`, `UI/Layouts/Notifications/AROR_Notifications.layout`.

Three kinds:
1. **Headline**: a full-width strip across the upper third, for major campaign events (e.g. HQ established).
2. **Notification**: a dispatch card in the top right with a coloured icon block, a small label set by the sender and the text, for routine events (e.g. location captured).
3. **Hint**: a pill at the bottom centre, for personal feedback (e.g. why an action failed). Its colour is optional (amber by default).

- **Server/client**: headlines and notifications are sent by the server only, to everyone (broadcast) or to one player (through the player controller). Hints are shown locally by a client, or sent by the server to one player when it rejects an action. Drawing is client-only, never on a dedicated server.
- **Queue**: headlines and notifications share one queue and show one at a time; headlines jump ahead of queued notifications. No cap. Hints skip the queue; a new hint replaces the current one.
- **Who sees them**: headlines and notifications show for every connected player, alive or dead, and over the map. Hints are dropped while the player is dead or not spawned. No replay for players who join later. Not saved.
- **Content**: text (English, the same for everyone), an optional icon and accent colour (e.g. the faction's), and a UI sound (one default per kind, which senders can override).
- Extension points: extender mods call the static API (`Send_S`, `SendHint_S`, `ShowHint`). Audiences beyond everyone and single players (roles, areas) and a notification log come later.

### Admin Tools — In progress
Purpose: in-game testing and admin tools, opened with **Ctrl+T** (`AROR_AdminMenuComponent` on the game mode, client only).

- Currently available to **every player** (an Enabled switch on the component turns it off; restricting it to admins comes later).
- **Teleport**: pick a point on the map; the player's own machine moves its character (movement replicates).
- **Game Master**: the server unlocks the base-game Game Master (Edit) mode for the player.
- **Support**: adjust the nearest settlement's support per faction (server-side).
- **Test notifications**: sends a test headline and notification to everyone and a test hint to the player.
- **Economy**: a sub-menu showing faction funds, Resistance manpower and the player's money, with buttons to give money to each faction or to the player and to pay income now (server-side).
- Built on the same base-game dialogs as the setup UI; no base-game config overrides (the hotkey is read directly from the keyboard).

### Map Templates — In progress
Purpose: per-map setup of the game mode, as a sub-scene of the terrain's world.

- Core ships a template layout: game mode entity (with `AROR_LocationManagerComponent`), starting bases, and a `Locations` layer.
- One sub-scene per map with its own mission header. First map: **Everon** (`Worlds/AROR_Everon/`).
- Extender map mods create a sub-scene of their own terrain and place location prefabs; no code needed.

<!--
### System name — Status
Purpose: one or two sentences.
Key classes/prefabs/configs: `AROR_...`, `Prefabs/...`, `Configs/...`
Server/client: what runs where and what is replicated.
Depends on: other systems.
Extension points: what extender mods can override or add.
Sub-systems:
- ...
-->
