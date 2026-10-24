# WarsBots: путь бота против пути игрока (движение)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`
(`warsbots/feature/weapon-goals`). Только чтение кода: игра и серверы не запускались.
Все пути ниже — от корня репозитория. Пометка «расчёт» означает арифметический вывод из
указанных строк, а не отдельную константу в коде.

Бот — это `CPlayer` с `channelId=0` и `IsPlayer()==false`:
`Code/CryGame/Actors/Player/Player.h:625`; `Code/CryCommon/CryAction/IActorSystem.h:123-126`.
Это важно: `CPlayerMovement` вызывает `AdjustMovementForEnvironment` только для `IsPlayer()`
(`Code/CryGame/Actors/Player/PlayerMovement.cpp:1070-1071`), поэтому бот не получает
Speed-mode множители `1.3` (ходьба) и `1.5` (слот Speed), которые есть у игрока
(`PlayerMovement.cpp:1422-1433`; `PlayerMovement.cpp:1426-1428`).

## 1. Короткий ответ для step-2 (pace/sprint)

| Что | Игрок | Бот сейчас | Отличие | file:line |
| --- | --- | --- | --- | --- |
| Базовая скорость | 4.5 м/с (стойка stand, полный стик) | 2.5 м/с (Run/Sprint), 1.3 м/с (Walk) | База бота на 44% ниже; дискретная Walk | `Code/CryGame/Actors/Player/Player.cpp:2597`; `Code/CryGame/Bots/BotLocomotion.cpp:121` |
| Спринт, обычный режим | 6.75 м/с (`4.5 × 1.5`) | 3.75 м/с (`2.5 × 1.5`) | Бот медленнее на 44% | `PlayerMovement.cpp:1041-1045`; `Player.cpp:2702`; `BotLocomotion.cpp:121` |
| Speed mode, без спринта | 5.85 м/с (`4.5 × 1.3`) | 2.5 м/с | Бот не получает `×1.3` | `PlayerMovement.cpp:1432-1433`; `PlayerMovement.cpp:1070-1071` |
| Speed mode, спринт | до 14.73 м/с (`4.5 × 1.5 × 1.455 × 1.5`) | до 5.46 м/с (`2.5 × 1.5 × 1.455`) | Бот не получает `×1.5` слота Speed; база ниже | `NanoSuit.cpp:1581-1588`; `PlayerMovement.cpp:1426-1428`; `PlayerMovement.cpp:1070-1071` (расчёт) |
| `pseudoSpeed` | 1.0 / 2.0 / 3.0 | 0.3 (Walk) / 0.5 (Run, Sprint на сервере) / 1.0 (Sprint на реплике) | У бота нет 2.0/3.0; анимация, не скорость | `Player.cpp:2474-2485`; `BotMovement.cpp:108`; `Player.cpp:5014` |
| Прыжок: высота/импульс | 1.0 м / v≈4.43 м/с | те же native-значения | Отличаются только ворота и проверки | `PlayerMovement.cpp:1164-1170`; `BotLocomotion.cpp:127` |
| Strength jump: энергия | ≥70 | ≥90 | Бот строже на 20 | `PlayerMovement.cpp:1138`; `BotLocomotion.cpp:105` |
| Прыжок: кулдаун | сразу после приземления (`m_jumped=false`); `jumpLock=0.2` не является воротами | 3.0 с от попытки | Бот ждёт 3 с; игрок может прыгнуть на следующем кадре после приземления | `Player.cpp:3529-3535,3790`; `PlayerMovement.cpp:1105,1268`; `BotLocomotion.cpp:111` |
| Suit mode: энергия | нет ворот, только `featureMask` | Strength 90; Speed 100/50; Cloak 120/60 | У игрока переключение бесплатное | `NanoSuit.cpp:776-777`; `BotSuit.cpp:22-24` |
| Воздух | `kAirControl=0.9`, `mp_strafeJump=1`, circle/wall jump | отдельного кода нет; тот же native-путь, но решение 10 Гц | Нет circle/wall/strafe-jump логики | `Actor.cpp:642-644`; `GameCVars.cpp:603-604,630`; `BotBrain.cpp:270`; `BotMovement.cpp:104-115` |

