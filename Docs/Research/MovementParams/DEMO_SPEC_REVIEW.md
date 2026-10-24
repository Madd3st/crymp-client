# WarsBots: ревью spec `wbdemo/1` против движка (+ puppet-подход для C-026)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`.
Ревью `Docs/Research/SteelmillTriage/DEMO_FORMAT.md` (Claude) против движка. Только чтение кода.

## 1. ACTOR_STATE: доступность на сервере для ботов и людей

| Поле | Боты (channel 0) | Люди (remote) | file:line | Разрыв |
| --- | --- | --- | --- | --- |
| `pos x,y,z` | ✅ сервер-авторитет | ✅ сервер-авторитет | `GetEntity()->GetWorldPos()` | нет |
| `yaw_cdeg/pitch_cdeg` (взгляд) | ✅ `m_viewQuatFinal` | ⚠️ сериализованный `viewQuat`, **сглаженный** aim | `GetViewRotation()` `Player.cpp:442-444`; `ser.Value("viewQuat"/"viewQuatFinal")` `Player.cpp:5292-5293`; lerp `mp_netAimLerpFactor` `Player.cpp:1941-1971` | человеческий взгляд на сервере — **реконструкция** (не пиксельный FP-вид клиента) |
| `stance` | ✅ `GetStance()` | ✅ | `GetStance()` | нет |
| `flags.bit0 alive` | ✅ `GetHealth()>0` | ✅ | `GetHealth()` | нет |
| `flags.bit1 sprint` | ✅ `m_warsBotSprint` | ❌ **не реплицируется** (C-015: «server sprint=0 for humans») | `Player.cpp:4953-4955` (бот); `m_stats.Serialize` `Player.cpp:5306` | человеческий спринт на сервере = 0 |
| `flags.bit2 grounded` | ✅ | ✅ | `m_stats.inAir`/`onGround` | нет |
| `flags.bit3 firing` | ✅ `WarsBotWeaponFiring()` | ✅ `weapon->IsFiring()` | `Player.cpp:4993` | нет |
| `flags.bit4 zoomed` | ✅ | ⚠️ zoom-состояние weapon клиент-локально | `CWeapon` zoom state | проверить репликацию zoom |
| `flags.bit5 reloading` | ✅ `weapon->IsReloading()` | ✅ | `BotWeapons.cpp:163` (`IsReloading`) | нет |
| `suit_mode` | ✅ | ✅ | `CNanoSuit::GetMode()`; `BotSuit.cpp` | нет |
| `energy` | ✅ | ✅ | `GetSuitEnergy()` | нет |
| `health` | ✅ | ✅ | `GetHealth()` | нет |
| `weapon_class_idx/weapon_id` | ✅ | ✅ | `GetCurrentItem()` | нет |

**Вывод по полям**: всё доступно, кроме двух вещей — (а) человеческий **sprint** (не реплицируется на сервер),
(б) человеческий **взгляд** (на сервере это сериализованный/сглаженный aim, а не точный клиентский вид).

## 2. eGE_* → EVENT kinds

| EVENT kind | Источник события | file:line |
| --- | --- | --- |
| 1 shot | `eGE_WeaponShot` | `Items/Weapons/FireModes/Shotgun.cpp:366,500` (проверить остальные fire-modes) |
| 2 hit | `eGE_Hit` / `eGE_WeaponHit` | `IGameplayRecorder.h:44,47` |
| 3 kill | `eGE_Kill` | `IGameplayRecorder.h:38` |
| (смерть) | `eGE_Death` | `Player.cpp:4612` |
| 4 revive | `eGE_Revive` | `IGameplayRecorder.h:40` |
| 5 pickup | `eGE_ItemPickedUp` | `IGameplayRecorder.h:56` |
| 6 drop | `eGE_ItemDropped` | `IGameplayRecorder.h:57` |
| 7 suit | `eGE_SuitModeChanged` | `NanoSuit.cpp:849` |
| 8 jump / 9 land | **нет eGE_\*** — нужен свой источник | `PublishWarsBotJump` (бот) `Player.cpp:4960-4965`; `SPlayerStats.jumped` (человек); MOVE TRACE `takeoffs/landEdges` |
| 10 chat/marker | свой (`wb_demo_mark`) | — |

**Разрыв**: jump/land не имеет нативного события; демо-райтер должен сам детектить взлёт/приземление (для бота —
`PublishWarsBotJump`/`BotLocomotion::Observe` JUMP TAKEOFF/LANDING; для человека — переход `SPlayerStats.jumped`
или заимствование латчей `takeoffs/landEdges` из MOVE TRACE).

