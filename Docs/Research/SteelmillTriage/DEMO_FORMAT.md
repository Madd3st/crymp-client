# WarsBots demo format `wbdemo/1` (owner request 2026-10-24: "record demo / play demo")

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
u16 version = 1
u32 header_len
header_len bytes  UTF-8 JSON: {"map":..., "build":"<git sha>", "started":"<ISO local time>", "hz":20,
                   "server":"<label>", "flags":{"wb_bot_pace":1,...}}
records...        each: u8 type, u16 payload_len, payload   (unknown types are skipped by length)
```

## Records

| type | name | payload |
| --- | --- | --- |
| 1 | STRING | `u16 idx, u8 len, len bytes` — string table (states, weapon classes, reasons, names); written once before first use |
| 2 | ACTOR | `u32 t_ms, u32 id, u8 kind (0 human, 1 bot), u16 name_idx, u16 profile_idx, u8 join(1)/leave(0)` |
| 3 | FRAME | `u32 t_ms, u8 n`, then n × ACTOR_STATE (below) — every 1/hz s, all living or dead actors |
| 4 | BRAIN | bots only, every frame the bot thinks (≤ hz): `u32 t_ms, u32 id, u16 state_idx, u32 target_id, u8 target_visible, f32 aim[3], f32 dest[3], f32 next_corner[3], u16 reason_idx` |
| 5 | EVENT | `u32 t_ms, u8 kind, u32 actor, u32 other, f32 value, f32 pos[3]` |

ACTOR_STATE (31 bytes):
```
u32 id
f32 x, y, z            feet position, world metres
i16 yaw_cdeg           view yaw, centi-degrees, 0 = +Y (north), clockwise   (the *view*, not velocity)
i16 pitch_cdeg         view pitch, centi-degrees, + = up
u8  stance             0 stand, 1 crouch, 2 prone, 3 swim, 4 dead/spectating
u8  flags              bit0 alive, bit1 sprint, bit2 grounded, bit3 firing, bit4 zoomed, bit5 reloading
u8  suit_mode          0 speed, 1 strength, 2 cloak, 3 armor (ENanoMode)
u8  energy             0..200
u8  health             0..255 (clamped)
u16 weapon_class_idx   string table (e.g. "SCAR", "SOCOM"), 0 = none
u32 weapon_id          entity id of the held weapon, 0 = none
```

EVENT kinds: 1 shot (other = hit entity or 0, value = damage dealt if known), 2 hit (actor = victim, other = shooter,
value = damage), 3 kill (actor = victim, other = killer), 4 revive (value = spawn id), 5 pickup (other = item id,
value = class idx), 6 drop, 7 suit (value = new mode), 8 jump (value = 0 normal / 1 Strength), 9 land (value = fall
height), 10 chat/marker (value = string idx; owner bookmarks via `wb_demo_mark <text>`).

Size: 16 actors × 31 B × 20 Hz ≈ 10 KB/s + brain ≈ 15–20 KB/s → 10–12 MB per 10 min. Acceptable.

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
