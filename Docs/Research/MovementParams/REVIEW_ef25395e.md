# Ревью Codex `ef25395e` — wb_bot_pace (против C-013 / C-018)

Читатель: OpenCode (read-only). База `fb578d52`; ветка `warsbots/feature/weapon-goals`.
Вердикт: **соответствует C-013 по всем 5 пунктам, OFF byte-identical; принять (5 замечаний, ни одно не
блокер, 1 пункт — подтвердить на pace-QA).**

## Соответствие C-013 (и моего PATCH_BOT_PACE.md)

| Пункт C-013 | Статус | Где |
| --- | --- | --- |
| 1. база 4.5 вместо клампа 2.5 | ✅ | `Player.cpp:4972` `pace ? GetStanceMaxSpeed(STANCE_STAND) : 2.5`; `BotLocomotion.cpp` `pace ? GetStanceMaxSpeed(stance) : 2.5` |
| 2. спринт 40/20, на ходу не стреляя | ✅ | `BotPacePolicy.h` `PaceSprintEnergy`; `BotLocomotion.cpp` eligible/sprint; `m_sprinting` в `BotLocomotion.h` |
| 3. Armor default, Speed 150/60 на ≥30 м | ✅ | `BotPacePolicy.h` `PaceSpeedBurst`; `BotBrain.cpp TravelGait` `suitMode = speedTravel ? SPEED : DEFENSE` |
| 4. множители Speed-mode через bot-путь (не `IsPlayer()`) | ✅ | `PlayerMovement.cpp:1072-1078` `else if (IsWarsBotActor() && pace)` — отдельная ветка, человеческая `IsPlayer()` не тронута |
| 5. Strength 70, кулдаун 0.5, pseudoSpeed 1/2/3 | ✅ | `BotLocomotion.cpp:105,111,49`; `BotSuit.cpp` strength 70; `BotMovement.cpp:108` + `Player.cpp:5014` `CalculatePseudoSpeed` |
| OFF byte-identical | ✅ | каждый хунок гейтится `BotPaceEnabled()`; при OFF — в точности старые ветки/пороги |

Отличия от моего скетча (в лучшую сторону): Codex взял `GetStanceMaxSpeed(...)` вместо хардкода `4.5`
(точнее), и нативный слот-Speed множитель `1 + slot*0.005` вместо моей упрощённой `1.5`
(`PlayerMovement.cpp:1077` = нативная формула `physinterface`-аналога `PlayerMovement.cpp:1426-1428`).

## Замечания (bugs/risks, file:line)

1. **`GetStanceMaxSpeed` может превысить 4.5** — `Player.cpp:2488-2491`
   `GetStanceMaxSpeed = GetStanceInfo(stance)->maxSpeed * m_params.speedMultiplier * GetZoomSpeedMultiplier()`.
   Кламп `SetWarsBotMotion` теперь динамический; если у бота когда-нибудь `speedMultiplier != 1`
   (снаряжение/зум), потолок уедет выше 4.5. Для ботов маловероятно, но это уход от «жёсткого» 4.5 из
   C-013. Низкий риск.
2. **`BotPaceEnabled()` — строковый `GetCVar` в горячих путях** — вызывается в `SetWarsBotMotion`
   (каждый кадр движения), `PlayerMovement` (on-ground), `BotLocomotion::Execute` (10 Гц), `BotSuit::Apply`.
   Это известный R3-антипаттерн (`C-010`). Совместимо с текущим стилем (`BotBrain.cpp:287` `wb_qa_socom_only`),
   но на 15 ботах — десятки лишних `GetCVar` в кадр. Средний риск производительности, не корректности.
3. **Sprint в pace: `combat != Fire` вместо `combat == None`** — `BotLocomotion.cpp` eligible-гейт.
   Мой скетч предлагал `combat == None`; Codex разрешает спринт при `Aim` (не стреляя). Это ближе к букве
   C-013 («not firing»), но означает спринт при активном прицеливании — проверить на pace-QA, что боты не
   «бегут, целясь» там, где должны идти шагом. Осознанное отклонение, флаг на QA.
4. **1-тик рассинхрон pseudoSpeed сервер/реплика при заблокированном ходе** — `BotLocomotion.cpp`
   `pseudo = CalculatePseudoSpeed(sprint)` вычисляется **до** `Move`/`accepted`, а `SetWarsBotActions
   (sprint && accepted, ...)` ставит `m_warsBotSprint = sprint && accepted`. Если `sprint=true` но
   `accepted=false` (упёрся), сервер шлёт pseudo 2/3, а реплика (`Player.cpp:5014`) по `m_warsBotSprint=false`
   — pseudo 1. Косметика (pseudoSpeed = анимация), не скорость. Низкий риск.
5. **Запутанная двойная `m_suit.Apply` в `ExecuteIntent`** — `BotBrain.cpp:659-668`: при `!suitReady` и
   `gait==Sprint` в pace повторно `m_suit.Apply(DEFENSE)`. DEFENSE обходит гистерезис `nextSuitChange`
   (`BotSuit.cpp:37`), поэтому отработает, но логика «сначала suitMode, потом DEFENSE» неочевидна; при
   будущем рефакторе (C-010 R1) легко сломать. Средний риск сопровождения.

## Тесты

`Tests/BotPacePolicyTests.cpp` покрывает только чистые `PaceSprintEnergy`/`PaceSpeedBurst` (пороги
40/20/150/60/30/8 корректны по assert'ам). Интеграция (`TravelGait`, `Execute`, `PlayerMovement`
Speed-ветка, сервер/реплика pseudo) тестами не покрыта — подтверждается pace-QA (`steelmill_triage.py pace`,
C-013 acceptance).

## Вердикт

Принять. C-013 реализован целиком, OFF byte-identical, replica-pseudoSpeed согласован с базой 4.5 через
`CalculatePseudoSpeed`. Перед default-ON — закрыть замечание 3 на pace-QA (спринт-при-прицеле) и при
возможности 2 (горячий `GetCVar`).
