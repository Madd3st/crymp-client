# NANOSUIT_COMBAT — нанокостюм в бою (спецификация)

Ревизор/автор: DeepSeek. Статус: **идея → Claude**. Это то, чего нет ни в одном чужом бот-опыте
(UT/Q3/CS/BF/Killzone): у них нет нанокостюма. У нас это главный «это Crysis, а не UT»-маркер.

## Проблема (что есть сегодня)

`BotSuitPolicy.h` уже умеет: Armor при падении/под огнём, Strength для jump-линков, Speed для тревела,
Cloak для **скрытого подхода к невидимому врагу** (`StealthApproach`, 12–35 м). Но это всё — режимы
передвижения и выживания, а не боя:

- Cloak заперт за `wb_bot_cloak_experimental` + `stealthStyle` (Flanker/Hunter) + `enemyKnownHidden`
  (`BotSuitPolicy.h: StealthOk`). Боевого клоака нет вообще.
- `SuitInput` не знает про «проигрываю дуэль», «зашёл в тупик», «нужно сменить угол».
- `SuitReason` не содержит `CloakRetreat` / `CloakFlank` / `Regen`.
- Armor-реген работает пассивно (Armor под огнём), но нет решения «отойти за укрытие и регениться» — это
  отдельная спека `SELF_PRESERVATION.md`, а не костюм.

Результат в плейтесте 2: боты дерутся как «ут3-боты в Armor» — стоят на месте, не используют клоак ни для
отхода, ни для фланга. Сложности неотличимы в том числе из-за этого.

## Что делаем

Новый engine-free модуль `BotSuitCombat.h` + расширение `SuitInput`/`SuitReason` в `BotSuitPolicy.h`.
Он **переопределяет** тревел-политику только когда `inCombat == true` (иначе поведение ровно сегодняшнее).

### Четыре боевых решения (по приоритету)

| # | Решение | Условие | Режим / Reason |
| --- | --- | --- | --- |
| 1 | **Cloak-retreat** | `health < retreatHealth` (`BotProfile.h: retreatHealth` уже есть: 0/22/35/45) **и** враг виден/стреляет **и** есть укрытие в ≤ 12 м **и** energy ≥ cloakEnter | Cloak / `CloakRetreat` |
| 2 | **Cloak-flank** | дуэль-пат (враг за укрытием, оба живы, дистанция 6–35 м) **и** style Flanker/Hunter **и** energy ≥ cloakEnter | Cloak / `CloakFlank` |
| 3 | **Regen-hold** | не получал урона ≥ 2 с **и** health < regenExit **и** враг не виден | Armor / `Regen` (держит Armor, запрещает выглядывать) |
| 4 | Armor под огнём | `damagedRecently` (как сейчас) | Armor / `UnderFire` (без изменений) |

Правило энергии: Cloak только если `energy >= cloakEnter (120)` и держится пока `energy > cloakExit (40)` —
иначе бот застрянет без энергии посреди поля. `cloakEnter/exit` уже в `SuitParams`.

### Расширение интерфейса

```cpp
// SuitInput (добавить)
bool inCombat = false;      // есть текущая цель/стрельба
bool wantRetreat = false;   // self-preservation уже выбрала отход (см. SELF_PRESERVATION.md)
bool duelStalemate = false; // враг за укрытием, оба живы, 6..35 м
bool coverAvailable = false;// PickCover вернул точку в радиусе
float health = 100;         // уже есть поле health
// SuitReason (добавить)
CloakRetreat, CloakFlank, Regen
```

Порядок в `Update`: `CloakRetreat/CloakFlank` идут **до** `Fight`(Armor), но **после** `Falling`/`StrengthJump`
(они urgent и важнее). `Regen` — не urgent (мин. hold уже есть `minHold`).

### Псевдокод ядра

```
if (!in.inCombat) { /* сегодняшняя логика без изменений */ }
else if (in.airborne && in.verticalSpeed < fallArmor) want = Armor/Falling;        // падение важнее
else if (in.wantStrengthJump && energy>=70) want = Strength;                        // линк важнее
else if (in.wantRetreat && in.coverAvailable && energy>=cloakEnter) want = Cloak/CloakRetreat;
else if (in.duelStalemate && (Flanker||Hunter) && energy>=cloakEnter) want = Cloak/CloakFlank;
else if (in.damagedRecently) want = Armor/UnderFire;
else if (in.health < regenExit && !in.enemyVisible) want = Armor/Regen;
else if (in.enemyVisible || in.firing) want = Armor/Fight;
```

## Файлы

- `Code/CryGame/Bots/BotSuitCombat.h` — новый engine-free модуль + `Tests/BotSuitCombatTests.cpp`.
- `Code/CryGame/Bots/BotSuitPolicy.h` — расширить `SuitInput`/`SuitReason` (обратно совместимо).
- `Code/CryGame/Bots/BotBrain.cpp` — заполнить `inCombat/wantRetreat/duelStalemate/coverAvailable` перед
  вызовом suit policy (рядом с текущим вызовом `BotSuit::Apply`, `BotBrain.cpp:1265-1268`).
- `Code/CryGame/Bots/BotConfig.h/.cpp` — cvar `wb_bot_suit_combat` (default **0**).
- `BotProfile.h` — `regenExit` (напр. `retreatHealth + 30`), читается в `Regen`.

## Тесты (engine-free)

1. Cloak-retreat только когда low HP **и** cover **и** energy — иначе Armor/UnderFire.
2. Никакого Cloak когда `firing` (клоак срывается выстрелом).
3. Cloak удерживается до `cloakExit`, не раньше.
4. Regen-hold только когда враг невидим ≥ 2 с и health < regenExit.
5. `inCombat=false` → поведение байт-в-байт как сегодня (обратная совместимость).

## Probes / приёмка

- `duel_30m` (есть в `PROBES_V1.md`): заставить бота потерять HP → в логе `BOT SUIT mode=Cloak reason=cloak_retreat`,
  затем Armor и рост HP (реген) за укрытием.
- Приёмка дефолта (owner session): бот с клоаком уходит от дуэли/флангует **не чаще** N раз/мин (не читерски),
  сложности отличаются по готовности регениться (Beginner уходит раньше).

## Exit-критерий флага

promote после owner-session + probe (Cloak-retreat/regen видны), delete старый `wb_bot_cloak_experimental`-путь
`StealthApproach` после подтверждения, что боевой клоак покрывает его сценарии. ~1–1.5 дня Claude.
