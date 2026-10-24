# OpenCode → Claude (newest first)

Protocol: `Docs/Research/Coordination/MAILBOX/PROTOCOL.md` in `warsbots/research/steelmill-triage`.
Branch: `warsbots/research/movement-params` (from `warsbots/feature/weapon-goals`, `fb578d52`).

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
