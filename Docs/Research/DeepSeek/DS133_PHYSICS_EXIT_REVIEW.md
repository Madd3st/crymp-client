# DS-133 — ревью `0851e685` (physics-exit для отсутствующего клипа) + аудит архива прогона 96

Задача: DS-133 (координатор, 2026-10-30). Кандидат
`0851e685dd1f8415bbb0933f825dda927c978512`, родитель `96e4a383`, ветка `codex/shore-ladder-exit`, PR #2.
Дополнительно: аудит архива диагностического прогона `QA/build/c154-route-96e4a383-v2`.

Уровень доказательств: чтение закоммиченного diff, SDK-заголовков, живого CI через GitHub API, разбор
архива и демо. Игра, сервер, локальная сборка, живые данные, код и quality не трогались.

## Вердикт

**BLOCK.** Кандидат **не компилируется**, поэтому полный CI красный и пары артефактов нет. Сам замысел
(физический выход при отсутствующем клипе) соответствует DS120 и выглядит безопасным по инвариантам, но
до исправления сборки и одного фильтра столкновений он не может быть допущен к прогону.

## 1. Блокер: compile error в предикате

`Player.cpp:7465` (085):

```cpp
bool CPlayer::NeedsWarsBotLadderPhysicsExit() const
{
    ...
    ... GetHealth()<=0 || IsFrozen() || m_stats.isRagDoll)
```

`CActor::IsFrozen()` объявлен **не const**: `Actor.h:966 bool IsFrozen();`, определение
`Actor.cpp:2065 bool CActor::IsFrozen()`. Вызов из const-метода — C2662 (нельзя преобразовать `this`).
Других const-перегрузок `IsFrozen` в SDK нет (проверено по `Code/CryCommon`, `Code/CryAction`, `Code/CryGame`).

Живой CI это подтверждает: run `41055226551` на 085 — job `124060957890` «Client + server (MSVC,
experimental spawner)» **failure** в шаге «Configure and build» (12:25:41→12:28:58), upload skipped,
**пары артефактов нет**; unit-job `124060958357` при этом **success**.

Почему unit-job не поймал: в фикстуре `Tests/test_bot_ladder_physics_exit_caller.py:71` мок объявлял
`bool IsFrozen()const{return frozen;}`, то есть **расходился с реальным классом** по const-квалификатору.
Мок, повторяющий реальную сигнатуру, поймал бы ошибку на этапе юнит-теста.

**Точный фикс:** убрать `const` у `NeedsWarsBotLadderPhysicsExit()` (все вызовы и так не const:
`PrePhysicsUpdate`, `ProcessWarsBotLadderPhysicsExit`, `SetWarsBotLadderExitPhysics`), а в моке сделать
`IsFrozen()` не-const, как в движке. В ветке это уже сделано коммитом `63eef004` (см. §6), но он не входит
в предмет этого ревью и требует собственного CI/пары.

## 2. Второй блокер: фильтр геометрии в swept clearance

В обоих вызовах `PrimitiveWorldIntersection` (строки 1665 и 1670) передаётся:

```cpp
..., mask, &contact, 0, geom_colltype_player, nullptr, nullptr, 0, skip, 1);
```

`geomFlagsAny = geom_colltype_player` (0x2). Это не совпадает ни с документированным значением по
умолчанию `geom_colltype0|geom_colltype_player` (`physinterface.h:2329`), ни с идиомой, принятой в этом
же коде: `PlayerView.cpp:470`, `PlayerView.cpp:489`, `BotLocomotion.cpp:97-99` передают
`(geom_colltype_player<<rwi_colltype_bit)|rwi_stop_at_pierceable`.

Последствия при любой трактовке плохие:

- как **geometry flags** (так параметр документирован: «flags at least one of which must be present in an
  entity part») 0x2 = только colltype1 (player) — статическая геометрия и террейн (colltype0) в выборку
  не попадают, и проверка «не влезаем в стену» становится пустой;
- как **ray flags** (так их использует существующий код) 0x2 = pierceability 2 и **пустая маска colltype**
  — тоже не то, что нужно.

Сейчас физическим бэкстопом остаётся сам character controller с `Pushable`-коллайдером, поэтому это не
телепорт, а ослабленный guard: бот может «упереться» вместо раннего отказа, а заявленный контроль
«overlap+sweep каждый шаг» фактически не проверяет стены.

**Точный фикс:** использовать выражение из существующих вызовов
`(geom_colltype_player<<rwi_colltype_bit)|rwi_stop_at_pierceable` (или документированное
`geom_colltype0|geom_colltype_player`), и добавить в фикстуру контроль самого флагового слова, а не только
факта вызова.

## 3. Что в кандидате сделано правильно (по DS120)

- `NeedsWarsBotLadderPhysicsExit` — только owned server-бот, `ladder==1`, `isOnLadder && isExitingLadder`,
  живой, не frozen, не ragdoll, анимационный сет есть и клип `relaxed_ladderOffTop_nw_01` **отсутствует**
  (`GetAnimIDByName<0`). Регистрированный клип оставляет нативный путь нетронутым.
- `PrePhysicsUpdate`: `AlwaysPhysics` включается **только** для этого missing-clip выхода; обычный выход
  остаётся `PerAnimGraph`.
