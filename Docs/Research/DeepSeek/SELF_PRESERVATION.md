# SELF_PRESERVATION — отход за укрытие + реген + повторный вход (спецификация)

Ревизор/автор: DeepSeek. Статус: **идея → Claude**. Покрывает то, что в плейтесте 2 названо «бросают
добивание» и «стоят на открытом», но с другой стороны: не «добить», а «выжить, когда проигрываешь».

## Проблема (что есть сегодня)

- `BotGoalArbiter.h` знает только `Roam/Hunt/Weapon/Fight`. Нет цели `Retreat`. Более того, прерывание
  `Damaged` (`ArbiterReason::Damaged`) толкает бота **в Fight** (отстреливаться), а не к отходу.
- `BotProfile.h` уже содержит `retreatHealth` (22/35/45 по скиллу) и `retreatEnter/retreatExit` — но
  `retreatHealth` нигде не управляет *целью* (это порог, а не решение).
- `BotTactics.h` имеет `PickCover` (статические cover-споты) и `BotAngleHold` — но это **наступательное**
  удержание угла после потери цели, и оно за `wb_bot_tactics` (OFF). `PickCover` можно переиспользовать для
  отхода, не переписывая.
- `BotCombat::FindEnemy` считает «свой урон» (`m_dmgDealt`, `m_retaliator`), но решения «я проигрываю» нет.

Итог: бот с 4 HP продолжает стоять и стрелять до смерти, вместо «клоак/укрытие → реген → вернуться».

## Что делаем

Новый engine-free модуль `BotSelfPreservation.h` + новая цель `ArbiterGoal::Retreat` в `BotGoalArbiter.h`.

### Правило входа (interrupt, выше `Damaged→Fight`)

Retreat срабатывает только когда бот **проигрывает дуэль**, а не просто ранен:

```
losing = health < retreatHealth            // BotProfile.retreatHealth по скиллу
      && damagedRecently                   // получил урон ~0.5 с
      && ( enemyVisible                     // противник передо мной
        || (m_retaliator близко && now<retaliatorUntil) )   // или только что стрелял в меня
      && health < enemyHealth*0.75f        // я ощутимо позади по HP (если враг виден)
      && !alreadyRetreating
```

Beginner уходит рано (`retreatHealth=22` уже так настроен), Elite почти никогда (honest aimbot добивает).
Никакого retreat при **выигрыше** дуэли или когда враг сам низкий.

### Что делает Retreat (порядок)

1. `PickCover` (`BotTactics.h`) — точка, чей `coverMask` закрывает сторону **в сторону угрозы**; до 12 м.
   Нет точки → фолбэк «спринт от угрозы + zigzag/jump-strafe» (не стой на месте).
2. Движение к точке (reuse CHASING-механики), при этом `BotSuitCombat` даёт Cloak (`CloakRetreat`) если энергия есть.
3. У точки — Armor (`Regen`) и **не выглядывать**, пока `health >= regenExit` или враг невидим ≥ 3 с.
4. Выход: регенился → back to `Fight`/`Hunt` (re-engage); враг умер/ушёл → `Hunt`/`Roam`; враг сам накрыл
   точку огнём → сменить точку (макс 2 раза, потом `Fight`).

### Интерфейс арбитра

```cpp
enum class ArbiterGoal { Roam=0, Hunt, Weapon, Fight, Retreat };
enum class ArbiterReason { /* + */ Retreating, Reengaged };
struct ArbiterInput { /* + */ float ownHealth=100; float enemyHealth=1e9f; };
```

`Retreat` в `Scores`: не конкурирует как обычная цель — это **interrupt** (как `EnemyClose`/`Damaged`),
проверяется до них, с приоритетом выше `Damaged→Fight`. Выход — через `RetreatDone` → обычный `Best(s)`.

### Псевдокод

```
if (m_goal == Retreat) {
    if (health >= regenExit || enemy not visible > 3s || enemy dead)
        return Switch(Best(s), Reengaged);          // re-engage
    if (cover threatened > 1.5s && retries < 2) { retries++; pick next cover; }
    return Hold;                                     // stay, regen
}
if (Losing(in)) return Switch(Retreat, Retreating);
// ... остальные interrupts/цели как сегодня
```

## Файлы

- `Code/CryGame/Bots/BotSelfPreservation.h` — новый engine-free модуль + `Tests/BotSelfPreservationTests.cpp`
  (состояния Retreat/Hold/Reengage, выбор cover по маске, лимит ретраев).
- `Code/CryGame/Bots/BotGoalArbiter.h` — цель `Retreat` + interrupt + reason (обратно совместимо).
- `Code/CryGame/Bots/BotBrain.cpp` — map `Retreat` → движение к cover-точке (reuse `Steer`/`CHASING`), заполнить
  `ownHealth/enemyHealth`, вызвать `BotSelfPreservation` перед suit policy.
- `Code/CryGame/Bots/BotConfig.h/.cpp` — cvar `wb_bot_self_preserve` (default **0**).
- `BotProfile.h` — `regenExit` (совместно с NANOSUIT_COMBAT.md), `retreatRetry=2`.

## Тесты (engine-free)

1. Retreat только при `losing` (low HP + damaged + враг близко + HP позади) — не при выигрыше дуэли.
2. Не retreat когда враг сам < моих HP.
3. Cover выбирается с маской в сторону угрозы; нет cover → фолбэк (не «стою на месте»).
4. Выход по регену / враг невидим 3 с; ретрай точки ≤ 2.
5. Elite почти не retreats (порог высокий), Beginner — часто (порог 22).
6. `wb_bot_self_preserve=0` → арбитр байт-в-байт как сегодня.

## Probes / приёмка

- `duel_30m`: дать боту проигрывать → лог `DECISION goal=retreat reason=retreating`, затем `BOT SUIT mode=Cloak`
  или движение к cover, рост HP, `goal=fight reason=reengaged`.
- Приёмка дефолта (owner session): бот реже умирает с 4 HP, но **не** трусливо убегает при выигрыше;
  сложности отличаются по готовности отступить.

## Exit-критерий флага

promote после owner-session + probe (retreat/reengage видны), delete `retreatEnter/retreatExit`-фолбэк если
Retreat покрывает его. ~1.5–2 дня Claude.

## Связь с DS-015 (finish-bonus)

Не конфликтует: DS-015 удерживает **цель** (добивание), SELF_PRESERVATION — уводит **себя** когда проигрываешь.
Вместе закрывают «бросают добивание» и «умирают в открытую».
