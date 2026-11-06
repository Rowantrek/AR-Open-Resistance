# PR review: WCS Russia, vehicle radios and response changes

Reviewed on 6 November 2026. Current source: `rowan-balance` at `0075d8d`, plus local radio edits/media. Target: Rowan's **merged [PR #59](https://github.com/Rowantrek/AR-Open-Resistance/pull/59)**, integration revision `57a4369f64bde91961d0e3c11678054e4a77a34b`.

This supersedes the earlier audit's comparison baseline, not its historical test results. GitHub confirmed that `57a4369` and PR #59 head `7c4fc6c` have identical trees (`206b60a9be5a1c5e9f148c4d7466960c5d4abd6c`). PR #59 merged after the earlier audit. That PR's own description says native compilation/gameplay acceptance is pending.

## Decision and prepared integration

User direction: **where our changes overlap a similar introduction, use Rowan's implementation**.

An isolated developer branch, `chicken-radio-wcs-pr`, is prepared in `.tmp/radio-wcs-pr`. The original checkout is preserved. The branch starts at Rowan's merged integration revision, normalizes one mixed line ending, and incorporates our distinct additions.

| Area | Resolution |
|---|---|
| Shared infantry formations, LAT/AT/AA composition | Keep Rowan's `AROR_Formations`, profile/mission formation attributes and garrison logic |
| WCS magazine-switch crash | Keep Rowan's `SCR_AISwitchMagazine` guard |
| US FORECON preset | Keep Rowan's whole current override, including MK153/Stinger changes, corrected AH-6M and Stryker MGS/Dragoon. Drop our competing US kit/aircraft additions from the candidate |
| Base-game import settings | Keep Rowan's current file, including three-facility AT gates. Drop our conflicting first-port edits from the candidate |
| Addon dependencies | Preserve every Rowan dependency, including `6AAE2A3269AD5C39`; add the independent WCS/RHS sources used by our additions |
| Published Ultimate WCS dependency | Exclude `1CAE49450A83DEAB`: its installed project depends on the alternate Ultimate core `6C68A5E5654D7210`, whereas this upstream stack uses `6AA282336705EED2` |
| WCS Russia | Retain our Russian readiness-2 kits and vehicle/static-weapon additions; Rowan's PR did not modify that preset |
| Radio | Retain our dashboard controls, station scheduling, audio project/signal, conversion tool and current local audio assets |
| QRF changes | Retain our distinct farther air origins, heavy Chicken settings and additional joint transports. Known issues below remain release blockers for the affected paths |

The original commit history remains available on `rowan-balance`; dropping competing changes from the candidate does not erase that work.

## Complete seven-commit review

| Original commit | Introduced change | PR disposition |
|---|---|---|
| `6f7e3b3` | Heavy Chicken ground/joint recipes, airborne share 100%, larger actor caps, escorts unlocked early | Retain as scenario balance, describe as behavior change rather than a bug fix; native balance/performance unverified |
| `161f01d` | Independently selected farther air origins, player-distance search and travel allowance | Retain; ownership and clearance defects need correction before release |
| `58a7f64` | Five base-game AT weapons and three rocket entries unlocked at first seaport | Omit from candidate under Rowan-first conflict policy. This commit does not introduce a broad AA shop-gate change |
| `7054533` | Optional extra unescorted transports proportional to joint ground elements, expanded package cap and index changes | Retain; package-specific pricing/live-cost issues need correction before release |
| `7c00549` | WCS/RHS Russian and US kits/vehicles plus dependencies | Retain Russia; keep Rowan's US file and dependency additions; remove alternate-product dependency |
| `f165981` | Radio manager, deterministic station timeline, player-controller requests and vehicle actions | Retain; native audio/replication and modded dashboard contexts need verification |
| `0075d8d` | Radio sample references and audio level changes | Retain and include the previously untracked WAVs/metadata and local range/level edits |

FIELD, personal recruitment, civilians and patrols from the separate local `chicken-dev` are outside this candidate. The candidate preserves Rowan's selected role-base-character/outfitter architecture rather than importing incompatible helper interfaces.

## Findings that prevent release approval