Ключевой нюанс: **2.5 м/с — это запрошенная базовая скорость, а не жёсткий предел итоговой
скорости**. `SetWarsBotMotion` клампит именно базу `m_warsBotSpeed` до 2.5
(`Player.cpp:4972`), а native `ACTION_SPRINT` затем умножает движение на `1.5`
(`PlayerMovement.cpp:1041-1045`). Поэтому обычный спринт бота — 3.75 м/с, а не 2.5.

## 2. Спринт

| Player path | Bot path | Difference | file:line |
| --- | --- | --- | --- |
| `OnActionSprint` ставит `ACTION_SPRINT` при `speedMultiplier × zoom > 0.99` | `BotBrain` выбирает `BotGait::Sprint`; `BotLocomotion` ставит `ACTION_SPRINT` через `SetWarsBotActions` | У игрока аналоговый ввод + отдельное действие; у бота дискретный gait | `PlayerInput.cpp:1367-1397`; `BotBrain.cpp:70-85`; `BotLocomotion.cpp:116-124` |
| Ввод идёт каждый кадр; `PreUpdate` собирает `CMovementRequest`, `SetPseudoSpeed`, `RequestMovement` | Решение о движении — 10 Гц; `BotLocomotion::Execute` вызывает `BotMovement::Move` и `CMovementRequest` | Бот обновляет направление/спринт реже | `PlayerInput.cpp:896-923`; `BotBrain.cpp:269-272`; `BotLocomotion.cpp:119-128` |
| База: полный стик = 4.5 м/с (`GetStanceMaxSpeed`) | База: Run/Sprint = 2.5 м/с; Walk = 1.3 м/с | База бота ниже; Walk — отдельный режим | `Player.cpp:2488-2491,2597`; `BotLocomotion.cpp:121` |
| Спринт: `move *= 1.5 × suitSprintMult` | То же native-умножение, потому что `ACTION_SPRINT` выставлен | Механика множителя общая; отличается база | `PlayerMovement.cpp:1041-1045`; `Player.cpp:4956` |
| Speed mode: `×1.3` без спринта и `×1.5` слота Speed при спринте | `×1.3`/`×1.5` не применяются: `AdjustMovementForEnvironment` только для `IsPlayer()` | Бот не получает Speed-mode бонусы ходьбы/слота | `PlayerMovement.cpp:1422-1433`; `PlayerMovement.cpp:1070-1071` |
| Speed mode ramp: `1 + 0.35 × min(1.3, t)` до 1.455 | Тот же ramp применяется в `GetSprintMultiplier` | Ramp общий; у бота нет слота Speed | `NanoSuit.cpp:1573-1588`; `PlayerMovement.cpp:1035` |
| Итог: 6.75 обычный; 14.73 пик Speed | Итог: 3.75 обычный; 5.46 пик Speed | Бот медленнее на 44% в обоих случаях | `PlayerMovement.cpp:1041-1058`; `NanoSuit.cpp:1581-1589` (расчёт) |
| `pseudoSpeed`: 1.0 без спринта, 2.0 спринт, 3.0 Speed >40 энергии | Сервер: 0.3/0.5; реплика: 1.0 при спринте | У бота нет 2.0/3.0 | `Player.cpp:2474-2485`; `BotMovement.cpp:108`; `Player.cpp:5014` |
| Спринт не требует отдельного порога энергии; только Speed-множитель требует `energy >= 1` | Спринт требует `energy > 45`; Speed mode — 100/50; TravelGait — 120/55; chase — 100/50 | У бота несколько энергетических ворот | `NanoSuit.cpp:1579`; `BotLocomotion.cpp:116-117`; `BotSuit.cpp:23`; `BotBrain.cpp:73-74,544` |
| Слив на спринте: 50/с (62.5/с в воде) | Тот же native-слив, если `bSprinting` и Speed mode | Слив общий; бот чаще прекращает спринт по порогу | `NanoSuit.cpp:1646-1648`; `PlayerMovement.cpp:144-145` |
| Остановка: отпускание ввода / `ACTION_SPRINT` снят | Остановка: gait не Sprint, `combat != None`, нет земли, suit не active, energy ≤45, jump/pending/airborne, firing | У бота много дополнительных условий | `PlayerInput.cpp:1384-1392`; `BotLocomotion.cpp:116-117`; `Player.cpp:4953` |
| Сеть: `input.sprint` → `ACTION_SPRINT` на сервере | Сеть: `m_warsBotSprint` → `ACTION_SPRINT` на реплике | Разные поля, одинаковая native-реакция | `PlayerInput.cpp:1028`; `NetPlayerInput.cpp:50-73`; `Player.cpp:5215,4998` |
| Спринт-чейс: игрок сам решает | `sprintChaseRange` 12–1000 м, `useSpeed`; Defender 1000 = никогда; Beginner `useSpeed=false` | У бота профильные ворота | `BotProfile.h:21,45,96-116,133`; `BotBrain.cpp:544-545` |

