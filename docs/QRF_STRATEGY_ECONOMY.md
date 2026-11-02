# QRF strategy, prices and response lifecycle

Status: source implementation and offline checks; **the 29 October 2026 tier-one/linear-balance changes need a fresh Workbench compile and Play test**. Chicken enables both armies' capture responses, light airborne selections from Readiness 1 and joint responses from Readiness 2. Combat-triggered QRF dispatch and the blanket REAPER override remain off. See [release validation](QRF_RELEASE_VALIDATION.md) for current evidence.

## What happens after a capture

Stronghold defence is a separate trusted scenario policy using the same executor. It can waive faction funding, freeze a named recipe and use a coastal launch point, while preserving actor capacity, launch safety and existing operations on the same role/target. Its requests do not establish capture-QRF attack eligibility. See [strongholds](STRONGHOLDS.md) and [reconciliation validation](RECONCILIATION_VALIDATION.md).

1. A canonical enabled settlement/military capture supplies a trusted role/location threat. Chicken keeps combat-triggered QRFs off: validated damage can update recent contacts and bounded crackdown but cannot request a response. No player RPC can create a threat or change readiness.
2. The manager coalesces reports into at most 32 pending threats. Repeated reports for one role/location contribute evidence at most every 30 seconds. A live QRF for the same target is reused.
3. Every five seconds, each role considers its highest-scoring eligible threat. Importance, sustained fighting, capture and bounded waiting age affect priority. An unaffordable threat waits; after its retry is deferred, another candidate can be considered on a subsequent tick. Existing per-role cooldown, outstanding-operation limits and conflicting-target checks still govern admission.
4. Current crackdown requests intensity 1–5 while readiness selects the equipment pool. Admission freezes both values. The executor prefers the highest authored tier at or below that intent which fits money and actor budgets. The complete allocation is reserved atomically before spawning; a second response cannot reuse its collateral. Chicken's explicit minimum-capture opt-in can instead admit only tier 1 without collateral when no paid recipe fits. Actor limits and safe friendly origins still apply; blocked threats remain queued. A race or later capacity change can still block an admitted operation until its normal spawn timeout.
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
| Armour support | 800 (LAV) | 450 (BRDM) |
| Transport helicopter | 1,600 | 1,800 |
| Escort gunship | 2,000 | 2,000 |

Prices live on each `AROR_ForceProfile`: `m_iPersonnelLossValue`, `m_iVehicleLossValue`, `m_iEscortLossValue`. A negative main-vehicle value uses the shared type fallback; Chicken explicitly authors each asset value. The same US LAV has the same hull value in mechanized and armour profiles. Crew and passengers are charged separately when lost. An airborne quote includes the transport, any authored escort and the profile's full personnel reservation.

Collateral uses **reserved personnel slots**, which can exceed actual spawned personnel. Only registered physical assets incur loss charges. An unused slot or unspawned wave never becomes a casualty bill. Cancelling an unspawned ground/air reserve releases its quoted collateral. Closing an operation releases unmaterialized collateral while retained physical assets continue to hold theirs until normal retirement or loss.

Confirmed dead characters, destroyed vehicles and non-native faction captures are charged once per registered asset. An empty vehicle's cleared/native affiliation is not capture. An absent or engine-virtualized entity alone is not evidence of death. Cleanup observes losses before deleting actors and retires living assets without a loss charge; departing transport aircraft/pilots do the same before their normal removal. Pilots dying and hulls being destroyed are separate losses. Surviving but stranded hulls are not automatically written off merely because the pilot died.

If another system spends reserved funds, a later bill consumes what the bank can pay and records the shortfall as QRF debt. The bank never goes negative. Future funds service that debt on the periodic finance tick; new paid responses are blocked while debt remains. Turning off strategic admission does not forgive existing bills/debt. The adapter never calls `AddFactionFunds_S`, so there is no fabricated refund or income loop.

The Bank phone app currently displays the ordinary balance. It does not display reserved QRF collateral or debt. Server inspection uses `manager.GetQRFFinance().Available(role)` and `.Debt(role)`.

### Minimum capture response

