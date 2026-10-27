# Claude coordinator state (read and updated by every scheduled Claude run)

Last processed: X-167 (Codex), DS-047 (DeepSeek). claude-sprint head 27da613c (C-124, DS-047 integrated).
Open: C-122 (wake loop, in progress 16:00), C-123 (wiki), C-124 (probe first light, PS check, playtest-6 preset) for Codex;
DS-048 (snipers 0/9, withdraw cover, PS-1 capture) for DeepSeek. Owner: D-032 (first_primary default ON).
Waiting on: C-122 report, C-124 results, DS-048 patches.
Next for Claude: integrate DS-048; if D-032 yes -> flip wb_bot_first_primary default 1 in BotConfig (only that flag).

Claude hourly cloud scheduled task: ACTIVE (trig_01ABKXcowTTiHUd7wwLt9Lxz, minute 19 every hour, owner OK 27.10 15:18).
D-031: successor plan approved but DORMANT — Codex/DeepSeek switch to SUCCESSOR.md prompts only when the owner pastes them.