Дополнительно: `2.7` в `BotLocomotion.cpp:121` — это длина obstacle-probe при спринте, а не
скорость. Сама скорость остаётся 2.5.

## 3. Обычный прыжок

| Player path | Bot path | Difference | file:line |
| --- | --- | --- | --- |
| `OnActionJump` ставит `ACTION_JUMP`; `PreUpdate` вызывает `request.SetJump()` (кроме prone) | `BotBrain` выбирает `BotJump::Normal`; `BotLocomotion` вызывает `request.SetJump()` | У игрока ввод, у бота решение/проверка | `PlayerInput.cpp:1280-1317,868-871`; `BotLocomotion.cpp:100-113,127` |
| Контроллер: `state.SetJump()` → `params.jump` | То же: `RequestMovement` → `state.SetJump()` → `params.jump` | Путь общий | `PlayerMovementController.cpp:234-235,1256-1257`; `BotLocomotion.cpp:125-128` |
| Условия: `onGround > 0.2` (или dt), не prone, `!m_jumped` | Условия: `state.grounded`, не prone, `!m_pending && !m_airborne`, `now >= nextJump` | Бот добавляет pending/airborne и 3-секундный кулдаун | `PlayerMovement.cpp:1105,1128-1131`; `BotLocomotion.cpp:102-104`; `BotBlackboard.cpp:15` |
| Высота 1.0 м; `v = g·t`, `t = sqrt(2·g·h·mult)/g − inAir·0.5` | Те же native-формулы; бот не добавляет свой импульс | Высота/скорость одинаковые | `Player.cpp:2694`; `PlayerMovement.cpp:1164-1170`; `BotLocomotion.cpp:127` |
| Импульс: `m_request.velocity = desiredVel + jumpVec`; отдельного impulse нет | То же; `SetWarsBotMotion` не передаёт импульс | Импульс общий, у бота нет своей физики | `PlayerMovement.cpp:1392`; `Player.cpp:4966-4975` |
| Энергия: обычный прыжок бесплатный | Обычный прыжок бесплатный, но в Strength mode бот отказывает до Defense | Игрок в Strength с энергией <70 делает обычный прыжок; бот ждёт Defense | `PlayerMovement.cpp:1138-1155`; `BotLocomotion.cpp:106` |
| Проверка места: нет | `JumpSpace`: стоячий клиренс 2.5 м, приземление с перепадом ≤0.6 м, forward-ray, sphere r=0.35 | Бот не прыгает в потолок/с обрыв; игрок может | `BotLocomotion.cpp:64-93` |
| Стойка: `OnActionJump` пытается встать; native запрещает только prone | При прыжке `request.SetStance(STANCE_STAND)` | Бот принудительно stand в запросе | `PlayerInput.cpp:1282-1284`; `PlayerMovement.cpp:1130`; `BotLocomotion.cpp:126` |
| Сеть: `SvRequestJump` → сервер повторяет jump | Сеть: `PublishWarsBotJump` только после отрыва; реплика не вызывает `SetJump` | У бота нет клиентского импульса/предикта | `PlayerMovement.cpp:1246-1247`; `Player.cpp:7933-7968`; `Player.cpp:5000-5009` |
| Кулдаун: `!m_jumped`, сброс на приземлении; `jumpLock=0.2` только уменьшается и не запрещает прыжок (использование закомментировано) | `nextJump = now + 3.0` ставится до принятия native-запроса | Бот ждёт 3 с даже при отказе | `PlayerMovement.cpp:1105,1268`; `Player.cpp:3529-3535,3790`; `BotLocomotion.cpp:111` |

