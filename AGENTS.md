Live project status: origin/warsbots/research/steelmill-triage:Docs/Research/STATUS.md.
Roadmap: https://github.com/Madd3st/crymp-client/wiki/Status-and-Roadmap ; tasks: triage Docs/Research/Coordination/MAILBOX/TO_CODEX.md.
This branch Docs/STATUS.md is the QA run log only.
# WarsBots for CryMP

Start with [Docs/STATUS.md](Docs/STATUS.md), [port provenance](Docs/WARSBOTS_PORT.md),
and `git status --short`. This fork is the main WarsBots development platform.
`upstream/master` is CryMP; `origin/master` preserves it. Integrate through
`warsbots/main`; use `warsbots/feature/*` for implementation. Never push upstream
or force-push. Preserve uncommitted user changes and the original WarsBots repo.

Bots live in `Code/CryGame/Bots`. Player integration is in `Actors/Player`;
GameRules and ShotValidator changes are scoped to owned bot entities.
Do not replace upstream gameplay files with Crysis Wars versions.

- Experimental CMake option defaults OFF. Runtime opt-in defaults 0. Only local
  loopback LAN InstantAction is allowed; spawn is an explicit console command.
- No fake INetChannel, OnClientConnect(0), global IsPlayer change or anti-cheat bypass.
- Keep ownership/EntityId identity guards, map cleanup, and native weapons/physics.
- Spawn metadata arrives BEFORE Init: store marker only, never access GameObject there.
- Experimental serialization changes require matching client/server builds;
  keep native CryMP input serialization and optional groups intact.
- No new engine binary patches without evidence and exact target verification.
- Read functions selectively. No agents unless the user requests delegation.
  One integrator owns shared Player/GameRules/CMake/network changes.
- Do not stop a user's game without permission. Do not use +quit or short game timers.
- BUILD PASS, server log evidence, user observations and visual QA are separate results.
  Crysis Wars results do not establish CryMP compatibility.
- Build: `Tools/Build-WarsBots.ps1` (OFF), add `-Experimental` for the port.
  MSVC x64 only. Read Docs/AI_TESTS.md before gameplay tests.

Current owner/developer entry point: START_HERE.md. AGENT_HANDOFF/ROADMAP/ARCHITECTURE_REVIEW are historical redirects, not active task queues. Update the authoritative triage state and relevant QA result only when project state changes; do not add machine housekeeping reports. Never delete build wholesale: it contains active Git worktrees, frozen route data and evidence.
