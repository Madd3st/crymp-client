# Движение пешего игрока в Crysis Wars (CryMP)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params` от `warsbots/feature/weapon-goals` (`fb578d52`). Только чтение кода: игра и сервер не запускались. Значения — для multiplayer (MP), если не указано иное. Единицы мира — метры, время — секунды (`jumpHeight` помечен как meters: `Pak/CryMP/Scripts/Entities/Actor/BasicActor.lua:863`). Пометка «расчёт» означает арифметический вывод из формулы в указанных строках.

## 1. Откуда берутся параметры

| Что | Значение / поведение | Источник | Комментарий |
| --- | --- | --- | --- |
| Включение MP-параметров | `gEnv->bMultiplayer && IsPlayerClass()` | `Code/CryGame/Actors/Actor.cpp:454-457` | Для MP-игрока Lua-стойки не используются |
| Применение MP-набора | `CPlayer::Revive` → `SetParamsMP(true)` | `Code/CryGame/Actors/Player/Player.cpp:3883-3888` | Полный сброс `m_params` при каждом респавне |
| MP-набор | стойки, `jumpHeight=1.0`, `sprintMultiplier=1.5`, `speedMultiplier=1.0`, `inertia=10` | `Code/CryGame/Actors/Player/Player.cpp:2581-2724` | Основной источник чисел для MP |
| Lua-дубль | те же стойки и множители | `Pak/CryMP/Scripts/Entities/Actor/Player.lua:75-159`; `BasicActor.lua:855,863` | Совпадает со stand-стойкой C++; актуален для SP/legacy |
| `normalSpeed` | не ограничивает скорость живого игрока | `Code/CryGame/Actors/Player/PlayerMovementController.cpp:1462-1486` | `minSpeed/maxSpeed/normalSpeed` в movement state принудительно `-1` |
| `speedMultiplier` | `1.0` | `Code/CryGame/Actors/Player/Player.cpp:2701` | В MP читается из скрипта (`Player.cpp:2511-2513`), но в `Player.lua` не задан |
| Zoom-множитель | `1.0` | `Code/CryGame/Actors/Actor.cpp:251,973-976` | Входит в `GetStanceMaxSpeed` (`Player.cpp:2488-2491`) |

## 2. Стойки MP

`GetStanceMaxSpeed = maxSpeed × speedMultiplier × zoomMultiplier` — `Code/CryGame/Actors/Player/Player.cpp:2488-2491`.

| Стойка | maxSpeed, м/с | normalSpeed, м/с | Коллайдер: height / size (x,y,z) | Источник |
| --- | --- | --- | --- | --- |
| stand («combat») | 4.5 | 1.75 | 1.2 / 0.4, 0.4, 0.3 | `Player.cpp:2591,2597,2600-2601` |
| crouch | 1.5 | 1.0 | 0.9 / 0.4, 0.4, 0.1 | `Player.cpp:2610,2616,2619-2620` |
| prone | 0.75 | 0.375 | 0.4 / 0.35, 0.35, 0.001 | `Player.cpp:2629,2635,2638-2639` |
| swim | 2.5 | 1.0 | 1.0 / 0.4, 0.4, 0.35 | `Player.cpp:2648,2654,2657-2658` |
| zerog | 3.5 | 1.75 | 0.0 / 0.6, 0.6, 0.001 | `Player.cpp:2667,2673,2676-2677` |

Те же числа в Lua: stand `Player.lua:81-82`, crouch `98-99`, prone `111-112`, swim `125-126`, zerog `139-140`.

## 3. Скорости

Формула: `v = min(1, |ввод|) × GetStanceMaxSpeed × [1.5 × suitSprintMult при спринте] × [множитель Speed mode]`. Ввод игрока нормируется до 1 — `PlayerMovement.cpp:1022-1026`; масштаб стойки — `PlayerMovement.cpp:1054-1058`.