## 4. Strength jump

| Player path | Bot path | Difference | file:line |
| --- | --- | --- | --- |
| Режим Strength + `energy >= 70` → `mult=4.7` | Режим Strength + `energy >= 90` + suit active → `mult=4.7` | Порог бота 90 против 70 | `PlayerMovement.cpp:1138-1154`; `BotLocomotion.cpp:105` |
| Игрок может быть в Strength с любой энергией; при <70 прыжок будет обычным | Бот не войдёт в Strength при <90 (`strength_energy`), а в Strength не прыгнет обычно | У бота нет «обычного прыжка в Strength» | `NanoSuit.cpp:768-820`; `BotSuit.cpp:22`; `BotLocomotion.cpp:105-106` |
| Высота 4.7 м; v≈9.60 м/с; `m×v≈768` кг·м/с при массе 80 | Те же native-значения после прохождения ворот | Высота/импульс одинаковые | `PlayerMovement.cpp:1150-1154,1392`; `Actor.cpp:611,646` (расчёт) |
| Стоимость 70 энергии | Та же native-стоимость 70 | После прыжка 90→20, у игрока 70→0 | `PlayerMovement.cpp:1203-1216`; `BotLocomotion.cpp:105` |
| Нет проверки места | `JumpSpace`: клиренс 7.0 м, перепад ≤1.5 м, forward 3.0 м, far-probe 5.5 м с ground ≥ feet−2.5 | Бот проверяет площадку и обрыв | `BotLocomotion.cpp:70-82` |
| Триггер: игрок нажимает jump в Strength | Триггеры: TravelGait (энергия >130, 70% шанс), dodge после попадания (энергия ≥110, шанс профиля), obstacle jump, `wb_bot_jump strength` | У бота несколько эвристик | `BotBrain.cpp:79-84,511-518,638-647`; `BotLocomotion.cpp:54-63` |
| Кулдаун: `!m_jumped`; после приземления прыжок снова доступен (`jumpLock=0.2` не ворота) | `nextJump=now+3`; travel-jump дополнительно 5–10 с; dodge 4 с | У бота длиннее | `PlayerMovement.cpp:1105,1268`; `Player.cpp:3529-3535,3790`; `BotLocomotion.cpp:111`; `BotBrain.cpp:82,517` |
| Сеть: `ClJump`/`SvRequestJump` передают `strengthJump` | Бот не вызывает `StrengthJump()`; сила определяется режимом костюма; реплика только анимирует | Разный механизм передачи strength | `Player.cpp:7933-7968`; `PlayerMovementController.cpp:1256-1260`; `Player.cpp:5000-5009` |

## 5. Переключение режима костюма

