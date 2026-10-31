# Resource-aware QRF planning — no dispatch

The Invader framework now has a server planning entry point, `PreviewInvaderQRF_S(targetId, context)`, on the operation executor. It does not create an operation, reserve stock, change cooldowns, enable Invaders or spawn units. No caller, timer, trigger or default resource provider is installed. Chicken now separately enables money-backed Invader ground capture responses; this preview still has no dispatch side effects.

## Selection inputs

`AROR_QRFSelectionContext` supplies the equipment readiness (`m_iReadinessLevel`, default 1), commander-assessed desired response intensity, minimum tier, location tier ceiling, explicit permission to downgrade and arrival deadline. Callers must supply the current role's readiness when previewing its current capabilities. These are trusted server policy inputs. Automatic threat scoring and location-importance policy are not implemented here; no player positions or unseen player counts are queried.

`AROR_QRFAvailabilityProvider.CaptureAvailability_S(role, factionKey, targetLocationId, now)` is the extension seam for the future military-resource owner. Register an implementation through `RegisterQRFAvailabilityProvider_S(provider)`; the default implementation returns null. A missing provider or snapshot rejects planning instead of creating free stock.

The provider returns `AROR_QRFAvailabilitySnapshot` containing:

- Campaign role/faction and a stable origin location ID.
- Snapshot time and validity window (30 seconds by default; maximum 300).
- Ground route availability and conservative arrival time for the available package types.
- Available group, personnel and vehicle capacity.
- Ready complete-package counts by force-profile ID. Each count must include the crew, infantry, condition, fuel and ammunition needed to deploy that package. Unknown or missing stock means zero.

Profile stock is not a resource reservation. A provider must account for overlapping inventory across profile variants and minimum garrison requirements; callers must not infer that independently quoted profile counts can all consume the same physical assets. A real atomic reservation/commit is still required before future automatic dispatch.

## Server checks and selection

The preview verifies an active campaign/executor, an existing Invader-controlled target and a currently friendly OUTPOST origin distinct from that target. The existing land and player/target launch-distance checks apply. It rejects stale/malformed snapshots and invalid origins; there is no configured-coordinate fallback in this resource-aware preview. The provider chooses the stocked origin; stock-aware nearest-origin searching remains the provider's responsibility.

The executor caps a copied snapshot against actual remaining shared groups/personnel/vehicles, including pending and retained forces. It does not modify the provider's snapshot. The pure `AROR_QRFSelectionPolicy.Select` then:

1. Validates context, role/faction, timestamps, route and ETA.
2. Resolves complete ground recipes against their readiness range and enabled faction-matching profiles, including each profile's minimum equipment unlock.
3. Counts repeated profile requirements correctly: two BTR entries require two ready BTR packages.
4. Checks aggregate profile personnel reservations, groups and vehicles.
5. Chooses the highest affordable authored tier within desired tier, minimum and location ceiling. Ties keep configuration order; tactical family weighting is not yet implemented.
6. Allows a lower tier only when downgrade permission is explicit. With downgrading off, a location ceiling below the desired tier also rejects the response.

The returned `AROR_QRFSelectionDecision` contains the recipe, origin, required budgets, downgrade flag and reason. A non-null recipe means a feasible preview, not authorization to dispatch. Do not pass it straight into spawning: the eventual integration must atomically reserve stock, revalidate ownership/availability, bind the approved plan/origin to an operation, commit spending at dispatch and release unused reservations on cancellation. That lifecycle is not wired here.

Example: desired tier 3 with only one ready BTR can offer the Ural/BTR tier-1 mix if downgrading is permitted. Desired tier 3 with enough assets but only three free groups rejects every current mix. A route that misses the response deadline is rejected even when stock is plentiful. No resource provider, no origin, captured origin or stale snapshot also yields a reason and no units.

## Current boundaries

The existing US truck/LAV fixture still dispatches through its explicit recipe. The new preview is not called by it. The Invader ladder and gate stay unchanged. Mi-8 and mixed land/air selection remain outside this ground planner. Territory/factory production, an economy ledger, automatic threat assessment and stock spending remain future work owned through their respective systems.

Fifteen source-adapted offline checks passed for highest feasible tier, exact budgets, repeated stock demand, downgrade policy, location ceiling, stale/future snapshots, route/ETA, role mismatch, runtime capacity, duplicate/missing stock and absent availability. Engine/profile dependencies were mocked. Workbench-only planner smoke tests exercise the production policy and snapshot-copy behavior through the optional existing test runner; they remain unrun. Native compilation, provider integration and server/multiclient acceptance remain outstanding. No Invader Play test was started.
