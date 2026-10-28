# C146 human-route analysis checklist (offline, after the owner recording)

Purpose: compare the owner's real US submarine exit on Shore with the bot's one-Strength-link candidate
(cockpit 2110.56,1885.58,103.18 -> east ring 2113.98,1885.51,106.07) and the ladder/shaft alternatives.
No game/build/install/triage; analysis only, on the archived session files.

## 1. Capture integrity (before looking at the route)

* Session dir `build/owner-playtests/C146-native-Shore-b06681ea-<tag>/` (`Record-WarsBotsShoreRoute.ps1:21`):
  `session.json`, `server-arguments.txt`, `client-arguments.txt`, server/client logs + `.bots.log`, `demo-archive.json`
  (name/bytes/sha256), `memory-before/after`, `memory-restored.json`; optional screen video.
* Exact source `b06681ea`, CI 40649771503 success, review `dd68f2ce`; **zero bots**, no nav install
  (native player route), no local build; server readiness `RULES INFO class=PowerStruggle` + `bots=0/1`.
  The launcher itself gates this: `native-route-quality.json` + `sha256.txt` (`Record-WarsBotsShoreRoute.ps1:8-14`),
  and it runs `+wb_bot_navmesh 0 +wb_bot_jump_links 0 +wb_move_trace 1 +wb_demo_record 1 +wb_demo_hz 20` (:32)
  with the 20 Hz client trace and `+bind f8 "wb_mark route"` (:33).
* Demo hashes match the archive; memory restored (or restore deferred with the other game active); session
  phase `archived_memory_restored` (or `cleanup_deferred_client_active` + reason).
* If the owner used noclip/dev/teleport or did not spawn on the native US team, mark the route **not comparable**.

## 2. Build the timeline

* Extract `wb_mark route` (F8) timestamps from the server/client logs and `.bots.log`; order:
  spawn -> first ladder look/grab -> climb/top -> exit -> water entry -> swim/shore -> capture/fight.
* Align the video frames to those timestamps; note the in-game clock and the session clock.
* The **human IS traced** (verified on b06681ea, not assumed):
  * client `BotMoveTrace.cpp:36-37` visits `GetClientActor()` at `wb_move_trace_hz` (20 Hz here), so the client
    `.bots.log` has `side=client kind=human local=1` rows with pos/vel/vz/grounded/flying/jump/land/peakZ/
    `swim/waterZ/relativeWater/bottomDepth/underwater` (`BotMoveTrace.cpp:49-82`, the MOVE TRACE line at :73).
  * server `BotMoveTrace.cpp:29-35` traces bots and any player with `channelId>0` at 4 Hz, so the same route is
    cross-checkable in the server `.bots.log` (net/replication timing, not the authority for apex).
  * `wb_mark` logs `OWNER MARK ... pos=(x,y,z) yaw= pitch= spectating=` on the client (`BotManager.cpp:818-835`),
    so each F8 mark is a measured 3-D point, not a guess. Demo marks are the server-side `Demo().Mark` only
    (`BotManager.cpp:828`); do not expect client demo events.
* Selecting the human rows: `Select-String 'MOVE TRACE .*side=client'` in the archived client `.bots.log`; the
  server rows are the same actor id at 4 Hz. Use demo 20 Hz only if a log gap appears; never invent coordinates.

## 3. Route facts to record (one line each)

* Spawn: exact position/team (upper cockpit ~103.25? lower deck ~90.4?).
* Exit type: **upper forwardhatch_B ladder** (base 2113.5,1885.5 or 2104.75,1883, top ~106.1) vs **lower shaft**
  (2116.125,1880.125, 90.25..100.29) vs a **jump/drop** from the cockpit/ring.
* Ladder interaction: look-at/grab (`mat_ladder`), climb direction, top/bottom exit animation, where the player
  lands (ring 106.0 / hull deck 103-104 / water 99.5).
* Lower deck (if shown): dry or flooded (`waterZ`, swim), the Water Room opening, exit coordinate to the sea.
* Water: first water contact coordinate/time, swim direction, time to shore/boat; any ladder/drop back to the deck.
* Route to the objective: capture/boat/base direction; whether the route matches the naval-base layout.

## 4. Compare with the candidate

* Does the human use the cockpit -> ring edge that the one Strength link models? If yes, the link is the bot's
  plausible substitute; the human's take-off/landing gives the target apex/landing and whether the ring is wide
  enough (in the offline arc audit the constant-arc full-sprint endpoint at vh 6.75 lands ~7.3 m past the ring,
  while vh 2.5-4 stays on it - DS084 review, `Docs/Research/DeepSeek/DS084_DATA_RECIPE_REVIEW.md`).
* If the human uses the upper **ladder**, the Strength link is a shortcut with a different (physical) route; the
  bot would need the ladder path or the link landing adjusted to the human's actual exit.
* If the human uses the **lower shaft/water room**, note dry/flooded, the shaft entry, and the sea exit; the
  lower-deck zmin/water question returns with real evidence.
* Record any spot where the human stops, jumps, crouches or uses the suit (the bot's speed/apex comparison).
* Pre-registered numbers to measure from the human rows (do not estimate by eye): last grounded sample before the
  jump (take-off xyz), `peakZ` minus take-off z, first grounded sample after it (landing xyz), horizontal
  take-off->landing distance, and elapsed time. Compare with the candidate link: rise **2.890**, horizontal
  **3.421** (2110.56,1885.58,103.18 -> 2113.98,1885.51,106.07). If the human's own jump reaches the ring with a
  shorter run-up than the bot's model needs, the route works without the speed cap; if the human only clears it
  with a full sprint plus a wider landing zone, that is the evidence the held cap was meant for.

## 5. Decision output (one short doc)

1. **Same exit**: human cockpit -> ring -> deck/water; the one Strength link matches -> the existing executor
   route is supported; decide whether the measured human take-off/landing needs the speed cap.
2. **Ladder/lower route**: the human does not use the modeled edge -> the ladder/shaft route is intended; the
   Strength link is not the human exit; keep the speed patch held.
3. **Inconclusive**: missing marks/video or the route is not the normal exit -> request one more targeted clip,
   not a bot run.

Do not claim Playable, do not install the candidate, do not change defaults. The prospective speed patch
`3ec91e44` stays on the own branch until this native evidence; B1 `0b39bdb6` stays staged only.
