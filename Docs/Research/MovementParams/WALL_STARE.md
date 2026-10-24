# WarsBots: почему боты стоят и смотрят в стену (аудит aim)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`
(`warsbots/feature/weapon-goals` HEAD). Только чтение кода, игра/серверы не запускались.
Аудит источников `intent.aim` / направления взгляда по состояниям FSM.

## 0. Как задаётся взгляд (два независимых механизма)

1. **`m_intent.aim`** — «куда смотреть», задаётся каждый тик решения в `BotBrain::Update`
   (`BotBrain.cpp:276` по умолчанию, переопределяется в каждом состоянии).
2. **Направление тела** — `CPlayer::UpdateWarsBotMotion` разворачивает корпус в
   `facing = m_warsBotStrafe ? (m_warsBotAim - pos) : m_warsBotDirection` (`Player.cpp:4981`).
   Т.е. без стрейфа бот **смотрит туда, куда идёт**, а прицел/голова — на `m_warsBotAim`
   (`SetLookTarget/AimTarget/FireTarget = lookAt`, `BotMovement.cpp:110-112`, `Player.cpp:5014-5019`).

Ключевой факт: **aim не сглаживается в сторону направления движения или открытого пространства**.
Сглаживается только боевой aim к врагу (`m_trackedAim`, `BotBrain.cpp:438-445`) и добавляется шум
(`:448`). Для roam/оружия aim — это «путевой узел + вверх», безо всякой фильтрации.

## 1. Таблица «состояние — источник aim — file:line — может смотреть в стену?»

| Состояние | Источник aim | file:line | В стену? | Почему |
| --- | --- | --- | --- | --- |
| (база, все состояния) | `pos + facing*10 + (0,0,1.4)` — 10 м вперёд по текущему развороту | `BotBrain.cpp:276` | **да** | если `move=false`, aim не переопределяется → взгляд остаётся по прошлому развороту (часто в стену, в которую упёрся) |
| SEEKING_WEAPON | базовый (стоит, `move=false`) | `BotBrain.cpp:322,326` + `276` | **да** | бот неподвижен, смотрит по инерции вперёд |
| MOVING_TO_WEAPON (seek) | `Steer(...)` узел + `(0,0,1.2)` | `BotBrain.cpp:318` | **да** | узел/цель оружия за стеной (fallback `Steer` возвращает сырой goal) |
| MOVING_TO_WEAPON (upgrade) | `Steer(...)` узел + `(0,0,1.2)` | `BotBrain.cpp:371` | **да** | то же |
| SEARCHING_ENEMY (roam) | `Steer(...)` узел + `(0,0,1.3)` | `BotBrain.cpp:417` | **да** | патрульный waypoint/hotspot за стеной |
| CHASING_ENEMY (враг не виден) | `m_trackedAim` = последняя видимая позиция врага | `BotBrain.cpp:523`, `438-445` | **да** | враг за стеной → aim сквозь стену |
| CHASING_ENEMY (враг виден, далеко) | `m_trackedAim` (видимый враг) | `BotBrain.cpp:523` | нет | враг на линии взгляда |
| ATTACKING | `m_trackedAim` + шум | `BotBrain.cpp:506,448` | нет | враг виден |
| withdraw (отход) | `m_trackedAim` (враг) | `BotBrain.cpp:463` | **да** | пятится, продолжая целиться во врага (иногда через стену) |
| retreat (отход вплотную) | `m_trackedAim` (враг) | `BotBrain.cpp:477` | **да** | то же |
| RecoverTrap (watchdog) | не меняется (застой) | `BotBrain.cpp:622` | **да** | шагает вбок/назад со старым aim |
| прибытие (arrival, `move=false`) | базовый | `BotBrain.cpp:314,366` + `276` | **да** | неподвижен, смотрит по прошлому направлению |

## 2. Случаи «aim указывает в геометрию»

