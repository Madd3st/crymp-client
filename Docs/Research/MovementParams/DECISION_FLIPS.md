# WarsBots: осцилляция решений CHASING_ENEMY ↔ MOVING_TO_WEAPON

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`
(`warsbots/feature/weapon-goals`). Только чтение кода: игра и серверы не запускались.
Разбор FSM в `Code/CryGame/Bots/BotBrain.cpp`, `BotCombat.cpp`, `BotWeapons.cpp`.

Контекст: по roadmap step 5 зафиксировано 4579 «A→B→A» переключений между
`CHASING_ENEMY` и `MOVING_TO_WEAPON` за 48 минут (~1.6/с). Ниже — все условия входа/выхода
для обоих состояний и причина, по которой пара может переключаться каждый тик.

## 0. Как устроен цикл решений

`BotBrain::Update` принимает решение раз в 100 мс (`m_tick >= 0.1f`, `BotBrain.cpp:269-272`),
каждый тик заново, без «commitment»-таймера. `Change()` (`BotBrain.cpp:38-45`) не имеет
минимального времени пребывания в состоянии: если `m_state` уже равен целевому — no-op,
иначе мгновенно переключает и сбрасывает прогресс. Порядок веток в `Update`:

1. мёртв → `DEAD`/`RESPAWNING` (`:244-268`);
2. нет пригодного оружия `!SelectAvailable` → блок поиска оружия, может дать
   `MOVING_TO_WEAPON` (`:294-329`);
3. `FindEnemy` → враг (`:331-332`);
4. нет primary и враг скрыт/потерян → блок апгрейда оружия, даёт `MOVING_TO_WEAPON`
   (`:334-376`);
5. врага нет → `SEARCHING_ENEMY` (`:377-429`);
6. враг есть → `ATTACKING` (близко и виден) или `CHASING_ENEMY` (`:470-550`).

`m_combat.Visible()` пересчитывается каждый тик (raycast в `Observe`, вызывается в
`FindEnemy` на `:139-140`), а «память» скрытого врага живёт 3.0 с
(`BotCombat.cpp:142`). Поэтому видимость врага может меняться от тика к тику.

## 1. Вход в MOVING_TO_WEAPON

| Переход | Условие | Порог / таймер | file:line | Может осциллировать? |
| --- | --- | --- | --- | --- |
| (любое) → MOVING_TO_WEAPON | нет пригодного оружия (`!SelectAvailable`) и существует цель-оружие, поиск начат <6 с назад | повторный поиск 2.0 с; окно 6.0 с | `BotBrain.cpp:294,297-302,309-311`; `BotWeapons.cpp:57-75` | Нет — в этой ветке `FindEnemy` не вызывается, с `CHASING_ENEMY` не конкурирует |
| CHASING_ENEMY / SEARCHING_ENEMY → MOVING_TO_WEAPON | враг скрыт или потерян (`!enemy \|\| !m_combat.Visible()`) и нет primary (`!HasPrimary`) и есть цель-апгрейд | поиск апгрейда 3.0 с; отказ от цели 10.0 с | `BotBrain.cpp:334,336-343,345-349,357-360`; `BotWeapons.cpp:45-55` | **Да** — по `!Visible()`, которое меняется каждый тик |

## 2. Выход из MOVING_TO_WEAPON

| Переход | Условие | Порог / таймер | file:line | Может осциллировать? |
| --- | --- | --- | --- | --- |
| MOVING_TO_WEAPON → SEARCHING_ENEMY | `PickUp` успешен | — | `BotBrain.cpp:312-316`; `BotWeapons.cpp:124-138` | Нет (одноразово, оружие получено) |
| MOVING_TO_WEAPON → SEARCHING_ENEMY | fallback `GiveTestSCAR` | окно 6.0 с истекло | `BotBrain.cpp:320-325`; `BotWeapons.cpp:140-159` | Нет |
| MOVING_TO_WEAPON → SEEKING_WEAPON | цель очищена (нет item / недоступна / окно истекло) | — | `BotBrain.cpp:303-307,326` | Нет |
| MOVING_TO_WEAPON → CHASING_ENEMY | следующий тик: апгрейд подобран/сброшен, враг есть и не «близко-видим» | — | `BotBrain.cpp:367-370,520-522` | **Да** (пара к п.1, см. §4) |

## 3. Вход в CHASING_ENEMY

| Переход | Условие | Порог / таймер | file:line | Может осциллировать? |
| --- | --- | --- | --- | --- |
| SEARCHING_ENEMY / MOVING_TO_WEAPON / ATTACKING → CHASING_ENEMY | враг есть и НЕ(видим && distance <= range) | range = `engageRange` 18 м (или `holdRange` 22 м, если был `ATTACKING`) | `BotBrain.cpp:520-522`; `BotProfile.h:16-17` | **Да** — по `Visible()`, пара к п.1 |

## 4. Выход из CHASING_ENEMY

| Переход | Условие | Порог / таймер | file:line | Может осциллировать? |
| --- | --- | --- | --- | --- |
| CHASING_ENEMY → ATTACKING | видим && distance <= range | `engageRange`/`holdRange` — есть гистерезис | `BotBrain.cpp:470,509`; `BotProfile.h:16-17` | Нет (engage < hold) |
| CHASING_ENEMY → MOVING_TO_WEAPON | см. §1 (нет primary, враг скрыт) | 3.0 / 10.0 с | `BotBrain.cpp:334,360` | **Да** |
| CHASING_ENEMY → SEARCHING_ENEMY | враг исчез (`!enemy`: память >3 с или смерть) | память 3.0 с | `BotBrain.cpp:379`; `BotCombat.cpp:142,180-181` | Нет (не с MOVING_TO_WEAPON) |
| CHASING_ENEMY → SEARCHING_ENEMY | погоня брошена: blocked && !visible && `> blockedChasePatience` | `blockedChasePatience` 2.0 с (по умолч.; 1.2–4.0 по скиллу) | `BotBrain.cpp:533-540`; `BotProfile.h:44,132` | Нет |

## 5. Почему пара осциллирует (главный механизм)

Единственная циклическая пара — **§1 (вход в MOVING_TO_WEAPON по апгрейду) ↔ §3 (вход в
CHASING_ENEMY)**. Она завязана на одно и то же мгновенное условие `m_combat.Visible()`:

- враг виден, но дальше `engageRange` (18 м) → `CHASING_ENEMY` (`BotBrain.cpp:520-522`);
- враг скрыт за укрытием → `!Visible()` → блок апгрейда → `MOVING_TO_WEAPON`
  (`BotBrain.cpp:334,357-360`).

Дополнительное условие — у бота **нет primary** (`!HasPrimary`, `BotWeapons.cpp:45-55`),
то есть в инвентаре только пистолет `SOCOM` или винтовка без патронов. Пока это так,
и пока `m_upgradeTarget` установлен (а он не сбрасывается до подбора/таймаута 10 с),
любая смена видимости врага мгновенно перекидывает бота между двумя целями:

```
враг виден (далеко)          -> CHASING_ENEMY
враг скрыт (<3 с памяти)     -> MOVING_TO_WEAPON   (если нет primary)
враг снова виден (далеко)    -> CHASING_ENEMY
... каждый тик, 10 Гц
```

Что НЕ мешает осцилляции:
- `Change()` без min-dwell (`BotBrain.cpp:38-45`);
- проверка апгрейда каждый тик, а не по дебаунсу (`BotBrain.cpp:334`);
- гистерезис есть только у пары `ATTACKING ↔ CHASING_ENEMY` (`engageRange` 18 <
  `holdRange` 22, `BotProfile.h:16-17`) и у retreat (`retreatEnter` 6 / `retreatExit` 9,
  `BotProfile.h:19-20`) — но **не** у пары `CHASING_ENEMY ↔ MOVING_TO_WEAPON`.

Частичные демпферы, которые есть, но не ломают цикл: троттлинг поиска апгрейда 3.0 с
(`BotBrain.cpp:342`) и отказ от цели 10.0 с (`:345`) — они ограничивают только *выбор
новой* цели оружия, а не повторный вход в уже выбранный `MOVING_TO_WEAPON`.

## 6. Минимальные правки (только предложения, кода не трогаю)

1. **Гистерезис на «враг потерян».** Вход в блок апгрейда гейтить не `!Visible()`, а
   «враг не виден дольше X» (например, `now - m_lastSeen > 1.0 c`). Короткое мерцание за
   укрытием перестанет запускать забег за оружием. Это самый прямой фикс.
   (`BotCombat.cpp:117` уже обновляет `m_lastSeen` только когда видно.)

2. **Commitment-окно (min-dwell).** Не покидать `CHASING_ENEMY` ради апгрейда раньше,
   чем бот пробыл в нём ~0.75–1.0 с (`m_stateTime`), либо проверять апгрейд не чаще
   1–2 с отдельным таймером.

3. **Гистерезис на «вернуться в погоню».** Однажды войдя в `MOVING_TO_WEAPON`, не
   возвращаться в `CHASING_ENEMY` от одного лишь мимолётного появления врага; разрешать
   бросить забег за оружием только если враг виден **и** в пределах `engageRange`
   (реально вступаем в бой), а не «виден, но далеко».

4. **Дебаунс видимости.** В `Observe`/`FindEnemy` делать переход true→false для
   *целевых* решений только после N подряд скрытых сэмплов (≥0.2 с); стрельбу это не
   должно затрагивать (существующая логика reacquire в `BotCombat.cpp:100-106` уже
   разделяет эти сигналы).

5. **Одна активная цель.** Если `m_upgradeTarget` выбран и бот уже двинулся к нему
   (>X с), довести забег до конца (подбор/таймаут), не прерывая его «далёким видимым»
   врагом.

## 7. Not found / отсутствует

- Отдельный таймер «минимального времени в состоянии» — не найден (`Change()` сразу
  переключает, `BotBrain.cpp:38-45`).
- Гистерезис/дебаунс на вход в `MOVING_TO_WEAPON` по апгрейду — не найден; условие
  `!Visible()` проверяется голым каждый тик (`BotBrain.cpp:334`).
- Публичный геттер для `m_lastSeen` в `BotCombat` — не найден (`BotCombat.h`); поле
  приватное и используется только в `FindEnemy` (`BotCombat.cpp:142`).
