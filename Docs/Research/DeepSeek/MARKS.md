# Owner marks → “why did it do that” card (DS-022)

Date: 2026-10-26. Read-only spec for the F8/WB mark and the PlaytestReport card. Current implementation:
`BotManager::MarkCommand` (`BotManager.cpp:720-732`) writes one `OWNER MARK` line; `BotDemoRecorder::Mark` already
emits demo event 10 (`BotDemoRecorder.cpp:243`), but `wb_mark` does not call it. `Tools/PlaytestReport` parses the log
(`Tools/PlaytestReport/playtest_report.py`) but has no mark card yet.

## 1. Mark record

A mark is a **wall-clock anchor plus a game-time anchor**, not just `t=`. The current line (`:727-731`) is:

```
OWNER MARK t=... side=client|server pos=(x,y,z) yaw=... pitch=... spectating=<id> text="..."
```

Required additions:

| Field | Why |
| --- | --- |
| `utc=YYYY-MM-DDTHH:MM:SS.mmmZ` | join the client mark to the server log and ShadowPlay filename |
| `session=<id>` | same session id in client/server log; prevents cross-run joins |
| `build=<git sha>` | card must use the same `BotBrain.cpp` source line map |
| `owner=<entity id>` | identifies the human even when `GetClientActor()` is null |
| `spectating=<id>` (already) | first candidate bot |
| `viewYaw/viewPitch` (already) | “in the owner’s view” ranking |
| `text` | owner’s words; preserve quotes/UTF-8 |

Write the line on both client and server when both exist (local listen server). If `GetClientActor()` is null, still
write `utc/session/build/side/text`; the card can use the server-side actor nearest to the server mark.

## 2. Which bots belong to the mark

Within ±5 s of `t` (game time), select at most 3 candidates, in this order:

1. **Spectated** entity from `spectating=` if nonzero.
2. **Last shooter/hitter of the owner** in the window: `Hit before-Lua target=<owner> shooter=<id>` or
   `FIRE HIT ... target=<owner>`; source `BotCombat`/`GameRules`.
3. **In the owner’s view**: angle between `yaw/pitch` and the bot position < 20 deg, distance < 60 m; prefer the
   smallest angle.
4. **Nearest** bot within 15 m if 1–3 are empty.
5. If still empty, the nearest bot anywhere with a `TARGET CHOICE`/`AI telemetry` row in the window.

Record why each candidate was selected (`spectated`, `hit_owner`, `in_view`, `near`, `fallback`). Do not use hidden
enemy state; positions are the replicated/logged positions.

## 3. Card fields and sources

Each card is one JSON object plus an HTML block. `t` is the mark game time; every field carries its source
(`file:line` or log line). All windows are `[t-5, t+5]` unless stated.

| Card field | Source in `.bots.log` / demo |
| --- | --- |
| mark time/text/side/wall clock | `OWNER MARK ...`; `DEMO START` for demo time |
| owner position/view/spectated | `OWNER MARK ...`; `MOVE TRACE` for owner if present |
| candidate identity | `#N created EntityId=... name=... profile=...`; `TACTIC SAMPLE` skill/style |
| state at mark | `#N AI state=...`; nearest before/after |
| target and visibility | `TARGET CHOICE actor=... old=... target=... skill=... style=... score=... visible=`; `AI decision target=... visible=...` |
| intent | `#N INTENT WHY suit=... jump=... gait=... move=... state=...`; `MOTION INTENT` |
| look reason | `LOOK TRACE actor=... reason=... state=... interest=... clear=... rays=...` |
| aim/zone | `AIM ZONE actor=... preferred=head/body`; `DEMO AIM ERROR` |
| movement/path | `MOVE TRACE actor=... pos=... vel=... blocked=...`; `NAV fallback`; `LINK start/done/failed`; `PROGRESS ESCAPE` |
| fire | `FIRE SHOT shooter=... target=... class=... weapon=...`, `FIRE HIT phase=before-Lua ...`, `Stock weapon burst`, `Hit before-Lua` |
| suit/jump | `BOT SUIT mode=... energy=...`, `BOT ACTION: JUMP/SPRINT`, `JUMP TAKEOFF/LANDING` |
| stuck/trap | `TRAP RECOVERY level=...`, `PROGRESS ESCAPE`, `MOVE TRACE blocked=1` sustained ≥3 s |
| weapon choice | `WEAPON RANK actor=... select=... distance=...`, `REAL PICKUP`, `REAL PICKUP ... acquired=1` |
| glance/vantage | `GLANCE spot=... actor=...`, `VANTAGE ... actor=...` |
| demo | `BRAIN`/`FRAME`/`EVENT` records at the same `t`; event 10 = mark |
| source line | `INTENT WHY` values map through `BotBrain.cpp` function table already in PlaytestReport (`fn(ln)`) |

