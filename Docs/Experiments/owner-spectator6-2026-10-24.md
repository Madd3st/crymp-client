# Owner spectator feedback, 2026-10-24

Build a6ee9e9e, Steelmill, 6 Normal; session spectator6-20261024-041505.
Owner reports excellent spectator/first-person camera. This confirms the observed
camera behavior only; HUD, late-client and complete lifecycle coverage are still open.

Reported AI issues:
- Reluctance to descend platforms and use stairs/ladders (type awaits clarification).
- Apparently unnecessary Strength jumps after kills.
- Suspected WarsBot_01 stuck in an unobstructed area, moves slightly then snaps back.

Evidence saved while session kept running:
`build/owner-playtests/spectator6-20261024-041505/feedback-snapshot/`
(engine server/client logs, bot log, bot1-stationary-windows.json). This is an interim
snapshot, not the final match archive; the live session has not been stopped.

Bot #1 (EntityId131012) revived at t362.53, spawn195, position
(1247.500,1319.750,136.450). At t364.25..618.55 it stayed within a 2m sphere around
(1247.50,1319.75,136.51) for254.3s while reporting MOVING_TO_WEAPON/CHASING/ATTACKING.
Example t364.25..368.31: requestedBase2.50, physicalSpeed0, grounded0, blocked0.
The motion-request acceptance flag does not establish physical movement. The observed
snapback could be client correction, but no client/server frame correlation establishes
that cause yet. Diagnose revive/physics state and actor movement before route tuning.
Do not fix this by teleporting the bot or disabling physical collision.

Source triage (no game-code edits in this feedback task):
- BotBrain::TravelGait contains a random Strength jump: >15m, grounded, energy>130,
  suit/timer gates, 70% chance, 5..10s cooldown. This can happen on post-combat travel;
  it is not sufficient evidence to attribute each observed jump to that branch.
  Proposed next isolated change: reserve traversal jumps for a sensed obstacle/route need;
  preserve explicit test jumps and combat evasion.
- BotMovement::SafeStep probes body rays and landing ground; BotNav::Walkable limits
  descent to3.2m, ascent1.9m and intermediate steps0.75m. These constraints can contribute
  to platform/stair refusal; exact geometry must be reproduced before relaxing safety.
- BotBlackboard::Sample explicitly excludes isOnLadder from canMove; there is no native
  ladder attach/climb executor in Bots. Ordinary stairs and climbable ladders need
  different fixes. No broad navigation rewrite or unsafe drop allowance is justified.

Next: reproduce the zero-speed-after-revive condition first; add bounded diagnostic
of native movement/physics state if needed. Then remove purposeless travel jumps and
measure stairs/drop handling at known coordinates. Preserve the working spectator flow.
No active EXEs replaced and no game/client/server stopped.
