# UI design system (summary)

A condensed copy of the **Open Resistance UI** design system, the source of truth for every screen this mod draws: HUD, notifications, map, and the HICOMM phone and its apps. The full system, with live previews of every component, is the design-system artifact (ask the team for the link). Where this summary and the full system disagree, the full system wins. Where the full system disagrees with [AGENTS.md](../AGENTS.md) (class and file prefixes), AGENTS.md wins.

## Feel and principles

The UI should feel like it shipped with Reforger: flat dark plates, condensed uppercase headings, off-white text, NATO-style map symbols. Our one identity is **HICOMM green** (`rebel`): it means *us* (our units, our territory, selected items, primary actions). Every screen is judged on one question: *can a player read it in one glance and act on it?*

1. **One glance.** One value per HUD line. Anything that needs reading belongs in HICOMM.
2. **Never cover the fight.** Nothing permanent in the middle third of the screen. Alerts appear top-centre, below the compass, and leave on their own.
3. **Colour plus shape.** Every colour state also has a glyph: ◆ danger, ▲ warning, ■ ours, ● info.
4. **Plates, not floating text.** HUD text always sits on `plate`.
5. **Diegetic inside, flat outside.** Only the HICOMM screen may show phosphor/CRT effects. HUD, map and notifications are flat: no glows, no shadows.
6. **Same word everywhere** (see Terminology).
7. **Gamepad and mouse.** HICOMM controls are at least `touch-min` (40px at 1080p) tall, reachable by D-pad, with a 2px `rebel` focus outline.

## Colour tokens

Scripts read colours only from `AROR_UIColors` (`AROR_UIColors.Get(AROR_UIColors.TEXT)`). Never hard-code a colour.

**MARS exception:** its explicitly requested identity uses dark red surfaces, rose-red controls and a circular instrument motif. These colours are separate `MARS_*` tokens in `AROR_UIColors`; they do not change alert severity colours or the other phone apps. Its response-type grid and native Readiness/Crackdown sliders use the same typography, spacing and contrast rules.

| Token | Value | Use |
|---|---|---|
| `SURFACE` | #0e1210 | HICOMM screen background, full-screen menus |
| `SURFACE_RAISED` | #16201a | Cards, rows, panels on surface |
| `PLATE` | rgba(8,11,9,0.82) | Every HUD element, toast and map tooltip |
| `PLATE_STRONG` | rgba(8,11,9,0.94) | Alert banners |
| `LINE` / `LINE_STRONG` | #2b3a30 / #5a7765 | 1px dividers, card borders / control borders, unselected tabs |
| `TEXT` / `TEXT_MUTED` / `TEXT_FAINT` | #eef2ee / #a9b5ad / #7f8c84 | Primary / labels and secondary lines / disabled only |
| `REBEL` / `REBEL_DIM` / `REBEL_INK` | #7fe39a / #2f6b42 / #06110a | Us, selected, primary / fills and meter tracks (not text) / text on a rebel fill |
| `WARN` / `WARN_INK` | #f2b84b / #1a1204 | Warnings, levels rising, always with ▲ |
| `DANGER` / `DANGER_INK` | #ff6a55 / #1c0805 | Critical only, always with ◆. Never decorative; rarely more than one on screen |
| `FACTION_USSR` / `FACTION_US` / `FACTION_CIV` | #e8574c / #5b9cf0 / #c9cdc8 | Factions in our own UI. The map uses vanilla faction colours |
| `DEVICE_BEZEL` | #050706 | The phone body only |

`WARN` amber is the only second accent: no purples, teals or gradients. Filled controls put the matching ink on the fill.

## Type

Use the Roboto family that ships with Reforger: Roboto Condensed for headings, labels and HUD values; Roboto for body text; Roboto Mono (or Roboto Condensed with tabular figures) for numbers. All cover Cyrillic. Every text widget in our layouts names `{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt`; weight comes from `SetBold`. Sizes are for a 1920×1080 reference.

