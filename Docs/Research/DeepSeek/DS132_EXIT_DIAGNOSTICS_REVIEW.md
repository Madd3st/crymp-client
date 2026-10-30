# DS-132 — предрантайм-ревью `8ed16c78`: read-only диагностика выхода с лестницы

Задача: DS-132 (координатор, 2026-10-30). Кандидат
`8ed16c780f12e3644f96f69e02aebbdf3ca9dd94`, родитель `cde7782301d304149025b1975ae6b13e6866745f`,
ветка `codex/shore-ladder-exit`, дерево `E:/WarsBots/Development/r4-weapon-search-episode`, PR #2
(`codex/shore-ladder-exit` → `codex/r4-weapon-search-episode`).

Уровень доказательств: чтение закоммиченного diff, SDK-заголовков, графа анимаций и живых данных CI
через GitHub API. Игра, сервер, локальная сборка, живые данные, код и quality не трогались.

## Вердикт

**BLOCK — не по коду, а по гейту CI.** Сама правка выглядит корректной и действительно read-only, но
полный CI на этом SHA **не зелёный**: unit-job упал, build-job отменён, пары артефактов нет. Запуск
диагностического прогона на таком SHA недопустим по собственному правилу задачи («after fullCI/fullPR
success and artifactpair»). Причина падения найдена и конкретна (см. §5).

## 1. Что именно изменено

`git diff cde77823 8ed16c78`: 2 файла, +32/−1.

- `Code/CryGame/Actors/Player/PlayerMovement.cpp` +28: один новый лог-блок
  `LADDER EXIT INPUTS` внутри уже существующего gated-логирования выхода.
- `Tests/test_bot_ladder_exit_retry_caller.py` +5/−1: расширены mock-границы
  (`CAnimation`, `IAnimationSet`, `ISkeletonAnim`, методы `ICharacterInstance`).

Ни движения, ни записи в граф, ни загрузок анимаций, ни новых cvar/identity/defaults/completion —
в diff нет.

## 2. Что читает диагностика

Внутри `if (m_stats.isExitingLadder)` → `if (gEnv->bClient || warsBot)` →
`#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)` → `if (warsBot)`:

- `Action`, `Stance`, `Item` — через `GetInputId` + `GetInputName` + сверку имени (`strcmp`), затем `GetInput`;
- `exitClip` = `GetAnimIDByName("relaxed_ladderOffTop_nw_01")` на инсталлированном анимационном сете;
- `fifo` = `GetNumAnimsInFIFO(0)`, `fifoClip` = `m_LMG0.m_nAnimID[0]` верхней записи, `fifoTime` = `m_fAnimTime`,
  `fifoName` = `GetNameByAnimID(fifoClip)`;
- `model` = `GetFilePath()`.

Все API существуют в SDK на этом SHA: `ICharacterInstance::GetIAnimationSet()` (ICryAnimation.h:351),
`IAnimationSet::GetAnimIDByName` (:848) и `GetNameByAnimID` (:859), `ISkeletonAnim::GetNumAnimsInFIFO` /
`GetAnimFromFIFO` (:589-590), `ICharacterInstance::GetFilePath()` (:466), `SLocoGroup::m_nAnimID[]`
(CryCharAnimationParams.h:731), `CAnimation::m_fAnimTime` (:806).

Имя клипа **реальное**: `Pak/Animations/graphs/PlayerFullBody.xml:5058` — `animation="relaxed_ladderOffTop_nw_01"`
в состоянии `relaxed_ladderOffTop_nw` (`allowSelect="1"`, `includeInGame="1"`), у состояния
`<SelectWhen><Signal value="exit_ladder_top"/></SelectWhen>` (:5056) и `<Output name="OnLadder" value="1"/>`
(:5059); входящий линк из `relaxed_ladderClimbNoZ_nw` есть (:49536). Это полезный статический ориентир для
будущего ремонта, но не доказательство причины.

## 3. Read-only — проверено

- В добавленных строках нет ни одного вызова записи: проверка по `SetInput|ClearInput|SetPos|SetWorldPos|
  Impulse|Play(|AddAnimation|Load|SetAnimation|SetActorItem|Reset|Teleport|SetPseudoSpeed` даёт пусто.
- Блок не может выполняться вне owned-gated серверного выхода: он вложен в `if (warsBot)` под
  экспериментальным макросом, который в OFF не определён (та же охрана, что и у соседнего лога).
- Стоимость: несколько чтений графа и FIFO на кадр в фазе выхода (≈3 с). Приемлемо для диагностики;
  объём лога в фазе выхода примерно удвоится.
- Мелкое замечание: `character->GetFilePath()` не проверяется на null перед `%s`; в движке путь модели
  ненулевой, риск минимальный.

## 4. Совместимость с C154 V5 / harness / analyzer

