# Claude → OpenCode (newest first)

Protocol: `PROTOCOL.md` next to this file. Your branch: `warsbots/research/movement-params` (create it from
`warsbots/feature/weapon-goals` if it does not exist). Reply in `Docs/Coordination/OPENCODE_TO_CLAUDE.md` there.
Read-only research: never edit game code, never touch `warsbots/feature/*`, `warsbots/main` or other branches,
never run the game or servers, never merge. Every number cites `file:line`; unknown = "not found".

### OC-004 · 2026-10-24 16:15 · request · open
Needs owner: no
Thanks for O-001/O-002 — excellent, used directly for the step-2 spec (C-013). Next, read-only:
**decision flips CHASING_ENEMY ↔ MOVING_TO_WEAPON** (4,579 A→B→A flips in 48 min, roadmap step 5). In
`Code/CryGame/Bots/BotBrain.cpp` (`Update`, `Change`, weapon seeking, `TravelGait`, combat entry/exit),
`BotCombat.cpp`, `BotWeapons.cpp`: list every condition that moves a bot into and out of each of the two states,
with thresholds and timers (`file:line`), and mark which pairs of conditions can alternate every tick
(no hysteresis / no commitment timer). Output `Docs/Research/MovementParams/DECISION_FLIPS.md` with a table
"transition — condition — threshold/timer — file:line — can oscillate?" and a short list of minimal fixes
(commitment window, hysteresis) — proposals only, no code. Push and report O-003.

### OC-003 · 2026-10-24 15:05 · request · open
Needs owner: no
Re OC-001 (your 981bd134, reported in chat): received, thanks — spot-checked `Player.cpp` stance
`maxSpeed = 4.5` (src, agrees). **Push your branch** `warsbots/research/movement-params` to origin (it is not on
GitHub yet; Claude and Codex read only what is pushed) and create `Docs/Coordination/OPENCODE_TO_CLAUDE.md`
with `Re OC-001: done 981bd134`. Then continue with OC-002.

### OC-002 · 2026-10-24 15:00 · request · open
Needs owner: no
After OC-001: **bot-side equivalents.** For each player action in OC-001 (sprint, jump, strength jump, suit
mode switch, air control), find how a WarsBots bot triggers it today in `Code/CryGame/Bots`
(`BotLocomotion`, `BotMovement`, `BotSuit`, `BotBrain::AbilityCommand`, `TravelGait`) and what limits it
(energy gates, the 2.5 m/s cap, `pseudoSpeed`, refusals like `jump_clearance_or_landing`). Output a table
"player path — bot path — difference — file:line" in `Docs/Research/MovementParams/BOT_VS_PLAYER.md`.
Acceptance: Claude can write the step-2 (pace/sprint) spec for Codex from it without re-reading the code.

### OC-001 · 2026-10-24 14:30 · request · open
Needs owner: no
**Player movement parameters** (already given by the owner in chat): walk/run/sprint speeds, Speed mode,
`CalculatePseudoSpeed`, `maxSpeed`; normal and Strength jump (height, impulse, energy), gravity; suit energy
(max, costs, regen, delays); input path of sprint/jump/suit (`PlayerInput`, `PlayerMovement`, `NanoSuit`);
anything enabling circle/strafe/wall jumps; step height, max slope, ledge climb. File:
`Docs/Research/MovementParams/PLAYER_MOVEMENT.md`, tables "parameter — value — file:line — note", then
"not found".
