# WCS integration review

Reviewed 31 October 2026 against the combined `chicken-dev` merge `b62116a`, including WCS commits `081b99e`, `88cac17` and `bb4567b`. The review was isolated while the shared workspace was being merged, then updated to that completed merge.

**Result:** the installed dependency chain and the reviewed asset references pass offline validation. The issues below are fixed in source. This is not a native loading or gameplay pass: the merged WCS stack still requires Script Editor compilation, World Editor Play and a dedicated-server test.

## Issues corrected

| Area | Before | After |
|---|---|---|
| Tank classification | Broad APC labels won before name checks. RHS/WCS T-14 variants could enter APC pools at Readiness 3, with APC shop terms. | Recognised tank families take precedence over APC/truck labels. Generated tanks use Readiness 5 and tank import terms. Fuel tankers remain trucks. |
| Static weapons | Every tripod and turret was a static machine gun, including WCS ZU-23 anti-aircraft guns. | Recognised AA/AT weapons get their respective types and readiness gates. They no longer enter the static-MG garrison choices. |
| Armed trucks | An armed ZU-23 carrier could be classified as a troop/supply truck. | Armed trucks use the armed-support vehicle pool, retaining actual crew/capacity validation. |
| Anti-air launchers | Stinger/Igla-style launchers were classified as anti-tank weapons. | Recognised AA launchers use AA pricing and facility requirements. Existing FIA exclusions remain in place. |
| Aircraft compatibility | A gunship needing a separate turret gunner could be accepted by an adapter that only boards one pilot. | Unsupported gunships, unpiloted aircraft, non-helicopters and cabinless transports fall back to the authored aircraft with a reason. Compatible aircraft remain eligible. |
| Launcher dependencies | Only direct dependencies were checked, using folder suffixes instead of actual project GUIDs. | The launcher follows every dependency, checks actual project identities, and rejects missing, cyclic or ambiguous required addons. Explicit repository/base-game projects take precedence over downloaded copies. |

Classification changes apply to generated catalogue entries. They do not rename classes/configs, add WCS dependencies to the core, replace authored preset choices or modify WCS package contents. No catalogue sorting or authored garrison-index renumbering was introduced; correcting generated pool membership can still change indices for previously misclassified modded variants, so existing WCS campaign saves need the restore check below. `WeaponType` retains a default for its new optional name argument, preserving one-argument callers.

## Evidence and scope

- The launcher resolved **48 dependencies**, or **49 projects including the WCS test project**. This includes the core mod, Arma Reforger, engine data, REAPER and all transitive WCS/RHS requirements. Nothing in this installed closure was missing.
- The read-only asset audit verified **639 unique asset path/GUID pairs** from faction preset configs, the base game-mode prefab and the Chicken manager layer against local `.meta` files and installed package/resource-database records. No missing paths or mismatched GUIDs were found in that scope. It does not validate every internal WCS texture, animation, script or prefab inheritance chain.
- All **644 purchasable curated preset entries** inspected across the four base presets have positive prices; units and unsold static weapons were excluded from that count. Generated entries copy curated type terms or use the classifier defaults.
- Installed package inspection confirmed the relevant RHS T-14 and ZU-23 classifications, and that RHS weapon prefabs are present under the `RHS_Core` filesystem used by the existing FIA exclusion rule. No exclusion-source change was necessary.
- The merged shops enumerate the selected Resistance preset plus the other registered presets and send prefab identities rather than catalogue indices. Their cache key changes after catalogue augmentation. The earlier index-order concern does not apply to the merged shop implementation.
- Core dependencies remain base game plus the already documented REAPER exception. WCS remains an optional dependency-loading project. Its hand-authored project GUID still needs the Workbench acceptance described in its README.

Checks executed:

| Check | Result |
|---|---|
| WCS launcher `-PrintOnly` on installed projects | Pass: 48 dependencies |
| `Test-WCSDependencies.ps1` | Pass: 10 fixture checks |
| `Test-WCSAssets.py` | Pass: 639 reviewed references |
| `Test-WCSIntegration.js` | Pass: 36 extracted-method checks |
| `Test-QRFBoarding.js` | Pass: 97 assertions |
| `Test-QRFInterruptions.js` | Pass: 956 assertions |
| `Test-QRFReleaseLifecycle.js` | Pass: 20,588 assertions |

JavaScript checks ran through a V8 runner with mocked filesystem/native services; their Node entrypoints are retained for repeat runs. They exercise source decisions and fixture state transitions, not engine AI or networking. Existing core-only Workbench logs are not evidence that the merged WCS project loads successfully.

## Required native acceptance

1. Launch the WCS test project with its launcher, compile all Game and WorkbenchGame scripts, and check the logs for missing dependencies, unresolved resource GUIDs, duplicate classes and script errors. Open/save the hand-authored test project once in Workbench.
2. Play Chicken and confirm catalogue-augmentation counts for US, USSR, FIA and CIV. Check that Black Market lists WCS items and Imports lists WCS vehicles after augmentation, that prices are non-zero, and that facility/port/air requirements hold on both client and server. Buy items/vehicles and confirm delivery to Storage/Garage.
3. Dispatch ground responses and missions at several Readiness levels. Confirm tanks stay out of the APC pool, armed gun trucks stay out of supply deliveries, and all actual pilot/turret seats are crewed within the reserved budget. Test garrison activation and a saved campaign reload.
4. Dispatch airborne packages. Confirm compatible transports board, fly, land and unload; compatible pilot-controlled escorts must actually fire. Confirm multi-crew gunships produce the documented fallback rather than deploying without their required crew. Supporting those gunships as AI assets requires a separate multi-crew flight-adapter change. Also inspect spawned soldiers' actual weapons: catalogue availability does not itself replace authored AI loadouts.
5. Repeat with a dedicated server and two clients, including joining late and save/load. Finally launch the normal core project without WCS to check optionality. Record exact game/mod versions and native logs before declaring the full stack compatible.