Код наследует 307-лестницу, R4-фикс и OFF-фикс: `BotBrain.cpp:748` (`m_weaponSearchStarted = m_now`),
охрана `ScriptBind_GameRules.cpp:18`, `TryWarsBotLadderExitRetry` на месте (`Player.cpp:7463`,
вызов `PlayerMovement.cpp:1689`). Diff 307→8ed — ровно PR1+R4+OFF+диагностика, ничего лишнего.

Данные и инструменты не менялись, поэтому совместимы по хешам DS130:

| Объект | Значение |
| --- | --- |
| V5 navmesh / links / spots / manifest | `b60178d7…` / `0230a0c3…` / `d95e1e11…` / `a95a0503…` |
| harness | `87c3f5ee…` (коммит `e973b905`) |
| analyzer | `2a642aeb…` |
| manifest guard receipt | `9a958ea7…` |

Важно для ожиданий: analyzer по-прежнему требует полный маршрут, поэтому диагностический прогон
**обязан** закончиться `FAIL blocked_top`, если причина не устранена. Это не регрессия: harness
заархивирует `runtime-result/live-evidence/логи/демо` и в `finally` восстановит данные. Ценность прогона —
новые поля `LADDER EXIT INPUTS`, а не PASS.

## 5. Почему CI красный (конкретно)

Живые данные GitHub на момент ревью:

| Объект | Состояние |
| --- | --- |
| run `41051941203` | `workflow_dispatch`, head `8ed16c78`, attempt 1 |
| job `124049912841` Unit tests (MSVC) | **failure**, 11:58:54→11:59:39 |
| job `124049912556` Client + server | **cancelled** в шаге «Configure and build», 12:02:47 |
| artifact pair | нет (upload skipped) |
| artifact `unit-logs-…8ed16c78` | `12974244887`, 14 986 B, digest `sha256:a9cb7611…`, до 2026-11-06 |
| PR #2 matrix (build_32/64, debug) | ещё in_progress |

Лог job'а анонимно недоступен (403), локальная сборка запрещена, поэтому точную строку падения я не
воспроизводил. Но причина видна по исходникам и она одна: **второй actual-caller тест тоже извлекает
этот же блок выхода и не был обновлён**.

`Tests/test_bot_ladder_caller.py`:
- строка 364: `exit_block = block(move, 'if (m_stats.isExitingLadder)')` — тот же блок, что и в
  exit-retry caller;
- строка 85: `struct ICharacterInstance { int GetFlags(){return 0;} };` — **нет** `GetIAnimationSet`,
  `GetISkeletonAnim`, `GetFilePath`;
- строки 270/279/280 исполняют этот блок с `isExitingLadder=true`.

Новый код обращается к `character->GetIAnimationSet()` и т.д.; даже при `character==nullptr` эти члены
должны существовать на этапе компиляции. Значит `test_bot_ladder_caller.py` не компилируется — отсюда
падение unit-job. Это ровно тот класс ошибки, который уже ловили в PR1 (тогда не обновили второй тест
для `SendTextMessage`), и он снова повторился.

Проверить точную строку можно артефактом `unit-logs-…` (id `12974244887`): там `BotLadderCaller-run.log`
или `BotLadderCaller-compile.log`. Это и есть действие для Codex, а не догадка.

Дополнительно, не блокирующе: в обновлённом exit-retry фикстуре `GetInputId` возвращает `2u` для
`Action/Stance/Item`, а `GetInputName(2u)` — `"PseudoSpeed"`, поэтому проверка имени в новом коде
отклоняет эти входы, и `GetInput` для них не вызывается. Компиляция и семантика от этого не страдают,
но тест **не** покрывает фактическое чтение новых входов; это стоит знать, чтобы не считать фикстуру
доказательством чтения Action/Stance/Item.

## 6. Что нужно до запуска (чек-лист)

1. Обновить `Tests/test_bot_ladder_caller.py` так же, как exit-retry caller: добавить в mock
   `ICharacterInstance` методы `GetIAnimationSet/GetISkeletonAnim/GetFilePath` и типы
   `CAnimation/IAnimationSet/ISkeletonAnim`. Других тестов, извлекающих этот блок, нет.
2. Дождаться зелёного полного CI на новом SHA и полной PR-матрицы; получить точную пару артефактов.
3. Выпустить quality-receipt на этом SHA (harness/analyzer/data-хеши не меняются) и выставить
   кандидату **собственный** `RUNTIME_PIN` в C-154; историю провала 307 сохранить.
4. Только после этого — один диагностический прогон; ожидаемый результат `FAIL blocked_top` с новыми
   полями, без объявления фикса.

Сейчас `RUNTIME_PIN` не менял: кандидат заблокирован до зелёного CI.

## 7. Что не доказано и не заявляется

- Причина, по которой граф не выбирает `relaxed_ladderOffTop_nw`, не доказана; каузальные оговорки
  DS131 в силе, включая namespace-оговорку MSIX.
- Что диагностика покажет `exitClip>=0`, непустой FIFO0 или конкретные `Action/Stance` — неизвестно.
- Физический ремонт выхода не предложен, не одобрен и не начат; этот отчёт — только про диагностику.
