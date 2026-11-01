# Fortified outpost trials

Chicken has equipped fortified layouts at all 14 ordinary outposts and four military outposts. Chotain Military Outpost retains its authored castle. Stable location IDs, names, owners, root poses and capture-flag transforms are preserved. Airport, factory and resource compounds are unchanged. The current bindings live in [FORTIFIED_OUTPOST_TRIALS.json](FORTIFIED_OUTPOST_TRIALS.json), separately from the historical catalogue.

## Current layouts

Prefab names below are in `Prefabs/Compositions/Locations/`. Each compact geometry prefab has `_Equipped_US.et` and `_Equipped_USSR.et` wrappers with four native supply containers.

| Geometry family | Footprint | Chicken sites | Fresh defenders |
| --- | --- | --- | --- |
| `AROR_Defence_Outpost_CompactFortified.et` | 36 x 36 m | Ten ordinary sites, including Laruns and Durras | Four four-person infantry teams; two crewed statics |
| `AROR_Defence_Outpost_CheckpointFortified.et` | 28 x 28 m | Quarry, Old Wood, Andres Beacon, Chotain Industrial | Same ordinary roster, with a compact checkpoint layout |
| `AROR_Defence_MilitaryOutpost_CompactFortified.et` | 48 x 48 m | Levie and Morton military outposts | Six four-person teams; four crewed statics; one manned vehicle |
| `AROR_Defence_MilitaryOutpost_CoastalFortified.et` | 40 x 40 m | Lamentin military outpost | Same military roster in a smaller coastal compound |
| `AROR_Defence_MilitaryOutpost_Fortified.et` | 64 x 64 m | Military Hospital, using `MilitaryOutpost_Equipped_US.et` | Same military roster plus one spare uncrewed armour hull |

The original 48 m ordinary and 64 m military prefabs remain available as larger prototypes. The compact military recipes explicitly omit only the spare uncrewed hull through `m_bOmitParkedArmour`; the manned vehicle and all weapon crews remain. Existing saved rosters keep their old count and casualties. Use a fresh campaign to test the revised recipes.

The compact pass follows the first native audit, which rejected all 18 larger trial footprints before grounding: 13 terrain-relief failures and five pad/arc failures. Results remain in [EVERON_FORTIFIED_OUTPOST_AUDIT_INITIAL.json](EVERON_FORTIFIED_OUTPOST_AUDIT_INITIAL.json). Seventeen sites now have smaller footprints; Hospital retains its larger layout for a detailed arc check. Earlier accepted layouts informed the sizes, but their measurements do not certify these new props or markers. Several saved site poses also differ from that historical baseline. No proposed relocation was automatically adopted, and terrain/collision gates remain enabled.

## Fortifications and equipment

The 36 m outpost has high/low sandbag perimeter sections, two low gun revetments, separate infantry sectors, a camouflaged command shelter, a supply row and an offset south entrance. The 28 m checkpoint trades the shelter for a command desk, distributes supplies and keeps a pedestrian opening. Military variants provide four outward-facing gun positions, separate infantry sectors, crew staging and a south entrance aligned with the manned vehicle. The 48 m variant includes a command shelter; the 40 m coastal layout uses a desk to preserve clearance.

Every equipped site has **weapons, ammunition, equipment and medical containers**: 72 boxes across the 18 sites. All current sites use US stores (Durras switched from USSR when the Invader foothold moved to [Fortress Vostok](STRONGHOLDS.md), which has its own USSR stores). These are native `EquipmentBoxWooden_*` prefabs, not decorative boxes or a new unlimited arsenal. The base-game prefab owns inventory population, access and replication. Their actual stock, access, depletion and save/load behaviour still require native acceptance.

`AROR_OutpostSupplyComponent` makes one delayed server-only inventory observation and logs `[AROR OUTPOST STORE] site=... category=... observedItems=...`. It never fills or restocks a container. Boxes remain independent of garrison proximity activation and ownership changes; changing the campaign faction preset does not retheme them. Compact wrappers retain the earlier equipped wrappers' container child IDs. This preserves authored identity, but does not establish native inventory save/load compatibility.

Fresh ordinary garrisons select M2 + M60 for US assets or NSV + PKM for USSR assets. Military sites have four guns: default heavy MG, medium MG, specialist heavy-MG variant (M2 APIT / NSV SPP), then the default heavy MG. Existing role bindings choose personnel and vehicles. Actual compartments and observed seating remain authoritative; weapon crews count against the shared personnel budget.

## Workbench check order