| Style | Size / weight | Use |
|---|---|---|
| `screen-title` | 28 / 700, uppercase | One per HICOMM screen |
| `alert-title` | 26 / 700, uppercase | Banner headlines |
| `section` | 18 / 700 | Card headings |
| `hud-value` | 18 / 600 | HUD widget values |
| `label` | 13 / 600, uppercase, tracked | Labels and button text |
| `body` / `body-strong` | 15 / 400 / 600 | Running text / the one thing to read first |
| `caption` | 13 / 400 | Secondary lines. Smallest size allowed on the HUD |
| `data` | 14 / 500 | Prices, times, counts, grid refs |

## Shape, space, glyphs, voice

- Square everything. `radius-sm` (2px) only on chips. Only the HICOMM device body, screen corners and home-screen app icons are rounded.
- Spacing steps 4, 8, 12, 16, 24, 32. HUD elements sit 16px plus the safe area from the screen edge.
- Glyphs: simple shapes on a 20px grid, one colour, tinted in code from one imageset (planned: `AROR_Icons.imageset`).
- Voice: short, plain, in-world. Lead with what happened, then where ("QRF INBOUND · Morton"). Uppercase for titles, labels and banners; sentence case for sentences. No exclamation marks or jokes. Digits for numbers; "$1,800", "8 min".

## HUD and notifications

| Zone | Contents |
|---|---|
| Top-centre | Vanilla compass; alert banners directly below |
| Top-right (or top-left) | REBINTEL status widget |
| Right edge, upper third | Toast stack, newest on top, max 3 |
| Bottom corners | Vanilla. Don't touch |
| Centre third | Nothing of ours, ever |

**REBINTEL status widget** (our version, agreed with the team): both enemy armies on one `plate`. Per army its name in the faction colour, then a Readiness and a Crackdown row: key in `TEXT_MUTED`, value in `hud-value`; Full adds a 5-segment meter. Values, meters and each army's edge stripe use the **stage colours** `LEVEL_1`…`LEVEL_5` (green, yellow-green, amber, orange, red), so red means the Resistance is in trouble. This deliberately departs from the full system's "level 4–5 goes inverse, red only for alerts". A changed row flashes for 1.5 s. Hidden while the map, inventory or phone is open.

**Level-change alerts** (`AROR_RebIntelAlerts`): a top-centre banner for 5 s. The header reads "REBINTEL // CRACKDOWN" (or READINESS / THREATCON) with the game time, then an intercept line in the army's voice ("ПЕРЕХВАТ · РЕЖИМ УСИЛЕН", "INTERCEPT · COMMAND NET"), the headline, "old → new · English" and what it means. Orange for Crackdown up, amber for Readiness up, green for either dropping.

| Priority | For | Look | Time |
|---|---|---|---|
| P1 Critical | Act now: QRF inbound on you, HQ attacked, port being lost | Banner, `DANGER` header band, ◆ | 8 s or dismissed |
| P2 Warning | Things got worse: Crackdown/Readiness up, contested, low funds | Banner, `WARN` header band, ▲ | 6 s |
| P3 Info | Things got better or done: port secured, Crackdown eased | Toast, `REBEL` ■ | 4 s |
| P4 Log | Background events | HICOMM Feed only | — |

One banner at a time (P1 replaces P2). The same event within 10 s updates the open one (×2). Players can turn off P2/P3 pop-ups, never P1. **Banner**: 520px by default, expanding for text up to 900 reference pixels and bounded by the viewport with 24px side margins. Every text row wraps and contributes to the banner height, including the main headline. Use `PLATE_STRONG`, a 1px border in the priority colour, header band (glyph, source such as REBINTEL/HQ/MARKET, game time), headline, where/what line and optional action line. **Toast**: 300px on `PLATE`, ■ glyph, one `body-strong` line plus an optional caption.