| Режим | Расчёт | м/с | Источник |
| --- | --- | --- | --- |
| Полный стик, без спринта | `4.5 × 1.0` | 4.5 | `PlayerMovement.cpp:1054-1058`; `Player.cpp:2597` |
| Назад | `4.5 × 0.7` | 3.15 | `Player.cpp:2687`; `PlayerMovement.cpp:1002-1010` |
| Спринт, обычный режим | `4.5 × 1.5` | 6.75 | `PlayerMovement.cpp:1041-1045`; `Player.cpp:2702` |
| Speed mode, без спринта | `4.5 × 1.3` | 5.85 | `PlayerMovement.cpp:1432-1433` |
| Speed mode, спринт прямо, полная энергия | `4.5 × 1.5 × 1.455 × 1.5` | 14.73 (пик) | `NanoSuit.cpp:1581-1589`; `PlayerMovement.cpp:1424-1429`; `NanoSuit.cpp:789` |
| Speed mode, спринт со стрейфом | `4.5 × 1.5 × 1.0 × 1.5` | 10.13 (пик) | `NanoSuit.cpp:1585-1586`; `PlayerMovement.cpp:1035,1426-1428` |
| Crouch / prone / swim / zerog | maxSpeed стойки | 1.5 / 0.75 / 2.5 / 3.5 | `Player.cpp:2616,2635,2654,2673` |

`SuitSprintMult` в Speed mode: `1 + 0.35 × min(1.3, секунды_спринта)` → от 1.0 до 1.455 за 1.3 с — `Code/CryGame/Actors/Player/NanoSuit.cpp:1581-1583`; cvar `g_suitSpeedMultMultiplayer=0.35` — `Code/CryGame/GameCVars.cpp:248`. Любой боковой ввод (`|desiredVelocity.x| > 0.01`) сбрасывает множитель в `1.0` — `PlayerMovement.cpp:1035`; `NanoSuit.cpp:1585-1586`.

Дополнительный множитель Speed mode в MP: `1 + 0.005 × real(Speed slot)` — `PlayerMovement.cpp:1426-1428`; `real = energy × desired/200` — `NanoSuit.cpp:628-630,1060-1065`; `desired=100` в Speed mode — `NanoSuit.cpp:789`. Итог: `1 + 0.0025 × energy` (1.5 при 200, 1.25 при 100, 1.1 при 40). Поэтому 14.73 м/с — кратковременный пик на полной энергии; при 100 энергии — 12.28, при 40 — 10.80 (расчёт).

## 4. Спринт и pseudoSpeed

| Параметр / условие | Значение | Источник | Комментарий |
| --- | --- | --- | --- |
| Включение спринта | `ACTION_SPRINT`, если `speedMultiplier × zoom > 0.99` | `Code/CryGame/Actors/Player/PlayerInput.cpp:1378-1382` | В MP множители равны 1.0, спринт разрешён |
| Запрет спринта | `bIgnoreSprinting` | `PlayerMovement.cpp:1043,1048` | Сбрасывается при нажатии спринта (`PlayerInput.cpp:1381`) |
| Ветка MP | `move *= 1.5 × suitSprintMult` | `PlayerMovement.cpp:1041-1045` | SP-ветка отдельная (`1048-1049`) |
| `g_walkMultiplier` | `1`, только `IsClient() && !bMultiplayer` | `PlayerMovement.cpp:1251-1252`; `GameCVars.cpp:245` | На MP не влияет |
| `bSprinting` (статистика) | onGround > 0.1 ИЛИ вода; `inMovement > 0.1`; `ACTION_SPRINT` | `PlayerMovement.cpp:144-147` | Используется для звука, слива энергии, pseudoSpeed |
| PseudoSpeed: нет спринта | 1.0 | `Player.cpp:2480-2481` | — |
| PseudoSpeed: спринт без Speed mode | 2.0 | `Player.cpp:2482-2483` | — |
| PseudoSpeed: Speed mode, энергия ≤ 20% (≤40) | 2.0 | `Player.cpp:2478,2482-2483` | Порог `0.2 × NANOSUIT_ENERGY` |
| PseudoSpeed: Speed mode, энергия > 40 | 3.0 | `Player.cpp:2484-2485` | — |
| Отправка pseudoSpeed | `request.SetPseudoSpeed(...)` | `PlayerInput.cpp:896-910`; `NetPlayerInput.cpp:50-55` | Влияет на анимацию, не на скорость: `PlayerMovementController.cpp:1298-1305` |
| Отличие клиент/сервер | клиент берёт `m_stats.bSprinting`, сервер — `m_curInput.sprint` | `PlayerInput.cpp:899`; `NetPlayerInput.cpp:53`; `PlayerMovement.cpp:144-147` | В первые ~0.1 с спринта клиент может ещё отдавать 1.0 |