`m_bMinimumCaptureQRF` defaults off in the shared manager and is explicitly on in Chicken. It requires strategic economy, a recorded capture threat, the existing Economy and a complete tier-1 ground recipe. Paid selection remains preferred. If no paid recipe is available, tier 1 can deploy without collateral even while debt remains. It never grants funds or changes the bank's non-negative spending rules: the one activation overhead and confirmed asset losses are paid from the ordinary bank or added to the same saved QRF debt. Future funds service debt before new paid responses. Retained physical survivors remain observed after terminal operation state so later confirmed losses are billed once.

Initial garrison rosters likewise do not require faction funds. They retain their separate personnel cap, placement checks and saved casualties; they do not replenish automatically. These two provisional policies preserve an opponent at low funds while leaving stronger QRFs dependent on the economy.

### Ground planning and capture retries

Chicken sets `m_bQRFRequireFriendlyOrigin`: dispatch needs a currently friendly outpost, military base or airport. Configured role coordinates cannot substitute for lost land territory, and the offshore corridor cannot launch ground vehicles. Losing the USSR's only land base therefore leaves its capture threat waiting for a valid origin; the minimum-funding policy does not override that requirement.

Plan each ground package's delivery point before constructing its vehicle/groups. Search the normal objective area first, then optionally a nearby reachable road within `m_fQRFMaximumRoadDismount` (300 m by default, maximum 350 m). The fallback checks a soldier-navmesh dismount point and package separation; it does not certify the entire foot route. Infantry must still physically enter the objective's security area before starting the hold. Military objectives follow the actual defence flag when it remains inside the canonical capture radius.

Explicit launch/approach failures before activation can requeue up to two further planning attempts. They retain the original capture timestamp, ten-minute capture lifetime and ordinary role cooldown, avoid previously failed origins, and do not add capture pressure again. Active package failures do not restart the deployment. A pending capture threat is discarded when its defending role regains the target. This is bounded recovery, not a guarantee that every site has a usable route.

## Readiness-5 combined responses for 3–4 capable players

These are initial tuning values, not a claim of measured difficulty. Vehicles fill native usable passenger capacity with separate retained crews. The table below records conservative personnel reservations, not measured soldier counts. US armour means another LAV; USSR armour uses a BRDM, not a main battle tank. Heavy ground recipes grow from two to six packages, adding one complete package at each tier.

| Tier | Complete recipe | Reserved US / USSR personnel | Lead + reserve elements | US funds needed | USSR funds needed |
|---|---|---:|---:|---:|---:|
| 1 | 1 truck + 1 IFV/APC | 34 / 34 | 2 + 0 | 1,910 | 1,810 |
| 2 | 1 truck + 2 IFVs/APCs | 46 / 46 | 3 + 0 | 3,020 | 2,820 |
| 3 | 1 truck + 2 IFVs/APCs + 1 armour | 58 / 54 | 2 + 2 | 4,130 | 3,480 |
| 4 | 2 trucks + 2 IFVs/APCs + 1 armour | 80 / 76 | 3 + 2 | 4,890 | 4,240 |
| 5 | 2 trucks + 2 IFVs/APCs + 2 armour | 92 / 84 | 3 + 3 | 6,000 | 4,900 |

Machine-readable values: [QRF_PRICES.csv](QRF_PRICES.csv). The needed-funds columns include collateral plus overhead; a loss-free return costs only the overhead. Existing six-live-ground-package limits and finite reserve deadlines remain authoritative, so healthy lead packages can prevent the entire reserve from appearing. Level is a response tier, not an infantry-squad multiplier.

For example, a tier-2 US deployment pays 70. Losing one LAV and two soldiers adds 850, for a total 920 campaign cost; the other surviving assets do not incur replacement charges. Losing a truck with its driver costs 225 in addition to that response's overhead.

## Airborne response share

Ordinary campaign QRFs target **30% airborne or joint responses per army** over successfully dispatched operations. The selector uses the nearest attainable cumulative proportion; it is not a separate random roll on each capture. The first QRF is ground, and the target is three air/joint responses per ten feasible successful dispatches. Small samples can differ from 30%. Whole operations count once, regardless of aircraft, passenger or reinforcement count.

When the air bucket is due, an eligible funded joint recipe is preferred; standalone airborne can fill it when no joint recipe is available. Otherwise ground recipes are selected. Readiness, funds/debt, actor limits, owned origins and occupied airspace remain authoritative. If no safe landing site or launch volume exists before funding/construction, the request can switch to ground. Already funded/materialized packages cannot be abandoned by a bucket switch. Ground fallbacks count as ground when actually dispatched, so a shortfall can be caught up by later eligible air responses. The target is not a guarantee when aviation is unavailable.