| Event | Priority | Headline |
|---|---|---|
| Crackdown up / down | P2 / P3 | CRACKDOWN INCREASED / CRACKDOWN EASED |
| Readiness up (USSR / US) | P2 | COMBAT READINESS RAISED / THREATCON RAISED |
| Readiness down | P3 | READINESS LOWERED / THREATCON LOWERED |
| Territory attacked | P1 if yours with players inside, else P2 | TERRITORY UNDER ATTACK |
| QRF dispatched at players | P1 | QRF INBOUND |
| Port captured / lost | P3 / P1 | PORT SECURED / PORT LOST |

The where/what line shows a level change in the army's own labels: "ЗАЧИСТКА → КАРАТЕЛЬНЫЕ МЕРЫ".

## Map

**Our choice (agreed with the team):** locations are drawn as military-frame symbols, Concept B of the map-icon concepts: a rectangle for the Resistance, a diamond for the enemy, a square for others, a pale faction tint inside, a faction border over a dark keyline, and a stroke glyph per type. Names are not drawn beside the icons; clicking a location opens its info popup (opaque `PLATE_STRONG`, `REBEL` top band, 28 title, 18 values), drawn above every marker. Names come from the surroundings ("Wolf Wood Outpost"; see docs/EVERON_CAMPAIGN_LOCATIONS.md). The full system below asks for native markers only; we have departed from it here.

The design system says the mod draws **no custom map markers**: forces use vanilla NATO military symbols (QRFs, enemy groups, our squads, last known enemy) and places and events use vanilla map-marker icons (towns, ports, HQ, roadblocks, attacks, deliveries), coloured by faction the vanilla way, labelled with our terms plus one detail ("QRF · 3 min"). Markers move by updating position, and are visible to the Resistance only. A P1/P2 alert about a place says "Press M to see it on the map".

## HICOMM (the phone)

A portrait phone (390×844 screen at 1080p) with a status bar (game time; funds in `REBEL`, signal, battery) and a nav bar (B/Esc back, A/Enter main action, Y/Backspace home). Apps open full screen with their title at the top; no pop-up dialogs, confirmations open inline. Home: big clock, one context line, a REBINTEL glance card, app icons 4 per row with `WARN` unread badges.

- **REBINTEL**: Readiness and Crackdown cards: heading with n/5, the army's label large, English line, 5-segment meter (`REBEL` on `REBEL_DIM`), one sentence on what it means; level 4–5 inverse. Then recent intercepts. Never show raw numbers alone.
- **Black Market**: item shop gated by import facilities held. Category tabs with counts, a search box over every category, rows with a picture, requirement lines (amber "NEEDS ..." when missing) and ADD / quantity controls, a slim scrolling cart, and one IMPORT button (no confirmation). Leaders choose PAY FROM faction or personal funds.
- **Imports** (Used Imports): vehicle shop with category tabs and a large picture per row. Two buttons per row, BUY FACTION (Leaders only) and BUY PERSONAL; no cart, click again to buy again. A garage-slots read-out sits in the facility strip.
- **Purchase feedback** (both shops): a popup under the top bar, green "PURCHASE SUCCESSFUL" or red "PURCHASE FAILED" with the reason (not enough funds, garage full), closing itself after a few seconds, with a UI sound. The same purchase again while it is open raises a count.
- **Units**: platoons and squads; recruit and sell, with inline confirmation.
- **Feed**: every P1–P4 message, newest first. **Settings**: status widget on/off, compact/full, top right/left, P2 pop-ups, P3 toasts, CRT effect.
- Controls: square buttons at least `touch-min`; primary is a `REBEL` fill with `REBEL_INK` (one per screen); secondary is a `LINE_STRONG` outline; toggles are full-width rows with a square switch (`REBEL_DIM` track and `REBEL` knob when on).

## Terminology

One name per thing. Code keeps integer levels 1–5; display labels come from `AROR_ThreatLabels`, keyed by the army (faction preset) holding the role.