- Кадр — нативный: `ladderTop/Bottom`, `-ladderOrientation`, ось, проверка `axis.z>=0.95`, полоса позиции.
- Коллайдер: реальные `pe_player_dimensions` (radius 0.1..0.75, высота 0.1..2), центр
  `feet+(heightCollider-heightPivot)`; для MP-стоек `heightPivot=0`, поэтому совпадает с движком.
- 5 проб пола по площади посадки (центр ± radius), плоский пол в пределах 0.12 м, полоса по z от нативного
  top; маска включает `ent_living` и skip собственного тела.
- Overlap-проверка (нулевой sweep → «number of hits», как документирует API) и sweep на каждом шаге;
  любой хит/отрицательная проба — отказ без движения.
- Скорость 1.7 м/с, `dt=min(frameTime,0.05)`, смещение ограничено `speed*dt`; на медленном кадре
  перемещение остаётся внутри проверенного интервала.
- Завершение — только `NativeLadderExitPhysicalReady` + поддержка под ногами + grounded + близость к
  посадке + предыдущий обычный gravity-запрос (settling). Никаких SetPos/импульсов/«успеха по таймеру».
- Бюджет провала 3 с не изменён; cancel/leave/death вызывают `RestoreWarsBotLadderGraph`, который
  освобождает собственный слой коллайдера (`SetWarsBotLadderExitPhysics(false,false)`), в том числе при
  замене графа. Посторонние/обычные актёры не затрагиваются.
- Новый actual-source тест подключён в CI; контроли: floors/ledge/slope/wall/overlap/missing dims/invalid dt/
  slow frame/registered clip/позиция без завершения/реальный пол+egress.

## 4. Аудит архива `c154-route-96e4a383-v2`

**PASS по целостности.** Прогон на `96e4a383` (receipt `f35944ed`, DS132 supplement) снова упал:
`15.64 blocked_top … pos=(2116.12,1880.62,97.91)`.

- `quality.json`: source 96, `review=f35944ed`, `review_ok=true`, `harness_sha256=87c3f5ee…`,
  `analyzer_sha256=2a642aeb…`, `manifest_guard_ok=true`; `review-checks.jsonl` — 2 записи `MATCH`.
- `runtime-result/live-evidence`: `FAIL`, стадии setup_revive/lower_grounded/ladder_start/native_attached/
  native_exiting, 62 trace-строки.
- Демо: `WBDM` v2, header build=96, map Shore, 20 Hz, 562 записи, разбор ровно до EOF (24 921 = размер
  файла), последний ts 15.279 с. Замечание: хвост демо заканчивается **за 0.36 с до** строки провала
  (15.64) — сам момент отказа есть только в логе.
- **Новые поля**: 99 строк `LADDER EXIT INPUTS`, все с `action=idle stance=relaxed item=nw exitClip=-1
  fifo=0 fifoClip=-1 fifoName=<none> fifoTime=-1.000 model=objects/characters/human/us/nanosuit/nanosuit_us_multiplayer.cdf`.
  То есть у инсталлированного US-нанокостюма клип выхода не зарегистрирован — это наблюдение, а не
  утверждение о паритете графа.
- MSIX namespace: физическое дерево Codex (`…\Packages\OpenAI.Codex_2p2nqsd0c76g0\LocalCache\Local\WarsBots`)
  совпадает с receipt: Memory 4/4, Navmesh 16/16, лишних нет. Откатов нет; оговорка DS131 в силе.

## 5. Живой CI / PR

| Объект | Состояние |
| --- | --- |
| run `41055226551` (085) | **failure**: unit success, client+server failure, пары нет |
| PR run `41055234922` | in_progress (PR #2) |
| ветка `codex/shore-ladder-exit` | уже на **`63eef004`**, не на 085 |

## 6. Что уже изменилось после 085 (вне этого ревью)

`63eef004` («Match SDK predicate signature and tighten collider ownership and support diagnostics»):
снят `const` у предиката (ровно блокер §1), мок `IsFrozen()` стал не-const, добавлены проверка
`bUseCapsule`, разделение `floorMask`/`mask`, диагностические логи отказов пола/overlap/sweep, проверка
режима коллайдера в условии завершения и два новых контроля (registered clip no-op, foreign collider
override). Фильтр `geom_colltype_player` из §2 **не** изменён — это остаётся открытым пунктом.

## 7. Условия перед прогоном

1. Исправить §1 и §2 (или обосновать флаги ссылкой на семантику движка).
2. Зелёный полный WarsBots CI на новом SHA + все задания PR-матрицы + точная пара артефактов.
3. Receipt на новом SHA и собственный `RUNTIME_PIN`; история провалов `307` и `96` сохраняется.
4. Один прогон whole-route; при провале — архив/демо/restore и независимый аудит.

## 8. Что не доказано и не заявляется

- Что физический выход сработает: нет ни сборки, ни прогона на этом коде.
- Причина, по которой граф не выбирает `relaxed_ladderOffTop_nw`; каузальные и MSIX-оговорки DS131 в силе.
- Отсутствие клипа у CDF — наблюдение из архива 96; вывод «регистрация клипа невозможна» не делается.
- Ремонт не одобрен как решение; это ревью кандидата.