Failures before ACTIVE do not count. Dispatch is recorded once before ACTIVE subscribers can synchronously resolve the operation. MARS, explicit profile/recipe selections, attacks and scenario-funded stronghold responses do not alter these totals. Per-army counts are saved in policy serializer version 4; older saves retain Readiness/Crackdown/debt and start fresh mix history. `m_iAirborneQRFShare` defaults to 30 and accepts 0–100. Full loads, crew separation and finite safety/recovery rules are unchanged.

Chicken retains the Readiness-5 `occupier_joint_l1`–`l5` / `invader_joint_l1`–`l5` recipes and adds `*_joint_r2_l1`–`l5` and `*_joint_r3_l1`–`l5`. Early recipes use an unescorted transport/fireteam with trucks/light support at Readiness 2 and mechanized options at 3–4. Within the selected airborne bucket, eligible funded joint recipes are preferred when joint, airborne and tactical execution are enabled. Affordable ground alternatives remain available; the unsecured minimum stays ground-only. See [the roster and native gates](QRF_RELEASE_VALIDATION.md#joint-airborne-and-ground-responses) and [QRF_PRICES.csv](QRF_PRICES.csv).

## Readiness and crackdown

Each enemy role has two independent server-owned values on a 0–1,000 scale. New Chicken campaigns start at Readiness 1 and 0% Crackdown (response tier 1). Loaded campaigns retain their saved values; this balance change does not reset them.

Each newly recorded capture loss adds five Crackdown percentage points. Without other combat/loss evidence or cooling, the capture-only ladder is:

| Capture losses by that army | Crackdown | Requested response tier |
|---|---:|---:|
| 0–3 | 0–15% | 1 |
| 4–7 | 20–35% | 2 |
| 8–11 | 40–55% | 3 |
| 12–15 | 60–75% | 4 |
| 16 onward | 80–100% | 5 |

Combat and confirmed losses can accelerate this; quiet periods can slow or reverse Crackdown. Equipment Readiness keeps its existing thresholds and does not decay. Recipe growth is linear in complete package count, not guaranteed equal increments of infantry or firepower: different vehicles have different native capacities and weapons. Every ground/joint band adds one package per tier, with personnel reservations and collateral increasing strictly within that band. Full occupancy, driver/gunner separation, safe unloading and actor/funding limits remain enforced.

| Readiness | Minimum saved readiness progress | Eligible ground pool |
|---|---:|---|
| 1 | 0 | Infantry trucks and light armed escorts |
| 2 | 100 | Infantry trucks and light armed escorts |
| 3 | 250 | Mechanized responses with APC/IFV support |
| 4 | 450 | Mechanized responses with APC/IFV support |
| 5 | 700 | Larger combined-arms mixes with wheeled armour support |

A newly recorded capture loss adds 25 Readiness progress and 50 Crackdown points (five percentage points) by default. Repeated reports for the same queued capture/active response do not repeat that award; planning retries do not award it. Validated combat evidence adds five Crackdown points at most once per role per 30 seconds. Confirmed QRF losses add bounded Crackdown. Combat and losses do not unlock equipment. After ten quiet minutes, Crackdown cools by 20 points (two percentage points) per minute; Readiness progress does not decay. Starting values, capture increments and cooling are designer attributes. Repeated real recaptures can still advance Readiness progress; this is provisional campaign pacing.

Crackdown directly requests response levels 1–5: 0–19% / 20–39% / 40–59% / 60–79% / 80–100%. Threat importance and report count affect queue priority, not response strength. There is no random tier bonus. Money/capacity can downgrade the requested level, and the explicit minimum-capture policy can admit level 1 without collateral. Every selection still respects the readiness pool. Both readiness pool and desired response level freeze at operation admission; its later waves keep the same recipe and allocation.

| Response | Readiness 1–2 | Readiness 3–4 |
|---|---|---|
| 1 | 1 truck | 1 APC/IFV |
| 2 | 1 truck + 1 light escort | 1 truck + 1 APC/IFV |
| 3 | 2 trucks + 1 light escort | 1 truck + 1 APC/IFV + 1 light escort |
| 4 | 2 trucks + 2 light escorts | 1 truck + 2 APCs/IFVs + 1 light escort |
| 5 | 3 trucks + 2 light escorts | 2 trucks + 2 APCs/IFVs + 1 light escort |

Readiness-5 mixes/prices are listed above. US light escort is the M2 Humvee; USSR light escort is the PKM UAZ, a functional light-support counterpart with different firepower. These profiles are QRF-only and do not add weight to the separate scheduled-attack pool. All Chicken ground profiles fill usable infantry passenger seats and retain separate vehicle crew during infantry unloading. Transport helicopter cabins also use the full-load policy; native seat count determines the actual load within reserved ceilings. Ground support remains LAV/BRDM rather than a tank. Separate light airborne and joint bands are described above; none of these source changes establishes native flight success. This progression controls QRFs; it does not rewrite saved garrison recipes.

Recipes expose minimum/maximum readiness levels, and profiles independently expose their minimum unlock. A mislabeled early recipe cannot introduce a locked profile. The readiness-provider preview now accepts an explicit readiness too; it still has no automatic dispatch caller. Legacy fixed truck/APC fixtures remain available when strategic selection is disabled.

Trusted server integrations use `SetReadinessLevel_S(role, level)` for equipment progress and `SetCrackdown_S(role, points)` for intensity. RebIntel reads replicated values and shows both response level and crackdown percentage. It cannot mutate them. The existing Invader campaign reads the separate readiness/crackdown for ambition and external backing; simulated victories do not increase either value.

## Priority and scope

Score = location importance + 15 × capped report count + capture bonus + bounded waiting credit. Factory/airport importance is highest, followed by military base/port, city, outpost, resources and ordinary settlements. Reports cap at six; capture adds 80; waiting adds at most 60. Ties retain queue order. Ordinary unrefreshed threats expire after 180 seconds in Chicken; capture threats remain eligible for at least 600 seconds. Retry eligibility is delayed 15 seconds. Ranking does not preempt an already ACTIVE response or mutate its bound recipe.

The damage adapter is separately opt-in for dispatch; Chicken enables settlement and military capture inputs, including airport/factory/port/resource events. Their importance is ready for trusted callers/garrison integration. No physical stock, production, territory ownership or economy income authority is duplicated. Chicken enables Invader ground capture admission; the shared default remains off.

## Persistence and verification

Economy persists bank debits. `AROR_OperationManagerComponentSerializer` version 4 persists both roles' readiness progress, crackdown and QRF debt. Version 1 saves migrate their old shared pressure into both axes once, preserving readiness and the prior crackdown percentage; subsequent changes are independent. Partial version-2 crackdown records and unsupported versions are rejected before applying state. Read-only snapshots include confirmed losses not yet processed by the finance/policy tick in debt and crackdown without changing live state or bank balances while serializing. The quiet-time grace restarts on restore. Session allocations, actor registrations, threats, cooldowns and contact/spotting reports are not serialized. Save/load does not restore a working deployed QRF or its collateral ledger; that remains a separate lifecycle milestone. Missing entities without confirmed destruction are not retrospectively charged. The native save path must be tested through a save-enabled mission, since the bare Chicken world reports saving disabled.

`tools/Test-QRFStrategy.js` runs source-adapted policy/account/queue/selection bodies with mocked native services. It covers readiness thresholds, crackdown levels, bounded selection, collateral, payment idempotency, loss/retirement, external-spend debt, queue priority/expiry, role gates, airborne downgrade, returned-package completion and spotting packet rules. Run with Node where installed; the same test body was evaluated in the tool V8 host here. Workbench's optional scheduler smoke-test switch also runs the new pure strategic checks without bank mutations or spawning.

After stopping Play, compile Game scripts and reload Chicken. Confirm fighting alone requests no QRF, then clear and capture the site. Inspect `[AROR STRATEGY]`, `[AROR FINANCE]` and `[AROR TACTICAL]`. Verify paid selection, zero-funds tier-1 fallback, one overhead payment, deliberate losses charged once, debt repayment and a tier-3+ reserve under the live cap. Exercise pre-activation route failure and bounded retries without empty spawned hulls. Test full hold/withdrawal/return and save/load separately. For USSR acceptance, provide at least two friendly land installations before capturing one. See [GAMEPLAY_READINESS.md](GAMEPLAY_READINESS.md) for the ordered handoff.
