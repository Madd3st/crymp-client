# WarsBots: faster-than-real-time QA (time scale / fixed step)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`.
Только чтение кода. Готовит C-029 для Codex.

## 1. Таймерные cvar и `SetTimeScale`

- **Таймерные cvar в дереве отсутствуют**: `t_Scale`, `t_FixedStep`, `t_MaxStep`, `sv_DedicatedMaxRate`,
  `sys_MaxFPS`, `g_timescale` — **не найдены** в `Code/` (grep 0 регистраций). Они живут в **закрытой
  CrySystem DLL**; в исходниках есть только API и одно упоминание `t_scale` в `Docs/AI_LEARNING_FEASIBILITY.md:28`
  («t_scale effects on physics/network are untested»). Codex должен либо найти их эмпирически в DLL, либо
  звать `ITimer::SetTimeScale` напрямую.
- **API**: `ITimer::SetTimeScale(s)` / `GetTimeScale()` (`CryCommon/CrySystem/ITimer.h:56-59`). Таймеры:
  `ETIMER_GAME` = «pausable, serialized, frametime smoothed/scaled/clamped»; `ETIMER_UI` = «unprocessed»
  (`ITimer.h:24-29`). `GetFrameTime`/`GetFrameStartTime`/`GetCurrTime` — по умолчанию `ETIMER_GAME` (масштабируются).
- **`CBulletTime`** (`BulletTime.cpp:20-51`) — единственный пользователь `SetTimeScale` (slow-mo <1.0, строка
  `:50`). **MP-гейт есть**: `Update` `if (... || gEnv->bMultiplayer) return;` (`:22`) и `Activate`
  `if (!changed || gEnv->bMultiplayer) return;` (`:57`); команда `bulletTimeMode` — `VF_CHEAT` (`GameCVars.cpp:165`).
  То есть время-скейлинг в CryMP **никогда не активируется в мультиплеере** — прямого cheat-гейта на самом
  `SetTimeScale` нет, но единственный вызов SP-only.

## 2. Wall-clock против game-time в `Code/CryGame`

Ключ: **боты и боевой код идут по GAME-time** (масштабируются), а `GetAsyncTime` — только профилирование.

| Категория | Что | file:line | Ломается при scale? |
| --- | --- | --- | --- |
| Боты — таймеры/кулдауны | `GetFrameStartTime().GetSeconds()` | `BotInstance.cpp:202,237` | нет (game time) |
| Боты — решение 10 Гц | `dt` из `OnPostUpdate` → `m_tick+=dt` | `BotBrain.cpp:269-272` | нет (dt = game frame time) |
| Боты — рейкасты/навигация | `GetFrameStartTime` | `BotManager.cpp:261` | нет |
| Боты — **профилирование** | `GetAsyncTime()` | `BotInstance.cpp:231,253`; `BotNav.cpp:158-159,314-366,447-453`; `Game.cpp:688` | **не должно** масштабироваться (это реальное время — правильно) |
| GameRules | freeze/revive/kill-таймеры | `GameRules.cpp:358,1464,1960,2115,4609,4626` | нет (game time) |
| ShotValidator | `GetFrameStartTime` | `ShotValidator.cpp:43,100,188` | нет |
| Voting | `GetFrameStartTime` | `Voting.cpp:113` | нет |
| NanoSuit energy/regen | `frameTime` | `NanoSuit.cpp` (Update-цепочка) | нет (game frame time) |
| HUD/chat/меню | `GetAsyncTime()` (метки) | `FlashMenuObject.cpp:714`, `HUDTextChat.cpp:119`, `HUDTagNames.cpp:196` | только UI-метки (косметика) |
| Радар (компас) | `GetRealFrameTime` | `HUDRadar.cpp:1005` | нет (реальное время, не масштабируется — ок) |
| Лог-таймстамп | `GetCurrTime` | `BotLog.cpp:50` | нет (game time — то, что нужно для трейса) |

Вывод: **`SetTimeScale(>1)` ускорит ботов корректно** — вся игровая логика (таймеры, реген, кулдауны,
решения) в game-time. Wall-clock (`GetAsyncTime`) — только замеры производительности, их не трогать.

## 3. Физика

- `pe_vars` (мировые параметры физики) содержит `fixedTimestep` (`physinterface.h:1868`), `lastTimeStep`
  (`:1909`), **`timeScalePlayers`** (`:1920` — масштаб живых сущностей; используется в
  `CryPhysics/PhysicalWorld.cpp:2111,2695` «advance living entities»).
- `pe_simulation_params::maxTimeStep` — «maximum time step that entity can accept (larger steps will be split)»
  (`physinterface.h:282`). `IPhysicalWorld::TimeStep(time_interval)` (`:2205`), `DoStep` (`:1774`).
- **Что делает больший dt с капсулой игрока**: CryPhysics делит крупный шаг на под-шаги (`maxTimeStep`), так
  что грубого «туннелирования» нет, но контакт/ступенька/наклон считаются на большем суммарном шаге — при
  2×–4× возможны мелкие отличия в `step up`/`grounded` (бот может чуть иначе цепляться за низкие препятствия).
  `timeScalePlayers` масштабирует только живых (игроков/ботов), не статику.

## Рекомендация

| Режим | Как | Плюсы | Минусы |
| --- | --- | --- | --- |
| **Scale (`t_scale`/`SetTimeScale(>1)`)** | `ITimer::SetTimeScale` или cvar закрытого DLL | просто, боты корректны (game time) | физика dt растёт (мелкие контактные отличия); сетевой/UI wall-clock не масштабируется |
| **Fixed step + uncapped rate** | `pe_vars.fixedTimestep` + снять потолок кадров/тика (`sv_DedicatedMaxRate`), гнать N шагов на реальный кадр | детерминировано, физика пошагово точная | сложнее (нужен драйвер тика в CrySystem DLL или свой loop); сеть может не поспевать |

**Итоговая рекомендация**: для грубого soak/pace-замера — **`t_scale`/`SetTimeScale(2–4×)`** (боты корректны,
см. §2), но **не использовать для точных физических регрессий** (spawn/revive/contact), где мелкие отличия
шага существенны; для них — **fixed step + uncapped rate** с неизменным `dt`. `t_scale`/`t_FixedStep`/
`sv_DedicatedMaxRate` — вне исходников (закрытая DLL): Codex должен подтвердить их наличие эмпирически либо
идти через `ITimer::SetTimeScale` + `pe_vars.fixedTimestep` в открытом коде.