## 5. Прыжок

Условия: `onGround > 0.2 с`, не prone, `!m_jumped || strengthJump` — `Code/CryGame/Actors/Player/PlayerMovement.cpp:1105,1128-1131`. Вертикальная скорость: `t = sqrt(2·g·jumpHeight·mult)/g − inAir·0.5`, `v = g·t` — `PlayerMovement.cpp:1164-1170`.

| Параметр | Значение | Источник | Комментарий |
| --- | --- | --- | --- |
| `jumpHeight` | 1.0 м | `Player.cpp:2694`; `BasicActor.lua:863` | MP-значение |
| Обычный прыжок | `mult=1`, v≈4.43 м/с, высота 1.0 м | `PlayerMovement.cpp:1134,1164-1170` | При g=9.81 и `inAir=0` (расчёт) |
| Импульс прыжка | отдельный impulse не задаётся: в физику уходит скорость `jumpVec`; `m×v ≈ 354` кг·м/с | `PlayerMovement.cpp:1163-1170,1392`; `Actor.cpp:611` | Расчёт при массе 80; тип запроса `eCMT_JumpAccumulate` |
| Strength jump | `mult=4.7`, v≈9.60 м/с, высота 4.7 м | `PlayerMovement.cpp:1150-1154` | Только при энергии ≥70 (или remote-client ветка, `1138`) |
| Strength jump, импульс | `m×v ≈ 768` кг·м/с | `PlayerMovement.cpp:1150-1154,1392`; `Actor.cpp:611` | Расчёт при массе 80 |
| Стоимость Strength jump | 70 энергии | `PlayerMovement.cpp:1201-1216` | Требует `energy >= 70`, иначе обычный прыжок |
| Прыжок в Speed mode при спринте | −10 энергии | `PlayerMovement.cpp:1229-1235` | Дополнительно к обычному прыжку |
| Направление | подмешивается к нормали земли | `PlayerMovement.cpp:1172-1177` | На склоне прыжок отклоняется от вертикали |
| Горизонтальная скорость | не увеличивается | `PlayerMovement.cpp:1182` | Из `move` убирается вертикальная часть |
| Гравитация | 9.81 м/с² вниз | `Actor.cpp:645`; `BasicActor.lua:730`; `LivingEntity.cpp:86` | — |
| ZeroG-прыжок | импульсы 10 / 15 / 25 (× mass) по view-оси | `PlayerMovement.cpp:1141-1149` | Не высота; зависит от режима костюма |
| Ограничение скорости прыжка | 22 м/с | `PlayerMovement.cpp:1393-1394` | Только при `dt_enable` и `inAir > 0.3` |
| Урон от падения | безопасно 8.0 м/с; смертельно 13.7 м/с в armor mode | `GameCVars.cpp:487-488` | Safe — для всех режимов |
| Сеть | клиент → `SvRequestJump`, сервер повторяет jump | `PlayerMovement.cpp:1246-1247`; `Player.cpp:7949-7968` | Strength-флаг: `PlayerMovementController.h:46-48`; `Player.cpp:7933-7946` |

Диапазон Strength jump по горизонтали в коде не задан: он определяется горизонтальной скоростью на момент прыжка и временем полёта; отдельного параметра нет.

## 6. Энергия костюма

Максимум: `NANOSUIT_ENERGY = 200` — `Code/CryGame/Actors/Player/NanoSuit.h:22`.

