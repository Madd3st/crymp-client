# WarsBots demo format `wbdemo/2` (frozen 2026-10-24 20:25; v1 = draft, never written by the game) (owner request 2026-10-24: "record demo / play demo")

Owner of the format: Claude (Triage parser `Tools/SteelmillTriage/wblab/demo.py`). Writer: Codex (`wb_demo_record`,
C-025). Player: Codex (`wb_demo_play`, C-026). Review: OpenCode. Background and engine audit:
movement-params `Docs/Research/MovementParams/DEMO_RECORDING.md` (OC-017, no CS-style recorder exists in CryMP).

Goals: (1) the owner replays a match **in the game** — free camera, follow anyone, first person, pause/seek/speed;
(2) Claude measures what logs cannot show — **where every player looks** (aim wander), reaction time, decision
flips, time to a real weapon — and sees *why* a bot acted (its brain track). Absolute poses, not inputs: playback
needs no determinism.

## Container

Little-endian. File: `%LOCALAPPDATA%\WarsBots\Demos\<map>-<YYYYMMDD-HHMMSS>[-<label>].wbdemo`, buffered, flushed
every ~1 s and on stop/map change; a map restart closes the file and opens a new one.

```
"WBDM"            4 bytes magic
u16 version = 2
u32 header_len
header_len bytes  UTF-8 JSON: {"map":..., "build":"<git sha>", "started":"<ISO local time>", "hz":20,
                   "server":"<label>", "navmesh":"<hash or none>",
                   "flags":{"wb_bot_pace":1,...}}   t_ms everywhere = server game time (GetCurrTime), not wall clock
records...        each: u8 type, u16 payload_len, payload   (unknown types are skipped by length)
```

## Records

| type | name | payload |
| --- | --- | --- |
| 1 | STRING | `u16 idx, u8 len, len bytes` — string table (states, weapon classes, reasons, names); written once before first use |
| 2 | ACTOR | `u32 t_ms, u32 id, u8 kind (0 human, 1 bot), u16 name_idx, u16 profile_idx, u8 join(1)/leave(0)` |
| 3 | FRAME | `u32 t_ms, u8 n`, then n × ACTOR_STATE (below) — every 1/hz s, all living or dead actors |
| 4 | BRAIN | bots only, every decision tick (≤ hz): `u32 t_ms, u32 id, u16 state_idx, u32 target_id, u8 target_visible, f32 aim[3], f32 dest[3], f32 next_corner[3], u16 reason_idx, u8 visible_count, u8 perception_bits (bit0 target in view cone, bit1 target cloaked, bit2 heard/damaged-by), u32 last_attacker` |
| 5 | EVENT | `u32 t_ms, u8 kind, u32 actor, u32 other, f32 value, f32 pos[3]` |
| 6 | INTENT | bots only, every decision tick — what the bot **asked for** (pose is what it got): `u32 t_ms, u32 id, f32 move_dir[2]` (unit XY, 0,0 = stop), `u8 gait` (0 walk, 1 run, 2 sprint), `u8 jump` (0 none, 1 normal, 2 Strength), `u8 suit_request` (ENanoMode, 255 none), `u8 fire` (0 none, 1 aim, 2 fire), `u32 target_id, u32 link_id` (navmesh off-mesh link, 0 none), `u16 reason_idx` |
| 7 | INV | on change, all actors: `u32 t_ms, u32 id, u8 n`, then n × `u16 class_idx, u16 clip, u16 reserve` (n ≤ 4) |

ACTOR_STATE (33 bytes):
```
u32 id
f32 x, y, z            feet position, world metres
i16 yaw_cdeg           view yaw, centi-degrees in [-18000, 18000), 0 = +Y (north), clockwise (the *view*)
i16 pitch_cdeg         view pitch, centi-degrees, + = up
u8  stance             0 stand, 1 crouch, 2 prone, 3 swim, 4 dead/spectating
u8  flags              bit0 alive, bit1 sprint, bit2 grounded, bit3 firing, bit4 zoomed, bit5 reloading
u8  suit_mode          0 speed, 1 strength, 2 cloak, 3 armor (ENanoMode)
u8  energy             0..200
u8  health             0..255 (clamped)
u16 weapon_class_idx   string table (e.g. "SCAR", "SOCOM"), 0 = none
u32 weapon_id          entity id of the held weapon, 0 = none
u16 peak_yaw_cdeg      max |Δyaw| since the previous FRAME (keeps 50–150 ms flicks visible at 20 Hz)
```

Humans on the server: view = serialized aim (`Player.cpp:5292-5293`, smoothed by mp_netAimLerpFactor), sprint
bit = 0 (not replicated). For exact owner data the **client** writes a sidecar `<same name>.wbdemo.client` in the
same container: STRING/ACTOR for itself + FRAME with n = 1 (its own view, sprint, peak yaw) at 20 Hz, its own game
time in `t_ms`. Triage aligns the sidecar to the server file by position cross-correlation (offset search ±5 s,
0.05 s steps) and replaces that actor's yaw/pitch/sprint/peak. Fields Codex must not drop: everything in FRAME,
BRAIN, INTENT; EVENT shot/hit/kill/revive/pickup/jump/land; INV.

EVENT kinds: 1 shot (other = hit entity or 0, value = damage dealt if known), 2 hit (actor = victim, other = shooter,
value = damage), 3 kill (actor = victim, other = killer), 4 revive (value = spawn id), 5 pickup (other = item id,
value = class idx), 6 drop, 7 suit (value = new mode), 8 jump (value = 0 normal / 1 Strength), 9 land (value = fall
height) — no native eGE for 8/9: bots via `PublishWarsBotJump`/locomotion take-off & landing, humans via the
`SPlayerStats.jumped` transition (same latches as MOVE TRACE), 10 chat/marker (value = string idx; owner bookmarks via `wb_demo_mark <text>`).

Size: 16 actors × 33 B × 20 Hz ≈ 11 KB/s + brain/intent ≈ 20–25 KB/s → 12–15 MB per 10 min. Acceptable.

## Console

`wb_demo_record 0|1` (default 0; when 1 the server records every match automatically), `wb_demo_hz` (default 20),
`wb_demo_start [label]`, `wb_demo_stop`, `wb_demo_mark <text>`; client `wb_demo_play <file>`, `wb_demo_pause`,
`wb_demo_seek <seconds>`, `wb_demo_speed <0.25..8>`, `wb_demo_stop`; follow/first person via the existing
`wb_watch` spectator + F1. Optional overlay `wb_demo_brain 1`: state text, target line, intended aim point.

## What Triage computes from it (`steelmill_triage.py demo <file>`)

- **aim wander**: per actor kind, median |Δyaw|+|Δpitch| in °/s while alive and no visible target, and the error
  between the bot's intended aim point (BRAIN) and its actual view (FRAME) — owner baseline vs bots;
- **reaction time**: first BRAIN frame with `target_visible` → first `shot` event at that target;
- **decision flips**: A→B→A state returns within 3 s per bot-minute;
- **time to primary**: revive → first pickup of a non-pistol class; share of kills by weapon class (SCAR first, C-022);
- everything the log-based `replay` page shows, now with real view direction.

Engine mapping of every field and event (file:line): movement-params `Docs/Research/MovementParams/DEMO_SPEC_REVIEW.md`
(OpenCode OC-018). Changes v1 → v2 after the external audit (Grok 4.7) and that review: yaw range, peak yaw, INTENT,
INV, perception bits, last attacker, navmesh hash, client sidecar for humans.
