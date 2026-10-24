# FPS bot AI: what is worth taking for WarsBots

Sources read (shallow clones, 2026-10-24): F.E.A.R. SDK `jsj2008/lithtech/FEAR/ObjectDLL`,
`id-Software/Quake-III-Arena` (`code/botlib`, `code/game/ai_*`), `yapb/yapb` (`src/`),
`jswigart/omni-bot` (`Omnibot/Common`), `cpowell/cppGOAP`. Old Crysis/CryMP bot mods: no open
implementation found beyond WarsBots' own Crysis Wars predecessor (`Docs/WARSBOTS_PORT.md`);
native CryAI is present in CryMP but stock MP maps carry no navigation/cover data
(`warsbots/research/crymp-native-audit`, `CRYMP_CAPABILITIES.md`).

## Licences (decides what may be reused as code)

| Project | Licence found | Use in WarsBots |
| --- | --- | --- |
| F.E.A.R. SDK | `(c) Monolith, All Rights Reserved` in every file | ideas only |
| Quake III | GPL-2 (`COPYING.txt`) | ideas only (would force GPL on the combined work) |
| YaPB | Unlicense, public domain (`UNLICENSE`) | code reuse legally possible; engine-bound, so ideas in practice |
| Omni-bot | `Omnibot/License.txt` is a title with no terms; dependencies carry their own | ideas only until clarified |
| cppGOAP | LGPL-3 (`LICENSE`) | ideas only (and quality is toy-level, below) |

The prototype in `Tools/FPSAILab` is original code.

## Per-project assessment

Scores: ++ strong, + useful, 0 neutral, – weak, n/a.

| Criterion | F.E.A.R. | Quake III | YaPB | Omni-bot | cppGOAP |
| --- | --- | --- | --- | --- | --- |
| Implementation quality | ++ disciplined, huge | + C, global state | + modern C++, tuned "by trial and error" (their comment) | + generic, script-heavy | – O(n) open/closed lists, `std::map` state |
| Fit to CryMP | + ideas; cover needs data we lack | + ideas | + ideas | ++ engine adapter split | 0 |
| CPU cost | event-driven, distributed sensors | per-frame rules, cheap | per-frame desires, cheap | per-frame priorities | fine for toys |
| 15+ bots | ++ (1 AI/frame does expensive sensing) | ++ | ++ (32 bots common) | ++ | n/a |
| Sudden change | ++ event replanning, interrupt priority | + node FSM transitions | + task stack, desire thresholds | + priority re-evaluation | n/a |
| Avoids flip-flop | + equal-relevance stickiness, interrupt priority | + loop guard: >50 node switches/frame is reported (`ai_dmnet.c` `MAX_NODESWITCHES`) | ++ explicit hysteresis band 40–90 and "keep running task unless strictly higher" (`tasks.cpp:181-223`) | + equal priority keeps current (`StateMachine.cpp`) | n/a |
| Limited enemy info | ++ facts with confidence | + visibility-gated, then reads full entity state | + last enemy origin | + sensory memory with age | n/a |
| Navigation integration | node validators, path-blocked validator | ++ AAS reachabilities with **travel types** (walk, jump, ladder, rocket jump…) and per-bot **avoid lists with expiry** (`be_ai_move.c:82-93`, 6 s, 4 tries) | ++ cumulative stuck detector by *moved distance while wanting speed* (`navigate.cpp:852-890`), collision probing (jump/duck/strafe) | path + steering + goal registry | n/a |
| ML compatibility | + clean goal/action boundaries | – logic in code | + numeric desires | + priorities as numbers | 0 |
| Team play | ++ squad tasks | + team leader orders (`ai_team.c`) | + radio, camp/defend tasks | ++ **MapGoals** with roles, team availability, max users | n/a |

## Architecture families

| Approach | Strength | Failure mode we already see | Verdict for WarsBots now |
| --- | --- | --- | --- |
| FSM (current) | simple, fast, debuggable | transitions written as `if` chains in one function; any flicker in an input flips state (measured 4,579 A→B→A flips in 48 min, G2) | keep as the *execution* layer |
| Behavior Tree | composable, readable | needs a blackboard and a decorator for commitment anyway; no gain without editor tooling | no |
| Utility AI | handles trade-offs, ML-ready scores | flaps without hysteresis | **yes**, with commitment rules |
| GOAP | composes multi-step plans, explains itself | under-constrained goals produce absurd chains (seen in our prototype: "retreat to break LOS, then pursue" to satisfy a patrol goal); cost tuning is as hard as utility tuning | **optional**, per-goal action masks, later for PS |
| HTN / hierarchical planning | strong for team strategy | heavy authoring | for PSCommander only, later |
| Learned policy | can beat hand-tuning | needs clean decisions, propensities and many episodes (Steelmill F5: not yet) | after the decision layer exists |

## The simplest thing that measurably helps

Prototype results (`Tools/FPSAILab`, synthetic decision-level scenarios, not game results):
the wins came from four mechanisms, none of which needs symbolic search:

1. **Navigation-verified options** (Q3 reachability, F.E.A.R. context preconditions): unreachable
   weapon abandoned at 0.6 s instead of 10 s of walking into the ledge (scenario C).
2. **Displacement-based stuck goal** (YaPB): trap detected at 6.6 s; legacy model stalls all 15 s (G).
3. **Commitment + goal separation** (F.E.A.R., YaPB, Omni-bot): pistol bot with flickering target
   had 0 walk-back flips vs 4 for the legacy rules in 12 s (I).
4. **Survival utility with verified cover**: low-HP bot under fire took 4 damage vs 23 (B), at the
   price of not killing the enemy.

It was *not* better everywhere: in H (empty clip in a fight) the planner switched to the pistol
and took 54 damage vs 30 for the legacy native auto-reload. Equal in A, D, F.

Recommendation: small changes in the existing BotBrain first (see `TACTICAL_PLANNER_DESIGN.md`
§Integration), then a separate decision layer with a utility scorer and commitment; keep the
planner as an optional component behind it.
