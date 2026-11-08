# Add WCS Russia equipment, vehicle radios and QRF options

**Working draft — not submitted.** Targets `master-dev` after [Rowan's PR #59](https://github.com/Rowantrek/AR-Open-Resistance/pull/59). See [submission review](PR_SUBMISSION_REVIEW_2026-11-06.md) for unresolved findings.

## Summary

Add Readiness-based WCS Russia equipment and vehicle music radios, plus farther airborne launch origins and extra transport options for joint responses. Preserve Rowan's shared formations, WCS magazine-switch fix, FORECON preset and overlapping import settings.

## Changes

- **Faction presets / WCS Russia:** add Readiness-2 kits for 12 Russian unit types and expand the Russian vehicle/static-weapon selection. Keep Rowan's US FORECON override. Preserve upstream dependencies and avoid pulling the alternate Ultimate core into this addon stack.
- **Radio:** dashboard power/volume/station controls, a synchronized deterministic station schedule, bounded nearby client audio, shared vehicle radio state and empty/destroyed-vehicle shutoff. Include the station audio project, signal, 18 WAV resources with metadata and conversion tool.
- **Operations:** select farther air launch sites separately from ground origins; allow additional unescorted joint transports based on the ground package count.
- **Chicken scenario:** heavier authored ground/joint recipes, 100% target airborne share and larger reservation limits. The normal Everon mission retains its own scenario settings.
- **Documentation:** describe system behavior, review findings and pending acceptance.

## Testing

- Compared against merged PR #59 and reproduced/resolved the three overlapping-file conflicts in an isolated developer worktree.
- Checked inherited unit IDs, Readiness variants and weapon selections against both sides.
- Candidate lexical checks passed across 265 addon scripts; whitespace checks passed. All 18 WAV headers/file lengths, config durations and metadata GUIDs matched. Critical Rowan files match #59; dependency inclusion/exclusion checks passed.
- Earlier source/resource audit and mocked policy probes are linked in the submission review; historical suite counts do not certify this final integration.
- **Not compiled or played by this preparation.** Required: Script Editor compile, World Editor Play, then dedicated-server/multiple-client tests of kits, AI boarding/flight/unloading, cleanup, finance and radio synchronization.

## Notes

- **Known draft blockers:** locally unavailable Russian kit assets, AT/LAT loadout distinction, independent air-origin ownership revalidation, unescorted transport quote/live-cap consistency, and strict launch-player clearance. Resolve before requesting release approval.
- **Extender mods:** new radio component/station types, a base vehicle action override, additional operation attributes and a larger accepted recipe package count. Modded vehicles need supported dashboard interaction contexts. New dependencies must be loaded on server and clients.
- **Saves:** radio state is session-only. This PR does not restore live QRF deployments. Rowan's #59 already changes saved garrison variant semantics and explicitly accepts old-garrison-save incompatibility; this submission does not repair that migration.
- **Replication/performance:** the server validates radio requests and owns radio/operation state; audio plays only on clients with bounded simultaneous sources. Heavier Chicken responses require populated-server performance acceptance.
- **Preserved upstream work:** formations, mission formation support, FORECON launcher/Stryker changes, main Everon readiness and the WCS magazine-switch crash guard belong to #59, not this submission.
