# Architecture

Tracks the architecture of Open Resistance: its systems, sub-systems, and how they fit together. Update this file whenever a system is added, removed, or changes shape. Keep it high level; implementation detail, test results and tuning notes belong in code comments, commit messages and `docs/`.

Status tags: **Planned** (designed, not built) · **In progress** · **Implemented**.

## Mod structure

### AR-Open-Resistance (CORE MOD)

All the scripts and functionality, plus templates for base-game assets.

- Lives in this repo: `AROpenResistance/Open Resistance/` (addon ID `OpenResistance`, GUID `6AA282336705EED2`).
- Class prefix: `AROR_` for all core classes, enums, and globals (e.g. `AROR_LocationComponent`). Reserved for the core mod.
- Intended to depend only on the base game (`ArmaReforger`, `58D0FB3206B6F859`). **Current exception:** it also depends on the REAPER helicopter mod (`69A4D664A6284E06`) for the airborne QRF experiment. This must be moved out (e.g. into `OptionalAddons/OpenResistanceREAPER`) before a base-game-only release.
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

Optional addons maintained in this repo under `OptionalAddons/` follow the same rules. `OpenResistanceRadio` (see [Radio](#radio--in-progress)) is one, kept out of the core so the mission does not have to ship its audio.

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

### Base-game config overrides

The core adds entries to two base-game configs with **Override in addon**, which stores only our additions on top of the base file: `Configs/System/chimeraMenus.conf` (the phone menu) and `Configs/System/chimeraInputCommon.conf` (the phone keybind). `Prefabs/Vehicles/Core/Vehicle_Base.et` is overridden the same way, for the Radio controls. Everything else uses our own configs.

### UI design system

All of our UI (HUD, notifications, map popups, phone apps) follows the Open Resistance UI design system, summarised in [docs/UI_DESIGN.md](docs/UI_DESIGN.md): Roboto fonts from the base game, colour tokens in `AROR_UIColors`, and one name per thing (Readiness, Crackdown, REBINTEL, HICOMM). Colours come only from `AROR_UIColors`.

## Systems

Add each system here as it is designed or built, using the template at the end of this section.

### Game Mode — In progress
Purpose: the game mode entity every map places; all core systems hang off it as components.

- `Prefabs/GameModes/AROR_GameMode_Base.et` inherits the base-game `GameMode_Plain.et` (`SCR_BaseGameMode`); core system components are added to it.
- Server/client: standard `SCR_BaseGameMode` replication.
- Extension points: extender mods inherit `AROR_GameMode_Base.et` to add, configure or swap components.

### Locations — In progress
Purpose: named areas on the map that other systems use to decide where things happen. World-placed types: **Town, City, Airport, Factory, Military Base, Port, Heliport, Resources, Outpost**; spawned at runtime: **HQ**.

- `AROR_LocationComponent` on each location: stable ID, name, type, radius, map icon and initial controlling role.
- `AROR_LocationManagerComponent` on the game mode: registry of all locations, with registered / unregistered / control-changed / support-changed events. Other systems query locations through it.
- Templates: `Prefabs/Locations/AROR_Location_Base.et` and one prefab per type; the military types carry a capture flag.
- **Control**: every location is always controlled by a role (Resistance, Occupiers or Invaders). Server-owned, replicated, saved per location (`AROR_LocationComponentSerializer`). The Resistance HQ is fixed to the Resistance.
- Server/client: placed locations are static world entities and the HQ is a replicated dynamic entity; both register on server and clients.
- Saved: controlling role, referenced by stable location ID.
- Depends on: Factions, Map Templates (placement).
- Extension points: new types via `modded enum AROR_ELocationType` (values are appended so existing ones stay stable) plus a prefab inheriting `AROR_Location_Base.et`; extender maps place location prefabs in their own sub-scene.
- Planned: hand-placed child points (spawn, garrison, objective); default names from base-game map descriptors.
- Everon campaign data: a catalogue of 41 stable locations (18 settlements, 23 military installations) generated into the Chicken world layers by scripts in `tools/`, plus hand-placed seaports (Lamentin, Saint-Philippe, Morton) and the Invader stronghold. See `docs/EVERON_CAMPAIGN_LOCATIONS.md`. Chicken has since moved the fight into towns (see Towns): its 18 settlements carry garrison posts, and only 3 of the 14 outposts remain (Wolf Wood, Beacon Point and Old Sawmill, now heavier military bases).

### Strongholds — In progress
Purpose: a faction's home base, built and defended so that the Resistance cannot take it with an early rush. Chicken has two: **Fortress Vostok**, the Invaders' starting base at Saint-Pierre harbour, and **Stronghold Delaware**, the Occupiers' home base at Everon International Airport.

- `AROR_StrongholdComponent` on the stronghold's location entity. Vostok is a `MILITARY_BASE` with a level-4 garrison (`AROR_Defence_Fortress_Vostok.et`, generated by `tools/Set-FortressVostok.ps1`). Delaware extends the airbase compound with an outer wall, extra guns and teams, and three unmanned helicopters on pads (`AROR_Defence_Stronghold_Delaware.et`, generated by `tools/Set-StrongholdDelaware.ps1`).
- Only its home role or the Resistance can take it (`AROR_LocationComponent.CanBeTakenBy` asks the component).
- **Retaliation**: during an early-game window (campaign time with players online), defenders killed by players are counted; at the threshold the home role sends a scripted level-5 response in two waves, and the count restarts.
- **Final defence**: after the window, a Resistance assault (a player inside the radius or a player kill) draws a scripted level-5 combined-arms response, re-armed after a cooldown once it ends.
- Scripted responses launch from the home role's best friendly base when the stronghold allows it (Delaware), otherwise from the coast (authored landing points, else a bounded coastal search), and run through Operations with funding waived (see Operations).
- Server/client: all logic is server-only; clients only see the replicated location and the headlines.
- Saved: early-window progress and the player kills counted towards the next retaliation (`AROR_StrongholdComponentSerializer`). Live responses are session-only.
- Depends on: Locations, Location Capture, Garrisons, Operations, Notifications.
- Extension points: every threshold, recipe, level and landing setting is an attribute on the component; extender maps place the composition (or their own) with the component and an AI home role.
- Not built: a campaign end when a stronghold falls. Feature detail: `docs/STRONGHOLDS.md`.

### Location Capture — In progress
Purpose: how a location's controlling role changes. All changes are made by the server through the location's control; the HQ cannot be captured.

- **Towns & Cities**: held by the role with **more than 50% support** (see Settlements).
- **Military locations** (airport, factory, military base, port, heliport, resources, outpost): captured at the location's **capture flag** by interaction, only when **every hostile** inside the radius is dead.
- **Settlements are support-only**: towns and cities reject flag capture and AI recapture; they change hands only through support (see Settlements). The one exception is an AI attack order on a town (Invader campaign): while an outstanding attack operation targets it, AI recapture weighs the town like a military site (over a longer contest time) and the winner occupies it by having its support raised past half (`OccupyTown_S`), so control still flips through Settlements. While an attack order is on a Resistance-held town, the contest (both counts and the time left) is mirrored onto the town's replicated lockdown state, so defenders see it on the Towns HUD indicator. A town with a settlement post (Garrisons) has a flag the Resistance can take once the post area is clear: it swings Resistance support by a set amount (which may flip the town through Settlements) and breaks the post, never changing control directly.
- **Flip cooldown**: after a Resistance capture, the location cannot flip back to the Occupiers or Invaders for **5 minutes**. Replicated, not saved.
- **AI recapture** (`AROR_RecaptureComponent`, server): every few seconds the forces inside each capturable location are weighed; defenders count ×2 and attacking AI ×1. An attacking AI role that outweighs the defenders takes the location, subject to the cooldown.
- **Aircraft occupants never count**: characters seated in a helicopter are ignored by both the flag's hostile check and AI recapture, so a passing or landing aircraft cannot block or flip a capture.
- **Capture flag placement**: optionally, the server moves a capture flag to a clear, walkable spot near the location centre when play starts ("Auto Place"), and replicates the result. At a town or city with a settlement garrison post it first looks for room for the post's building, raises the holding army's building there (server-spawned, replicated; swapped on a change of control) and stands the flag in front of it. The flag is a flagpole whose cloth shows the holding faction on every client.
- Key classes: `AROR_CaptureLocationAction` (user action on `AROR_Prop_CaptureFlag.et`), `AROR_RecaptureComponent`, `AROR_CaptureFlagPlacementComponent`.
- Server/client: clients check the rules only to show feedback; the server re-checks and decides.
- Depends on: Locations, Factions, Settlements. Operations reacts to capture events without changing these rules.

### Settlements — In progress
Purpose: towns and cities as places with people: population, and how much each role is backed there (support), which decides who holds them.

- `AROR_SettlementComponent` on town and city prefabs: **population** (set per placed location) and **support** per role, which always sums to 100%.
- A new campaign starts at 100% for the initial controlling role. When another role goes **above 50%**, control flips to it. Exception: while an AI army holds a town whose settlement garrison post (Garrisons) still stands, the Resistance cannot take control on support alone; taking the post's flag breaks the post and the flip happens then.
- Server-owned, replicated, raises a support-changed event, saved per location (`AROR_SettlementComponentSerializer`).
- **Regional support** (`AROR_RegionalSupportComponent`, optional scenario policy, off by default): the server gradually moves each settlement's support toward the share of nearby military installations each role holds. Uses the existing support and ownership authority; nothing extra is saved. Otherwise support changes only through the server API and admin tools.
- Depends on: Locations, Factions.

### Towns — In progress

Purpose: make settlements places worth fighting over: life in the streets, an occupying presence, and a fixed wave response when the Resistance attacks a town's garrison. Separate from QRFs.

- **Role doctrines**: town responses and physical town attack orders use rapid split assaults for Invaders and deliberate convoy approaches with covering and advancing elements for Occupiers. Both share expiring, server-validated damage-source reports through Operations; native AI owns individual combat. Doctrine settings and an opt-out live on the town component. Doctrine and reports are session-only. See [docs/TOWN_AI_REVIEW.md](docs/TOWN_AI_REVIEW.md) for the Workshop review and detailed behaviour.
- **Response lifecycle**: taking a post preserves its relief request across an immediate support flip. Ending a response cancels outstanding child operations through normal force retention. Town lockdown and attack-order occupation own their support outcome without additional QRF pacification.
- **Town waves** (`AROR_TownWavesComponent`, on a town or city): counts players' kills of the town's defenders (settlement post, checkpoint guards, patrol crews). At a share of the defenders seen (25% by default, at least 3), or when its post is taken, the response runs in phases: an alert (the first wave forms up after a short random delay), then three convoy waves a few minutes apart through Operations (Town wave responses). Readiness picks each wave's recipe (trucks at 1–2, mechanized at 3–4, armour at 5), Crackdown its size (cities and the last wave one more). Waves stage on roads 1–2 km out and dismount short of the post; each wave comes down a different road (axes rotate from the army's nearest base) and split into two columns when their role doctrine calls for it. A dedicated transport insertion with gunship support (a joint wave recipe, launched from the army's bases) is requested with the first ground wave, available from early Readiness. When every wave and the assault have ended the town holds: a Resistance support reward and an HQ headline. Players are told through a contact report naming who hit the garrison and a response report per wave (side and arrival estimate), not the QRF alerts. Only one town response runs on the map at a time; another town under attack gets none until it ends. It re-arms after a cooldown. Waves stand in for the capture QRF: a town with them draws no capture QRF.
- **Lockdown**: once the Resistance takes the post's flag during a response, the town counts the response's own reinforcements (never the garrison, checkpoints or patrol already there) against the Resistance's fighters within its radius. Outnumbered for a total time (10 minutes by default) and the army retakes the town: the post is restored and the Resistance loses the support gained since the contact (which flips the town back if that gain had flipped it). Leaving the town has the same result. The count and countdown are replicated from the town component and shown to nearby players on a HUD indicator (`AROR_TownContestHud`, driven by the notification manager's HUD loop).
- **Escalation**: every enemy the players kill in a town and every garrison post taken raise the army's Crackdown and Readiness pressure through Operations; see Operations → Town escalation.
- **Balance logging**: each response writes `[AROR BALANCE]` lines (start, waves, lockdown samples, escalation, outcome and its effect on support and both scales); `tools/Export-BalanceLog.ps1` turns them into CSVs.
- **Town life** (`AROR_TownLifeComponent`): built only while a player is near and removed when they leave. Always: civilians walking between street points (count from population; killing one costs Resistance support there) and unmanned civilian cars parked in the streets (players can take them). While an AI army holds the town: road checkpoints on the way in (sandbags and a guard team; more with Crackdown and in cities) and a crewed patrol vehicle looping the streets (its type from Readiness, a second one at high Crackdown). Wiped-out checkpoints and patrols stay gone for a respawn time.
- Server/client: server only; clients see replicated actors and headlines.
- Saved: nothing; kill progress, running responses and town life are session-only. The post's roster is saved by Garrisons.
- Depends on: Locations, Settlements, Garrisons, Operations, Factions (Civilians role preset for civilians and cars), Notifications.
- Extension points: every threshold, distance, count and prefab is a component attribute; wave recipes are authored per role and Readiness band in the scenario's executor.
- Feature detail: `docs/TOWN_WARFARE.md`.

### Garrisons — In progress
Purpose: defend flag-capturable military installations with a saved roster whose physical defenders spawn when players are near. HQs are excluded; towns and cities take part only through a settlement post.

- **Perimeters** (`AROR_PerimeterComponent`, Chicken's kept outposts): at play start the server raises high sandbag walls with two gates, barbed wire, gate hedgehogs and two corner towers around the compound, each piece snapped to the terrain and skipped where something is in the way; the props replicate themselves and nothing is saved.
- **Settlement posts** (`m_bSettlementPost`, scenario opt-in per town): the garrison holds the town's capture-flag spot, sized by the holding army's Crackdown when the roster is raised (cities one level more). Taking the flag breaks the post (nothing left deploys) and swings support (see Location Capture); after a re-form time, while the army still holds the town and nobody is near, a fresh roster is raised at the Crackdown of that moment.

- `AROR_GarrisonComponent` (on military location templates) owns the roster, losses and placement results; `AROR_GarrisonManagerComponent` (game mode) activates and retires defenders by player proximity and paces spawning. `AROR_GarrisonFaction` defines each role's requested static-weapon and vehicle types, resolved from the role's selected faction preset and enabled content packs; defenders and crews are faction preset units (Factions), a team's members come from its **formation** (see Factions -> Formations) and each saved roster variant is the member's unit type (tower sentries are marksmen; crew are crew).
- Defence layouts (`AROR_DefenceLayout`, `AROR_DefencePositionComponent` and the `AROR_Defence_*` compositions) define where defenders and vehicles stand. Static-gun markers may be raised onto towers or roofs, guard towers tagged as sentry posts each hold a marksman (his own group, saved through the infantry variant), tagged helipads hold parked helicopters on their decks, and a garrison may declare a rectangular footprint for a narrow site. Editor tools audit and ground these sites.
- Garrison catalogues can provide static weapon variants; authored recipes can override their count, add infantry teams and add unmanned parked vehicles such as helicopters (`m_iExtraInfantryTeams`, `m_aExtraParkedVehicleTypes`). Logical variants and losses use the existing roster save data. Optional equipped compositions carry weapon, ammunition, equipment and medical stores for Resistance looting. `AROR_OutpostSupplyComponent` stocks each store once from the roster army's faction preset when the roster is first generated (a saved roster never restocks), then the native inventory owns the items and their replication. The current store props are decorative and cannot hold items; they need replacing with an openable container. Native stock/depletion and inventory save/load acceptance remain pending.
- Server/client: roster, spawning and saves are server-owned; clients see the replicated defenders. A replicated gate stops capture while a defence is still deploying.
- Saved: the roster (what remains and what was lost) per location, through Persistence. Defenders are not refilled when players leave and return.
- Depends on: Locations, Location Capture, Factions, Operations (damage reporting).
- Extension points: scenario and editor settings (activation range, capacity, defence levels, layouts); role asset catalogues can be replaced for extender factions.
- Planned: restoring exact inventory and live positions, replenishment.
- Feature detail: `docs/GARRISONS_AND_REGIONAL_SUPPORT.md`, `docs/MILITARY_DEFENCE_PRESETS.md`, `docs/FORTIFIED_OUTPOSTS.md`.

### Game Flow — In progress
Purpose: the lifecycle from world boot to play, separating **scenario data** (from the world, never saved) from **playthrough data** (from a save).

Phases (mapped onto the base-game states PREGAME → GAME → POSTGAME):
1. **Scenario load**: world boots, locations register, location markers appear.
2. **Setup** (PREGAME): the server admin creates a new playthrough in steps (an army/faction preset per role and the content packs in play, campaign settings, the resistance name, then the Resistance HQ position on the map) or loads a saved one. A server config fallback for unattended dedicated servers is planned.
3. **Playthrough load**: new → fresh state; load → restore from the save. Then GAME.
4. **Running**: state changes are saved (see Persistence).

- `AROR_GameFlowComponent` (server, game mode) holds PREGAME until a playthrough exists and keeps player controls disabled until then; other systems can ask whether the playthrough is ready.
- **Setup UI** (`AROR_SetupUIComponent`, client): a full-screen setup menu (`AROR_SetupMenu`, registered in `chimeraMenus.conf`, drawn by `AROR_SetupApp` from the shared pooled-widget layout) with the start screen (new, load), the save list (one card per playthrough: its name, newest save time and save count; loading a card loads its newest save, and it expands to pick any of its saves) and the new campaign steps: **Armies and content** (a dropdown of presets per role; toggles for the content packs whose mods are loaded, at least one on), **Settings** (campaign settings by category) and **Name**. The choices are kept in a client-side draft (`AROR_SetupDraft`) until the HQ is confirmed on the base-game map (a configurable dialog, `Configs/UI/AROR_SetupDialogs.conf`), then sent in one request; backing out of the map or a server refusal returns to the name step with the choices kept.
- **Resistance name** (`AROR_ResistanceName`): up to 32 letters, digits and single spaces (default "Resistance"), checked live on the client and again on the server at setup; it cannot be changed afterwards. Save points are named after it (autosaves prefixed "Autosave - ", which no resistance name can start with); the load list shows each playthrough under the name of its newest save.
- **Campaign Settings** (`AROR_CampaignSettingsComponent`, game mode, every machine): categories of typed settings (on/off, choice, number) defined on the component; values are strings keyed by setting ID. The server sets them when a campaign is created (invalid or missing values take the default), replicates them and saves them (`AROR_CampaignSettingsComponentSerializer`); saved values for settings no longer defined are kept. Systems read values by ID and can listen for changes. No settings are defined yet. Extension point: extenders add categories and settings through game-mode prefab inheritance.
- **Staging spawn** (`AROR_StagingSpawn.et`): before a playthrough exists, players spawn there as the Resistance and move freely. Afterwards it is disabled and players are moved to the HQ.
- **Resistance HQ**: placed on land, outside every location's radius, and on a clear 10×10 m pad free of buildings and props (checked on the client, enforced by the server; invalid picks are rejected). A dynamic location of type HQ that carries the Resistance spawn point; players always spawn there. A new playthrough also spawns an arsenal beside the HQ via `SpawnArsenal` and a garage on the opposite side of the flag (same offset negated) via `SpawnGarage` before the initial save.
- Planned: campaign settings editable mid-game; lifecycle events (scenario ready, playthrough created/loaded) for other systems.
- Server/client: the server owns phase transitions and all generated state; admin actions are client → server requests checked against admin roles.
- Depends on: Game Mode, Locations, Persistence.

### Persistence — In progress
Purpose: persistent campaign saves, built on the **base-game save system** (`SaveGameManager` plus entity persistence), so the core needs no persistence mods.

- A **playthrough** (one campaign) holds numbered save points; saves belong to a mission, so each map needs a mission header. Saving must be enabled for the scenario (Workbench Play on the bare world has saving disabled).
- **Our data lives inside the base-game save, no separate files.** Entities with a `Persistence` component are saved; the game mode and locations have one.
  - Playthrough-wide state lives in our components on the game mode; per-location state lives on the location entities.
  - Each component with saved state has a matching `ScriptedComponentSerializer`. Serializers write a version first and keep loading older versions.
  - Locations are referenced by **stable ID**, never by position in a list.
- Save generated results, not random seeds, so saves survive map and code changes.
- Saved so far: location control, settlement support, the Resistance HQ, arsenals, faction presets per role, content pack overrides, campaign settings, the resistance name, faction funds, manpower, player money, Resistance item stock, Resistance garage vehicle snapshots, claimed Resistance vehicles in the world, per-player arsenal loadout slots, garrison rosters, QRF readiness, crackdown and debt, the Invader campaign state, stronghold state, active Resistance missions and the leaderboard.
- Boot rule: launching the mission always starts fresh with setup (saving stays off until the admin chooses). A save is restored only when the admin loads it from setup. If the game resumed a save by itself, the server reloads the mission once without it (requested after the game has started, since a world change during world init is ignored, and retried a few times); setup is held back until then. If the reload never happens, "new" is refused until a save is loaded. A new playthrough writes an initial save point (an autosave), so new and loaded games share one restore path.
- Limits: about 101 playthroughs and save points per mission, hence autosave pruning.
- Autosave (Game Flow, server): every few minutes (configurable) and on request from other systems after key events (none use it yet) a new save point named "Autosave - <resistance name>" is written. Only the newest few autosaves per playthrough are kept (configurable); the engine's own shutdown saves count as autosaves. After a load, a garage prop is respawned beside the HQ if none came back with the save.
- Manual saves: admins save from the phone's quick dock (slot 4); the save point is named after the resistance and never pruned.
- Planned: saves on other key events and shutdown; verification on a dedicated server.

### Factions — In progress
Purpose: the sides in play, referenced by **role** so the actual factions are configurable.

Three layers:
- **Roles** (gameplay): `AROR_EFactionRole` = `RESISTANCE`, `OCCUPIERS`, `INVADERS`, `CIVILIANS`. Code refers to roles, never to faction keys or prefabs.
- **Factions** (identity): name, colour, flag and relationships. `AROR_FactionRolesComponent` (game mode) maps each role to a faction key: `AROR_RESISTANCE`, `AROR_OCCUPIERS`, `AROR_INVADERS`, `AROR_CIVILIANS`.
- **Faction presets** (units and vehicles): the infantry and vehicles an army fields, independent of identity, so a role can keep its name and colour while using another army's units. Items (gear, weapons, ammunition) belong to Content Packs.

Core factions live in `AROR_FactionManager.et`. The three military factions are mutually hostile; Civilians are friendly to all. Only the Resistance is playable, and players always join it.

| Faction | Side | Default preset |
|---|---|---|
| `AROR_RESISTANCE` | INDFOR | `FIA` |
| `AROR_OCCUPIERS` | BLUFOR | `US_ARMY` |
| `AROR_INVADERS` | REDFOR | `USSR_ARMY` |
| `AROR_CIVILIANS` | civilian | `EVERON_CIV` |

**Faction presets — In progress**
- A preset (`AROR_FactionPreset`, one `.conf` each) lists what an army fields in two categories: **Units** (AI characters, at most one per unit type, each with its loadouts) and **Vehicles** (empty, no crew), plus an optional player loadout. Every entry is only a **prefab of a type** (e.g. RIFLEMAN, MOTOR_TRUCK); a vehicle type can have any number of entries. Items (gear, equipment, weapons, accessories, ammunition) are not in presets: they live in Content Packs, with their prices, readiness and import requirements. The category enum (`AROR_EFactionPresetCategory`) is shared with Content Packs.
- A vehicle's prefab, and every prefab in a loadout, is resolved through Content Pack **replacements**, so with a weapon mod's pack enabled an army whose kit lists the base-game M16 fields the mod's variant instead. A vehicle must also be offered by an enabled pack.
- Vehicle entries can specify a selection priority. Shared selectors choose among the highest-priority offered entries of the requested type in the role's selected preset; other entries remain same-army fallbacks. The combined WCS/RHS addon uses this to prefer its WCS combat vehicles while retaining complete RHS infantry kits and WCS specialist equipment. This policy is configuration-owned; the core has no WCS dependency.
- Presets only make things available; the systems that use them (garrisons, QRFs, missions, recruitment) decide what to pick. Operations and garrisons request vehicle types from the selected role preset, including static weapons, parked vehicles and airborne escorts. Missing types make operation profiles unavailable or omit garrison assets for that activation; they never fall back to another army. Vehicle faction affiliation is assigned to the campaign role once at server spawn. Existing garrison roster ordinals and losses remain saved; hulls bind to the selected preset on load. Each preset lists the roles it can be used for. A vehicle used for two jobs is listed once per type (for example a covered transport truck as both `MOTOR_TRUCK`, which troop transports request, and `SUPPLY_TRUCK`, which also holds arsenal, ammo and repair trucks). `tools/Test-ExecutorSettings.ps1` checks every force profile's vehicle type against every enemy preset its army can use.
- `AROR_FactionPresetRegistry` (`Configs/FactionPresets/AROR_FactionPresetRegistry.conf`) lists the core's presets (extenders add theirs from script, see below). The base-game presets (`FIA`, `US_ARMY`, `USSR_ARMY`, `EVERON_CIV`) are built into the core, written the same way an extender would.
- `AROR_FactionPresetsComponent` (game mode) loads the presets on every machine and answers queries for a role's preset. The server owns which preset each role uses: chosen by the admin per role when creating a campaign (defaults otherwise), replicated, and saved (`AROR_FactionPresetsComponentSerializer`). A saved preset that no longer exists falls back to the role's default.
- **Unit loadouts**: a unit entry can carry its full kit (`AROR_UnitLoadout`): clothing with its contents, weapons with attachments and loaded magazine, and loose items. A unit can have several loadouts as **Readiness variants**; the highest tier at or below the role's current Readiness is used, picked at random by weight among equals. With no loadouts the character keeps its prefab's own kit. Content Pack replacements apply to every loadout prefab when it is read; Content Pack readiness does not gate loadout items. A preset can also carry one **player loadout** (`AROR_PlayerLoadout`) in the same format but without Readiness or weight: the starting kit for players spawning in the role it serves (the FIA preset's is a copy of its rifleman kit, for the gear-less Resistance player character). The server dresses each spawning player who wears nothing from their role's player loadout (`AROR_LoadoutOutfitter`, on `AROR_FactionPresetsComponent`).
- **AI spawning from presets**: Garrisons, Operations, Missions and High Command ask for **unit types**, never character prefabs. Every AI soldier spawns as its role's gear-less **base character** (`AROR_RoleBaseCharacters`, `Configs/FactionPresets/AROR_RoleBaseCharacters.conf`: `AROR_Character_Resistance_AI_Base`, `AROR_Character_Occupiers_Base`, `AROR_Character_Invaders_Base`, keyed by role because one preset can serve several roles) and the server then dresses it from the unit's loadout at the role's current Readiness. `FillGroup_S` fills an AI group's slots and attaches an `AROR_GroupOutfitter` that dresses members as the engine spawns them, including members respawned when a dormant group wakes; `Outfit_S` dresses a single character. Missions and High Command create their groups through `AROR_ForceSpawner` (static server helpers: groups by unit type, vehicles, waypoints, clear and road positions, living counts), which always routes soldiers through `FillGroup_S`; addons that spawn AI (patrols, civilians, recruitment) should use it too. A unit's prefab is now only the importer's kit source; Civilians, which have no base character, still spawn as their preset's (randomized base-game) character.
- **Formations**: `AROR_Formations` names the groupings of unit types that garrisons and QRFs field (rifle, weapons, AT and AA fire teams; rifle squad), so what a team contains is decided in one place. Garrisons give each team a formation by index (`AROR_GarrisonPolicy.TeamFormation`); a QRF profile lists formations that are repeated over its passenger seats, the end cut off when the vehicle is too small.
- **Missing units**: when the role's preset has no unit of a type with a loadout (e.g. a mod army without pilots), the same type from the role's **configured default preset** is used, else that preset's rifleman; each stand-in is logged once. The core defaults are base-game armies; extenders can override them. REAPER airborne pilots stay REAPER's own characters.
- The base-game import plugin keeps each preset's existing units (prefabs and loadouts) and its player loadout and regenerates only the vehicles (it also sorts the base-game items, but only for the Content Pack export plugin). A Workbench-only option on `AROR_FactionPresetsComponent`, run in Play mode, reads the base-game infantry kits from spawned characters and writes them into the FIA, US and Soviet presets as readiness 1 loadouts, skipping units that already have loadouts.
- **Loadout capture** (Workbench only, `AROR_LoadoutCapture`): the Admin menu's Tools tab has a name field and a Capture Loadout button. The server reads the kit the admin's character currently wears and carries, with the same reading as the base importer, and writes it as a loadout block to `$profile:AROR_Loadouts/<name>.txt`, to be pasted into a preset unit with a chosen readiness. The character is never changed.
- Planned: outpost stores were stocked from the preset's items and are now unstocked until they get a new item source (Content Packs or the army's loadouts).

- `AROR_LoadoutManager.et` offers only Resistance loadouts; `AROR_Character_Resistance_Base.et` is the player character.
- **Preset additions** (`AROR_FactionPresetAddition`): an extender mod can add units and vehicles to a preset another addon owns, with a small config naming the target preset ID, registered from script by overriding `GetExtraAdditionPaths` on the registry. Any number of addons can add to the same preset. The preset applies them on load: its own entries come first, then additions in registration order, so a unit type the preset already has is never replaced (logged). An addition whose target preset is not loaded is skipped.
- Extension points: extender mods add presets (new `.conf` files registered from script by overriding `GetExtraPresetPaths` on the registry in a `modded class`, so several addons can each add several without sharing a config), give units loadouts in their own presets, and override the default unit characters config. The categories and their types are a fixed core list; entries with any other type are ignored. Server admins or extenders point a role at another faction key without touching core code.
- **Optional WCS/RHS integration**: the RHS compatibility addon registers its armies and content pack; WCS compatibility registers its catalogue. The combined WCS/RHS addon overrides the RHS armies' kits and vehicles and the game mode's enemy defaults, so new campaigns use those armies when the combined addon is loaded. Valid saved faction selections retain precedence. Resistance and civilian defaults remain core-owned. These armies combine WCS, RHS and base-game assets; packs make assets available, while presets decide what each role fields.

### Content Packs — In progress
Purpose: everything that can be bought or fielded, per content source (the base game, a mod such as RHS or WCS), with its economy data, independent of which factions are in play. With RHS enabled its gear is sold even when every faction uses base-game presets.

- A pack (`AROR_ContentPack`, one `.conf` each) holds **groups** (`AROR_ContentPackGroup`), each with the addons it needs and a list of entries. Entries cover the preset categories except Units, each a typed subclass (e.g. a Vehicle of type APC, a Weapon of type MARKSMAN_RIFLE), mixed freely in a group's list. Every entry has a prefab, a **base cost**, a **minimum readiness** (default 1), a **Resistance can purchase** flag (default on; off keeps it out of the shops but leaves it available to presets and enemy missions) and optional **import facilities** (a count of held seaports, heliports and airports together, plus optionally one held seaport or one held heliport/airport, counted by `AROR_ImportPoints`).
- **Compatibility**: weapon and vehicle entries have an optional, informational list of prefabs they work with (a weapon's magazines and attachments, a vehicle's ammunition and munitions); `AROR_ContentPacksComponent.GetCompatible` returns those an enabled pack offers. Nothing uses it yet; it is meant to feed shop display and AI loadouts, and later to be filled from the prefabs' own data.
- **Replacements**: an entry can list prefabs it replaces. While its pack is enabled, those prefabs are not sold and faction preset vehicles and loadout items listing them resolve to the replacer (chains followed).
- **Availability**: a pack names the addon GUIDs it needs and is only available while they are all loaded, so the core can ship packs for popular mods. A group names addons too and its entries are ignored (with what they replace) unless those are loaded, so one pack can span a mod family's optional sub-mods, one group per sub-mod. An available pack is on or off per campaign: its own default, overridden by the admin.
- `AROR_ContentPackRegistry` (`Configs/ContentPacks/AROR_ContentPackRegistry.conf`) lists the core's packs (`BASE_GAME`); extender mods add theirs from script by overriding `GetExtraPackPaths` in a `modded class`, so any number of addons can each register any number of packs without sharing a config file. Priority order: core list, then extender packs in script load order. `AROR_ContentPacksComponent` (game mode) loads them on every machine and builds one catalogue from the enabled packs (the earlier pack wins a duplicate prefab or replacement). Queries: enabled entries per category, an entry by prefab, a prefab's minimum readiness (1 if unlisted), replacement resolution.
- Server/client: the server owns the per-campaign overrides (packs turned off, or turned on when off by default); they replicate and every machine rebuilds the same catalogue. A new campaign starts with the defaults.
- Saved: the overrides (`AROR_ContentPacksComponentSerializer`). A saved pack whose mod is missing is kept in the save and simply unavailable.
- Used by: Black Market and Imports (catalogue, prices, import gate); the Resistance arsenal and the admin stock fill (every enabled entry); missions (an enemy role only fields prefabs its current readiness allows). Outpost stores will draw from packs once reworked.
- Extension points: extender mods register packs from script (`GetExtraPackPaths`) or ship packs that replace core prefabs.
- The admin chooses which available packs are on when creating a campaign (Setup UI); packs whose mods are not loaded are not shown. The pack list is a collapsed-by-default tree that scrolls: a pack expands to its **groups**, and a group to the **categories** (vehicles, gear, equipment, weapons, accessories, ammunition) it has entries in; any level can be switched off. A pack or group with a mix shows *partial* (yellow), and a warning explains that turning parts off can break things (e.g. an army whose vehicles are no longer offered). A turned-off group or category's entries are not sold, replace nothing and are not offered to faction presets. A group whose required addons are missing is always off and cannot be turned on. Group and category overrides are stored in the same disabled list as pack overrides (as `PACK_ID:GROUP` and `PACK_ID:GROUP:CATEGORY`, the group being its label), so replication and saves are unchanged.
- Planned: admin UI to toggle packs during play (the server API exists).

### Commanders — In progress
Purpose: one server-owned command context per role. The Resistance is player-directed; the Occupiers and Invaders are AI-directed through the same evaluation interface.

- `AROR_Commander` (role, faction, control mode) and `AROR_CommanderManagerComponent` (game mode), which periodically offers each AI commander for evaluation once the playthrough is ready.
- `AROR_InvaderCampaignComponent` (optional, game mode): the Invaders' push north from an offshore support corridor. On a fixed period they push on the southernmost military site they do not hold, within reach of their own sites. Against the Occupiers the battle is simulated and resource-funded (never near players; the push waits instead), and against the Resistance it is a physical attack through Operations. At their goal share they stop taking Occupier ground, and above it the Occupiers win back the outermost Invader site in the simulation, so the front settles around the goal. It spends Economy funds and changes control through the normal location authority. Chicken opts in.
  - **Town occupation** (opt-in, Chicken): towns and cities are push targets too and count towards the goal share; a won battle occupies them through support instead of setting control. Occupied towns then get the Invaders' garrison post, town life, building and flag through the usual systems.
  - **Attack orders** (opt-in, Chicken): when the target is near players or an active garrison, the push issues a physical attack order instead of waiting, as a player squad would go at it. Its tier is at least one above the target's garrison level (a city one more, Resistance targets at least level 2) and never below the Invaders' Crackdown level, launched from an Invader base through Operations (`LaunchTerritoryAttack_S` with a minimum tier). It wins by holding the force advantage on the objective: AI recapture for military sites, the town occupation contest for towns. A town fighting off a garrison response is not ordered against until that ends. When the Occupiers win ground back from the Invaders (above the goal share), a target near players gets a physical Occupier attack order instead of the simulation, so the two armies fight over the front where the players are.
- Server only; not replicated. Saved: the Invader campaign state (its own serializer).
- Depends on: Game Mode, Factions, Game Flow.
- Extension points: server systems subscribe to the evaluation event (Operations does).
- Planned: enemy target selection and distinct Occupier/Invader strategies.

### Player Squads — In progress
Purpose: players recruit individual Resistance riflemen at owned territory flags and command them with Reforger's native squad command wheel, including movement and vehicle boarding/disembarking.

- `AROR_SquadManagerComponent` (game mode) manages recruitment transactions and a compact replicated phone roster. `AROR_RecruitRiflemanAction` on the shared capture-flag prefab covers the HQ and all current/future flag-bearing territories. The server checks the actual flag's location owner, player faction/life state, proximity, squad leadership and capacity, including pending spawns.
- Squads use native `SCR_GroupsManagerComponent` playable groups and their native AI subgroups. A first recruitment creates a squad if needed; existing squads retain their players and leadership. Native commanding owns orders, AI membership, player respawn behaviour and replication. Personal squads are independent of HICOMM's campaign forces and need no extra mod.
- Riflemen spawn and receive equipment through the selected Resistance faction preset and shared Force Spawner. Recruitment is currently free and limited per native player squad. A queued member joins through the native server recruitment API after equipping; failed spawns release their reserved capacity. The last player's departure retires the managed recruits, while leadership changes within the same squad retain them.
- **Squad phone app**: available to every player, shows flag-recruited riflemen (including arrivals and vehicle occupancy), renames the native squad, and dismisses individual recruits. Only the current native squad leader may mutate the squad; owner-controller RPCs revalidate this on the server. Player-controlled characters are protected from dismissal.
- Server/client: recruitment, dismissal and lifecycle are server-only; native squad state and a small recruit status board replicate to clients. Phone rendering is client-only.
- Saved: no custom squad persistence in this first version; recruits and their roster are session-only.
- Depends on: Game Mode (native Groups and Commanding managers), Game Flow, Locations, Factions, Phone, Notifications.
- Extension points: group spawn resource, rifleman cap, flag interaction distance, spawn search and timeout attributes. Existing resource references are reused; new flag templates should inherit `AROR_Prop_CaptureFlag` and remain children of their location.
- Feature detail and native acceptance checklist: `docs/PLAYER_SQUADS.md`.

### High Command — In progress
Purpose: Resistance Leaders raise AI squads and command them through the **HICOMM** phone app.

- `AROR_HighCommandComponent` (game mode): the Resistance Leaders (the host automatically; the host delegates others), the resistance name, a squad catalogue (unit types per squad, fielded from the Resistance faction preset) and the raised squads. Squads cost Resistance funds **and** manpower, form up at the HQ and take **attack**, **defend**, **move** (standing by on arrival) and **hold** orders. Each leader commands only their own squads; everyone sees all of them.
- Client: the HICOMM app (see Phone) sends requests through RPCs on the player controller.
- Server/client: everything is decided and validated on the server; a compact snapshot (leaders, name, squad summaries) replicates to clients.
- Saved: the resistance name (`AROR_HighCommandComponentSerializer`), set at campaign setup and fixed for the campaign. Squads are not saved: they are refunded at full cost when the campaign ends, and saved funds and manpower already include the refund for living squads.
- Depends on: Factions, Locations, Economy, Game Flow (the HQ), Phone, Notifications.
- Extension points: squad catalogue, per-leader cap, group/waypoint prefabs and map bounds on the component.
- Feature detail: `docs/HIGH_COMMAND.md`.
- Planned: more order types, platoons, enemy intel on the map, balance.

### Leaderboard — In progress
Purpose: a campaign leaderboard of Resistance players, scored by what they destroyed, in the spirit of Antistasi's player score.

- `AROR_LeaderboardComponent` (game mode) counts kills of soldiers and vehicles of any faction hostile to the Resistance (the Occupiers and Invaders, but also e.g. base-game units placed with Game Master) per player in five categories (`AROR_ELeaderboardCategory`, infantry first): **Infantry**, **Light vehicles** (cars, trucks, boats, armed light vehicles such as an M2 Humvee), **APC / IFV** (APCs, IFVs, armoured cars and MRAPs such as the JLTV), **Armour** (tanks) and **Aircraft** (every helicopter and plane). Each kill is worth points by category; the overall score is their sum. **Assists** are kills made by AI the player commands (their HICOMM squads and the AI in their native squad), shown as one general count and worth a share of the kill's points.
- **Kill credit**: on the server, modded character and vehicle damage managers give every character and vehicle an `AROR_KillCredit` that reports hostile hits. The last creditable hit within a time window earns the death (hits nobody can be credited for, such as enemy AI, leave it in place), so bleeding out and burning out count; a death is also settled when the body or wreck is deleted. Vehicles are classified by prefab rules first (attribute; built-in rules put JLTV, MRAP-type vehicles and BRDMs in APC / IFV), then by their faction preset or content pack vehicle type; static weapons are not counted.
- Server/client: credit and counting are server-only; a compact board sorted by score (best players plus everyone online) replicates to every client.
- Client: one drop-down scoreboard, Arma 3 style (`AROR_LeaderboardOverlay`): its keybind (**P**, spare **F8**, `AROR_ToggleLeaderboard` in `chimeraInputCommon.conf`) slides a single table down from the top of the screen with every player's rank, kills per category under each category's icon, assists and score; it closes itself when a menu or the map opens. Drawn from its own copy of the phone's pooled widget layout. Category icons are the base game's entity catalogue pictures of what the enemy armies field (or, failing that, an enabled content pack's vehicle of that type), so modded armies and packs such as WCS show their own vehicles.
- Saved: per player identity, the last known name and the kill and assist counts per category (`AROR_LeaderboardComponentSerializer`); scores are recomputed from the counts, so retuning points applies to existing campaigns.
- Depends on: Factions (roles, presets), Content Packs, High Command, Player Squads (native squads), Notifications (vehicle kill hints), UI colours.
- Extension points: points per category, assist share, credit window, extra vehicle prefab rules and replicated row count are attributes on the component.

### Missions (OPS) — In progress
Purpose: Resistance side missions, offered on a board and run through the **OPS** phone app, paid in Resistance funds and manpower.

- `AROR_MissionBoardComponent` (game mode): keeps a board of mission offers (rerolled as offers expire or are taken) and the active missions. Only **Resistance Leaders** (see High Command) accept or abort them; active missions are capped. A mission's enemies and props spawn when a Resistance player comes near, and its leftovers are removed once players leave.
- Mission types (`AROR_EMissionType`), one `AROR_Mission` subclass each: **Supply Theft**, **Convoy Ambush**, **Prisoner Rescue**, **Supply Sabotage**, **Officer Assassination** and **Arms Cache Recovery**. Each picks its target from Locations (usually an enemy-held site), takes its units and vehicles from the enemy role's faction preset, and checks its own objectives and failure conditions. Rewards, time limits, guard counts and the formations the guards and convoy passengers are drawn from (a rifle fire team by default; see Factions -> Formations) are per type (`AROR_MissionType`) and rise with the target's threat level.
- Results: success pays the Resistance through Economy; start, success and failure are announced through Notifications; a short network log is kept for OPS.
- Server/client: everything is decided on the server; a compact snapshot (missions and log) replicates to every client. Clients request through RPCs on the player controller. Active missions are drawn on the game map (Map Markers), in OPS and in HICOMM.
- Saved: active missions (identity, target, rewards, time left) through `AROR_MissionBoardComponentSerializer`; a loaded mission restarts when Resistance players approach and can be aborted by any current Resistance Leader. Offers are not saved; the board rerolls.
- Depends on: Locations, Factions (presets), Economy, High Command (leaders), Notifications, Phone, Map Markers.
- Extension points: per-type settings and board size, caps, ranges and prefabs on the component; new mission types through `modded enum AROR_EMissionType` plus an `AROR_Mission` subclass returned from a modded `CreateMission`.
- Feature detail: `docs/MISSIONS.md`.
- Planned: enemy crackdown and support effects, item rewards, more mission types, balance.

### Black Market and Imports — In progress
Purpose: two phone shops where Resistance players buy equipment from the enabled Content Packs. **Black Market** sells items (everything except units and vehicles) into Resistance Storage; **Imports** ("Used Imports") sells vehicles into the Resistance Garage.

- **Import facilities**: held seaports, heliports and airports (`AROR_ImportPoints`). Each pack entry carries a **facility count** and an optional **special location** (one held seaport, or one held heliport/airport). Below the count, or without the special location, an entry is shown locked with what it needs. Zero facilities held closes both shops. Prices come from the pack entries' base cost.
- `AROR_BlackMarket` (items) and `AROR_Imports` (vehicles) build their catalogues from the enabled Content Packs, the same on every machine, and rebuild when the packs change. The client sends prefabs and quantities, never prices; the server resolves them against its own catalogue, checks every requirement, takes payment and delivers. **Black Market** takes a whole cart all-or-nothing (`Import_S`). **Imports** buys one vehicle per request (`Buy_S`, no cart) and refuses when the Garage is full.
- **Who pays**: personal funds, or **Resistance faction funds** when the buyer is a Resistance Leader (HICOMM host or delegate) and chooses so. A faction-funded purchase is announced to everyone. There is no confirmation step: one click buys.
- **Delivery**: items go to Resistance Storage (`Add_S`). Vehicles go to the Garage as a **factory-new entry** (`AddVehicleByPrefab_S`): no snapshot, so retrieving one gives the prefab's own fuel and cargo.
- **Phone apps** (`AROR_BlackMarketApp`, `AROR_ImportsApp`, shared base `AROR_ShopAppBase`): category tabs, lists that scroll by whole rows (mouse wheel or arrows), a picture per entry (the base game's entity-catalogue image, else an inventory-style item preview of the prefab), and a purchase popup with a sound for every success or failure. The Black Market adds a search box over all categories and a slim scrolling cart; Imports shows two buttons per vehicle (faction, personal) and a garage-slots read-out.
- Server/client: the client asks through the player controller (`AROR_RequestBlackMarketImport`, `AROR_RequestVehicleImport`); the server answers the buyer only with a result (`AROR_ShopResult`) that opens the popup. Facility counts read replicated location control.
- Saved: nothing of its own. Delivered items and vehicles are saved by Storage and the Garage.
- Depends on: Locations (import facilities), Content Packs, Factions (Resistance check), Economy (personal money, faction funds), High Command (Leaders), Resistance Storage, Resistance Garage, Phone, Notifications.
- Extension points: the catalogue is the enabled Content Packs (change prices, requirements and entries there); `GetOnImported_S` on each shop for server-side reactions; `modded enum AROR_EImportPointRequirement` for new special locations.
- Planned: Attention (black market heat), per-item stock limits.

### Operations (Attacks & QRF) — In progress
Purpose: enemy **attacks** and defensive **quick reaction forces (QRF)** for the Occupiers and Invaders: deciding when to respond, choosing and paying for a force, spawning and directing it, and cleaning up afterwards.

Main parts:
- **Scheduling** (`AROR_OperationManagerComponent`): accepts attack requests and threat reports, ranks threats, applies per-role cooldowns, capacity and deduplication, and tracks each operation from planned to a final outcome. Invader operations are **disabled by default** until territory and resource integration is ready.
- **Force selection**: designer-authored force profiles and ground recipes per role and faction (motorized, mechanized, armour, airborne); a profile requests its vehicles and optional airborne escort by faction preset vehicle type, and lists its infantry as formations or explicit faction preset unit types (Factions), a rifle squad by default, and its crew as a unit type, response levels 1–5, and an optional strategy layer: saved **Readiness** (was "war level") decides which equipment a response may use, and a separate saved **Crackdown** (was "aggression") sets its strength. Both are integer levels 1–5 per enemy role.
  New campaigns begin at Readiness 2 and response tier 2 (both scenario settings), and Crackdown does not cool during quiet periods (cooling is a setting, off by default). Capture losses build intensity in regular steps; authored ground/joint bands add one full package per tier while preserving equipment gates and separate infantry/vehicle crews. Loaded escalation remains saved campaign state.
  Ordinary campaign QRFs target a running airborne share per army, counting standalone and joint deployments together. Successful dispatch totals are saved; manual/debug/scripted responses are excluded. Equipment, finance and launch safety can force a ground fallback without consuming an airborne dispatch.
- **Threat labels** (`AROR_ThreatLabels`): display names for each level, keyed by the army (faction preset) holding the role, e.g. ГОТОВНОСТЬ/РЕЖИМ for the Soviet Army and THREATCON/CRACKDOWN for the US Army, with a generic English fallback. Logic only uses the integers.
- **Funding** (`AROR_QRFFinance`): responses reserve and spend the role's funds through the **Economy** API; losses are charged and shortfalls become saved QRF debt. Economy remains the only bank.
- **Execution** (`AROR_OperationExecutorComponent` and helpers): spawns and reserves forces, delivers them by road or by air, runs searches and town security, and staged reinforcement waves under a shared tactical commander (`AROR_QRFTacticalCommander`). Forces launch from friendly outposts, bases or airports.
  Ground vehicle dispatch requires an assigned living AI driver and crew in every turret seat, including mounted weapons attached to cargo-style compartments; passenger occupancy alone cannot satisfy combat readiness. Crew and passengers are teleported into their seats; the engine refuses this for AI it has not woken (packages spawned far from players), so the package's existing AI is woken before boarding (renewed on each retry) and boarding fails the package after a timeout (`m_iQRFBoardingTimeout`, 90 s). Soldiers are never replaced to work around a refused seat.
  Road launches fall back to a road beside each probe when the base centre is cut off on the navmesh, and a base with no reachable road at all is dropped as a launch site for the session. Road approaches fall back to the nearest launch-reachable road within the dismount limit, with infantry walking in. The active-operation timeout runs from the estimated arrival.
  Packages left behind by a failed or ended operation are retained until players are far enough away. A package that never boarded does not count toward the live ground cap and is cleared once no player is within 300 m. Launch waits (live cap, missing commander, unusable bases by reason, missing recipe) are logged.
  Ground profiles can fill native passenger capacity with a separate infantry group within their reserved personnel budget. Chicken enables this policy for its ground vehicles: infantry unload at the delivery point while driver and turret crews retain the hull for transport or support. Personnel reservations and collateral cover the full-load ceiling before construction; actual seat capacity is checked before queuing actors.
- **Airborne execution** (experimental, `AROR_ReaperQRFExecutor`): helicopter insertion with optional gunship support using REAPER. Chicken uses escorted packages (transport plus gunship) from Readiness 1. Support investigates reported positions through native perception, with traffic avoidance and separate cruise/support routes; airborne execution can be switched off. Aircraft launch from their own owned origin, the nearest usable one at least a set distance from the target (else the farthest), moved back over land, or to another site, until every player is a set distance from each aircraft (distance only, no line-of-sight test), falling back to ground or waiting when no point qualifies, and cancelled if that site is lost before they are built; the ground origin stays the tactical origin, and both bases take the launch cooldown.
  Chicken transport cabins use native usable passenger capacity within pre-reserved full-load ceilings. Transport and escort pilots remain separate from infantry; boarding targets distinct compartments and requires actual occupancy before the shared lead barrier releases flight. Initial admission includes finite landing-search, construction and boarding allowances.
- **Joint responses** (experimental, scenario opt-in): authored recipes combine one airborne package with ground elements under one operation and tactical commander. Chicken makes every ordinary campaign QRF joint (airborne share 100%), with ground recipes as the fallback when aviation cannot launch: escorted air with truck/light-support mixes at Readiness 1–2, mechanized mixes at 3–4 and armoured mixes at 5, heavy from tier 1 and in two waves at tier 5. A scenario can add unescorted transport helicopters beside the airborne pair, as a percentage of the ground elements (`m_iJointTransportPercent`, off by default; both shipped Everon scenarios use 100%), and fewer are sent when actor limits or funds run short. Funding, actor reservations, activation, outcome and cleanup are shared. Joint attacks hold airborne insertion for ground assault release; native acceptance is pending. Live aircraft and construction queues reserve the airspace; delivered infantry and retained wreck records do not block later flights once aircraft have departed. Landing-plan workspaces are released after their coordinates transfer to queued packages. Gunship support time starts near the objective and remains bounded.
- **Territory attacks** (`AROR_TerritoryAttackManager`, optional scenario policy): the Occupiers attack Resistance-held military sites from the moment the Resistance holds any, front site nearest an Occupier base first, after a short per-site capture grace and with a rest between their attacks. A site's deployed capture QRF holds attacks on it, plus a recovery time after it ends. The Invaders' physical attacks come from their push (Commanders) through the same launch path. Attacks share QRF force selection, funding, tactical coordination and cleanup. Only one attack may be outstanding globally, whichever army, unless the scenario allows one per army (`m_bOneAttackPerArmy`, Chicken: the two armies may attack at once, never the same target); normal capture authority decides success. No attack order launches while a town garrison response (Towns) is running, and a town or city is not targeted for a breathing-room time after it changes hands (`m_iTownAttackGrace`, also after loading; the Invader push skips it, simulated battles included). With town attacks (`m_bTownAttacks`, Chicken) Resistance-held towns and cities are targets too and are won through the town occupation contest (Location Capture). Every attack order is sized to take its target (`AttackOrderTier`: one tier above the target's garrison level, never below the attacker's Crackdown level), shared with the Invader push. HQ is excluded, and strongholds are only attacked by an army that can capture them. The server logs why no attack is starting whenever that reason changes. In Workbench builds, MARS can launch any response as an attack (staging and assault) without the campaign timers; packaged servers refuse it, and test attacks never move the real attack timers or rest.
- **Response policy** (`AROR_ResponsePolicy`): a shared, side-effect-free selector uses Readiness for equipment availability and Crackdown for intensity. Response families resolve configured profiles or authored recipe tiers, with the same eligibility gates in campaign planning and MARS. Recipes can declare their response family; extenders can add families, profiles and tiers without adding phone dispatch logic.
- **MARS debugger** (Phone): role-separated response-type grid with simulated Readiness and Crackdown sliders, live force preview and explicit REAPER airborne packages, dispatched through the same server operation system to the nearest Resistance-owned territory. The server resolves and freezes the selected metrics. Debug dispatch requires an owned safe military origin, preserves actor limits and bypasses campaign finance without changing campaign Readiness or Crackdown. The client uses its own red layout and cached circular backdrop.
- **Scripted responses** (`AROR_ScriptedResponses.c`): trusted server callers (Strongholds) can request a QRF with a named recipe, Readiness and Crackdown frozen at a level, funding waived and a fixed launch point instead of a friendly site, optionally forced into two waves. It skips scheduler cooldowns and the per-role cap, but keeps target conflicts, Invader admission, executor capacity and launch safety; execution is otherwise a normal QRF. Coastal staging is not a friendly recovery base or simulated sea transport.
- **Town escalation** (`AROR_TownWaveResponses.c`): Towns report kills and taken posts (`RecordTownEscalation_S`): Crackdown and Readiness pressure rise per kill (attributes on the town), a taken post applies the scenario's capture escalation, and each report resets the army's quiet timer. Readiness pressure can cool after the same quiet time as Crackdown (`m_iReadinessDecay`, never below the starting level; off unless a scenario sets it, as Chicken does with Crackdown cooling).
- **Town wave responses** (`AROR_TownWaveResponses.c`): trusted server callers (Towns) request one wave of a town's response: a wave recipe (`m_iAssaultWave`, never chosen by the campaign selector) at the army's current Readiness, cut to an element count from its Crackdown (optionally starting further down the list, for a split column), funding waived, launched from a road staging point near the town or, for the helicopter assault, from the army's own sites. Waves run beside each other and beside ordinary QRFs at the same target without absorbing them, arrive as one column (no reserve split) and stop together short of the objective, with infantry walking in.
- **Defence and spotting**: assigned defenders report hostile damage; capture events can trigger a response for the previous owner. Resistance players who visually confirm hostile response vehicles or deployed dismounted squad leaders get shared last-seen map markers with separate transport, IFV, armour, light-vehicle, transport-helicopter, gunship and infantry symbols; a vehicle the Resistance has claimed (see Resistance Garage) is no longer reported, and operation cleanup leaves it in the world.
- **Dispatch alerts** (`AROR_OperationAlerts.c`): when a QRF or attack is dispatched, everyone gets a headline naming the army, the target and a rough arrival estimate (`AROR_OperationEta`, from each lead package's distance). A replicated board of outstanding QRFs and attacks drives a target marker on the threatened location (Map Markers).
- **Assault and security**: ground attacks stage before a shared assault release. Targeting uses recent attributed combat reports; hidden defenders are not revealed by a world-state census. A response physically holding a lost settlement can shift support through Settlements while outnumbering all hostile military forces there; support remains the settlement ownership authority.

- Server/client: all decisions and execution run on the server. Clients only receive QRF spotting reports (their own, through the player controller), send whether their map is open, and read a public readiness/crackdown summary (RebIntel) and the outstanding-response board (dispatch alerts).
- Saved: readiness, crackdown, QRF debt and aggregate ordinary-QRF dispatch/airborne counts per army. Live operations, deployed forces, queues, territory attack eligibility and cooldowns are session-only. Loaded territory establishes a fresh baseline: each held site becomes attackable once the capture grace has passed in the current session.
- Depends on: Commanders, Locations, Location Capture, Factions, Economy, Game Flow; the REAPER mod for airborne execution.
- Extension points: scenario and prefab configuration of schedules, limits, force profiles, recipes and prices; trusted server callers can request attacks and report threats; extender armies add Readiness/Crackdown labels with a modded `AROR_ThreatLabels.Create`.
- Spotting and aviation refinement: `docs/SPOTTING_AND_AIR_RESPONSE.md` (visual confirmation, map contact lifecycle, common transport insertions and gunship response).
- Feature detail: `docs/OPERATIONS.md`, `docs/QRF_STRATEGY_ECONOMY.md`, `docs/QRF_TACTICAL_COMMANDER.md`, `docs/AIRBORNE_QRF_PROTOTYPE.md`, `docs/QRF_SPOTTING.md` and the other QRF files in `docs/`. Testing in the Chicken world: `docs/OPERATION_EXECUTOR.md`, `docs/FORCE_POOL_QUICKSTART.md`, `docs/TRUCK_IFV_QRF_TEST.md`.
- Release validation and native acceptance: `docs/QRF_RELEASE_VALIDATION.md`; route and response matrices live in `docs/validation/`. The Workbench-only route audit probes ground planning, airborne launch formations and landing sites without deploying forces.
- Planned: broader strategic target selection, physical military stock, durable save/restore of live operations, client summaries.

### Map Markers — In progress
Purpose: our own markers on any open map (player map, deploy screen, setup screen). Client only.

- `AROR_LocationMapIconsComponent` (game mode) draws an icon per location, tinted by the controlling faction's colour, and keeps it positioned as the map pans and zooms. Markers are sized by importance: key sites (bases, outposts, airfields, heliports, ports, the HQ, strongholds) are largest and carry their name in bold on a plate beside them; industry is smaller and towns and cities smallest (vanilla prints their names). Settlement garrison posts get a pennant in the holder's colour on their flag once zoomed in (an outline once taken). Clicking an icon opens the info popup, which is drawn above every marker. Icons are drawn as badges (`AROR_MapFrameIcon`: solid faction-coloured shape, square ours, circle otherwise) carrying the base game's own map icon for the type (Conflict's monochrome building icons and the topographic set, tinted for contrast); towns and cities are plain dots unless the component switches back to textures. Strongholds, the campaign objectives, show the holding army's national flag instead (`AROR_ArmyFlag`), on the game map and in HICOMM. Each location prefab chooses its icon image (white, so it can be tinted).
- Hovering shows a pulsing selector; clicking opens an info card (name, type, controller; population and support for settlements) that updates live.
- QRF last-seen markers and incoming QRF/attack target markers (from Operations: a pulsing reticle ring on the threatened location with the army, response and arrival countdown; red for attacks, amber for QRFs) and active Resistance missions (from Missions) are currently drawn by the same component. Moving the map framework into its own system, with each feature providing its own layer, is under discussion.
- Player markers (`AROR_PlayerMapMarkers`): an infantry symbol and name per living player, Resistance colour, red while unconscious. Clients only stream nearby characters, so the server publishes a board of all players' positions and consciousness about once a second (`AROR_PlayerMarkersComponent`, game mode, replicated string); not saved.
- The markers themselves are not replicated or saved; they read replicated state. No base-game marker configs are used.

### Economy — In progress
Purpose: money and manpower for the campaign: balances, and periodic income from held locations. General purchasing rules come later.

- `AROR_EconomyComponent` (game mode): **faction funds** per role, Resistance **manpower**, and **personal money** per player. Server-owned; funds and manpower replicate to everyone, a player's money only to that player. Player money is keyed by identity ID so it survives reconnects. Players can transfer personal money to another online player or donate it to Resistance funds (Bank Transfer RPCs on the player controller).
- `AROR_IncomeComponent` (game mode, server): every **15 minutes** (not during setup) each role is paid for what it holds:
  - Money = held Resources × $500 × (1 + 0.3 per held Factory + 0.5 per held Port), plus $5 per inhabitant of held towns and cities.
  - The Occupiers and Invaders also receive fixed **external support** ($500 each).
  - The Resistance gains manpower per held town and city, and every online Resistance player is paid 10% of the Resistance's money income as personal money (extra, not taken from the funds).
  - Resistance players get an income notification. Rates and the interval are attributes.
- Rules: balances never go below zero; spending succeeds only if the balance covers it. All changes happen on the server.
- Saved: funds, manpower and player money (`AROR_EconomyComponentSerializer`); Resistance funds and manpower include the refund for living High Command squads. Income timing is not saved.
- Used by: Operations (QRF costs), High Command (squad purchases), Missions (rewards), the phone's Bank app, admin tools.
- Extension points: server API to add, spend and read balances, and to trigger a payment.

### Resistance Storage — In progress
Purpose: the Resistance's shared item stock for the campaign (what gear is on hand), separate from Economy money and manpower.

- `AROR_ResistanceStorageComponent` (game mode): a ledger of **item prefab + quantity**. Any inventory item can be stocked; slot, type and volume stay on the item when it is later spawned or equipped. Starts empty.
- Server API: `Add_S` / `Take_S` (atomic) by prefab; `GetQuantity` / `GetEntries` on every machine; `AddEntity_S` deposits a world/inventory item by unpacking nested contents (vest cargo, weapon attachments, magazines, and so on) into **separate stock lines**, then deleting the entity. Magazines are credited by their **rounds**: leftover rounds pool per magazine type (server-only) and become stock as they add up to full magazines, so an emptied magazine is worth nothing. Item health is not preserved.
- Server-owned; stock replicates to all clients as parallel prefab/quantity arrays. Change invoker for future UI.
- Saved: the stock ledger and the pooled loose rounds (`AROR_ResistanceStorageComponentSerializer`).
- Admin: Economy → More can fill stock with one of every Resistance preset item (excl. units/vehicles).
- **Arsenal**: Prefab inherits vanilla `ArsenalBox_Base` plus `AROR_ResistanceArsenalComponent` (same entity as `SCR_ArsenalComponent`). Lists stocked prefabs (qty > 0), sorted by content pack category then path, with stock quantity on each slot. Takes call `Take_S`; refunds call `AddEntity_S`. Supplies are never the budget (`IsArsenalUsingSupplies` forced off). Game mode has `SCR_ArsenalManagerComponent` so vanilla type flags do not disable the box; Resistance arsenals also force-enable. Modded `SCR_ArsenalComponent`, arsenal slot UI, and `SCR_ResourcePlayerControllerInventoryComponent` implement the bridge. Access: open the box like a normal arsenal. Empty stock shows an empty arsenal until admin Fill or deposits. A Resistance arsenal opens as its own container panel (modded `SCR_InventoryMenuUI` redirects the open-storage step to `OpenStorageAsContainer` with our layout and hides the Vicinity panel). The panel has a column of category buttons (all preset categories, regardless of stock) whose selected entry expands into sub-types; only the selected category/type is listed and the vanilla grid paging browses it (modded `SCR_InventoryOpenedStorageArsenalUI`; classification in `AROR_ArsenalCatalog`). Placement is not HQ-exclusive; Game Flow spawns the first one beside the HQ via `SpawnArsenal`.
- **Loadout slots**: `AROR_PlayerLoadoutsComponent` (game mode) holds **three** kits per player, keyed by identity ID (same as personal money) and saved with the playthrough (`AROR_PlayerLoadoutsComponentSerializer`). F-key actions (`AROR_SaveLoadoutSlotAction` / `AROR_LoadLoadoutSlotAction`) save a snapshot of worn gear or load a slot. Load **swaps with stock**: deposit current gear via `AddEntity_S`, take the kit’s flat prefab counts via `Take_S`, then re-equip with vanilla `SCR_PlayerArsenalLoadout` apply. Fails with a hint, before anything is deposited, if stock plus what the worn gear is worth (Storage's deposit rules) would be short. Slot occupancy is pushed only to the owning client. Magazine and item health handling follow Storage. Wire the six actions in Workbench onto the arsenal’s **inherited** `ActionsManagerComponent` as `additionalActions +` (do not add a second ActionsManager — that breaks the prefab / spawn).

### Resistance Garage — In progress
Purpose: the Resistance's shared vehicle garage for the campaign — unique stored vehicles with condition preserved, separate from Resistance Storage (fungible item stock).

- `AROR_ResistanceGarageComponent` (game mode): a ledger of **unique vehicle entries** (monotonic id + prefab + per-hit-zone health + fuel by tank id + cargo tree + mounted weapon ammo). Any world vehicle may be stored (not limited to Resistance presets); Occupiers/Invaders have no garage.
- **Capacity**: 10 slots from the Resistance HQ (`m_iHqSlots`) plus 2 slots per Resistance-controlled military objective (`m_iObjectiveSlots`, `IsCapturableType` only — towns and cities excluded). Recomputed from the location registry. If territory is lost and count exceeds capacity, existing entries are kept and new stores are blocked until under capacity.
- Server API: `StoreVehicle_S` (snapshot + despawn; rejects destroyed/occupied/full), `AddVehicleByPrefab_S` (a brand-new vehicle bought through Imports: a **factory-new** entry with no snapshot, retrieved with the prefab's own fuel and cargo), `RequestStoreVehicle_S` (player store with site/range checks), `DiscardVehicle_S`, `RetrieveVehicle_S` (spawn within place radius of a garage anchor, restore snapshot, affiliate **Resistance**, remove entry), soft `ReserveEntry_S` / `ReleaseReservation_S` while placing. `GetCapacity` / `GetCount` / `GetEntries` on every machine.
- **Store (looked-at)**: phone home quick dock slot 1 calls `AROR_GarageStoreAPI.TryStoreLookedAtVehicle()` (then closes the phone). Raycasts from the local character camera (max **5 m**, `m_fStoreLookDistance`), only when the player is within **50 m** (`m_fStoreSiteRadius`) of a Resistance **HQ** or capturable military site (towns/cities excluded). RPC `AROR_RequestGarageStore`.
- **Retrieve UI**: `AROR_GarageRetrieveUI.Open(anchor)` opens a simple configurable dialog (browse Prev/Next, Spawn). Spawn starts a client ghost placement mode (`AROR_GaragePlacementController`) with an overhead camera — place within **50 m** (`m_fPlaceRadius`) of the garage anchor on clear ground (Space place, Esc/RMB cancel, Q/E rotate; the ghost is excluded from the clearance TraceBox). `AROR_GarageRetrieveAction` on `AROR_Prop_Garage` (retrieve only). Player-controller RPCs carry detail/reserve/retrieve.
- **HQ garage prop**: `AROR_Prop_Garage` inherits `PalletFuel_01` with Persistence; retrieve action on its Workbench `ActionsManagerComponent` (never add a second ActionsManager or Rpl by hand). Game Flow spawns it opposite the arsenal at HQ creation (`m_vArsenalOffsetFromHQ`, default 0.5 m either side of the flag).
- **Resistance fleet** (`AROR_ResistanceFleet`, owned by the garage component, server): any vehicle a living Resistance player sits in is claimed for the Resistance: it is affiliated Resistance and kept until destroyed, deleted or stored. Other systems check `AROR_ResistanceFleet.IsClaimed` and never delete it (Operations and Missions; Garrisons treat it as transferred to the world), and Operations stops reporting it as an enemy QRF asset. Claimed vehicles are saved with the garage (same snapshot plus position and orientation) and spawned back shortly after a load; a vehicle of the same prefab already standing there is adopted instead.
- Server-owned full snapshots; compact id/prefab/health/fuel summaries replicate. Change invoker for UI.
- Saved: full entries, including the factory-new flag, and the fleet's world vehicles (`AROR_ResistanceGarageComponentSerializer`, version 4). Reservations are session-only.
- Depends on: Game Mode, Locations, Location Capture (capturable type filter), Persistence, Notifications (hints).

### Phone — In progress
Purpose: an in-game phone that hosts player-facing apps. Client only.

- `AROR_PhoneMenu` (a menu registered in `chimeraMenus.conf`) and `AROR_PhoneComponent` (game mode), which opens it with a rebindable keybind (**Ctrl+Y**) for living players when no other menu is open. Esc closes it.
- A phone frame over a wallpaper with a status bar. Apps open in portrait, or turn the whole phone to landscape and enlarge it. The frame's home button returns to the home screen.
- Closing and reopening resumes the last app and its presentation state, including HICOMM's screen/map, OPS sign-in, shop browsing and draft cart, and the Squad roster page. The client Phone component owns this session state independently of menu widgets; it survives player respawn and is discarded with the world. HICOMM sign-in is retained until logout or loss of leadership; live access and all server request checks still apply. Pending actions and destructive confirmations are not replayed.
- **Quick dock**: four slots along the bottom of the home screen. Slot 1 stores the looked-at vehicle (Resistance Garage) and closes the phone; slots 2–3 are empty placeholders; slot 4 saves the campaign (admins only once a campaign is running, checked again by the server; see Persistence).
- Apps: **Bank** (account holder, personal money, Resistance funds, live from the Economy; Transfer screen sends personal money to an online player or Resistance funds), **REBINTEL** (one tab per enemy army: Readiness and Crackdown cards in that army's labels with a 5-segment meter and what the level means; the player's HUD widget and alert settings; read-only: the levels are driven by the campaign), **HICOMM** (landscape High Command app for Resistance Leaders: leadership, a zoomable map for orders, squad purchases; the map shows the base game map by default with military-frame location symbols, the procedural tactical map behind a toggle), **OPS** (landscape mission board from Missions: browse operations, dossier with an area map; leaders accept and abort), **Black Market** (landscape item shop from Black Market and Imports: category tabs, search, a scrolling cart, one-click import into Storage, purchase popup and sound) and **Imports** (landscape vehicle shop: pictures, one click per vehicle into the Garage, faction or personal funds) and **MARS** (landscape response debugger from Operations, replacing the temporary Test tile).
- **Squad** (landscape): native player squad name and flag-recruited riflemen; the squad leader can rename and dismiss members (Player Squads).
- Depends on: Economy, Notifications (hints), Operations (RebIntel), High Command (HICOMM), Missions (OPS), Player Squads (Squad), Resistance Garage (quick store).
- Saved: REBINTEL player settings (HUD widget on/off, compact/full, top left/right, alert pop-ups, tracked enemy) in the player's own profile folder, client-side (`AROR_RebIntelSettings`). Nothing on the server.
- Planned: app registration so extender mods can add apps; a Settings app and Feed (see `docs/UI_DESIGN.md`).

### Radio — In progress
Purpose: music in vehicles. Anyone seated in a vehicle can turn its radio on from the dashboard, and everyone nearby hears the same song at the same point. Ships two stations, **Radio Free Altis 101.3** and **Everon Underground 94.5**.

Lives in the optional addon `OptionalAddons/OpenResistanceRadio` (addon ID `OpenResistanceRadio`, class prefix `ORRadio_`), which depends on the core. The core has no radio code or audio, so a server without the addon has no radio and no controls.

- `ORRadio_ManagerComponent` owns the system, added to the game mode through an Override in addon of `Prefabs/GameModes/AROR_GameMode_Base.et`. Vehicles carry no component, so an idle radio costs nothing.
- **Controls** (`ORRadio_VehicleAction`): dashboard interaction actions for power, volume and next station (shown with more than one station), added to every vehicle through an Override in addon of `Prefabs/Vehicles/Core/Vehicle_Base.et`. They attach to the contexts base-game vehicles start their engine from (`starter_switch`, `ignition`, `engine_start_button`, `battery_on`), so a modded vehicle needs one of those to show them.
- **Stations** (`ORRadio_Station`, listed in `Configs/Radio/AROR_RadioStations.conf`) are worked out, not ticked: every machine builds the same looping running order (`ORRadio_Schedule`): either shuffled songs with an interlude every few, from a replicated seed (Radio Free Altis), or the album in its numbered order with interludes where the album puts them (`m_bAlbumOrder`, Everon Underground), beginning at a song picked from the seed (`m_bRandomStart`) so a game does not always open on the first tracks. Each machine finds its place from the server clock and a replicated start time. The server sets the seed and start time when the first radio of a game is turned on, not while the world loads, as values set during load did not reach clients; they then hold for the rest of the game. Nothing replicates as tracks change.
- **Radios**: the server keeps the list of radios that are on (vehicle, station and volume), replicated to clients, and turns one off when its vehicle is gone or destroyed; an optional shut-off for empty vehicles is off by default. Clients ask through the player controller; the server checks the player sits in that vehicle.
- **Audio** (client only): each client plays the nearest few radios in range through `AudioSystem`, seeking into the current track, and follows the moving vehicle. Each radio has a replicated volume, sent to the audio project as a signal when the sound starts, so changing it restarts the sound in place.
- **Now playing card** (client only, `ORRadio_NowPlayingCard`): shows the station's album art, name and current track for a few seconds to the player who turns the radio on or switches station. Nothing is replicated for it.
- **Assets**: `Sounds/Radio` holds one audio project per station (built on the base-game Kolguyev radio's sound setup) sharing one signal (`AROR_Radio.sig`, the base-game radio signals with a longer seek range and a volume); `UI/Textures/Radio` holds the album art. `tools/Convert-RadioTracks.ps1` builds a station from an album folder: it makes the WAVs and art, measures durations and links the GUIDs Workbench generated.
- Saved: nothing; stations restart and radios are off after a load.
- Depends on (all in the core): Game Mode, UI colours, Resistance Garage (occupant check).
- Extension points: other addons add stations through Override in addon of the stations config, each with its own audio project using the same signals. Range, sound limit and empty shut-off are attributes on the component.
- Planned: placeable radios.

### Notifications — In progress
Purpose: tell players what is happening in the campaign, plus quiet personal feedback. Any system can send them.

- **Headline** (alert banner, top-centre below the compass: priority-coloured header with its source, headline, optional detail line), **Notification** (toast on the right edge, routine events) and **Hint** (bottom pill, personal feedback such as why an action failed).
- `AROR_NotificationManagerComponent` (game mode) with a static API; `AROR_NotificationDisplay` draws them.
- **REBINTEL status widget** (`AROR_RebIntelHud`, client): both enemy armies' Readiness and Crackdown in a top corner, each army with its national flag, coloured by stage (green to red), compact or full, hidden while a menu or the map is open. Level changes raise local 5-second banners (`AROR_RebIntelAlerts`): orange for Crackdown up, amber for Readiness up, green for drops, if the player allows pop-ups.
- Server/client: headlines and notifications are sent by the server to everyone or one player. Hints, the status widget and level-change alerts are local. Drawing is client-only.
- Headlines and notifications share one queue (headlines first) and show to all players, alive or dead. Hints replace each other and are dropped while the player is dead. Nothing is saved or replayed.
- Planned: priorities P1–P4 as their own field (sound, duration, opt-out), duplicate merging, a notification log (HICOMM Feed).

### Admin Tools — In progress
Purpose: in-game testing and admin tools, opened with **Ctrl+T** (`AROR_AdminMenuComponent`, game mode).

- **Interface**: a full-screen menu (`AROR_AdminMenu`, drawn by `AROR_AdminApp` from the shared pooled-widget layout) with four pages: **Locations** (capture on map, territory held per side), **Economy** (balances, add money, pay income now, fill Resistance stock from the preset), **Support** (nearest town or city) and **Tools** (teleport, Game Master, test notifications).
- **Capture on map**: the admin picks a side, the base-game map opens, and every click gives the nearest location to that side until the map is closed (the menu then reopens). The server (`CaptureLocation_S`) sets a town or city to 100% support for the side, and changes military sites directly (`ForceControllingRole_S`, ignoring the flip cooldown and strongholds, announced like a normal capture). Locations with fixed control are refused. The change goes through the normal control events, so capture QRFs can react.
- Each tool asks the server, which applies the change.
- Packaged sessions require an administrator or session-administrator role for the local menu and every server mutation. `AROR_AdminAccess` supplies the same role policy for MARS (single player counts as administrator); a UI credential cannot authorize gameplay changes. Workbench retains local testing access. Native client-role replication and permission-revocation acceptance remain pending.

### Map Templates — In progress
Purpose: per-map setup of the game mode, as a sub-scene of the terrain's world.

- One sub-scene per map with its own mission header. First map: **Everon** (`Worlds/AROR_Everon/`, mission `Missions/AROR_Everon.conf`).
- Each map places the game mode, faction and loadout managers, a staging spawn and its location prefabs.
- Developer test sub-scenes (`AROR_Everon_Rowantrek`, `AROR_Everon_Chicken`) carry their own test settings. Chicken holds an island-wide catalogue of 41 campaign locations generated from `docs/EVERON_CAMPAIGN_LOCATIONS.csv` (see `docs/EVERON_CAMPAIGN_LOCATIONS.md`).
- Extender map mods create a sub-scene of their own terrain and place location prefabs; no code needed.

<!--
### System name — Status
Purpose: one or two sentences.
Key classes/prefabs/configs: `AROR_...`, `Prefabs/...`, `Configs/...`
Server/client: what runs where and what is replicated.
Saved: what is persisted.
Depends on: other systems.
Extension points: what extender mods can override or add.
Planned: what is designed but not built.
-->