| Player path | Bot path | Difference | file:line |
| --- | --- | --- | --- |
| `OnActionSuitMode` буферизует 0.3 с, до 4 нажатий | `BotBrain` выбирает `m_intent.suitMode`; `BotSuit::Apply` вызывает `SetMode` | У игрока ручной ввод, у бота решение | `PlayerInput.cpp:1251-1266,948-986`; `BotBrain.cpp:655`; `BotSuit.cpp:13-42` |
| `SetMode` проверяет только `m_active` и `featureMask` | Дополнительно: `canMove`, suit active, mode available, энергия, cloak-cvar | У бота больше ворот | `NanoSuit.cpp:768-777`; `BotSuit.cpp:19-24` |
| Нет энергетических ворот на переключение | Strength <90 → `strength_energy`; Speed <100 (вход) / <50 (удержание) → `speed_energy`; Cloak <120/60 → `cloak_energy` | Игрок может переключиться при 0 энергии; бот нет | `NanoSuit.cpp:776-777`; `BotSuit.cpp:22-24` |
| Cloak доступен по `featureMask` | Cloak дополнительно требует `wb_bot_cloak_experimental=1` | У бота экспериментальный cvar | `NanoSuit.cpp:819-822`; `BotSuit.cpp:8-12,20` |
| Нет cooldown на смену (только буфер 0.3 с) | `nextSuitChange = now + 2.0`, кроме Defense | Бот не может менять режим чаще 2 с | `PlayerInput.cpp:1259`; `BotSuit.cpp:36-39` |
| Переключение в воздухе/плавании не запрещено | `!canMove` запрещает все режимы, кроме Defense при падении; `canMove=false` в т.ч. при плавании | Бот не переключает Speed/Strength в воде/freefall | `PlayerInput.cpp:1251-1266`; `BotSuit.cpp:17-19`; `BotBlackboard.cpp:20-22` |
| Сеть: режим как `uint8`, получатель вызывает `SetMode` | То же; плюс optional-группа `WarsBotSuitActive` | Механизм общий | `NanoSuit.cpp:1347-1353`; `Player.cpp:5190-5198` |
| Влияние: Speed ×1.3/×1.5; Strength только прыжок | Speed ×1.455 ramp (без ×1.3/×1.5); Strength только прыжок | У бота меньше Speed-бонусов | `PlayerMovement.cpp:1422-1433`; `PlayerMovement.cpp:1070-1071`; `NanoSuit.cpp:1573-1588` |

## 6. Управление в воздухе

| Player path | Bot path | Difference | file:line |
| --- | --- | --- | --- |
| `kAirControl=0.9`, `kAirResistance=0.5` в `pe_player_dynamics` | Тот же `CPlayer`/physics; отдельного bot-параметра нет | Native-параметры общие | `Actor.cpp:642-644`; `BotMovement.cpp:104-115` |
| `LivingEntity` добавляет `vel += req × (8×dt)` при условии `vel·req < |req|²×0.9` | Тот же native-путь, если бот шлёт `CMovementRequest` | Механика общая | `LivingEntity.cpp:1734-1752`; `BotLocomotion.cpp:119-128` |
| Passive air control: `mp_strafeJump=1`; `PlayerMovement.cpp:1291-1315` | Тот же cvar/native-путь; бот просто продолжает слать `SetMoveTarget` | Нет отдельной bot-логики | `GameCVars.cpp:630`; `PlayerMovement.cpp:1291-1315`; `BotMovement.cpp:104-115` |
| Steering каждый кадр (input) | Steering 10 Гц (brain), `Move` вызывается на решении | В воздухе бот подруливает реже | `PlayerInput.cpp:923`; `BotBrain.cpp:269-272`; `BotLocomotion.cpp:119-121` |
| Circle jump: `mp_circleJump=0`; вертикальная часть вычитается | В `Code/CryGame/Bots` ссылок нет | У бота нет circle jump | `GameCVars.cpp:603`; `PlayerMovement.cpp:1363-1368`; not found in `Bots` |
| Wall jump: `mp_wallJump=1.0`, импульс `−eyeDirection × 600` | В `Code/CryGame/Bots` ссылок нет | У бота нет wall jump | `GameCVars.cpp:604`; `Fists.cpp:309-320,360-369,397-400`; not found in `Bots` |
| В воздухе можно менять suit mode вручную | `BotSuit` разрешает только Defense при `!canMove`; в обычном прыжке `canMove` обычно true | В freefall бот не переключит Speed/Strength | `PlayerInput.cpp:1251-1266`; `BotSuit.cpp:17-19`; `BotBlackboard.cpp:20-22` |
| Двойной прыжок запрещён `!m_jumped` | `!state.grounded || m_pending || m_airborne` → отказ | Оба не прыгают в воздухе; бот ещё и ждёт 3 с | `PlayerMovement.cpp:1105`; `BotLocomotion.cpp:102` |

