# WarsBots AI gaps

Code: `warsbots/feature/weapon-goals@0f338b12` (read only). Data: owner's 11-Elite Steelmill
session (`WarsBots-Play11Elite-20261024-023225-Server.bots.log`, build 5e8daa65, 459 live
bot-minutes) and the Codex P1 regression logs of 8f0abd5f, analysed offline with
`Tools/SteelmillTriage` (branch `warsbots/research/steelmill-triage`).
**CONFIRMED** = shown by code and data; **CODE** = visible in code, effect not measured;
**HYPOTHESIS** = plausible, unproven.

## Why does a bot walk back and forth?

**G2 CONFIRMED.** 12,357 logged state changes = 26.9 per bot-minute. 4,579 are A→B→A within
5 s. The biggest pairs: CHASING_ENEMY↔MOVING_TO_WEAPON 2,147 and ATTACKING↔MOVING_TO_WEAPON
1,230. Mechanism (`BotBrain.cpp:329`): a bot without a primary takes the weapon branch whenever
`!enemy || !m_combat.Visible()`. Line of sight flickers, so the bot alternates between walking to
the enemy and walking to a gun — two different destinations, a physical back-and-forth. There
is no commitment time or hysteresis on that branch. The prototype reproduces it (scenario I:
legacy 4 walk-back flips in 12 s, decision layer 0).

Secondary, **HYPOTHESIS**: patrol alternates hotspot and spawn waypoints every leg
(`BotBrain.cpp:389`) with 30 s timeouts; after a blocked leg a local detour is taken. At 2 s
telemetry this did not show as loops (0 oscillation windows detected), so it is not the main cause.

## Why does it not take good positions?

**CODE.** There is no position evaluation at all: no cover, no high-ground, no sightline
concept. Combat movement is strafing around the target at `preferredRange`
(`BotBrain.cpp:465-503`), local 3 m back-off when closer than `retreatEnter`, and on low health a
**blind 4 m step directly away** from the enemy for 2.5 s (`BotBrain.cpp:447-463`) without a route
or exposure check. Native CryAI cover data does not exist on stock MP maps (CryMP audit), so
positions must be generated from BotNav.

## Why does it search for weapons for a long time?

**CONFIRMED.** MOVING_TO_WEAPON is 28% of telemetry samples in the 11-Elite session; 911
"unreachable; skipping" events (2 per bot-minute). Item choice is by distance within 45 m without a
route test (`BotBrain.cpp:330-345`, documented in `Docs/STEELMILL_AI.md`); the only exit is a 10 s
timeout then a 90 s skip. Strict route rejection was tried and rejected by Codex (fewer pickups).
The prototype's answer is narrower: never *commit* to an unverified pickup, but keep local
steering as a fallback outside the decision layer.

## Why is vertical geometry used badly?

**CONFIRMED.** BotNav edges allow ≤1.9 m rise, ≤0.75 m step (`BotNav.cpp:98,110`); there are no
jump links. 42–62% of failed route queries target another floor (Steelmill F3). Strength jumps
are random travel decoration (`BotBrain.cpp:82-83`), not route steps, and produced 23–31-minute
traps (Steelmill F2).

## Why do bots move slowly and never break out of cover?

**CONFIRMED (owner report + data).** 11-Elite session, live samples while moving:

| State | Real Speed sprint | Median speed | Suit mode | Median energy /200 |
| --- | --- | --- | --- | --- |
| MOVING_TO_WEAPON | 12% | 2.40 m/s | Armor 72% | 117 |
| SEARCHING_ENEMY | 15% | 2.42 m/s | Armor 82% | 100 |
| CHASING_ENEMY | 7% | 2.49 m/s | Armor 90% | 89 |
| ATTACKING | 0% | 2.45 m/s | Armor 100% | 39 |

Travel sprint starts only above 120 energy and 15 m to go (`BotBrain.cpp` `TravelGait`), and
chasing a visible target sprints only beyond `sprintChaseRange` with >100 energy; median energy
sits below those gates, so bots run at the 2.5 m/s base almost always. Combat never sprints by
design (strafe only). 330 `speed_energy` and 147 `strength_energy` refusals, 165 jump landing
refusals in the session. There is no peek/break-out action because there is no cover concept.
**Unknown**: the human baseline in the same conditions; no human movement is traced today.

## How does it choose between attack and retreat?

**CODE.** Two fixed thresholds: `health <= retreatHealth` (22 default), or Elite with ≥2 visible
enemies and health < 60 (`BotBrain.cpp:446-448`). No weapon, ammo, cover, distance, height or
team context enters the choice. There is no "survive" option other than the blind withdraw.

## Do subsystems contradict each other?

**CONFIRMED, two cases.**
- Stuck handling reads BotMovement's `blocked` (`BotLocomotion.cpp:123`); a step that is accepted
  but does not move the body never raises it. Spawn (1247.5, 1319.75) pins 17 of 22 lives for
  their whole life (Steelmill F1).
- Map memory penalises the node where the bot stood, for every route through it, permanently
  (`BotNav.cpp:238,274`), while roaming ignores half of that memory (`BotNav.cpp:297`, `i += 2`).

## Where is target choice mixed with character control?

**CODE.** `BotBrain::Update` (670 lines) decides and executes in the same branches: each branch
fills `m_intent` and calls `ExecuteIntent` and returns (`BotBrain.cpp:279, 287, 322, 367, 414, 463,
546`). Consequences: no single place where "the decision" exists, so alternatives cannot be
logged, scored, A/B-switched or learned (Steelmill F5: TACTIC SAMPLE `action=` is the state name in
95–99% of rows). Targeting itself is cleanly separate in `BotCombat`/`BotTargeting`.

## What blocks good team behaviour?

**CODE.** Bots know only "same team = not hostile" (`BotTargeting.cpp:42-44`). No shared memory
of enemy sightings, no task assignment, no reservation of positions or items (two bots run to the
same gun), no roles. The decision/execution mix above is the main obstacle: a commander has no
input to write into. F.E.A.R. squad tasks and Omni-bot MapGoals both need a goal-selection layer
that accepts external tasks.

## Summary by cost of fixing

| Gap | Fix | Size | Owner |
| --- | --- | --- | --- |
| F1/F2 traps and pins | displacement watchdog + escalation | small | Codex (game code) |
| G2 weapon/chase flip | commitment on the upgrade branch (min time + visibility hysteresis) | small | Codex |
| Blind withdraw | withdraw only to a verified point; otherwise keep fighting | small | Codex |
| Unverified weapon goals | route state in the decision, local fallback kept | small | Codex |
| No decision layer | `BotDecision`: observation → candidates → scorer → choice → existing execution | medium | after the small fixes |
| No positions/cover | generated cover candidates from BotNav + ray probes | medium | needs `wb_nav_export` first |
| No vertical links | verified Strength-jump edges | medium | after export + Spectator review |
| No team layer | tasks from PSCommander into the decision layer | large | PS phase |
