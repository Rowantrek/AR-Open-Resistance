# QRF strategy, prices and response lifecycle

Status: source implementation and offline checks complete; **new Workbench compilation, multiplayer and combat acceptance are pending**. Both opposing roles use the same framework. Chicken enables strategic economy and visual spotting; Invader operation admission and REAPER dispatch remain disabled.

## What happens after an attack

1. An assigned defender's validated hostile damage, or a canonical settlement/outpost capture event, supplies a trusted role/location report. No player RPC can create a threat or change war level.
2. The manager coalesces reports into at most 32 pending threats. Repeated reports for one role/location contribute evidence at most every 30 seconds. A live QRF for the same target is reused.
3. Every five seconds, each role considers its highest-scoring eligible threat. Importance, sustained fighting, capture and bounded waiting age affect priority. An unaffordable threat waits; after its retry is deferred, another candidate can be considered on a subsequent tick. Existing per-role cooldown, outstanding-operation limits and conflicting-target checks still govern admission.
4. Random variation is rolled once for that queued threat; its ceiling can still react to new evidence/war pressure while waiting. On admission the desired intent is fixed. The executor finds the highest authored tier at or below that intent which fits money and actor budgets. The complete allocation is reserved atomically before spawning; a second response cannot reuse its collateral. No affordable tier means the threat remains queued before admission. A race or later capacity change can still block an admitted operation until its normal spawn timeout.
5. Existing construction, boarding, route planning and tactical execution deliver the response. The activation fee is charged once when the response becomes ACTIVE. Later waves belong to that allocation and do not charge another deployment fee.
6. Vehicles unload, infantry investigates the reported contact/town and searches sectors, and support vehicles receive separate road cover orders. Physical progress and timeout diagnostics distinguish arrival from merely issuing an order. Native AI retains navigation, perception and combat.
7. Normal withdrawal sends survivors toward a friendly origin. Ground recovery requires living personnel within 100 m and a surviving vehicle within 125 m of that origin in Chicken, with no player-commanded groups. The executor exposes both recovery radii as attributes. A returned package leaves the tactical roster without a casualty/loss notification. When no operating or virtual-reserve package remains, a response with recovered survivors can complete its task; that result never flips ownership or declares a territorial victory.
8. Returned or terminal actors still obey retention and all-player cleanup rules. Dead members, wrecks and nearby survivors are not immediately deleted. Missing origin/target, casualties and operation deadlines retain their existing safety handling.

The latest native run before this strategic work verified truck delivery, tolerated the emptied truck's cleared faction, and continued infantry/LAV movement. It ended before an infantry arrival message. The new return/completion behavior has only offline coverage so far.

## Prices and payments

The existing `AROR_EconomyComponent` remains the only bank. Income, Resistance money/manpower, settlement control and Rowan's serializers are unchanged. The operation manager owns a QRF finance adapter, not a second balance registry.

**Required available funds = full asset replacement collateral + one deployment overhead.** Collateral is a session reservation against the bank, not an upfront purchase debit. It makes other QRFs account for the value at risk while preserving the requested rule that surviving assets cost only the deployment overhead.

**Overhead = 50 + 10 × bound response tier**, so tiers 1–5 cost 60/70/80/90/100 to deploy. These are campaign balance units, not real-world equipment prices. Manager attributes expose both terms.

| Asset | US/Occupiers loss value | USSR/Invaders loss value |
|---|---:|---:|
| Soldier or crew member | 25 | 25 |
| Transport truck | 200 | 200 |
| IFV/APC | 800 (LAV) | 700 (BTR) |
| Armour support | 800 (crew-only LAV) | 450 (BRDM) |
| Transport helicopter | 1,600 | 1,800 |
| Escort gunship | 2,000 | 2,000 |

Prices live on each `AROR_ForceProfile`: `m_iPersonnelLossValue`, `m_iVehicleLossValue`, `m_iEscortLossValue`. A negative main-vehicle value uses the shared type fallback; Chicken explicitly authors each asset value. The same US LAV has the same hull value in mechanized and armour profiles. Crew and passengers are charged separately when lost. An airborne quote includes both aircraft and the profile's full personnel capacity.

Collateral uses **reserved personnel slots**, which can exceed actual spawned personnel. Only registered physical assets incur loss charges. An unused slot or unspawned wave never becomes a casualty bill. Cancelling an unspawned ground/air reserve releases its quoted collateral. Closing an operation releases unmaterialized collateral while retained physical assets continue to hold theirs until normal retirement or loss.

Confirmed dead characters, destroyed vehicles and non-native faction captures are charged once per registered asset. An empty vehicle's cleared/native affiliation is not capture. An absent or engine-virtualized entity alone is not evidence of death. Cleanup observes losses before deleting actors and retires living assets without a loss charge; departing transport aircraft/pilots do the same before their normal removal. Pilots dying and hulls being destroyed are separate losses. Surviving but stranded hulls are not automatically written off merely because the pilot died.

If another system spends reserved funds, a later bill consumes what the bank can pay and records the shortfall as QRF debt. The bank never goes negative. Future funds service that debt on the periodic finance tick; new paid responses are blocked while debt remains. Turning off strategic admission does not forgive existing bills/debt. The adapter never calls `AddFactionFunds_S`, so there is no fabricated refund or income loop.

The Bank phone app currently displays the ordinary balance. It does not display reserved QRF collateral or debt. Server inspection uses `manager.GetQRFFinance().Available(role)` and `.Debt(role)`.

