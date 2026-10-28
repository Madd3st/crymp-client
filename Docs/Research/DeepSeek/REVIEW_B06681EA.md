# Review b06681ea (cumulative DS-079 + DS-080) — OK

Read-only review of exact `b06681eaa4365afe5bf7309994012dce13a7ac83`, parent `825b82a5`, cumulative vs `f66406d2`.
No build, game, install or triage edit.

## Composition

`git diff f66406d2..b06681ea` is 4 files (+85/-5) and is exactly my two patches:
`ds079_removal_lifecycle` (byte-identical at `825b82a5`) and `ds080_reset_roster` (byte-identical at `b06681ea`).
No other source, config, network, Lua, physics or flag/default changes.

## Adapter / lifecycle

* `OnEvent` (`BotManager.cpp:466-481`): `Find` proves ownership, then `BotDoneDecision(known, levelReady, removing,
  removalPending, expired)` restores only a true external deletion. Explicit immediate (`Removing`) and deferred
  (`RemovalPending`) removals are ignored; a duplicate DONE after `OnEntityDone` is ignored via `Expired`; level teardown
  is ignored via `levelReady`.
* `eAE_resetBegin` (`:537-553`): existing external-DONE roster entries are kept, live bots are added, and
  `ShouldRosterForReset` skips `RemovalPending`, `Expired` and already-queued numbers. A deferred explicit removal
  observed for ~0.2 s in the v3 log can no longer be resurrected by a reset in that window; a genuinely external reset
  still restores the same number/profile/AI flag.
* `EndLevel` sets `m_levelReady=false` before removing and clears the registry, so late DONE events find no slot;
  `RestoreRoster` runs only from the retained/external + live union. Identity and `OnEntityDone` cleanup are unchanged.
* Explicit remove path (`BotInstance::Remove/RemovalPending`) and spawn path are untouched except for the classifier.

## Evidence

CI40649771503 on `b06681ea`: unit job PASS, 36 run logs downloaded, `BotRemovalPolicyTests OK` and
`BotMoveTraceEligibilityTests OK`; client/server build was still in progress at review time. Runtime still requires
that exact build to finish GREEN.

## Non-blocking (already recorded, unchanged)

* `BotInstance::Spawn:40-102` does not reset `m_removeRequested`; no reachable same-instance re-spawn exists today, so
  this is future hardening only.
* `OnLevelEnd` does not clear `m_roster`/`m_restorePending`; it relies on `OnLoadingStart`/`OnLevelNotFound`. A normal
  level switch invokes them.

Verdict: `review b06681eaa4365afe5bf7309994012dce13a7ac83: OK`. `825b82a5` remains review-BLOCK/never run.