- **Fallback `Steer` возвращает сырой goal**: если граф не готов (`BotBrain.cpp:90
  `if(!nav.Ready())return goal;`) или путь пуст (`:109` «sparse graph can miss a physically
  reachable item»), `Steer` возвращает саму цель (оружие/waypoint). Тогда `MoveTo(step, step+up)`
  ставит aim **на цель за стеной** — бот идёт в стену и смотрит в неё (`:318,:371,:417`).
- **Достижимый узел за препятствием**: даже с путём, `m_path[m_pathIndex]` (`:117`) — следующий
  узел; если бот застрял (blocked) до него, он продолжает целиться в узел на той стороне стены.
- **Последняя позиция врага за стеной**: `CHASING_ENEMY` без LOS держит `destination =
  m_combat.LastPosition()` и `aim = m_trackedAim` (`:431-445,:523`) — обе точки за стеной, пока
  память врага (3.0 с, `BotCombat.cpp:142`) не истечёт.
- **Оружие сквозь стену**: `MOVING_TO_WEAPON` идёт к `m_weaponTarget`/`m_upgradeTarget`, aim =
  узел в его сторону; если оружие на полке/за стеклом, бот смотрит сквозь геометрию (`:318,:371`).
- **Неподвижный бот**: `move=false` (SEEKING_WEAPON, arrival) → `BotLocomotion::Execute` вызывает
  `Stop`+`Aim(actor, intent.aim)` (`BotLocomotion.cpp:122` → `BotMovement.cpp:26-40`), и `Aim`
  ставит look-цель на `intent.aim` (базовый = 10 м вперёд). Разворот бота — от прошлого движения,
  поэтому «стоит, упёршись взглядом в стену».

## 3. Сглаживается ли aim в сторону движения/открытого пространства?

**Нет.** Единственное сглаживание — `m_trackedAim += (aim - m_trackedAim)*BotAimControl::Alpha(...)`
(`BotBrain.cpp:444`), и оно тянет aim **к врагу**, не к направлению хода. Для roam/оружия aim —
прямое «узел+вверх», без ray-проверки и без разворота в открытое пространство. «Разворот в открытое
пространство» отсутствует: `Steer` лишь возвращает узел/цель, а `UpdateWarsBotMotion` доворачивает
тело к `m_warsBotDirection` (направление к этому же узлу), т.е. в ту же стену.

## 4. Предложение диагностики (MOVE TRACE)

Добавить в запись `MOVE TRACE ...` три поля (см. контракт в `move-trace-2026-10-24.md`):

```
aimYaw   = atan2(aim.x-pos.x, aim.y-pos.y)   # куда смотрит aim
velYaw   = atan2(vel.x, vel.y)                # куда фактически движется
wallDist = ray вдоль aim (1–2 м), 0 если сразу упёрся
```

- `wallDist < ~1.0` при `speed < ~0.2` → «стоит лицом в стену» (idle + facing wall).
- `|aimYaw - velYaw| > ~0.5 рад` при движении → «идёт/смотрит в разные стороны» (aim ≠ ход).
Triage получает счётчик «facing wall while idle» = `speed<0.2 && wallDist<1.0`, и долю кадров, где
`aimYaw` расходится с `velYaw` — это и есть метрика «sheep against a wall».

## 5. Минимальный набросок фикса (предложение, код не трогаю)

Когда нет **видимого** врага (roam / MOVING_TO_WEAPON / SEEKING_WEAPON / arrival / CHASING без LOS),
ставить aim вдоль **направления движения** или в открытое пространство, а не на путевой узел:

1. `MoveTo(destination, destination + dir*N + up)`, где `dir` — текущий вектор хода (`destination - pos`),
   вместо `step + up`. Это разворачивает взгляд по ходу, а не в узел за стеной.
2. При `move=false` (stall/arrival) — ray-проверка базового aim (10 м): если упёрся в геометрию,
   довернуть в ближайшее свободное направление (веер лучей, как уже есть в `BotMovement::SafeStep`,
   `BotMovement.cpp:97-101`) или в `velYaw`.
3. CHASING без LOS — держать `aim` на `destination` (последняя позиция врага), но добавлять
   ray-проверку: если `wallDist < 1 м`, доворачивать aim к направлению обхода (`Steer` уже даёт
   направление обхода), а не в точку за стеной.
4. Не менять боевой aim (видимый враг) — там `m_trackedAim` работает корректно.

Всё за будущим флагом (например, расширить `wb_bot_pace` или отдельный `wb_bot_look`), default OFF.