## 7. Лимиты, отказы и таймеры

| Лимит/отказ | Когда | Эффект | file:line |
| --- | --- | --- | --- |
| `movement_state` | `state.canMove == false` | `Stop`, движение не выдаётся | `BotLocomotion.cpp:96`; `BotBlackboard.cpp:20-22` |
| `jump_not_grounded_or_pending` | нет земли, `m_pending` или `m_airborne` | Прыжок не выдаётся | `BotLocomotion.cpp:102` |
| `jump_prone` | stance PRONE | Прыжок не выдаётся | `BotLocomotion.cpp:103` |
| `jump_cooldown` | `now < state.nextJump` | Прыжок не выдаётся | `BotLocomotion.cpp:104`; `BotLocomotion.cpp:111` |
| `jump_strength_mode_or_energy` | не suitActive, не Strength или energy <90 | Strength jump не выдаётся | `BotLocomotion.cpp:105` |
| `normal_jump_waiting_for_defense` | обычный прыжок в Strength mode | Бот ждёт Defense | `BotLocomotion.cpp:106` |
| `jump_clearance_or_landing` | `JumpSpace` не нашёл безопасную площадку | Прыжок не выдаётся | `BotLocomotion.cpp:107`; `BotLocomotion.cpp:64-93` |
| `jump_not_accepted_by_native_movement` | `m_pending` >1 с без отрыва | Сброс pending, лог | `BotLocomotion.cpp:43` |
| `suit_inactive_or_actor_unavailable` | suit нет/не active или `!canMove` (кроме Defense при падении) | Режим не меняется | `BotSuit.cpp:19` |
| `cloak_experimental_disabled` | Cloak при выключенном cvar | Режим не меняется; Cloak→Defense | `BotSuit.cpp:20,27-31` |
| `mode_unavailable` | `!suit->IsModeActive(mode)` | Режим не меняется | `BotSuit.cpp:21` |
| `strength_energy` | Strength при energy <90 | Режим не меняется | `BotSuit.cpp:22` |
| `speed_energy` | Speed при energy <100 (вход) / <50 (удержание) | Режим не меняется | `BotSuit.cpp:23` |
| `cloak_energy` | Cloak при energy <120/60 | Режим не меняется | `BotSuit.cpp:24` |
| `state.blocked` | `intent.move && !accepted` | Бот считает шаг заблокированным; сюда же попадает прибытие <0.8 м | `BotLocomotion.cpp:123`; `BotMovement.cpp:73-77,103` |
| 2.5 м/с cap | `SetWarsBotMotion` | Клампит `m_warsBotSpeed` (базу), не native-скорость | `Player.cpp:4972` |
| `pseudoSpeed` 0.3/0.5 | `BotMovement::Move` | Анимация; не влияет на скорость | `BotMovement.cpp:108`; `PlayerMovementController.cpp:1298-1305` |
| `pseudoSpeed` 1.0 на реплике | Sprint на клиенте | Анимация реплики | `Player.cpp:5014` |
| Arrival 0.8 м | `direction.len² < 0.8²` | `Stop`+`Aim`, `Move` возвращает false | `BotMovement.cpp:73-77` |
| SafeStep drop ~3 м | Нет высокого препятствия, но есть земля вниз 3.6 м | Бот может шагнуть с уступа до ~3 м | `BotMovement.cpp:57-60` |
| Stuck 2 с / 0.09 м² | Смещение <0.3 м за 2 с | `m_avoidTime=1.2`, смена стороны | `BotMovement.cpp:82-91` |
| Jump cooldown 3 с | После попытки прыжка | `nextJump` | `BotLocomotion.cpp:111` |
| Suit cooldown 2 с | После смены режима | `nextSuitChange` | `BotSuit.cpp:39` |
| Travel-jump 5–10 с | Strength jump в TravelGait | `m_nextTravelJump` | `BotBrain.cpp:82` |
| Dodge 4 с | Strength jump после попадания | `m_nextDodge` | `BotBrain.cpp:517` |