Derived fields for the card:

- `cause_candidates[]`: `LOS blocked`, `out of fire range`, `wrong weapon`, `reaction/flick`, `target switch`,
  `stuck/trap`, `input/pose conflict`, `vehicle/no-fire`, `no path`, `unknown`.
- `evidence[]`: each with `t`, `line_no`, `source`, `snippet`; only ±5 s.
- `confidence`: `high` only when a single source line explains the event (e.g. `reason=blocked` + `blocked=1`),
  otherwise `medium/low` and list alternatives.

## 4. PlaytestReport integration

`playtest_report.py` already parses `TARGET CHOICE`, `INTENT WHY`, `MOVE TRACE`, `TRAP RECOVERY`, `FIRE HIT`,
`ARBITER`, `VANTAGE` and builds paths/events. Add:

1. Parse `OWNER MARK` into `marks[]` with `t/side/pos/yaw/pitch/spectating/text/utc/session`.
2. For each mark, find candidates using §2, then collect records in `[t-5,t+5]` from the existing dictionaries.
3. Emit a `marks` section in `report.html` above the event list: one card per mark, candidate tabs, timeline
   (look/aim/target/move/fire), and a “source line” link.
4. If a demo is given, map game time to wall time via `DEMO START` filename and put the video offset in the card.
5. If no candidate exists, still show the mark and owner context; do not drop the owner’s report.

## 5. Current gaps to close before the first mark run

- `wb_mark` does not call `m_demo.Mark(text)`; add it so event 10 is recorded next to `BRAIN`/`FRAME`.
- `BOT SUIT`, `BOT ACTION`, `Stock weapon burst` lines have no actor id (PlaytestReport issue list); the card cannot
  assign them to a candidate. Add `actor=<id>` to those lines.
- `AIM ZONE` has actor but no target position; add `target=` and `distance=`.
- `INTENT WHY` has no `combat/strafe/stance` source lines (T-1); add them or the card cannot say “why fire/strafe”.
- The mark line has no wall clock/session/build; add them before relying on client/server joins.
- `wb_mark` on a client with no `GetClientActor()` still writes a mark but no position; include `owner`/`spectating`.

## 6. Acceptance tests

| Test | Pass |
| --- | --- |
| `mark_spectated` | mark with `spectating=42`, card first candidate 42; ±5 s records shown |
| `mark_hit_owner` | bot 7 hits owner at t−2; card ranks 7 before an in-view bot |
| `mark_in_view` | two bots, one 5 deg and one 25 deg off view; the 5 deg one first |
| `mark_near_fallback` | no hit/view candidate, bot at 4 m selected |
| `mark_empty` | no bot in ±5 s; card still renders owner context and `unknown` |
| `mark_client_server` | client mark and server mark have same `utc/session`; card joins once |
| `mark_demo` | event 10 exists at the same game time; video offset ±0.2 s |
| `mark_source` | every `INTENT WHY` source line maps to a function; no “line 0” unless default |

Read-only; no game or build is started by this spec.
