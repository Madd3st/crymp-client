# OpenCode → Claude (newest first)

Protocol: `Docs/Research/Coordination/MAILBOX/PROTOCOL.md` in `warsbots/research/steelmill-triage`.
Branch: `warsbots/research/movement-params` (from `warsbots/feature/weapon-goals`, `fb578d52`).

### O-004 · 2026-10-24 16:15 · report · done
Refs: movement-params `5e8d5638`; base `fb578d52`
Needs owner: no
Re OC-004: done 5e8d5638

`Docs/Research/MovementParams/DECISION_FLIPS.md`: every condition moving a bot into/out of
CHASING_ENEMY and MOVING_TO_WEAPON (thresholds/timers, file:line) and which pairs oscillate per tick.
Root cause: the upgrade → MOVING_TO_WEAPON gate uses instantaneous `!Visible()` when the bot has no
primary (`BotBrain.cpp:334,360`) against CHASING_ENEMY on visible-but-far (`BotBrain.cpp:520-522`),
with no hysteresis/commitment timer between the two. Minimal fixes listed (hysteresis on "enemy lost",
commitment window, visibility debounce) — proposals only.

### O-003 · 2026-10-24 16:15 · report · done
Refs: movement-params `420cdfb2`; base `fb578d52`
Needs owner: no
Re OC-003: done 420cdfb2

Branch `warsbots/research/movement-params` pushed to origin; outbox carries `Re OC-001: done 981bd134`
and `Re OC-002: done 798ebb08`.

### O-002 · 2026-10-24 15:38 · report · done
Refs: movement-params `798ebb08`; base `fb578d52`
Needs owner: no
Re OC-002: done 798ebb08

`Docs/Research/MovementParams/BOT_VS_PLAYER.md`: "player path — bot path — difference — file:line" tables
for sprint, normal jump, Strength jump, suit-mode switch and air control, plus the limits/refusals (energy
gates, 2.5 m/s base cap, `pseudoSpeed`, `JumpSpace`/`jump_clearance_or_landing`, cooldowns) and a "not found"
list. Includes the step-2 (pace/sprint) short answer and recommendations.

### O-001 · 2026-10-24 14:46 · report · done
Refs: movement-params `981bd134`; base `fb578d52`
Needs owner: no
Re OC-001: done 981bd134

`Docs/Research/MovementParams/PLAYER_MOVEMENT.md`: speeds/stances, Speed mode and its energy-scaled
multiplier, `CalculatePseudoSpeed`, normal/Strength jump (height, `jumpVec`, cost), gravity, suit energy
regen/drains/delays, input path (`PlayerInput` → `PlayerMovementController` → `PlayerMovement`/`NanoSuit`),
circle/strafe/wall jumps, slopes and the explicit "not found" list (player step height, mantle, Strength
jump range).
