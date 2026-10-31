# Modding

Open Resistance is built to be extended from your own addon, without editing the core mod. You add configs and prefabs in your addon and register them with the core through **Override in addon**.

Start here:

| Page | Use it to |
|---|---|
| [Faction Presets](Faction-Presets) | Add an army: the units, vehicles and equipment a faction fields, the kit each unit wears, and the players' starting kit. |
| [Content Packs](Content-Packs) | Add equipment for sale and use: prices, when items unlock, what must be held to import them, and swapping base-game items for a mod's versions. |

The two work together. A **faction preset** says *what an army has*; a **content pack** says *what each item costs, when it becomes available, and which version is used*. A preset can only use items that an enabled content pack offers (units excepted).

## Before you start

- **Dependency:** make your addon depend on Open Resistance (`OpenResistance`) as well as Arma Reforger.
- **Prefix:** name your classes and files with your own prefix. `AROR_` is reserved for the core.
- **IDs are saved:** preset and pack IDs are stored in campaign saves. Never change an ID once released.
- **Workbench:** create configs in Workbench (Resource Browser → right-click → Create → Config File) so their `.meta` files and GUIDs are generated for you.
- **Check the log:** Open Resistance validates presets and packs when the world loads. Look for `[AROR]` warnings and errors in the console after pressing Play.

## Override in addon

Both systems are registered through a registry config in the core:

- `Configs/FactionPresets/AROR_FactionPresetRegistry.conf`
- `Configs/ContentPacks/AROR_ContentPackRegistry.conf`

To add yours, find the registry in the Resource Browser, right-click it, choose **Override in "YourAddon"**, and add your config to its list. Your addon then stores only your additions.

> **Note:** that additions to these plain lists merge with other addons' additions (rather than one addon's list replacing another's) has not been verified with two addons loaded together yet. If you hit this, report it.