| Параметр | Значение | Источник | Комментарий |
| --- | --- | --- | --- |
| Восстановление в MP | 200 за 20 с → 10/с | `GameCVars.cpp:261`; `NanoSuit.cpp:442-443,458,527-531` | Начисляется каждый кадр |
| Пауза после попадания | 3.0 с | `NanoSuit.cpp:694-699` | Только MP |
| Пауза при пересечении 20% | 3.0 с | `NanoSuit.cpp:682-683` | При любом расходе, если `g_mpSpeedRechargeDelay=1` (`GameCVars.cpp:253`) |
| Пауза восстановления | блокирует начисление | `NanoSuit.cpp:527` | Пока `m_energyRechargeDelay > 0` |
| Слив на спринте | 50/с; 62.5/с в воде | `NanoSuit.cpp:1646-1648`; `GameCVars.cpp:251` | `×1.25`, если `ShouldSwim()` |
| Слив при энергии ≤ 20% | 28/с | `NanoSuit.cpp:1650-1662` | Вместо 50/с |
| Баланс на спринте > 20% | −40/с (расчёт) | `NanoSuit.cpp:458,1647` | `10 − 50` |
| Баланс в первые 3 с паузы | −28/с (расчёт) | `NanoSuit.cpp:527,1662` | `0 − 28` |
| Баланс после паузы при ≤20% | −18/с (расчёт) | `NanoSuit.cpp:458,1662` | `10 − 28` |
| Strength jump | −70 | `PlayerMovement.cpp:1215` | Требует ≥70 |
| Speed sprint jump | −10 | `PlayerMovement.cpp:1233` | — |
| Speed mode без спринта | слива нет | `NanoSuit.cpp:1611-1649` | Слив только по `m_currentMode == SPEED && bSprinting` |
| Порог pseudoSpeed | 20% = 40 | `Player.cpp:2478` | — |
| Порог sprint-множителя | `energy >= 1.0` | `NanoSuit.cpp:1579` | В MP не 20% |

## 7. Режимы костюма

Переключение: обработчик `suitmode` — `Code/CryGame/Actors/Player/PlayerInput.cpp:67`; буфер 0.3 с и до 4 нажатий — `PlayerInput.cpp:1251-1266`; применение: 2 = Speed, 3 = Strength, 4 = Cloak, иначе Defense — `PlayerInput.cpp:948-986`.

| Режим | armor / strength / speed / medical | Источник | Комментарий |
| --- | --- | --- | --- |
| Speed | 25 / 50 / 100 / 25 | `NanoSuit.cpp:788-789,722-733`; `PlayerMovement.cpp:1426-1428` | Speed slot 100 → +50% к спринту при полной энергии |
| Strength | 50 / 100 / 25 / 25 | `NanoSuit.cpp:805-806` | Прыжок ×4.7 |
| Defense | 75 / 25 / 25 / 75 | `NanoSuit.cpp:812-813` | — |
| Cloak | 50 / 50 / 50 / 50 | `NanoSuit.cpp:819-820` | — |

`SetMode` проверяет `featureMask` (не все режимы разрешены) — `NanoSuit.cpp:776-777`; смена режима отключает cloak — `NanoSuit.cpp:792,809,816`; отмечает сетевой аспект — `NanoSuit.cpp:882-883`. Аспект `ASPECT_NANO_SUIT_SETTING` совпадает с `INPUT_ASPECT` — `Player.h:371`; на другой стороне режим читается как `uint8` и вызывает `SetMode` — `NanoSuit.cpp:1347-1353`.

Влияние на движение: Speed mode даёт `×1.3` без спринта — `PlayerMovement.cpp:1432-1433`, и ramp-множитель при спринте — `NanoSuit.cpp:1573-1605`; Strength mode меняет только прыжок — `PlayerMovement.cpp:1150-1154`; обычная скорость в Strength/Defense/Cloak остаётся 4.5 / 6.75.

## 8. Воздух, стрейф, circle jump, wall jump

