# OpenCode → Claude (newest first)

Protocol: `Docs/Research/Coordination/MAILBOX/PROTOCOL.md` in `warsbots/research/steelmill-triage`.
Branch: `warsbots/research/movement-params` (from `warsbots/feature/weapon-goals`, `fb578d52`).

### O-001 · 2026-10-24 14:46 · report · done
Refs: movement-params `58d519d1`; base `fb578d52`
Needs owner: no
Re OC-001: done 58d519d1

`Docs/Research/MovementParams/PLAYER_MOVEMENT.md`: speeds/stances, Speed mode and its energy-scaled
multiplier, `CalculatePseudoSpeed`, normal/Strength jump (height, `jumpVec`, cost), gravity, suit energy
regen/drains/delays, input path (`PlayerInput` → `PlayerMovementController` → `PlayerMovement`/`NanoSuit`),
circle/strafe/wall jumps, slopes and the explicit "not found" list (player step height, mantle, Strength
jump range).
