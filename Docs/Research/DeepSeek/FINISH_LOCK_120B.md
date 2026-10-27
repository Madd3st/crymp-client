# finish_lock in C-120b (DS-047 #2)

1. Reachable: `BotCombat.cpp:246` needs `oldValid && best && best != old`; all three occur (every switch logs `TARGET CHOICE`), and the HP source is live `old->GetHealth()` at `:249`, not a cache.
2. The helper at `BotAuditFixes.h:17-21` rejects only `!enabled || !oldValid || !newTarget || oldHealth > 25 || lastSeenAge >= 1`, plus the <=8 m retaliator exception.
3. The HP stage is really hit: the C-120b metric records 42 episodes with the old target alive at HP 1..25 (`c120b-finish-target-metrics.py:16`), 4+13+7+16 (OFF) and 6+17+9+10 (ON).
4. `0 FINISH LOCK` is therefore no case, not unreachable: both arms have `switch_aways: 0` in `finish-target-metrics.json`; the low-HP target died or left before any target change.
5. Main reason: `wb_bot_finish=1` runs the DS-015 block at `BotCombat.cpp:233-243` first and already keeps a target this bot damaged by >=30 HP for up to 2 s; a <=25 HP target the bot shot satisfies it.
6. Second reason: between scans `FindEnemy` returns the remembered target (`BotCombat.cpp:159-161`), so a switch is only possible on a 0.6 s scan tick.
7. The log is verbose-only (`BotCombat.cpp:254`), but `wb_debug 1` is in both C-120b arms (`BotManager.cpp:794-800`), so the empty log is meaningful.
8. PS-0 is not evidence either way: Shore had 2 target acquisitions and 4 self-kills, no switch-away at all.
9. No behavioral fix: the gate is reachable and had no case; moving the threshold would change decisions without evidence.
10. If QA wants a guaranteed tick, add a level-1 counter when `best != old` and `old->GetHealth() <= 25` regardless of the keep result; keep the decision itself unchanged.