1. **WCS Russia resource availability:** the previous installed-index audit found four absent Russian kit resources, including `Rifle_AK105_SQUAD` and `MG_RPK74M_SPECTER_SQUAD`, plus a helmet and boots. Their GUIDs were absent from all 289 available indexes. Confirm installed versions or reimport valid kits; partial outfitting does not guarantee a usable primary weapon. The discarded US additions account for the other four absent kit resources in the original eight-resource finding.
2. **AT/LAT distinction:** our Russian Readiness-2 AT and LAT overrides both carry RPG-22. The inherited AT kit uses an RHS RPG-7. Since higher-readiness loadouts replace lower ones, starting at Readiness 2 can replace heavy AT with a disposable LAT weapon. Preserve the intended AT/LAT distinction before claiming modernized heavy AT support. Our Russia override contains 12 unit types; it does not introduce an AA override, so AA remains inherited.
3. **Air-origin ownership:** the new independent air site gets a cooldown but its stable ID is not retained for the queued construction ownership check. That check validates the ground origin. Capture/contest the air origin between planning and construction to reproduce the gap.
4. **Extra-transport costs:** physical extras are unescorted, but selection/funding retain the escorted profile quote. Queued construction also uses the profile's two-aircraft live cost instead of the extra's one-aircraft cost. The earlier focused probes reproduce these predicates; the six-aircraft boundary is a supported configuration edge case, not a claim every current 50% Chicken recipe deadlocks.
5. **Air appearance clearance:** the 1,500 m search returns its best candidate even when it cannot meet the distance; final construction only applies the generic 100 m default player check. Enforce per-aircraft clearance or describe it as a preference.
6. **Scenario delivery:** heavy settings live in Chicken; the core mission header selects the main Everon world. Do not describe the normal server mission as having Chicken's balance. A dedicated Chicken header is a separate decision.
7. **Verification debt:** the earlier 21/34 results apply to `0075d8d` and the pre-#59 Rowan baseline `3dba0bd`. They are not evidence that this new integration passes. #59 changes formation APIs and accepts old-garrison-save incompatibility; retain that limitation in our PR notes.

Radio media are copied into the isolated candidate and included in the requested commits, addressing repository delivery of the 18 tracks. Native audio acceptance remains required.

Committed preparation: `688f087` on `rowan-balance` includes all radio edits/media; `a314745` on `chicken-radio-wcs-pr` records the resolved integration. The latter also contains the one-line checkout normalization `088871d`. The PR candidate is committed and has no pending merge conflicts. The review documents and audit evidence are committed separately on `rowan-balance`.

Candidate verification: lexical checks passed for 265 addon scripts; `git diff --check` passed; all 18 WAV headers, file lengths, config durations and metadata GUIDs matched. Rowan's content pack, formations, magazine-switch guard and FORECON override were compared with the merged #59 source and are unchanged apart from checkout line endings. The new dependency remains and the alternate-product dependency is absent. [Candidate evidence](validation/PR_REVIEW_2026-11-06_CANDIDATE.json) records these checks. These checks are not native compilation or gameplay acceptance.

## Conflict evidence

Five files overlap. A three-way simulation found textual conflicts in three: base-game content pack (8 regions), US FORECON override (2), addon project (1). Architecture and executor merge cleanly. The actual isolated merge reproduced the same three conflicted files, now resolved according to the table above. A clean textual merge does not establish compatible behavior.

Evidence: [merge simulation](validation/PR_REVIEW_2026-11-06_MERGE.json), [unit IDs, types and weapon selections](validation/PR_REVIEW_2026-11-06_PRESETS.json), [earlier resource audit](validation/BUILD_AUDIT_2026-11-06_WCS_RHS.json), [earlier force-package probes](validation/BUILD_AUDIT_2026-11-06_PROBES.json).

## Submission notes

- Use a developer branch into **`master-dev`**. No push or remote PR was requested/performed during this preparation.
- Keep the submission a **draft** until resource, package/origin and native verification issues are resolved. The staged/committed candidate is concrete review material, not a release approval.
- Preserve Rowan's formations and magazine-switch fix as baseline work; do not claim them as newly authored here.
- Do not use the old `docs/PR_CHANGELOG.md` as this PR's body: it describes an older broad feature bundle and obsolete balance/testing details.
- Use [the new draft](PR_RADIO_WCS_RUSSIA_DRAFT.md), then update its testing section with actual results on the final commit.
- Required native sequence: Script Editor compile, Play in World Editor with both armies and selected aircraft, then a packaged dedicated server with at least two clients. Check complete kits, real boarding/unloading, both origin ownership changes, exact finance/caps, late-join radio synchronization and every track.

The broad [build audit](BUILD_AUDIT_2026-11-06.md) retains additional shared unfinished systems; those should not be mixed into the authorship or scope of this PR.