## 3. Правки spec (diff-предложение)

```diff
@@ ACTOR_STATE flags
- bit1 sprint
+ bit1 sprint     # bots: m_warsBotSprint; humans: 0 на сервере (не реплицируется) —
+                 #   для точного человеческого спринта писать клиентский доп. поток и сливать с серверным.
@@ yaw/pitch
  i16 yaw_cdeg, i16 pitch_cdeg
+ # люди: = сериализованный viewQuat/aim (Player.cpp:5292-5293), сглажен mp_netAimLerpFactor (:1941-1971);
+ #   это "куда смотрит", не пиксельный FP-вид. Для owner-точности — гибрид: клиент пишет свой view/sprint сам.
@@ EVENT 8/9
  8 jump / 9 land  # нет eGE_*: бот PublishWarsBotJump (Player.cpp:4960-4965) + Observe JUMP TAKEOFF/LANDING
-                   # (BotLocomotion.cpp:41,49); человек — SPlayerStats.jumped-переход или MOVE TRACE takeoffs/landEdges.
+                   # (BotLocomotion.cpp:41,49); человек — SPlayerStats.jumped переход или MOVE TRACE takeoffs/landEdges.
```

Рекомендация: **гибридная запись** — сервер пишет ботов + сущности + события; owner-клиент дописывает свой
точный `view`/`sprint` (одна строка на кадр), Claude сливает по `t_ms`. Это закрывает оба разрыва без новой
репликации в движке.

## 4. Puppet-подход для C-026 (in-game play)

**Спавн channel-less `CPlayer`** — уже есть: `CryMPEngineAdapter` создаёт бота как
`ActorSystem::CreateActor(0, name, "Player", position, Quat(IDENTITY), Vec3(1,1,1))` (`CryMPEngineAdapter.cpp:32`)
— channel 0, без `INetChannel`, `CActor::Init` доводится через `BindToNetwork` (`:31-33`). Для puppет делаем то
же, но **без** `BotBrain`/`BotManager` (чистый визуальный актор).

**Ведение позы/взгляда без физики**: переиспользовать реплика-путь бота `SetWarsBotMotion` +
`UpdateWarsBotMotion` (`Player.cpp:4966-5019`): в снапшоте пишем `m_warsBotDirection`/`m_warsBotAim`/
`m_warsBotSpeed`/`m_warsBotStance`, а `UpdateWarsBotMotion` (реплика-ветка `:4996-5019`) разворачивает
`m_baseQuat`/`m_viewQuat` и шлёт `CMovementRequest` (`SetMoveTarget/SetDesiredSpeed/SetLookTarget/SetAimTarget`)
— **без физической симуляции** (движение — это только поза/анимация). Для «замороженной» позы достаточно
ставить `SetPos`/`SetViewRotation` (`Player.cpp:433-438`) + входы animation-graph (stance/sprint/pseudoSpeed).

**Следование `wb_watch`/F1**: spectator уже умеет следовать за канальными-0 ботами — `wb_watch
list|<n>|next|prev|status|off` + нативный F1 FP/TP (см. `AGENT_HANDOFF`, `BotSpectator.h:1-19`). FP-реконструкция
aim для бота уже сделана через `m_netAimDir = (m_warsBotAim - eye)` (`Player.cpp:1924-1929`). Для puppет расширяем
список `wb_watch` (или отдельный `wb_demo_watch <actorId>`) на puppет-акторов; F1 наследуется бесплатно.

**Риски**: (1) puppет без коллизий не «врезается» в геометрию — интерполяция между снапшотами может пройти
сквозь стену (визуально терпимо на 20 Гц); (2) анимация ходьбы/прыжка на puppет — нужен pseudoSpeed/stance из
снапшота (уже в ACTOR_STATE); (3) сущность без `BotManager::Owns` должна обходить ownership-гейты
(`SetWarsBotMotion` требует `Owns(GetEntityId())`, `Player.cpp:4968`) — добавить «puppet-ownership» флаг или
отдельный setter; (4) lifecycle: пупеты чистить на `wb_demo_stop`/map change.

## Итог

Spec рабочий; два поля требуют решения (человеческий sprint/view — гибридная запись), jump/land нужен свой
источник, puppet-подход строим на существующем channel-0 `CreateActor` + реплика-пути бота. Согласен со spec,
правки выше — предложения, к коду не применял.
