# Bot review — architecture position (Claude, 26→27.10 night)

Owner 23:18: review the bot from several positions; 23:19: "do it yourself". Positions 1–2 (pro-player audit of the
Bible, exploits) go to DeepSeek (DS-042), 4–6 (scorecard, client view, server cost) to Codex (C-115). This file is
position 3: where the code itself produces the dumb behaviour. Code: claude-sprint fd12a5eb.

## How a tick works (BotBrain::Update, 10 Hz)

`m_intent` is cleared every decision tick, then the first `Update*` that "handles" the tick writes it and calls
`ExecuteIntent`: Test → WeaponSeek → Arbiter (if on) → Upgrade → Roam | Combat. Inside Combat: withdraw → vantage hold →
hold angle → Attacking | Chasing → CombatPost. `ExecuteIntent` then lets several late writers overwrite channels
(look policy, link executor, obstacle jumps, suit rules, suit policy, corner slow-down, turn cap) before locomotion.

## Findings (claim · evidence · fix)

| # | Finding | Evidence | Status |
| --- | --- | --- | --- |
| 1 | Two writers of the AI state in one tick (roam → upgrade); every `Change()` restarts `m_stateTime`, which is also the reaction clock and the stuck clock | DS-034: 13,004 + 12,999 SEARCHING↔MOVING flips in playtest 3 | fixed behind `wb_bot_state_commit` (81.8 → 6.9 A→B→A/min, C-111) |
| 2 | Fire is suppressed by movement decisions: close-range back-off and low-HP withdraw set `combat=Aim` | `UpdateAttacking` retreat branch, `UpdateCombat` withdraw branch | fixed behind `wb_bot_pro_fire` |
| 3 | Cloak forbids firing (`ExecuteIntent`), and today's Elite cloak fix made Elites cloak more → silent Elites | `if (Fire && mode != CLOAK)` | `wb_bot_pro_fire` drops cloak when it wants to fire |
| 4 | Weapon re-select every 2 s mid-fight with margin 0.12 → switch animation without shots | `UpdateCombat` C-022 block | `wb_bot_pro_fire`: margin 0.3, every 4 s |
| 5 | Two reaction clocks (state time AND selection time, each ≥ reaction) | DS-035: Elite first shot 0.38 s vs 0.08 table | fixed (DS-035 patch, 2fee59c2) |
| 6 | Features gated by an unrelated flag: threat scan needed `wb_bot_vantage` to load spots | C-098/C-108: 0 GLANCE with threat_scan 1, vantage 0 | fixed (spots load for any of the three) |
| 7 | Feature deadlock: vantage needs a sniper rifle in hand, weapon rank removed it | C-098 0 holds | fixed a890f2ad; snipers still never fetch DSG1 (DS-038 open) |
| 8 | Suit mode has ~7 writers per tick (combat rule, pace rule, fall rule, obstacle jump, link executor, suit policy, corner slow-down, pro fire); last writer wins, order is implicit | `ExecuteIntent` | open: T-1 "one writer per channel" — next refactor |
| 9 | Height had no goal at all outside sniper logic | owner: "nobody wanted to go to the roof" | `wb_bot_high_ground` |
| 10 | Owner playtests ran on the legacy graph (`NAVMESH none`) for two sessions | DS-033 | fixed by Codex C-110 (fail-closed) |
| 11 | Every fix adds a flag and an `if` inside BotBrain (≈ 48 `wb_*` flags) | FLAGS.md (DS-021) | open: "flags die" cleanup after this week's batches |

## Next (architecture)

1. **T-1 control channels**: one owner per channel (state, move target, look, fire, suit, jump), written once per tick in
   a fixed layer order; late "rules" become inputs to the owner, not overwrites. Start with the suit (finding 8).
2. **Promote or delete**: state_commit, pro_fire, high_ground, duel patch, jump fixes after the night batches; then
   delete their OFF paths (D-021 #3).
3. **Detectors in every report** (DeepSeek detectors.py, Codex C-115) so findings like 2–4 show up as numbers, not
   as the owner's impression.
