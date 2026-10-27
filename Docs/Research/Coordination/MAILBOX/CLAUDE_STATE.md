# Claude coordinator state (read and updated by every scheduled Claude run)

Last processed: X-172 (Codex), DS-049 + DS-S6 (DeepSeek). claude-sprint head 4f3a1d72 (C-130 CI fix on top of eabb3824, whose CI FAILED). DS-050 integration held locally until 4f3a CI is green.
Open: C-127 PRIORITY (Shore playtest), C-128 (wake proof), C-129 (nightly 4 h soak),
  C-127 (Shore PS owner playtest preset, after C-126 step 3), C-123 (wiki), C-126 (replaces C-125: probe, snipers, PS capture/buy, 15-bot x2, playtest-6 preset) for Codex;
DS-050 (flags cleanup, IBotGameMode, PS measurement) for DeepSeek. Owner: D-032 (first_primary default) open; D-033 YES 17:52 -> UPSTREAM_READINESS.md is the plan.
Waiting on: C-126 results, DS-050. C-122 done (X-169): Windows wake task live, Codex stopped after it at 16:56.
Next for Claude: integrate DS-050 (behaviour-identical, needs a paired run); if D-032 yes -> flip wb_bot_first_primary default 1 in BotConfig (only that flag).

Claude hourly cloud scheduled task: ACTIVE (trig_01ABKXcowTTiHUd7wwLt9Lxz, minute 19 every hour, owner OK 27.10 15:18).
D-031: successor plan approved but DORMANT — Codex/DeepSeek switch to SUCCESSOR.md prompts only when the owner pastes them.
