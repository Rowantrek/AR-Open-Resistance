# Faction Presets

A **faction preset** describes an army: the units it fields, the kit each unit wears, its vehicles, and (for the Resistance) the players' starting kit. Items such as weapons and gear are not listed in presets: they come from [content packs](Content-Packs). When creating a campaign, the admin picks a preset for each role. A role keeps its name and colour whatever preset it uses, so the Occupiers can field Soviet units, or a modded army, while staying the Occupiers.

The core ships four presets, written the same way you would write yours:

| ID | Army | Usable for |
|---|---|---|
| `FIA` | FIA | Resistance |
| `US_ARMY` | US Army | Occupiers, Invaders |
| `USSR_ARMY` | Soviet Army | Occupiers, Invaders |
| `EVERON_CIV` | Everon civilians | Civilians |

They live in `Configs/FactionPresets/BaseGame/`. Open one next to this page as a working example.

## How it works

- A preset is one `.conf` file of class `AROR_FactionPreset`, listed in the preset registry.
- Its **usable roles** decide where it can be picked. The setup screen only offers a role the presets that list it, and the server refuses any other choice (falling back to the role's default).
- A preset has two lists: **Units** (AI characters, each with its [loadouts](#unit-loadouts)) and **Vehicles** (empty, no crew). It can also carry a [player loadout](#player-loadout). Every unit and vehicle is a **type** (for example RIFLEMAN or APC) plus a prefab. The types are a fixed core list; entries with any other type are ignored.
- A preset only says what an army *has*. The game's systems (garrisons, QRFs, missions) decide what to pick from it.
- **Vehicles must be in a content pack.** A vehicle must be offered by an enabled [content pack](Content-Packs), or it is skipped (with one warning in the log). If your preset uses a mod's vehicles, ship a content pack for them too.
- **Replacements apply.** If an enabled content pack replaces a vehicle your preset lists, the replacement is used instead. The same goes for every prefab in a loadout.

## Making a preset

1. **Create the config.** In your addon's Resource Browser, right-click a folder (e.g. `Configs/FactionPresets/`) → **Create → Config File**, pick the class `AROR_FactionPreset`, and name it e.g. `MYMOD_Preset_Marines.conf`.
2. **Fill in the header:**
   - **ID**: unique and permanent, e.g. `MYMOD_MARINES`. It is saved with campaigns, so never change it after release.
   - **Name**: shown to the admin, e.g. `US Marines`.
   - **Usable Roles**: the roles it may be picked for.
3. **Add entries**: pick each entry's **type** and its **prefab**.
   - Units: at most one per unit type (RIFLEMAN, AUTORIFLEMAN, MACHINEGUNNER, GRENADIER, MEDIC, MARKSMAN, SNIPER, AT, AA, ENGINEER, RADIO_OPERATOR, LEADER, CREW, PILOT, OFFICER, CIVILIAN).
   - Vehicles: as many entries per type as you like.
4. **Give units their kit.** See [Unit loadouts](#unit-loadouts).
5. **Register the preset.** Add a script to your addon (e.g. `Scripts/Game/MyMod_FactionPresetRegistry.c`) that adds your preset's path. An addon can register several presets, and several addons can register presets without clashing:
   ```
   [BaseContainerProps(configRoot: true)]
   modded class AROR_FactionPresetRegistry
   {
   	override void GetExtraPresetPaths(notnull array<ResourceName> outPresets)
   	{
   		super.GetExtraPresetPaths(outPresets);

   		outPresets.Insert("{GUID}Configs/FactionPresets/MyMod_Preset.conf");
   	}
   }
   ```
   Copy the preset's path with its GUID from the Resource Browser (right-click the preset → Copy Resource Name).
6. **Test.** Press Play and check the console for `[AROR] Faction preset` warnings. Your preset should then appear in campaign setup for its roles.

A minimal preset, as text:

```
AROR_FactionPreset {
 m_sId "MYMOD_MARINES"
 m_sName "US Marines"
 m_aUsableRoles {
  OCCUPIERS
  INVADERS
 }
 m_aUnits {
  AROR_FactionPresetUnit {
   m_sPrefab "{GUID}Prefabs/Characters/MyMod/Character_Marine_Rifleman.et"
   m_eType RIFLEMAN
  }
 }
 m_aVehicles {
  AROR_FactionPresetVehicle {
   m_sPrefab "{GUID}Prefabs/Vehicles/MyMod/Humvee.et"
   m_eType MOTOR_CAR
  }
 }
}
```

## Unit characters

A unit's **prefab** is its character. You can either:

- **Name a character** that already wears its gear (as the base-game presets currently do), or
- **Leave the prefab empty.** The unit then uses its role's default gear-less character, from `Configs/FactionPresets/AROR_DefaultUnitPrefabs.conf` (e.g. `AROR_Character_Occupiers_Rifleman`), and its [loadout](#unit-loadouts) dresses it.

The default characters inherit a base-game rifleman of the role's *default* army, so they carry that army's faces and voices. If your army sounds different, make your own gear-less characters and name them in your preset.

To make a gear-less character, create a prefab inheriting a base-game character, then in Prefab Edit Mode empty:

- every slot in `BaseLoadoutManagerComponent` (hat, jacket, vests, pants, boots, backpack…)
- `CharacterWeaponSlotComponent` and `CharacterGrenadeSlotComponent` weapon templates
- the equipment slots (watch, binoculars) under `SCR_EquipmentStorageComponent`
- every group in `SCR_InventoryStorageManagerComponent` → Initial Inventory Items

Then set its faction in `SCR_CharacterFactionAffiliationComponent`. Drop it into a world to check that it stands there with nothing on.

## Unit loadouts

A unit can carry its full kit in its **Loadouts** list. Each loadout has:

- **Clothing**: each piece worn (the slot comes from the prefab itself) and its **contents**, e.g. the pouches on a vest.
- **Weapons**: each weapon carried (the slot comes from the prefab), its **attachments**, and optionally its loaded **magazine** (empty keeps the weapon's own).
- **Items**: loose items such as a watch or binoculars.

Items can contain items: a pouch lists the magazines inside it. **Listed contents are complete**: when a loadout is applied, a container's own default contents are removed first, so the unit gets exactly what you list. Use **Count** for several of the same item.

### Readiness variants

**Readiness** is how far an enemy army has escalated, from 1 to 5. It rises as the Resistance captures that army's locations and never falls. A unit can have several loadouts, each with a **Minimum Readiness**:

- The loadouts with the highest minimum readiness at or below the army's current readiness are used.
- Among several at that level, one is picked at random by **Weight**.
- If every loadout needs a higher readiness, the lowest one is used.

So a rifleman can start with an iron-sight rifle at readiness 1, and get an optic and body armour from readiness 3. Variants are optional: one loadout at readiness 1 is enough.

Every prefab in a loadout goes through content pack **replacements** (so a weapons mod's pack can swap the rifles every army carries), but content pack readiness does *not* gate loadout items. The variant's own readiness decides.

> **Status:** unit loadouts are data only for now. AI still spawn with their character prefab's own kit until the AI outfitter is added.

## Player loadout

Players always join the **Resistance**, and the Resistance player character has no gear. A preset usable for the Resistance must carry a **Player Loadout**: the kit players spawn in. It uses the same format as a unit loadout (clothing, weapons, items), with no readiness or weight.

When a player spawns wearing nothing, the server dresses them from their role's player loadout. A Resistance preset without one logs an error, and its players would spawn with no gear.

## Getting kits from existing characters

Writing a loadout by hand is slow. To copy kits from existing characters, look at how the core does it: a Workbench-only option on `AROR_FactionPresetsComponent` (**Import Base Unit Loadouts**) spawns the base presets' infantry in Play mode, reads what they carry, and writes it into the preset files as readiness 1 loadouts. It currently covers the core's base presets only, but the base-game loadouts it produced are a good reference for your own.
