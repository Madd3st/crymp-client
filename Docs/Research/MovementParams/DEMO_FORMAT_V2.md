# wbdemo/2 — freeze (diff to DEMO_FORMAT.md)

Заморозка формата `wbdemo/2` из внешнего аудита + `DEMO_SPEC_REVIEW.md`. Это **diff к
`Docs/Research/SteelmillTriage/DEMO_FORMAT.md`**; Claude применяет и обновляет Triage-парсер.

```diff
--- DEMO_FORMAT.md  (wbdemo/1)
+++ DEMO_FORMAT.md  (wbdemo/2)
@@ Container
- u16 version = 1
+ u16 version = 2
@@ ACTOR_STATE
   u16 weapon_class_idx   string table (e.g. "SCAR", "SOCOM"), 0 = none
   u32 weapon_id          entity id of the held weapon, 0 = none
+  u16 peak_yaw_cdeg      max |Δyaw| since the previous frame, for Triage "aim wander"
+  # yaw range: [-18000, 18000) cdeg (wraps at ±180°, no 360° aliasing)
@@ Records
 | 5 | EVENT | `u32 t_ms, u8 kind, u32 actor, u32 other, f32 value, f32 pos[3]` |
+| 6 | INTENT | bots only, one per decision tick (≤ hz): `u32 t_ms, u32 id, u8 move, i16 dir_cdeg, u8 gait, u8 jump, u8 suit, u8 fire, u32 target_id, u32 link_id, u16 reason_idx` |
+| 7 | INV   | on inventory change: `u32 t_ms, u32 id, u8 n (<=4), n × { u16 class_idx, u8 clip, u16 reserve }` |
+  # t_ms is GAME time (GetFrameStartTime / BotBrain::m_now), never wall-clock GetAsyncTime.
@@ BRAIN
- | 4 | BRAIN | bots only, every frame the bot thinks (≤ hz): `u32 t_ms, u32 id, u16 state_idx, u32 target_id, u8 target_visible, f32 aim[3], f32 dest[3], f32 next_corner[3], u16 reason_idx` |
+ | 4 | BRAIN | bots only, every frame the bot thinks (≤ hz): `u32 t_ms, u32 id, u16 state_idx, u32 target_id, u8 target_visible, u8 visible_count, u8 in_cone, u8 target_cloaked, u32 last_attacker_id, f32 aim[3], f32 dest[3], f32 next_corner[3], u16 reason_idx` |
@@ header JSON
   header_len bytes  UTF-8 JSON: {"map":..., "build":"<git sha>", "started":"<ISO local time>", "hz":20,
-                   "server":"<label>", "flags":{"wb_bot_pace":1,...}}
+                   "server":"<label>", "navmesh":"<geometry hash>", "flags":{"wb_bot_pace":1,...},
+                   "actors":[{"id":..,"kind":0|1,"name_idx":..,"skill":..,"style":..,"profile_idx":..}, ...]}
```

## Hybrid recording (обязательно в spec)

```diff
@@ new section
+ ## Hybrid (server + owner client)
+ The server writes records 2..7 for bots + humans + events. The owner's CLIENT appends its own
+ precise view/sprint as a sidecar `<demo>.wbdemo.client` (one line per frame:
+ `t_ms yaw_cdeg pitch_cdeg sprint viewQuat(x,y,z,w)`), because the server-side human view is the
+ smoothed/serialized aim (Player.cpp:5292-5293 + mp_netAimLerpFactor :1941-1971) and human sprint is
+ not replicated. Triage merges sidecar by t_ms; missing sidecar → fall back to server FRAME.
+ Client hook: reuse the existing MOVE TRACE client-side writer (wb_move_trace side=client, local=1);
+ add yaw/pitch/sprint from the LOCAL actor's GetViewRotation()/m_viewQuatFinal and bSprinting.
```

## INTENT / INV — источники полей (для Codex)

- `INTENT.dir_cdeg` = `BotIntent.destination` минус pos (yaw хода); `gait/jump/suit/fire` =
  `BotIntent` (`BotIntent.h:17-24`); `target_id` = `m_combat.Target()`; `link_id`/`reason_idx` — будущие
  off-mesh link (Detour) и строка-таблица (STRING record).
- `INV.class_idx/clip/reserve` = `GetInventory()` + `IWeapon::GetFireMode()->GetAmmoCount()` /
  `GetInventory()->GetAmmoCount(ammoType)` (`BotWeapons.cpp:35-37`), эмитится на `eGE_ItemPickedUp`/
  `eGE_ItemDropped`/reload.

## Поля, которые Codex НЕ должен выкинуть (Triage/owner зависят)

- ACTOR_STATE: `pos`, `yaw_cdeg`, `pitch_cdeg`, `stance`, `flags` (все 6 бит), `suit_mode`, `energy`,
  `health`, `weapon_class_idx`, `weapon_id` (+ новый `peak_yaw_cdeg`).
- BRAIN: `state_idx`, `target_id`, `target_visible`, `aim[3]`, `dest[3]`, `reason_idx` (+ новые
  `visible_count/in_cone/target_cloaked/last_attacker_id`).
- EVENT kinds 1–10 (shot/hit/kill/revive/pickup/drop/suit/jump/land/chat) — не менять номера.
- `t_ms` — **game time** везде (не `GetAsyncTime`), иначе seek/merge по времени разъедется.