**Readiness** (was "war level"): how much the enemy can bring. Heading ГОТОВНОСТЬ / Combat readiness (USSR), THREATCON (US).

| Lvl | USSR | US | What it means (proposed QRF effect) |
|---|---|---|---|
| 1 | МИРНОЕ ВРЕМЯ · Peacetime | THREATCON NORMAL · General threat | Garrisons only. QRFs on foot or by truck |
| 2 | ПОСТОЯННАЯ · Constant readiness | THREATCON ALPHA · Possible threat | Motorised QRFs in light utility vehicles |
| 3 | ПОВЫШЕННАЯ · Increased readiness | THREATCON BRAVO · Increased threat | Armed light vehicles join QRFs |
| 4 | ВОЕННАЯ ОПАСНОСТЬ · Threat of war | THREATCON CHARLIE · Attack likely | Armoured cars and APCs join QRFs |
| 5 | ПОЛНАЯ · Full readiness | THREATCON DELTA · Attack imminent | Helicopter insertions and gunship support |

**Crackdown** (was "aggression"): how hard the enemy reacts. Heading РЕЖИМ / Crackdown (USSR), CRACKDOWN (US).

| Lvl | USSR | US | What players see (proposed) |
|---|---|---|---|
| 1 | ПАТРУЛИРОВАНИЕ · Patrols | PATROLS · Routine presence | Patrols on set routes. Slow response |
| 2 | БЛОКПОСТЫ · Checkpoints | CHECKPOINTS · Roadblocks on main routes | Roadblocks; vehicles searched |
| 3 | ЗАЧИСТКА · Sweeps | SWEEPS · Raids on towns | Towns near rebel activity raided |
| 4 | КАРАТЕЛЬНЫЕ МЕРЫ · Punitive measures | REPRISALS · Civilians punished | Bigger, faster QRFs; civilian support drops |
| 5 | ВОЕННОЕ ПОЛОЖЕНИЕ · Martial law | MARTIAL LAW · Curfew | Curfew; armed rebels engaged on sight; every alert gets a QRF |

| Use | Not |
|---|---|
| HICOMM | phone, tablet, PDA |
| REBINTEL | intel app, enemy status |
| Black Market (items), Imports (vehicles) | shop, store, arsenal |
| Import facilities | import points, docks |
| Port / Seaport | harbour; dock (except "Collect at … dock") |
| QRF | reinforcements, response team |
| Territory | zone, sector, area |
| Attention (black market heat) | heat, aggro |
| Squad, Platoon | group, team, unit (for these) |
| Funds | money, cash, supplies |

Cyrillic labels always carry the English line. Don't abbreviate faction labels on the HUD.

## Our conventions (where they differ from the full system)

- Prefix `AROR_`, not `OR_`: `AROR_UIColors`, planned `AROR_Icons.imageset`, string keys `#AROR-…`.
- Layouts here declare named widgets; scripts position and colour them (no absolute styling in layouts).

## Status in code

| Area | State |
|---|---|
| Tokens (`AROR_UIColors`), Roboto in every layout | Done |
| Readiness / Crackdown rename, label tables (read-only: driven by the campaign) | Done |
| REBINTEL app (tabs per enemy, cards, HUD settings) | Done; intercepts list not yet |
| HUD status widget, level-change alerts | Done |
| Banner / toast restyle | Done; P1–P4 field, ×2 merging, game time, glyph imageset not yet |
| Map popup on `PLATE_STRONG`, above all markers | Done; custom map icons still differ from "native markers only" |
| Bank | Baked art (gold/navy, pill buttons) needs replacing |
| HICOMM app, OPS | Fonts only; palette pass pending |
| Black Market and Imports apps (facilities, tabs, search, cart, popups, sounds) | Built; needs a Workbench pass. Attention not yet |
| String table (`#AROR-` keys), Settings / Feed / Units apps | Not started |
