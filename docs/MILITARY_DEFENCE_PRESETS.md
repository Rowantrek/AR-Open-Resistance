# Military defence presets

Two separate fortified prototypes add detailed ordinary/military outposts with stronger garrison recipes, sheltered bays and protected gun positions. Chicken now trials them at all 14 ordinary outposts and four military outposts, preserving Chotain castle. The eleven original prefab resources remain available; see [Fortified outpost prototypes](FORTIFIED_OUTPOSTS.md) for the current bindings and native test steps. Historical audit totals below do not accept the replacements.

Eleven reusable location compositions live in `Prefabs/Compositions/Locations/`. They inherit the existing military location templates and keep their control, flag, persistence and proximity garrison components. Town/City/HQ use their existing templates; settlements do not gain walls, flags or generated defenders.

| Preset | Nominal footprint | Contents at the maximum default defence tier |
| --- | --- | --- |
| `AROR_Defence_Outpost.et` | 36 x 36 m | Low sandbag perimeter, south entrance, two infantry-team positions, one outward-facing MG position; no hull pads |
| `AROR_Defence_MilitaryOutpost.et` | 64 x 64 m | Six team positions, three MGs, parked armour pad and one manned-vehicle pad |
| `AROR_Defence_Resources.et` | 52 x 52 m | Six team positions, three MGs, parked armour and one manned hull; the rolled level chooses which are populated |
| `AROR_Defence_Factory.et` | 56 x 56 m | Same maximum role positions, intended beside an existing factory rather than replacing industrial scenery |
| `AROR_Defence_Port.et` | 56 x 56 m | Same maximum role positions; the complete compound must sit on dry land with usable access |
| `AROR_Defence_Airbase.et` | 88 x 88 m | Eight team positions, four MGs, parked armour and three manned hull pads; an airbase defence compound, not an entire airport recreation |

The sandbag walls are actual base-game prop children with a nominal 10 m south entrance. Each carries `Hierarchy` and an `AROR_DefenceWallComponent` tag so editor grounding can find attached walls independently of their names. MG/infantry/vehicle locations are editable, named `AROR_DefencePositionComponent` marker children. Weapons, role-specific vehicles and crews are spawned by the garrison at the 1 km activation distance, including after teleporting inside the site. There are no duplicate permanent US guns or crews on Invader sites. Empty marker entities carry no visual model; select them in the editor hierarchy. Walls remain static scenario geometry rather than proximity actors.

The flag is at the preset centre, with the model base 0.1 m above its nominal placement plane and the capture context at 1.6 m. The preset root must be placed on the accepted terrain plane. Wall segment pitch, model pivots, joints, gun muzzle clearance, door/path access and native navmesh behaviour need a Workbench inspection; installed GUID validation cannot establish those properties. These prefabs are authored first versions, not native-certified compounds.

## Compact garrison layout

Legacy military locations now use the same standard positions around their actual flag. A layout's size is independent of the location's much larger capture radius. The server samples a 7 x 7 grid across the full rotated footprint, including a two-metre margin. Any water or more than 2 m total relief rejects activation. Rejected terrain does not consume the active garrison personnel budget. This is a conservative placement gate, not terrain flattening.

Infantry teams start in opposed perimeter sectors and receive 6 m defend radii. Native AI can still manoeuvre and engage enemies; this does not tether soldiers or disable combat. MGs sit four metres inside the wall line, face outward and require three clear 20 m visibility rays. Individual positions can adjust by at most 1.5 m; a blocked position cannot fall back into bushes elsewhere in the capture area. Four-person team footprints and vehicle/MG footprints still need obstruction/navmesh checks. Preset marker transforms take priority over the standard layout. Existing world-space pad arrays apply only to MG/hull pads, in roster order.

Version 3 saves logical slot choices, casualties and health rather than prefab paths or transforms. Versions 1/2 migrate without restoring obsolete positions; survivors bind to current role assets and validated pads. Partial record corruption withholds affected slots without granting replacements or discarding unaffected records. Unsupported headers quarantine the roster.

Levie Military Outpost is an explicit exception to the default 2m whole-footprint relief limit: its measured 2.78125m height range has a 3m allowance in the catalogue's `DefenceMaxRelief` field. Levie now uses a measured 48m compact layout; the allowance remains explicit. All per-position water, slope, clearance, navmesh and MG arc checks still apply. Runtime and editor auditing use the same site allowance. Native activation/individual-pad acceptance is pending; this configuration is not a measured approval of the entire compound. Other sites retain the 2m limit.

The subsequent native Play run deployed Levie infantry and assets, but some MG pads remained blocked. The corrected capture gate no longer treats unused recipe positions as defenders once real personnel deploy and native queues resolve. Individual pad failures have bounded retries and can omit unqueued positions for the current activation without recording losses. This keeps sites capturable after their physical defenders are cleared; it does not validate those failed positions or replace the need for a tailored compound.

## Historical Chicken placement

The baseline described below predates the current fortified trials. Chicken now uses smaller equipped variants at 17 outposts and a larger equipped Hospital layout; their native grounding, current-placement audit and Play checks are pending. See [FORTIFIED_OUTPOSTS.md](FORTIFIED_OUTPOSTS.md). The older 23-site pass does not approve these replacements.

Compact templates support these drafts: `OutpostCompact` (28 x 28m), `MilitaryOutpostCompact`, `FactoryCompact` and `ResourcesCompact` (48 x 48m). They retain the original recipe roles. Custom military team/MG positions preserve vehicle clearance; standard cardinal team positions are moved inward to avoid the former 4m conflict with MGs. All pad spacing is checked offline against the runtime clearance rules. Crew staging uses 3m player clearance so standing on the flag does not block the 4m staging ring; infantry and vehicle pads retain the normal 6m clearance and native collision checks remain active.

`DefenceAudit` selects Original, Dense, Placed, Grounded, Followup or explicit Pending provenance. The airport retains its fixed root and translated defence children. Authorised factory/resource moves affect Chicken only. Audits reserve earlier chosen footprints to prevent overlapping proposals.

New preset GUIDs are hand-authored and checked against sibling metadata. Keep adoption archives. After native grounding and Save, run `Import-DefenceWallGrounding.ps1 -LogPath <script.log> -Apply` before regenerating layers. Saved wall offsets are recorded in `EVERON_DEFENCE_WALL_PLACEMENTS.json`; only matching site poses reuse them. Relocations, rotations and preset changes invalidate old offsets.

1. Stop Play, compile Game/Workbench scripts, discover all eleven preset resources and reload Chicken from disk.
2. Run **Ground military preset walls**, inspect the perimeters and Save. It grounds actual model bases in one Undo action and warns if no attached tagged walls are found.
3. Run **Audit military defence sites**. The dense 25m-grid search uses eight orientations within the configured editor radius: normally 800m, 1200m at East Resources. Fixed-anchor sites stay within their capture circle. Archive each completed audit separately with `Import-DefenceSiteAudit.ps1`; preserve adoption evidence.
4. Check flag reach, gun arcs, infantry clearance, vehicle exits and practical access in Play. Verify the 1km activation and clear/capture/QRF sequence. A terrain/box audit is not navigation or lifecycle acceptance.

Andres Beacon near Gillnet Pond uses its measured replacement clearing about 783m from the original flag; the latest rotation resolves a rejected MG arc. Inspect the changed orientation in World Editor and Play.

After the final East Resources move, native grounding completed all 1526 walls with zero warnings. The world was saved and the measured offsets were imported. All 23 current footprints passed the subsequent audit; continue with Play testing.
