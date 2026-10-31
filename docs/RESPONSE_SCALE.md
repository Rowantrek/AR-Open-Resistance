# Randomized attack and QRF strength: levels 1–5

Strength and force type are independent choices. Both enemy roles use the same configurable scale, with their own faction-matching force pools. Every level has an equal default weight of 20 for attacks and 20 for QRFs; all five levels are selectable when the complete package fits the available budgets.

| Level | Description | Motorized | Mechanized | Armour | Airborne (REAPER adapter) |
|---|---|---|---|---|---|
| 1 | Limited | 1 truck + infantry/crew | 1 APC/IFV + infantry/crew | 1 crewed vehicle | 1 transport + infantry + configured gunship escort |
| 2 | Reinforced | 2 packages | 2 packages | 2 crewed vehicles | 2 air packages |
| 3 | Heavy | 3 packages | 3 packages | 3 crewed vehicles | 3 air packages |
| 4 | Major | 4 packages | 4 packages | 4 crewed vehicles | 4 air packages |
| 5 | Maximum | 5 packages | 5 packages | 5 crewed vehicles | 5 air packages |

An air package contains one transport, its infantry/pilot and the profile's required zero or one gunship escort. Five light packages mean five transports; five escorted packages mean five transports and five gunships. REAPER owns the airborne execution path and enforces actor, landing and safety limits. Native acceptance remains pending. Current ground armour is wheeled armour rather than a tank.

Soldier counts come from the selected infantry group and separate crew. Homogeneous levels multiply complete packages. Authored ground and joint recipes deploy their element lists once; their tier does not multiply the list. Joint recipes combine one airborne package with ground elements under shared tactical and funding authority.

## Selection

1. Find levels with at least one enabled, supported, faction-matching profile that can fit its entire group/personnel/vehicle reservation.
2. Choose a level using its attack or QRF weight. Adding more profile variants does not multiply a level's chance.
3. Within that level, choose a force profile using the profile's independent attack or QRF weight.
4. Bind profile and level to the operation once, reserve the full response, then construct its packages.

With all levels feasible, each has a 20% default chance. Profile weights remain motorized 40, mechanized 30 and armour 10, normalized over the supported entries; airborne's configured weight is excluded. If the budget can only fit levels 1–3, those levels are renormalized among themselves. Zero weights disable an individual level for that operation type. No eligible choice leaves the request pending until capacity appears or its existing spawn deadline expires.

Levels do not bypass the native AI queue or alter AI difficulty. The initial package launch positions are offset along the world X axis by `m_fPackageSpacing` (35 m in Chicken); this is a test formation, not automatic convoy/path planning. Inspect the entire launch area for the largest level, not only the first vehicle's position.

All remaining packages must finish materialising and boarding before the operation becomes ACTIVE and movement orders are issued. A destroyed package releases its reservations and is not replaced. Other packages continue; losing the final viable package fails the operation. Configuration/faction/seating errors abort the operation, and cancellation or timeout cleans up every package. Degraded responses retain their original selected level for logging and identity.

## Workbench controls

On Chicken's executor:

- `m_aResponseLevels`: five `AROR_ResponseLevel` entries. Each entry specifies its level and independent attack/QRF weights. An empty array creates the five equal defaults at server initialisation.
- `m_aForceProfiles`: existing faction/type resource definitions and type-selection weights.
- `m_iWorkbenchResponseLevel`: `0` means weighted random; `1`–`5` forces that level for testing, subject to eligibility and budgets.
- `m_sWorkbenchForceProfileId`: empty means weighted random; a valid matching profile ID forces that profile for its role, subject to eligibility.
- `m_fPackageSpacing`: separation between package launch points.

Both overrides are now cleared in the saved Chicken setup. One simulated Occupier QRF is still enabled as the first test. Chicken's executor limits are 20 groups, 128 reserved personnel and 10 ground vehicles, enough in principle for two five-package ground responses using the prepared profiles. The native world AI limit and other AI users still apply.

Reload Chicken from disk before saving the world. Compile in Script Editor, then Play and complete setup. Look for `selected force profile ... response level=N, packages=N`. Count the resulting vehicles and check the package-specific loss logs. Use a forced profile/level and restart to inspect each combination, then clear overrides for random selection. An explicit test override is never used in a packaged build.

## Verification still needed

`AROR_ResponseScaleSmokeTests` exhaustively checks the 100 equal-weight tickets, all five reachable levels, budget filtering, out-of-range tickets and separate attack/QRF weights. It runs with the existing scheduler test control in Workbench. Operation tests check invalid levels and immutable profile/level binding. These Enforce tests have not been executed by Codex.

Runtime acceptance must also cover levels 1–5 for each supported profile and both roles, complete-response reservations, cancellation partway through construction/materialisation, waiting for all packages before activation, one-package loss versus final-package loss, and no replacement or duplicate package after repeated evaluations. The existing 90-second Chicken spawn deadline applies to the entire response; queue saturation may require deliberate tuning.

For aircraft findings and the proposed waypoint state machine, see [AIR_OPERATIONS_RESEARCH.md](AIR_OPERATIONS_RESEARCH.md).
