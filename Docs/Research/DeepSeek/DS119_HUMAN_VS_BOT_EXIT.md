# DS119 — same-shaft human exit vs owned bot, owner-video session (offline)

Owner is playing: no game/server/probe/live-data/install/recovery action. Sources used: QA report
`Docs/Coordination/C154_OWNER_VIDEO_20261029.json` (46 human samples), owner session archive
`build/c154-owner-video-20261029/archive` (server + client logs), video frames (vision), extracted graph.

## 1. Video

Vision is available and was used on one frame only (`frame-070.jpg`, in the bracketed region): a first-person
spectator view of the upper corridor/deck with the bot and objective HUD, no shaft visible in that single frame.
No frame-by-frame sync was attempted, the client timebase is not the video/server timebase, and the audio was not
transcribed or used. Nothing below depends on the video.

## 2. Native human at the same shaft (actor 65479, client side)

- Climb `90.454 -> 97.839` at `vz = 1.077` (bot climb is `0.900`), same x/y `2116.125, 1880.625`.
- Exit curve (client time): `186.398 z=97.934 vz=0.449` (trigger), `186.65..187.652` vertical `~0.73 m/s` to
  `98.85`, then diagonal: `187.899 z=99.097 y=1880.183`, `188.151 z=99.35 y=1879.792`; `188.399 grounded=1
  flying=0 z=99.392 y=1879.727`; settles to `z=99.75` at `189.151`, then walks away.
- So the native exit is ~1.9 m up plus ~0.85 m laterally over ~2 s: a two-phase root-motion curve that lands on
  the deck at `(2116.2, 1879.7, 99.39..99.75)`.

## 3. Owned bot in the same owner session (server log)

- `1.05 LADDER start`, `3.74 GRAPH prepare activeBefore=1 stanceBefore=combat`, `12.30 LADDER EXIT output=49
  state=RelaxedIdleNW`, `12.33 output=0 state=RelaxedWalkNW`, `12.36..14.42 state=relaxed_walkToIdle_nw_01`,
  feet frozen `(2116.125,1880.625,97.932)`, `14.42 blocked_top`.
- Correction to DS118 v1: the movement request is **decaying**, not stale — the graph runs Idle -> Walk ->
  walkToIdle while the AI sends nothing. The exit signal is issued at 12.30 and the graph never enters a ladder
  exit state. (Headless attempt 4 showed RunNW instead of walkToIdle; the difference is real but not explained.)
- After the scheduled `60.07` revive (`spawnId=545`, same actor, alive=100) there is **no second `LADDER start`**:
  the first attempt had already failed and the link is banned for the level; BanLink is per-mesh state, not
  something a revive clears.

## 4. Caller difference (source)

- Human: `CPlayer::RequestLeaveLadder` on a client sends `SvRequestLeaveLadder` to the server
  (`Player.cpp:7310-7324`); the client's own animated character plays the exit and its root motion moves the
  client's actor (client-authoritative movement in MP). The observed human curve is that client-side motion.
- Bot: the owned server actor calls `LeaveLadder` locally with no client; the same exit branch then sets no
  `m_request` (`PlayerMovement.cpp:1587-1620`), so the only possible mover would be the server-side animation.
  The server graph does not enter the exit state, and nothing else can move the capsule.
- This updates DS118: C-2 (first-person/template) stays a hypothesis (the graph contract only skips skippable
  states, no proof the template is player-only), but the client/server asymmetry is now the concrete correlate
  for why the same shaft exits for a human and not for the bot. The v1 "6-20 graph updates" figure is also
  corrected: at ~30 Hz that is about three frames per 0.1 s.

## 5. Preview schedule flaw vs defect

- The ban is failure-derived: the bot's first immediate attempt fails at the top and bans the link, so the
  delayed 545 revive has nothing to climb. With a working exit the first attempt would not ban, and the same
  60 s schedule would present the reveal.
- Therefore the presentation issue is a **consequence** of the exit defect, not an independent package bug. Do not
  clear runtime bans blindly and do not edit the frozen owner package.

## 6. Narrow candidate (needs an explicit owner decision)

Evidence now says the native exit motion at this shaft is a **client-side** phenomenon. There is no evidence that
the dedicated server's graph can produce that motion for a non-player actor. Two options:

- (a) Prove/hold the server-side exit state — still unproven, and the only way to learn is the AG input/state
  trace, which the owner has ruled out as a standalone probe.
- (b) A physically validated server exit executor for owned bots, using the human trace as the reference
  contract: continue the stock native climb (real `eCMT_Fly`, same axis/speed rules) to the shaft top, then a
  real forward/lateral movement onto the deck at `y ~= 1879.7`, with completion only when the existing
  `NativeLadderExitPhysicalReady` (near top + lateral out of shaft) holds and the policy sees the upper floor
  grounded (`z ~= 99.39..99.75`). No SetPos, impulse, timer detach, manual `ReachedEnd` or new cvar; humans,
  OFF and non-owned actors unchanged.

(b) is an invariant change (the owned bot would no longer depend on the animation exit) and must be approved
before integration, then follow the normal path: new exact source, new full green CI, fresh review, and the owner
explicitly finished before the single route.
