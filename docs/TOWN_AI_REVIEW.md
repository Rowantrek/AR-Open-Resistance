# Town attack and defence review

Reviewed 2026-11-09 against the current `chicken-dev` source. Implemented source and mocked policy checks are
available; **Workbench compilation, navigation, combat and dedicated-server acceptance remain outstanding**.

## Five Workshop references

This is a shortlist ranked by relevance to Open Resistance's town battles, not a claim about the five most
downloaded mods or a benchmark of their AI. The evidence is the authors' Workshop descriptions and changelogs,
not installed-mod play tests or a source-code audit. No external AI mod code or new dependencies were added.

| Reference | What the author describes | Decision for Open Resistance |
|---|---|---|
| [RC Dynamic Counter Attacks](https://reforger.armaplatform.com/workshop/6A3AEB1AC7B02185-RCDynamicCounterAttacks) | Successive or simultaneous approaches, combined arms, air insertion after ground pressure, and an explicit end to the attack. Its [changelog](https://reforger.armaplatform.com/workshop/6A3AEB1AC7B02185-RCDynamicCounterAttacks/changelog) records early victory and stranded-helicopter fixes. | Keep the finite response and ground-before-air sequencing already present. Include every column and aircraft operation in resolution; cancel outstanding children when the parent response ends. |
| [DCO_Extensions](https://reforger.armaplatform.com/workshop/6A16AC294F079592-DCO_Extensions) | Suppression, flanking, bounding, convoy columns, morale, reserves and an AI commander. Requires CRX. Its Workshop licence is APL-ND. | Separate covering and advancing tasks, preserve vehicle crews, and give each role a loss tolerance. These are independent policies using existing core waypoints; room-by-room clearing and suppression mechanics were not implemented. |
| [CRX Enfusion A.I.](https://reforger.armaplatform.com/workshop/5F268647F8A1A1F4) | Threat-aware investigation, infantry flanking and cover movement, vehicle combat and configurable combat behaviour. | Put the town doctrine above individual combat AI. Share bounded report snapshots and task positions; retain native perception, cover selection, firing and movement. No accuracy or perception buffs. |
| [REAPER_ImprovedAI](https://reforger.armaplatform.com/workshop/6A5D2DE1B3F96D7F-REAPER_ImprovedAI) | Moving and covering elements, limited sound investigation, spacing and priority for player orders; event-driven enhancements without continuous world searches. The author warns about overlapping native overrides. | Reuse damage subscriptions and existing scheduled commander checks. Keep human-controlled groups excluded. This review does not establish compatibility with REAPER Improved AI or CRX; test each separately before combining behaviour overrides. |
| [BattleGen - High Command](https://reforger.armaplatform.com/workshop/615F07D39925670F) | An automatic commander issuing orders to groups, plus generated missions. The page also lists broken custom waypoint commands and a multiplayer-support limitation. | Keep strategic selection, group task planning and native soldier behaviour separate. Useful architectural reference, but its description is not evidence of dedicated-server readiness. |

## Resulting faction identities

Doctrine follows the configurable **role**, not a hard-coded faction key or vehicle. Defaults are USSR = Invaders
and US/BLUFOR = Occupiers. A replacement faction inherits its role's behaviour. These policies apply to town
response waves and physical attack orders against towns carrying `AROR_TownWavesComponent`. Ordinary QRFs,
military-site attacks, idle garrisons and player-commanded squads retain their existing policies.

| Rule | Invaders: shock assault | Occupiers: deliberate assault |
|---|---|---|
| First response delay | 0.65 times the town's alert delay, approximately 20–60 seconds by default | Existing 30–90 seconds |
| Time between wave requests | 78–117 seconds by default | 120–180 seconds by default |
| Split approaches | From wave 1, when there are at least 3 elements | Final wave, when there are at least 3 elements |
| Convoy dismount | Prefer 160 m from the post | Prefer 220 m from the post |
| Infantry with a fresh contact | Direct pressure plus lateral ingress, then close on the reported position | With 3+ active packages, assign a covering infantry element for each three ranks; others flank and close |
| Support position | Existing close support standoff | Prefer 100 m from the report |
| Minimum tactical retask interval | 10 seconds | 25 seconds |
| Break-off without reserves | At or below 25% of initial recorded personnel | At or below 45% |

All values are town component attributes. `m_bFactionDoctrine = 0` restores the shared tactical policy. Safe road
and dismount checks can move a preferred dismount point; the 160/220 m values do not override navigation safety.
Cover assignment uses live package ranks: a package without an infantry group still has its native vehicle task.

The shock army spends the same finite recipe budget faster and from more directions. Splitting divides the
requested elements between operations; it does not duplicate a wave. If no second safe staging road exists,
the entire wave uses the first road. This is coordinated intent through shared reports, not guaranteed
simultaneous arrival: travel, combat and available capacity can separate the columns.

## Exact town loop

1. **Contact:** player kills reach 25% of the defenders observed, with a minimum of 3. Taking the post also queues
   a relief response. Taking it cannot evade the response by flipping support before the next kill poll. Both
   triggers respect the global town slot, player proximity and the rearm cooldown.
2. **Alert:** reserve the single town response slot, retain the original responding army and snapshot Resistance
   support. A post-triggered response uses support from before its capture reward. Issue the existing contact report.
3. **Three finite waves:** Readiness selects equipment; Crackdown selects element count. City and final-wave
   bonuses remain. Stage on safe roads 1–2 km away, rotate approaches, and split according to the role's doctrine.
   A wave gets at most six launch attempts, 20 seconds apart; exhausted or missing recipes are skipped.
4. **Report-driven infantry:** positive hostile damage on watched defenders reports the attributed source's
   position. Validate victim role and diplomacy; exclude healing, GM damage, self hits and unidentified attackers.
   Reports can come from AI-on-AI fighting too. Civilians never publish military reports.
5. **Sharing and expiry:** Operations shares the position snapshot with friendly columns. It holds no enemy
   entity reference and performs no continuous player lookup. Town doctrine accepts reports at most 45 seconds
   old; expiry is acted on at the next permitted retask. Existing five-second polling and a 75 m movement threshold
   limit churn. Small snapshot drift does not repeatedly replace waypoints. This is damage-source intelligence,
   not proof of a visual sighting. Native AI handles combat encountered between orders.
6. **Close and secure:** advancing teams approach different sides of the report, then close on it. After the
   contact task, teams retaking a lost town return to its objective. Cover teams hold a defend waypoint while
   the report remains valid. Without contact, teams secure bounded objective sectors. All positions stay within
   the existing security boundary. Native navmesh reachability still requires Play testing.
7. **Air support:** following the spotting/aviation refinement, a dedicated transport insertion with gunship
   support is requested at Readiness 1+ once the first ground wave is committed. Existing airspace, capacity and
   safe launch/landing rules still apply. A rejected request can retry; an accepted request is tracked once.
   [Spotting and air response](SPOTTING_AND_AIR_RESPONSE.md) details the transport-heavy packages and gunship window.
8. **Outcome:** the response is repelled only after all tracked operations are terminal and at least one reached
   the field. The support reward is +20%. The visible lockdown remains the authority during a garrison response:
   10 accumulated minutes outnumbered, draining while the Resistance recovers. An attack order uses the separate
   3-minute continuous occupation contest with defenders weighted double. Neither also applies ordinary QRF
   support pacification; an ordinary QRF alongside a running town response cannot stack that pacification either.
9. **End:** abandonment, handover, timeout or removal cancels outstanding child operations through the normal
   manager. Unlaunched children cannot deploy later. Existing retention and player-preservation rules handle
   surviving actors; cancellation is not proof every survivor is dead or physically gone. The town cooldown is
   30 minutes. Kills during it can escalate the army, but do not pre-charge the next response's kill trigger.

The force recipes, faction assets, support system and existing HUD/notification vocabulary remain in use.
This is group-level doctrine, not a new soldier behaviour tree, guaranteed suppression, or building-clearing AI.
Those mechanics need a separately validated native AI integration.

## Validation evidence and remaining gates

`tools/Test-TownDoctrine.js` executes selected actual Enforce method bodies with mocked engine services. It checks
doctrine settings, finite wave splits, single-road fallback, launch rejection bounds, contact expiry and role
isolation, hostile-damage reporting, objective bounds, post-capture triggering, support baselines and child cleanup.
It is included in `tools/Validate-QRFRelease.js`.

See [the validation record](validation/TOWN_GAMEPLAY_VALIDATION_2026-11-09.md) for current results and the existing
regression-suite failures. A green mocked test is not native compilation or proof of combat behaviour.

Extenders: no class names, required dependencies, resource GUIDs or recipe IDs changed. New town attributes
default to role doctrines; disable the switch to retain the previous policy. `RecordPostTaken_S` adds an optional
support-baseline argument, so existing two-argument callers still work; callers that award support should pass
the value from before their reward. Running responses and contact reports remain session-only.