1. Stop Play, compile Game and Workbench scripts, discover the new resources and **reload Chicken from disk**. The layouts are already bound to their locations. Do not save a stale open scene over the updated layer.
2. Run **Ground fortified outposts (preserve Chotain castle)**. Inspect walls, shelters, desks and stores, then **Save the world**. This undoable command excludes Chotain castle and the strategic installations.
3. Run **Audit fortified outposts (current placements)**. It checks all 23 military locations once at their current poses, without a clearing search. Failed scene-wall checks report up to eight segment positions per site; blocked spawn pads and MG arcs report their kind/ordinal and world position. The original **Audit military defence sites** remains available for a bounded relocation search.
4. Start a fresh campaign with **New campaign**, keeping the default base-game armies. Approach within 1 km and stay off spawn pads while deployment finishes. Check two ordinary/four military gunners, infantry and the military vehicle's driver/weapon crew. Scenery is visible in edit mode; defenders appear during Play.
5. Open all four containers, inspect supply logs, take items and check that inventory decreases. Leave/return, capture and save/load must not grant replacements. Check entrances, flag access, gun fire, shelter access, vehicle clearance and AI navigation. Clear living defenders, capture and verify the capture-driven QRF.

The shared personnel budget can delay nearby garrisons. A terrain/pad audit is not full deployment or navigation acceptance. Record each site separately; a passing prototype does not approve the entire island.

## Gun embrasures

Static guns sit on low tripods and fire from about 0.6 m, so even a low sandbag wall directly in front blocks them. Every defence composition therefore has an open embrasure: perimeter sandbags in a gun's +/-15 degree firing lane, out to 30 m, are left out (`tools/GunEmbrasures.ps1`). `tools/Open-GunEmbrasures.ps1` applies this to the existing compositions and removes world-layer overrides of the removed pieces; without `-Apply` it checks that no lane is blocked. The preset and stronghold generators apply the same rule. At runtime the gun placement check also traces the first 8 m at tripod height, so a gun is never placed facing a prop.

## Checks completed

- Installed-resource validation passes 53 native/local references. New resource GUIDs are hand-authored with matching metadata; Workbench discovery remains required.
- Actual-source mocked checks pass 187 garrison, 37 crew, 56 editor-audit, 21 grounding and 12 supply-observation assertions (313 total).
- `Test-FortifiedOutpostLayouts.js` passes 1,549 estimated authoring assertions across four compact geometry families and eight US/USSR equipped wrappers: marker coverage, actor spacing, prop/pad clearance at arbitrary root yaw, flag space, outward high-cover gun rays, container IDs, walking routes and military vehicle entrance lanes.
- All 23 military IDs and 18 equipped bindings remain; 72 stores are bound. Compact revisions preserve all site poses/flag transforms. Chotain castle and the four strategic installation root blocks remain unchanged.

Estimated envelopes are not measured model bounds. Native compile, terrain fit, grounding, actual collision, inventory, seating, replication and Play acceptance remain pending for the revised layouts. Keep the trial manifest separate from the historical CSV and wall-offset archives. The campaign-layer generator deliberately refuses `-Apply` while these overrides exist, preventing accidental loss of the castle and authored trial bindings.

Extenders can configure static variants and counts on the existing garrison bindings and inherit the geometry/equipped wrappers. The optional compact recipe switch defaults off, preserving existing templates. Server and clients need matching updated scripts/resources. No new economy, spawn authority or ownership system is introduced.

## Towers and fighting positions

`tools/Add-OutpostFortifications.ps1 -Apply` adds AI fortifications to the shared outpost, military base, factory and depot compositions (the strongholds have their own generators):

- a US guard tower beside the north-wall gun pit, and beside the east flank gun at the larger sites. Each tower is a sentry post (`AROR_SentryPostComponent`): the garrison posts one marksman on it as his own group, and he holds the platform without move orders. A tripod gun cannot fire over the tower's sandbag parapet, so the guns stay in their pits. Lamentin's coastal base has no room for a tower.
- a low sandbag fighting position 5-8 m in front of each infantry team position, where it fits without crowding a spawn position, a gun lane or another prop.
- a bunker where a layout has none and one fits; none currently fits by the layout test's clearance rules. Every existing bunker is turned to face out of the compound (to the nearest quarter turn) and is a ground-level sentry post: one soldier holds it.
 it again after regenerating presets with `Set-MilitaryDefencePresets.ps1 -Apply`, which rewrites those compositions. Without `-Apply` it checks the additions are present. The tower floor height (3.5 m) is an estimate until checked in Play; without a platform the marksman stands at the tower's foot. Bunkers and slotted compositions must not repeat their prefab's `Hierarchy` component: Workbench rejects the whole component block. The script strips any it finds.
