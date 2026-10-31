# Content Packs

A **content pack** lists everything one content source (the base game, or a mod such as a weapons or vehicles pack) makes available to Open Resistance, with its economy data: what each item costs, when it unlocks, and what the buyer must hold to import it. Packs are independent of factions: with a weapons mod's pack enabled, its guns are sold even when every army uses base-game presets.

The core ships one pack, `BASE_GAME` (`Configs/ContentPacks/BaseGame/AROR_ContentPack_BaseGame.conf`). Open it next to this page as a working example.

## How it works

- A pack is one `.conf` file of class `AROR_ContentPack`, listed in the content pack registry.
- It uses the same categories and types as [faction presets](Faction-Presets), **except Units**: Vehicles (empty, no crew), Gear, Equipment, Weapons, Weapon accessories and Ammunition.
- **Availability:** a pack can name the addons it needs. It only exists in a game while all of them are loaded, so the core can ship packs for popular mods without depending on them.
- **On or off per campaign:** each available pack has a default (Enabled By Default). The admin can turn packs on or off when creating a campaign; the choice is saved with it. Packs whose addons are not loaded are not shown.
- **One catalogue:** the enabled packs are merged in **registry order**. If two enabled packs list the same prefab, or replace the same prefab, the earlier pack wins.

What uses it:

| System | Uses |
|---|---|
| Black Market (phone) | Sells everything except vehicles into Resistance Storage, at each entry's cost and import requirements. Trading needs at least one held import facility. |
| Imports (phone) | Sells vehicles into the Resistance garage, at each entry's cost and import requirements |
| Faction presets | Only use items an enabled pack offers, and use pack replacements |
| Enemy missions and outpost stores | Only field items the army's current Readiness allows |

## Entries

Every entry has a **type** (e.g. a Weapon of type MARKSMAN_RIFLE) and:

| Field | Meaning |
|---|---|
| **Prefab** | The item or vehicle |
| **Cost** | Base price in money |
| **Minimum Readiness** | Enemy Readiness (1–5) at which an enemy army may field it. Default 1. |
| **Import Point Requirement** | `NONE`, `PORT` (the buyer must hold a seaport) or `AIR` (a heliport or airport) |
| **Required Import Points** | How many import facilities (seaports, heliports and airports together) the buyer must hold. Default 0. |
| **Replaces** | Prefabs this entry replaces while the pack is enabled (see below) |

The types per category:

| Category | Types |
|---|---|
| Vehicles | MOTOR_CAR, MOTOR_TRUCK, ARMED_CAR, ARMED_TRUCK, APC, IFV, TANK, AA_VEHICLE, RADAR_AA_VEHICLE, SUPPLY_TRUCK, FUEL_TRUCK, AMBULANCE, HELI_TRANSPORT, HELI_ATTACK, BOAT, STATIC_MG_LOW, STATIC_MG_HIGH, STATIC_AT, STATIC_AA, STATIC_MORTAR, STATIC_ARTILLERY |
| Gear | UNIFORM, VEST, HELMET, HEADGEAR, BACKPACK, FACEWEAR, OTHER |
| Equipment | MEDICAL, BINOCULARS, RADIO, MAP, COMPASS, WATCH, FLASHLIGHT, TOOL, EXPLOSIVE, OTHER |
| Weapons | PISTOL, ASSAULT_RIFLE, CARBINE, SMG, MACHINEGUN, SNIPER_RIFLE, SHOTGUN, LAUNCHER_AT, LAUNCHER_AA, EXPLOSIVE_GRENADE, SMOKE_GRENADE, MARKSMAN_RIFLE |
| Weapon accessories | OPTIC, SUPPRESSOR, BAYONET, UNDERBARREL, LIGHT |
| Ammunition | MAGAZINE, BELT, ROCKET, SHELL, GRENADE_ROUND, MISSILE, BOMB |

Ammunition includes munitions loaded onto vehicles and aircraft as items, such as helicopter rocket pods.

## Replacements

An entry can **replace** other prefabs. While its pack is enabled:

- The replaced prefabs are no longer sold.
- Any faction preset (or loadout) that lists a replaced prefab gets the replacement instead.
- Chains are followed: if A replaces B and B replaces C, presets listing C get A.

This is how a weapons mod plugs into every army at once. For example, a pack whose M16A2 entry replaces the base-game `Rifle_M16A2.et` makes every US Army preset and kit use the mod's M16A2, without touching the presets.

If the replacement takes different magazines or attachments, replace those as well, so kits stay compatible.

## Making a pack

1. **Create the config.** In your addon's Resource Browser, right-click a folder (e.g. `Configs/ContentPacks/`) → **Create → Config File**, pick the class `AROR_ContentPack`, and name it e.g. `MYMOD_ContentPack.conf`.
2. **Fill in the header:**
   - **ID**: unique and permanent, e.g. `MYMOD`. It is saved with campaigns, so never change it after release.
   - **Name**: shown to the admin, e.g. `My Weapons Mod`.
   - **Required Addons**: the GUID of each addon the pack needs, as in its `addon.gproj` (usually your own). Leave empty for an always-available pack.
   - **Enabled By Default**: whether new campaigns start with it on.
3. **Add entries** in each category: type, prefab, cost, and any readiness, import or replacement settings.
4. **Register the pack.** Override `Configs/ContentPacks/AROR_ContentPackRegistry.conf` in your addon and add your pack to its list (see [Modding](Modding#override-in-addon)). Its position sets its priority against other packs.
5. **Test.** Press Play and check the console: it lists the enabled packs and their entry and replacement counts, and warns about entries without a prefab or with an unknown type. Your items should then show in the Black Market or Imports.

A minimal pack, as text:

```
AROR_ContentPack {
 m_sId "MYMOD"
 m_sName "My Weapons Mod"
 m_aRequiredAddons {
  "1234567890ABCDEF"
 }
 m_aWeapons {
  AROR_ContentPackWeapon {
   m_sPrefab "{GUID}Prefabs/Weapons/MyMod/Rifle_M16A2_MyMod.et"
   m_eType ASSAULT_RIFLE
   m_iCost 900
   m_aReplaces {
    "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et"
   }
  }
 }
}
```

## Tips

- **Presets need packs.** If your [faction preset](Faction-Presets) uses your mod's items, your pack must offer them, or the preset skips them (the log says which).
- **Pricing:** use the `BASE_GAME` pack's costs as a scale.
- **Readiness** only limits enemy armies. Resistance players can buy anything the Black Market offers.
