# CryMP native audit for WarsBots (research only)

Branch `warsbots/research/crymp-native-audit` (from `warsbots/feature/weapon-goals` @ `d32e1b9a`).
Isolated from active development: no game C++, GameRules, Lua, BotBrain/BotCombat/BotNav, build
configuration or shared document (`AGENT_HANDOFF.md`, `STATUS.md`, `AI_TRAINING.md`,
`POWER_STRUGGLE_DESIGN.md`) was modified.

| Document | Content |
|---|---|
| [INTEGRATION_OPPORTUNITIES.md](INTEGRATION_OPPORTUNITIES.md) | **Start here.** Ranked proposals, replaceable WarsBots code, PS blockers, smallest safe PS prototype plan |
| [CRYMP_CAPABILITIES.md](CRYMP_CAPABILITIES.md) | Per-subsystem capabilities for a channel-0 actor (players/GameRules, movement, nav/AI, weapons, vehicles, items, areas, turrets, events, spectator) |
| [POWER_STRUGGLE_NATIVE_FLOW.md](POWER_STRUGGLE_NATIVE_FLOW.md) | Traced PS call chains (join, spawn group, death/revive, PP, buy, factory, capture, victory) + verification of the PS design doc |
| [CHANNEL_ZERO_AUDIT.md](CHANNEL_ZERO_AUDIT.md) | Every EntityId-vs-channel point found, classified, with risk |
| [generated/](generated/) | Tool output: Lua channel sinks (PAK lines redacted), PS level inventories (counts/positions only) |

Regenerate (needs locally extracted game scripts and installed level paks; nothing is copied):

```sh
python3 Tools/CryMPResearch/lua_channel_audit.py --repo . \
  --pak-scripts <extracted ZPatch1>/Scripts --pak-scripts <extracted GameData>/Scripts \
  --markdown Docs/Research/CryMP/generated/lua_channel_audit.md
python3 -I Tools/CryMPResearch/level_inventory.py <Crysis>/Game/Levels/Multiplayer/PS/Mesa/level.pak \
  --markdown Docs/Research/CryMP/generated/level_inventory_Mesa.md
```

Evidence grades used throughout: SRC (open source read), LIVE-IA (seen in earlier IA playtests),
UNPROVEN (closed dispatcher / untested replication), NO (proven unusable).
