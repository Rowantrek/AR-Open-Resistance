# Open Resistance

An Insurgency-style game mode for **Arma Reforger**. Players are the **Resistance** (INDFOR, default FIA assets), the only player faction, fighting two AI factions:

- **Occupiers** (BLUFOR, default US assets)
- **Invaders** (REDFOR, default USSR assets)

Each playthrough is a persistent campaign that a server admin creates or loads. Factions are referenced by role and are configurable, so extender mods can swap the assets.

> Work in progress. Systems are built incrementally; their status is tracked in [ARCHITECTURE.md](ARCHITECTURE.md).

## Features

- Capturable locations (towns, cities, bases, ports, factories and more) with Resistance, Occupier or Invader control
- Town and city support, so a settlement changes hands as regional opinion shifts
- Garrisons and defence compositions at locations
- AI attacks and quick reaction forces (QRF) from the Occupiers and Invaders
- Economy: personal and Resistance funds, income, and faction funds for the AI roles
- In-game phone (bank and apps), map markers and location info popups
- Dispatch notifications and headlines
- Admin menu for testing and campaign management
- Campaign persistence

## Repository layout

| Path | Contents |
|---|---|
| `AROpenResistance/Open Resistance/` | The core addon. Open `addon.gproj` in Workbench |
| `OptionalAddons/` | Optional add-ons that depend on the core (e.g. the REAPER helicopter experiment) |
| `docs/` | Design notes and data for individual systems |
| `tools/` | Offline validation scripts for configs and data |
| `ARCHITECTURE.md` | Systems, responsibilities and extension points |
| `AGENTS.md` | Project and coding conventions |

## Requirements

- Arma Reforger and Arma Reforger Tools (Workbench)
- The core addon depends on the base game (`ArmaReforger`). It currently also depends on the REAPER helicopter mod, used by the airborne QRF experiment (see ARCHITECTURE.md)

## Getting started

1. Clone the repository.
2. In Arma Reforger Tools, open `AROpenResistance/Open Resistance/addon.gproj`.
3. Check that scripts compile in the Script Editor.
4. Open the Everon world and press Play in the World Editor.

There is no command-line build.

## Extending

Other mods can add maps, factions, loadouts, vehicles or missions by listing the core addon as a dependency and inheriting from its template prefabs and configs. Extender mods must use their own class prefix, never `AROR_`. See [ARCHITECTURE.md](ARCHITECTURE.md) for the extension points.

## Contributing

- Work on your own developer branch; PRs go into `master-dev`, which is merged into `master` by PR.
- Follow the conventions in [AGENTS.md](AGENTS.md) (naming, replication, commit and PR format).
- The server is authoritative for all game state.

## Status

Systems are tagged Planned, In progress or Implemented in [ARCHITECTURE.md](ARCHITECTURE.md).

## License

No license has been specified yet.