| Параметр | Значение | Источник | Комментарий |
| --- | --- | --- | --- |
| `kAirControl` | 0.9 | `Code/CryGame/Actors/Actor.cpp:642` | MP-физика игрока |
| `kAirResistance` | 0.5 | `Actor.cpp:644` | — |
| `kInertia` | 8 (default движка) | `Code/CryPhysics/LivingEntity.cpp:152` | В MP не переопределяется: `Actor.cpp:641` |
| Inertia анимационного персонажа | 10 / 11 | `Player.cpp:2692-2693,4344` | Сглаживание desired velocity |
| `timeImpulseRecover` | 1.0 | `Actor.cpp:653` | — |
| Управление в воздухе | `vel += req × (8 × dt)`, если `vel·req < |req|² × 0.9` | `LivingEntity.cpp:1734-1752` | Прибавка `8×dt×req` за шаг (не постоянное ускорение) |
| Passive air control (`mp_strafeJump`) | 0.3 / 0.5 / 1.5 | `PlayerMovement.cpp:1291-1315`; `GameCVars.cpp:630` | Торможение/подруливание в прыжке |
| `mp_circleJump` | 0 (default) | `GameCVars.cpp:603` | — |
| Circle jump | из desiredVel вычитается `(1 − mp_circleJump) × вертикальная часть` | `PlayerMovement.cpp:1363-1368` | Отдельного импульса/бонуса нет |
| `mp_wallJump` | 1.0 (default) | `GameCVars.cpp:604` | — |
| Wall jump, импульс | `−eyeDirection × 600`, умножается на `mp_wallJump`; `iApplyTime=1` | `Fists.cpp:360-369,397-400`; `Actor.cpp:611` | Δv зависит от массы 80 и физики; `dt` входит только в оценку кэпа |
| Wall jump, условие | анимация running/jumping, оружие ещё не поднято | `Fists.cpp:309-320` | Триггер — forward-raycast `UpdateWeaponRaising` (`Weapon.cpp:3078-3148`) |
| Wall jump, анимация Speed mode | скорость 1.75 | `Fists.cpp:326-335` | — |
| Ограничение при `mp_wallJump > 1` | скорость капается значением cvar | `Fists.cpp:374-392` | При 1.0 кэп не применяется |
| `maxVelPlayers` (физика) | 150 | `Code/CryPhysics/PhysicalWorld.cpp:218` | Защитный предел, не игровой баланс |
| `mp_netSerializeMaxSpeed` | 9.0 | `GameCVars.cpp:622` | Только масштаб сетевой сериализации (`PlayerInput.cpp:1012`) |
| Мелкая вода | `×0.6` | `GameCVars.cpp:200,202-203`; `PlayerMovement.cpp:1272-1288` | Плавно от 0.3 до 1.0 м глубины |

## 9. Склоны, ступени, уступы

| Параметр | Значение | Источник | Комментарий |
| --- | --- | --- | --- |
| `minSlideAngle` | 45° | `Actor.cpp:647`; `BasicActor.lua:739` | Переводится в `cos(45°)` — `LivingEntity.cpp:614-617` |
| `maxClimbAngle` | 50° | `Actor.cpp:648`; `BasicActor.lua:740` | `cos(50°)` — `LivingEntity.cpp:618-621` |
| `minFallAngle` | 50° | `Actor.cpp:650`; `BasicActor.lua:741` | `cos(50°)` — `LivingEntity.cpp:626-629` |
| Default физики | `cos(36°) / cos(54°) / cos(70.2°)` | `LivingEntity.cpp:99-101` | Перекрываются MP-значениями выше |
| Игровое замедление на склоне | 10°…50°, кубическая интерполяция | `PlayerMovement.cpp:1320-1356` | До 90° модификации нормали |
| Prone на склоне | `slopeSlowdown=3.0` | `Player.cpp:2700`; `PlayerMovement.cpp:1074-1093` | — |
| Высота ступени игрока | **не найдено** | — | Нет константы/cvar; только navmesh `max_step=.75` (`Code/CryGame/Bots/BotNav.cpp:337`, к игроку не относится) |
| Геометрия шага | цилиндр/капсула + sweep, snap по контактам | `LivingEntity.cpp:2337-2355,2989-3006` | Порог `(m_hCyl − m_size.z)×1.01` зависит от коллайдера |
| Коллайдер stand | height 1.2, size z 0.3, capsule | `Player.cpp:2591,2601-2602` | Physicalize: `Actor.cpp:628-634` |
| `maxUnproj` | 0 (`heightPivot`) | `Actor.cpp:632`; `Player.cpp:2592` | — |
| `maxVelGround` | 200 | `Actor.cpp:651-652` | Стоять на быстрых платформах/транспорте |
| Ledge climb / mantle | **не найдено** | — | Отдельного кода нет; лимит — `maxClimbAngle=50°`; лестницы отдельно (`PlayerMovement.cpp:1699`; `Player.cpp:7294-7295`) |

