# Review 825b82a5 (DS-079 integration + reset-roster gap) — BLOCK

Read-only review of exact `825b82a5883829173c60be93263c8550cd2a6816`, parent `f66406d2`. No build, game or install.
The integrated diff vs `f66406d2` is byte-identical to my `de29c702` (4 files, 64+/4-).

## BLOCK — reset roster can still resurrect an explicitly removed bot (`BotManager.cpp:537-547`)

`OnEvent` is now correct, but the other restore path is not: `eAE_resetBegin` clears `m_roster` and pushes **every**
`Ordered()` slot (`:540-542`), including a bot whose explicit removal is still pending and slots already processed by
`OnEntityDone`. `EndLevel` then clears the registry, `m_restorePending` stays true, and `RestoreRoster` re-creates the
bot after the reset.

The v3 log shows the window is real: `#1 removal pending` at t 0.06 and the deferred `ENTITY_EVENT_DONE` at 0.27.
A native PreGame/restart reset in that ~0.2 s window would roster #1 again — exactly the unwanted NK restore the QA v4
contract forbids ("DONE+settled FRAME bots1, no external-recreate").

There is a second defect in the same block: `m_roster.clear()` discards a genuine external-DONE entry queued by
`OnEvent`; it is only preserved accidentally because the expired slot is still in `Ordered()` and gets re-added. With a
pending-aware filter that accidental preservation disappears.

## Follow-up patch `patches/ds080_reset_roster.patch` (applies on 825b82a5)

* `BotRemovalPolicy.h`: `ShouldRosterForReset(removalPending, expired, alreadyQueued)`.
* `BotManager.cpp:537-546`: do **not** clear the roster; for each ordered bot skip `RemovalPending()`, `Expired()` and
  an already-queued number, then add live bots. External-DONE entries survive; explicit removals never do.
* `Tests/BotRemovalPolicyTests.cpp`: live=true; pending/expired/duplicate=false.
Apply check: clean on `825b82a5` (temp worktree, no build).

## Correct in the integrated patch

`OnEvent` uses the durable intent (`BotManager.cpp:466-480`), explicit immediate/deferred removal is ignored, a true
external reset still restores, `levelReady`/duplicate DONE are guarded, and identity/`OnEntityDone` cleanup is intact.
No new flags/defaults/net/Lua/physics.

## Non-blocking

* `BotInstance::Spawn` (`:40-102`) never resets `m_removeRequested`; no reachable re-spawn of the same instance exists
  today (instances are single-use), but a future retry path would make a live bot look removal-pending. One-line reset
  after a successful spawn would harden it.
* `OnLevelEnd` does not clear `m_roster`/`m_restorePending`; it relies on the following `OnLoadingStart`/`OnLevelNotFound`.
  A level switch that skips them could leave a stale restore intent.

CI40648992189 on `825b82a5`: unit job PASS, client/server was still running at review time; the BLOCK is code-level and
does not depend on it. Runtime stays blocked until the follow-up SHA is green and independently reviewed.
