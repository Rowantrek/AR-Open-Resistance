# Vostok retaliation and Crow Hill QRF failure

Native evidence from the Workbench session `logs_2026-10-28_17-45-14`, dated 28 October 2026. This records an observed failure and a source fix; successful native deployment after the fix is still unverified.

## What happened

- At 18:40:30, Workbench explicitly warned that seven script defaults in `AROR_GroundQRFRecipe` and `AROR_ForceProfile` overwrite values from the resource.
- At 18:45:34, force-pool validation rejected `occupier_joint_l1` as an ordinary ground recipe at Readiness 1, then disabled the entire operation executor. The authored resource specifies AIR_GROUND and Readiness 5.
- Vostok counted all four player kills. At 18:49:56 it accepted retaliation `operation_1` from a coastal point and displayed the banner. The operation entered SPAWNING, then failed at 18:51:27 with `Spawn acknowledgement timed out`.
- At 18:51:57 it retried as `operation_2`, displaying another banner despite no confirmed deployment.
- At 18:54:33 Crow Hill was captured by the Resistance and a capture threat was queued for the Occupiers. The capture trigger worked; the executor remained disabled.

The failure was not a missing banner or missing kill threshold. The blocked executor prevented deployment. Earlier mocked tests did not model Enfusion's resource-loading order.

## Correction

Removed declaration initializers from the attributed response family, recipe Readiness range, profile Readiness gate and three pricing fields. Their Attribute metadata retains the same defaults, allowing authored values to load without being overwritten. Script-created smoke fixtures use explicit `CreateForScript()` factories for the former inline defaults; callers still populate required identities, roles, assets and other properties. Extenders constructing these objects directly should do the same or explicitly populate the fields.

Stronghold announcements now wait for ACTIVE or a confirmed tactical dispatch receipt. PLANNED/SPAWNING attempts and failed preparation retries do not announce inbound forces. Repeated polls announce a deployment once.

Regression checks reject attributed declaration initializers, verify script-factory defaults and exercise silent retries versus confirmed deployment. Three additional mutations prove those checks detect the previous mistakes. Current result: **29 suites / 97,366 mocked assertions / 43 mutations**. This does not establish native travel, spawning or rendering success.

## Retest

1. Compile in Script Editor. Confirm the seven `script default value overwrites value from source` warnings are gone for the recipe/profile classes.
2. Stop Play, close/reopen Chicken from disk and start a new disposable campaign so cached resource instances are recreated.
3. Confirm an `Executor configuration` startup line and no `force pool rejected` / `executor disabled` error.
4. Kill four living Vostok defenders as a player. Expect a retaliation request first, then package construction and ACTIVE dispatch. The retaliation banner should appear once after deployment. Failed preparation should retry without repeatedly announcing inbound forces.
5. Capture Crow Hill from the Occupiers. Verify the queued capture threat progresses to selection, construction and activation, subject to normal capacity, origin and timing checks.

Keep the complete next log. A later road, boarding or capacity failure is a separate native issue; this fix removes the observed startup blocker.