## Five ground tiers for 3–4 capable players

These are initial tuning values, not a claim of measured difficulty. Infantry/crew counts below assume the current four-person fireteam prefabs and are estimates until each faction's full tier is verified in Play. US armour currently means another LAV; USSR armour uses a BRDM, not a main battle tank.

| Tier | Complete recipe | Estimated US / USSR personnel | Lead + reserve elements | US funds needed | USSR funds needed |
|---|---|---:|---:|---:|---:|
| 1 | 1 truck + 1 IFV/APC | 12 / 12 | 2 + 0 | 1,510 | 1,410 |
| 2 | 1 truck + 2 IFVs/APCs | 19 / 19 | 3 + 0 | 2,570 | 2,370 |
| 3 | 2 trucks + 2 IFVs/APCs + 1 armour | 27 / 26 | 3 + 2 | 3,880 | 3,330 |
| 4 | 3 trucks + 2 IFVs/APCs + 2 armour | 35 / 33 | 4 + 3 | 5,190 | 4,290 |
| 5 | 3 trucks + 3 IFVs/APCs + 2 armour | 42 / 40 | 4 + 4 | 6,250 | 5,250 |

Machine-readable values: [QRF_PRICES.csv](QRF_PRICES.csv). The needed-funds columns include collateral plus overhead; a loss-free return costs only the overhead. Existing six-live-ground-package limits and finite reserve deadlines remain authoritative, so healthy lead packages can prevent the entire reserve from appearing. Level is a response tier, not an infantry-squad multiplier.

For example, a tier-2 US deployment pays 70. Losing one LAV and two soldiers adds 850, for a total 920 campaign cost; the other surviving assets do not incur replacement charges. Losing a truck with its driver costs 225 in addition to that response's overhead.

## War level

Each AI role has independent server pressure, clamped to 0–1,000. Initial war level is an attribute; Chicken starts at 2.

| War level | Minimum pressure | Ordinary desired tier |
|---|---:|---|
| 1 | 0 | 1 |
| 2 | 100 | 70% tier 2; 30% tier 1 |
| 3 | 250 | 70% tier 3; 30% tier 2 |
| 4 | 450 | 70% tier 4; 30% tier 3 |
| 5 | 700 | 70% tier 5; 30% tier 4 |

Validated hostile evidence adds five pressure at most once per role per 30 seconds. A captured settlement adds 80 once per queued threat/active response. Newly observed asset losses add bounded pressure according to value. After ten minutes without new evidence/losses, pressure decays by two per minute. A captured target, at least three spaced threat reports, or a strategic site with importance 160+ can raise that threat's response ceiling one level, capped at 5. These bonuses do not stack. Affordability/capacity can still lower the actual bound tier.

`SetWarLevel_S(role, level)` is a trusted server integration/debug seam. No new unrestricted admin/client RPC is introduced. Policy defaults and importance thresholds are currently code policy; starting level, overhead, report expiry and profile prices are designer attributes. There is no automatic inspection of the players' inventories or real-time positions for difficulty selection.

## Priority and scope

Score = location importance + 15 × capped report count + capture bonus + bounded waiting credit. Factory/airport importance is highest, followed by military base/port, city, outpost, resources and ordinary settlements. Reports cap at six; capture adds 80; waiting adds at most 60. Ties retain queue order. Ordinary unrefreshed threats expire after 180 seconds in Chicken; capture threats remain eligible for at least 600 seconds. Retry eligibility is delayed 15 seconds. Ranking does not preempt an already ACTIVE response or mutate its bound recipe.

The damage, settlement capture and separately enabled military-site capture adapters supply current automatic inputs. The military-site capture opt-in also covers airport/factory/port/resource events after alignment with Rowan placements. Their importance is ready for trusted callers/garrison integration. No physical stock, production, territory ownership, garrison system or economy income authority is duplicated. Invader admission remains off at every existing dispatch gate.

## Persistence and verification

Economy persists bank debits. `AROR_OperationManagerComponentSerializer` version 1 persists both roles' war pressure and QRF debt. It also reads any confirmed losses not yet processed by the five-second finance tick into the save's debt value without changing bank state while serializing. Session allocations, actor registrations, threats, cooldowns and contact/spotting reports are not serialized. Save/load does not restore a working deployed QRF or its collateral ledger; that remains a separate lifecycle milestone. Missing entities without confirmed destruction are not retrospectively charged. The native save path must be tested through a save-enabled mission, since the bare Chicken world reports saving disabled.

`tools/Test-QRFStrategy.js` runs source-adapted policy/account/queue/selection bodies with mocked native services. It covers war thresholds, bounded selection, collateral, payment idempotency, loss/retirement, external-spend debt, queue priority/expiry, role gates, airborne downgrade, returned-package completion and spotting packet rules. Run with Node where installed; the same test body was evaluated in the tool V8 host here. Workbench's optional scheduler smoke-test switch also runs the new pure strategic checks without bank mutations or spawning.

After stopping Play, compile Game scripts and reload Chicken. Test a normal defender attack, let both infantry and vehicles complete their investigation, then inspect `[AROR STRATEGY]`, `[AROR FINANCE]` and `[AROR TACTICAL]`. Verify one overhead payment, a deliberate vehicle loss charged once, a loss-free package's retirement, low-funds downgrade/wait, debt repayment after external spending/income, and a tier-3+ reserve under the live cap. Test full withdrawal/return and save/load separately. Keep Invader admission disabled until an explicit USSR test has appropriate controlled targets and origins.