## 8. Что это значит для step-2 (pace/sprint)

1. **Не менять только `pseudoSpeed`.** Он влияет на анимацию
   (`PlayerMovementController.cpp:1298-1305`), а не на скорость. Скорость бота задаётся
   `SetDesiredSpeed` в `BotMovement.cpp:107` и native-множителями в
   `PlayerMovement.cpp:1041-1058`.
2. **База 2.5 → 4.5.** Сейчас бот просит 2.5 (`BotLocomotion.cpp:121`). Если цель —
   паритет с игроком, база Run/Sprint должна стать 4.5 (`Player.cpp:2597`), иначе
   спринт останется 3.75 вместо 6.75.
3. **`SetWarsBotMotion` cap.** При подъёме базы выше 2.5 нужно поднять и кламп
   `Player.cpp:4972`, иначе реплика получит `m_warsBotSpeed=2.5` (`Player.cpp:5212,5013`)
   и будет двигаться/анимироваться медленнее сервера.
4. **Speed mode.** Игрок получает `×1.3` без спринта и `×1.5` слота Speed
   (`PlayerMovement.cpp:1432-1433,1426-1428`). Бот их не получает из-за
   `IsPlayer()==false` (`PlayerMovement.cpp:1070-1071`). Если нужен паритет, надо либо
   звать `AdjustMovementForEnvironment` для бота, либо закладывать множитель в запрос.
5. **`pseudoSpeed`.** Для паритета с игроком: 1.0 без спринта, 2.0 спринт, 3.0 Speed >40
   (`Player.cpp:2474-2485`). Сейчас серверный бот отдаёт 0.3/0.5
   (`BotMovement.cpp:108`), реплика — 1.0 при спринте (`Player.cpp:5014`).
6. **Энергия.** Игрок не имеет бинарных ворот на спринт; Speed-множитель требует
   `energy >= 1` (`NanoSuit.cpp:1579`). Бот: >45 в executor (`BotLocomotion.cpp:117`),
   100/50 в `BotSuit.cpp:23`, 120/55 в TravelGait (`BotBrain.cpp:73-74`), 100/50 в chase
   (`BotBrain.cpp:544`). Решить, какие пороги сохранить.
7. **Прыжок.** Если step-2 трогает и прыжок: Strength-порог 90 (`BotLocomotion.cpp:105`)
   против 70 (`PlayerMovement.cpp:1138`), кулдаун 3 с (`BotLocomotion.cpp:111`) против
   «сразу после приземления» (`m_jumped=false`; `jumpLock=0.2` не ворота —
   `Player.cpp:3529-3535,3790`; `PlayerMovement.cpp:1268`), и `JumpSpace`
   (`BotLocomotion.cpp:64-93`) против отсутствия проверок у игрока. Для чистого
   pace/sprint это можно не менять.

## 9. Not found / отсутствует

- Отдельный «аналоговый» walk/run у бота: только `BotGait {Walk, Run, Sprint}`
  (`BotIntent.h:10`), скорости 1.3/2.5 (`BotLocomotion.cpp:121`).
- Использование `CPlayer::CalculatePseudoSpeed` в `Code/CryGame/Bots`: не найдено;
  бот хардкодит 0.3/0.5 (`BotMovement.cpp:108`).
- Отдельный bot-air-control, circle jump, wall jump, strafe jump: в `Code/CryGame/Bots`
  не найдено; работает общий native-путь (`Actor.cpp:642-644`;
  `PlayerMovement.cpp:1291-1315`).
- Передача jump-импульса бота клиентам: не найдена; реплика только анимирует
  (`Player.cpp:5000-5009`).
- `StrengthJump()` для бота: не вызывается; сила определяется режимом костюма
  (`PlayerMovementController.cpp:1256-1260`; `PlayerMovement.cpp:1138-1154`).
- Speed-mode `×1.3`/`×1.5` для бота: не применяются, потому что
  `AdjustMovementForEnvironment` вызывается только для `IsPlayer()`
  (`PlayerMovement.cpp:1070-1071`).
