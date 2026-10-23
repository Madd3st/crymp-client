# Learning AI feasibility (WarsBots / CryMP)

Short answer: **adaptive tuning is realistic now; imitation learning is blocked on data;
self-play RL is possible only offline and slowly.** No ML dependency enters the game build.

## Approaches

| | A. Adaptive (bandits) | B. Imitation | C. RL / self-play |
|---|---|---|---|
| Learns | which profile parameters work (range, strafe period, suit policy) | human action choices | a policy from reward |
| Data | our own match logs | recorded *state+action* of strong players | millions of steps |
| In-game cost | a table lookup | inference | inference |
| Blocker | none | no state/action recording exists; fragmovies are edited video, not data | CryMP runs real-time only |
| Verdict | **do first** | after a recorder exists | research track |

## Observation / action / reward (shared interface)

- **Observation** (what a human could know): own health, energy, suit mode, velocity, grounded,
  weapon/ammo; per visible enemy: relative position, velocity, distance, LOS, last-seen age;
  recent damage direction. Never: hidden enemies' positions.
- **Action** = existing `BotIntent` fields: move target (discretised directions/ranges), strafe on/off,
  gait, jump type, suit mode, fire/aim. The executor stays native; the model never bypasses physics.
- **Reward**: +kill, −death, +damage dealt − damage taken, −stuck time, small −energy waste;
  objective terms later.

## CryEngine constraints

- No time-scaling headless training mode is established; `t_scale` effects on physics/network are
  untested. Assume ~real time → ~10 decisions/s/bot → ~36k steps/hour/bot. 6 bots ≈ 200k steps/h.
- Training must run in a separate process (Python/PyTorch) on logs; never in the server tick.
- Inference in game: small MLP via ONNX Runtime only if a proven benefit; otherwise C++ table.
- Reproducibility: fixed map, spawn seeds, profile seeds; physics is not deterministic anyway →
  compare by many-match statistics, not replays.

## Minimal experiment (after M3/M4 pass)

1. Log per engagement: profile parameters, distance band, suit mode, outcome (kill/death/disengage).
2. Offline: per distance band, a UCB1 bandit over 3–4 candidate `preferredRange`/`strafePeriod` pairs.
3. Load chosen arms as a profile override; run 20 matches baseline vs adaptive, same map/seeds.
4. Metrics: K/D, damage ratio, stuck events, time-to-first-hit, suit usage. Not only kills.

## Risks

Overfitting to one map and to other bots; learning exploits of the executor (stuck spots);
self-play can converge on degenerate behaviour. No promise that learned bots beat humans.
