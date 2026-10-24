# ML integration options

Principle: rules decide what is *allowed*, a model may only rank what is allowed, and every
game action still goes through BotIntent → native CryMP movement, weapons and suit.

## Where a model plugs in (already shaped in the prototype)

```
Observation ──► Facts / masks (rules)        never learned
            ──► GoalFeatures (17 floats)     logged
            ──► Scorer::Score(goal, x)       HeuristicScorer today; LearnedScorer later
            ──► commitment rules             never learned (stability)
            ──► planner + executor           never learned (safety, explanations)
```

`fpsai::Scorer` is the only replaceable part. A learned scorer cannot pick an inapplicable goal
(mask), cannot skip verification (context checks), and cannot flap (commitment). It can only
change *which applicable goal wins*.

## Prerequisites (Steelmill F5 says none of these exist yet)

1. One decision record per *choice*, not per 2 s telemetry tick: time, actor, life id,
   features, candidate goals with mask and score, chosen goal, **propensity** (probability the
   logging policy chose it), seed.
2. Exploration on the logging side: ε-greedy (ε = 0.05–0.1) among applicable goals whose heuristic
   score is within 0.15 of the best — bounded, so play stays plausible. Propensity = exact
   probability of that rule.
3. Outcome per decision window (until the next goal change or 10 s): HP lost, damage dealt
   (native health loss), kill/death, distance progress, stuck seconds, ammo used. Authoritative
   outcomes are labels only, never features.
4. Actor id on every line (BOT SUIT/JUMP/BOT ACTION today have none).
5. Train/eval split by session; fixed memory snapshot per session.

## Option ladder

| Step | What | Data needed | Risk |
| --- | --- | --- | --- |
| 0 | Heuristic scorer + logging with propensities | none | none; enables everything else |
| 1 | Off-policy evaluation (IPS / doubly robust) of a *candidate* heuristic against logged data | ~5–10k decisions | none: no deployment |
| 2 | Contextual bandit on goal choice (linear or small GBDT per goal, Python training) | 20k+ decisions with exploration | low: masks and commitment stay |
| 3 | Export as a flat table or tiny MLP evaluated in C++ (`LearnedScorer`), behind `wb_scorer` cvar | — | low; A/B as in TACTICAL_PLANNER_DESIGN |
| 4 | Imitation of the owner (human decision windows labelled from human telemetry) | human traces | medium: humans' state is only partly observable |
| 5 | Self-play / RL on positions and movement | many matches, fast server | high; only after 1–3 show value |

Inference cost target: < 2 µs per scored goal set (6 goals × 17 features is a 102-multiply
linear model; even a 2-layer MLP of width 32 is ~1.3k FLOPs).

## What not to do

- Do not train on TACTIC SAMPLE rows: `action` is the FSM state, there are no alternatives and no
  propensities, and rows are emitted while dead.
- Do not let a model output positions or aim; those stay with BotNav and BotCombat.
- Do not count counters (kills/deaths per node, stuck per node) as learning.
- Do not accept a model that wins only by kill count; guard metrics as in the A/B contract.