## 10. Путь действий

### Спринт

1. `OnActionSprint` ставит `ACTION_SPRINT` — `PlayerInput.cpp:1367-1397`.
2. `PreUpdate` собирает `CMovementRequest`: отфильтрованный ввод (`FilterMovement`, `pl_inputAccel=30` — `PlayerInput.cpp:664-694`; `GameCVars.cpp:207`), pseudoSpeed, `RequestMovement` — `PlayerInput.cpp:863,896-910,923-924`.
3. Клиент сериализует `SSerializedPlayerInput.sprint` — `PlayerInput.cpp:1028`; сервер в `CNetPlayerInput::PreUpdate` пересобирает запрос и ставит `ACTION_SPRINT` — `NetPlayerInput.cpp:50-73`.
4. `CPlayerMovementController::RequestMovement` сохраняет delta movement / pseudoSpeed — `PlayerMovementController.cpp:242-248`.
5. `ProcessOnGroundOrJumping` применяет `×1.5×suitMult`, затем масштаб стойки и Speed-mode множители — `PlayerMovement.cpp:1041-1058,1420-1434`.

### Прыжок

1. `OnActionJump` ставит `ACTION_JUMP` и делает suit-tap — `PlayerInput.cpp:1280-1317`.
2. `PreUpdate` вызывает `request.SetJump()` (кроме prone) — `PlayerInput.cpp:868-873`.
3. Контроллер выставляет `params.jump` и `params.strengthJump` — `PlayerMovementController.cpp:1256-1260`.
4. `ProcessOnGroundOrJumping` проверяет ground/prone, считает `jumpVec`, списывает энергию — `PlayerMovement.cpp:1128-1235`.
5. Клиент шлёт `SvRequestJump`; сервер применяет `StrengthJump` и повторяет jump — `PlayerMovement.cpp:1246-1247`; `Player.cpp:7933-7968`.

### Режим костюма

1. `OnActionSuitMode` буферизует нажатие (0.3 с, до 4) — `PlayerInput.cpp:1251-1266`.
2. `Update` по таймеру вызывает `SetMode` (2/3/4/Defense) — `PlayerInput.cpp:948-986`.
3. `SetMode` меняет слоты/эффекты и помечает `ASPECT_NANO_SUIT_SETTING` — `NanoSuit.cpp:768-907`.
4. Режим уходит `uint8` в сеть; получатель вызывает `SetMode` — `NanoSuit.cpp:1347-1353`.

## 11. Не найдено

- Отдельная «скорость ходьбы»: в MP нет дискретной ходьбы, только аналоговый ввод 0…maxSpeed; `normalSpeed` для движения игрока не используется (`PlayerMovementController.cpp:1462-1486`).
- Точная горизонтальная дальность Strength jump: параметра нет, зависит от горизонтальной скорости и времени полёта.
- Точная высота ступени игрока: явной константы/cvar нет; `max_step=.75` в `Code/CryGame/Bots/BotNav.cpp:337` — только navmesh.
- Ledge climb / mantle: отдельного кода/импульса нет; есть только `maxClimbAngle=50°` (`Actor.cpp:648`) и лестницы.
- Исходные `Game/`-скрипты и `Player.xml`: в репозитории отсутствуют; MP-числа заданы в `SetParamsMP`.
- Точное Δv wall jump в м/с: в коде только импульс `600`, масштабируемый `mp_wallJump`; результат зависит от массы/физики.
- Бонус скорости от circle jump: отдельного множителя нет; cvar только убирает вертикальную составляющую desired velocity.
- Слив энергии Speed mode при обычной ходьбе: не найден; слив включается только при `bSprinting`.
