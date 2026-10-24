# ML data readiness: `wb_tactics_report.py` episodes (schema 2)

Independent audit; the exporter was **run unmodified** on log copies, output into
`build/steelmill-lab/ml/`, and checked with `Tools/SteelmillLab/ml_validate.py --runs`.
No model was trained and nothing was added to the game.

Dataset: 7 runs (2 human-hosted matches, 5 QA runs; builds `5e8daa65`, `8f0abd5f`, `0b70b24b`,
`eea973f0`) → **4212 episodes**, 2.4 MB, not capped. 70 % of rows come from one 48-min match.

## 1. Structural validity

**0 errors**: required fields/types, outcome set, start ≤ end, first sample inside the episode,
`context_complete` consistency, no duplicates, no overlapping episodes of one actor, **no episode
crosses a death or revive** of its actor, every `kill` outcome has a matching `combat kill` line,
every `death` outcome a `death observed` line. The exporter's bookkeeping is sound.

## 2. Field roles

| Role | Fields | Note |
|---|---|---|
| Context before the decision | `skill`, `style` | fixed per bot; profile *values* are not recorded |
| Context sampled late | `first_sampled_distance`, `first_sample_visible`, `distance_source` | taken at the first 2 s telemetry tick **after** acquisition |
| Aggregated over the episode (post-decision) | `weapons`, `suit_modes`, `actions` | must not be used as context |
| Outcome / reward | `outcome`, `damage_dealt_hp`, `damage_taken_hp`, `end_seconds − start_seconds` | HP loss only (Armor energy excluded) |
| Bookkeeping | `actor`, `bot`, `target`, `run`, `source_sha`, `schema`, `damage_kind`, `progress_experiment_states` | |

## 3. Quality and leakage findings (warnings, counts out of 4212)

| Code | Rows | Meaning |
|---|---|---|
| `W_context_sampled_after_first_hit` | 1187 | a hit between actor and target happened **before** the context sample → context partly reflects the outcome |
| `W_context_sampled_late_gt1s` | 1692 | context sampled > 1 s after the episode start (p50 0.95 s, p90 1.82 s) |
| `W_no_context_sample` | 658 | no TACTIC SAMPLE in the episode (short episodes) |
| `W_distance_from_last_seen` | 871 | distance from last-seen, not visible, target |
| `W_suit_changed_during_episode` | 497 | `suit_modes` mixes modes chosen in reaction to the fight |
| `W_weapon_changed_during_episode` | 72 | pickups/switches inside an episode |
| `W_death_caused_by_other_than_target` | 110 of 885 deaths | `death` outcome credited to an engagement with a different killer |
| `W_censored_outcome` | 59 | `log_end` / `entity_done` |
| `W_episode_shorter_than_0.2s` | 33 | |

Mirrored episodes: 1048 rows have a partner (target↔actor, same start) — bot-vs-bot pairs are
not independent. Usable rows after removing censored, contextless, late (> 1 s) and post-hit
contexts: **1453** (35 %).

## 4. Answers

* **Enough features for a contextual bandit?** No. There is no *action*: `actions` counts FSM
  states over the whole episode, and the behaviour parameters a bandit would choose
  (preferred range, strafe period, suit policy, `AI_LEARNING_FEASIBILITY.md`) are not logged.
  Context is two categorical profile names plus one distance/visibility sample.
* **Pre-decision vs result fields**: §2. Only `skill`, `style` are clean; distance/visibility are
  usable only when sampled before the first hit (validator flag).
* **Future-information leak**: yes, if `weapons`, `suit_modes` or `actions` are used as features,
  and for 1187 rows whose context sample follows the first hit.
* **Episodes after death/respawn**: correctly split (0 crossings); but there is no life id, and the
  actor EntityId is reused after revive (`sameEntity=1`), so lives must be rebuilt from the log.
* **Reward**: definable as `damage_dealt_hp − damage_taken_hp` with ±kill/death, but (a) damage from
  third parties is not attributed, (b) 12 % of `death` outcomes were caused by someone else,
  (c) Armor energy damage is excluded, (d) `target_changed`/`lost_target` (57 %) end without a
  terminal outcome.
* **Missing for an independent evaluation**: an explicit arm/action id and its propensity (random
  exploration), the profile parameter values or a policy hash, a life/episode id, the context
  vector logged *at decision time*, a fixed opponent pool (not only other learning bots), human
  opponents marked, and held-out runs split by run with frozen map memory.

## 5. Minimal logging additions to request later (Codex, additive)

1. `TACTIC DECISION actor= life= arm= p= range= strafe= suit= distance= visible= hp= energy=` at the
   moment a target is selected (not on the telemetry timer).
2. `life=` counter on telemetry and episode lines.
3. Policy id (hash of the profile table) in the BotManager build line.
Until then, use the data for descriptive evaluation only, never as bandit training data.
