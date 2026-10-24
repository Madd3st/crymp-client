# WarsBots: почему спринт только 23 % (SPRINT GAP, wb_bot_pace)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`; цель разбора — weapon-goals
**`ef25395e`** (`wb_bot_pace`). Только чтение кода; парный QA не запускался. Метрика — `sprint=1` в
MOVE TRACE server, доля от движущихся сэмплов. Цель 40–55 %, факт 23 %; Speed-mode 4 % от времени движения.

## 1. Цепочка условий «бот едет» → `sprint=1` (file:line по ef25395e)

`MOVE TRACE sprint=1` = нативный `ACTION_SPRINT` установлен и движение принято
(`BotLocomotion.cpp:131` `SetWarsBotActions(sprint && accepted, ...)`).

| # | Условие | file:line | Что ограничивает |
| --- | --- | --- | --- |
| 1 | `intent.move` (бот хочет идти) | `BotBrain.cpp` `MoveTo(...)` в каждом состоянии | статичные/aim-only состояния |
| 2 | `intent.gait == Sprint` | `BotBrain.cpp:76` `TravelGait`: `allowed && distance>=8.f ? Sprint : Run` | **дистанция < 8 м** и/или `allowed=false` |
| 3 | `allowed = skill != Beginner \|\| useSpeed` | `BotBrain.cpp:74` | **Beginner никогда не спринтует** (`useSpeed=s>0`, `BotProfile.h:133`) |
| 4 | `intent.combat != Fire` | `BotLocomotion.cpp:120` (pace-ветка) | стрельба/атака — нет спринта |
| 5 | `state.grounded` | `BotLocomotion.cpp:121` | в прыжке/воздухе нет |
| 6 | `state.suitActive` | `BotLocomotion.cpp:121` | костюм неактивен (редко) |
| 7 | `!jump && !m_pending && !m_airborne` | `BotLocomotion.cpp:121` | фаза прыжка/взлёта/полёта |
| 8 | `!actor.WarsBotWeaponFiring()` | `BotLocomotion.cpp:121` | нативный выстрел |
| 9 | `PaceSprintEnergy(sprinting, energy)` = `sprinting ? >20 : >=40` | `BotPacePolicy.h:5`, `BotLocomotion.cpp:122` | энергетический гистерезис 40/20 |
| 10 | `accepted` (движение реально принято) | `BotLocomotion.cpp:128-131` | blocked/прибытие |

Speed-режим (`mode=0`) отдельно: `PaceSpeedBurst(active, distance, energy)` = `active ? (>60 && ≥8) :
(≥150 && ≥30)` (`BotPacePolicy.h:6`), применяется в `BotBrain.cpp:75`; при false костюм = `NANOMODE_DEFENSE`
(Armor) (`BotBrain.cpp:77`).

## 2. Ключевой факт: спринт в Armor **бесплатный**

Слив энергии за спринт живёт только в `CNanoSuit::UpdateSprinting` под
`if (m_currentMode == NANOMODE_SPEED && stats.bSprinting)` (`NanoSuit.cpp:1611`). В pace-режиме обычный
спринт идёт в **Armor** (Speed только при `m_speedTravel`), значит **энергия при спринте не тратится** —
гистерезис 40/20 (условие 9) в спокойном перемещении почти никогда не срабатывает (энергия держится у
максимума 200). Энергия убывает только от Strength-прыжков (70), попаданий (Armor) и Speed-рывков.

Вывод: **главный ограничитель не энергия, а условия 2–4 и 7** (дистанция/состояния/боевые ворота).

## 3. Оценка вклада каждого ограничителя

| Блокер | Оценка | Почему |
| --- | --- | --- |
| 2 `distance >= 8` (`BotBrain.cpp:76`) | **наибольший** | последние ~8 м каждого перехода идут Run; roam пере-выбирает цель при <2 м (`BotBrain.cpp:382`), поэтому «финальная фаза» почти всегда Run |
| 4 + ATTACKING не зовёт `TravelGait` | **большой** | в бою (`ATTACKING`, `withdraw`) gait=Run и `combat=Fire`; Steelmill боевая карта, боёвое перемещение тянет долю вниз |
| 3 Beginner (`BotBrain.cpp:74`) | средний, ростер-зависимый | Beginner даёт 0 % спринта; при наличии в паре тянет среднее |
| 7 jump/airborne | средний | кулдаун 0.5 с → прыжки чаще, больше времени в воздухе |
| 9 энергия 40/20 | **малый** | Armor-спринт не тратит энергию (см. §2) |
| Speed 150/60 + ≥30 м (`BotPacePolicy.h:6`) | отдельная причина 4 % Speed | 150 энергии почти не достигается (нужно не прыгать/не получать урон); owner ~13 % Speed, бот 4 % |

## 4. Минимальный патч-скетч (предложение, код не трогаю)

Цель: спринт 40–55 %, Speed-рывки owner-подобные, не ломая OFF.

1. **Снизить дистанционный гейт** — `BotBrain.cpp:76`: `distance >= 8.f` → `distance >= 3.f`. Это
   переводит «финальные» 3–8 м и короткие ноги в спринт. (Главный рычаг для доли спринта.)
2. **Разрешить спринт в подходе к бою** — `BotLocomotion.cpp:120`: pace-ветку `combat != Fire` оставить,
   но в `ATTACKING` при `distance > engageRange` звать `TravelGait` (сегодня `ATTACKING` никогда не
   спринтует). Либо проще: не менять `ATTACKING`, а принять боевое перемещение как Run (owner тоже не
   спринтует вплотную) — тогда цель достигается за счёт пункта 1.
3. **Ослабить Speed-рывок** — `BotPacePolicy.h:6`: `energy >= 150 && distance >= 30` → `energy >= 130 &&
   distance >= 18`; возврат `<= 60` → `<= 50`. Поднимет Speed с 4 % к ~10–13 % (owner-подобно), Armor
   останется доминирующим (84–87 %).
4. **Опционально** — энергия `PaceSprintEnergy` 40/20 → 30/10 (`BotPacePolicy.h:5`) на случай, если после
   боёв бот часто стартует ниже 40; не главный рычаг (см. §2).

Проверка — `steelmill_triage.py pace` (C-013 acceptance): moving median ≥ 4.5, sprint 40–55 %, Armor ≥ 75 %,
Speed 8–15 %; `summary.json` traps/stalls/pickups без регресса; парный `wb_bot_pace 0` vs `1`.
