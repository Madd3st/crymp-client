# DS-130 — свежее предрантайм-ревью C154 на точном `307037bc`

Задача: DS-130 (координатор, 2026-10-30). Владелец закончил C157 Steelmill и явно попросил
продолжить Shore. Нужно заново, а не перепином DS126, проверить точный набор C154 перед одним
приёмочным прогоном.

Источник: `307037bc08050d39f26310dc6cc7071dddddf4a6` (`warsbots/feature/claude-sprint`),
CI `40919291234`, артефакт `12917623035`, данные V5, harness и analyzer из QA-каталога
`C:/Users/Magic8/WarsBotsBuild/crymp-multibot`.

Уровень доказательств: пересчёт хешей на диске, чтение harness/analyzer/CMake и живая проверка
CI через GitHub API. Игра, сервер, клиент, проба, установка и живые данные не запускались;
quality/live-файлы не редактировались.

## Вердикт

**DS130 `307037bc` + CI `40919291234` + V5 + harness/analyzer: OK.**

Код, пара, артефакт, данные, манифест-гард и инструменты совпадают с заявленными побайтно и
семантически. Физический результат по-прежнему **UNTESTED**. Ревью DS126 устарело: с тех пор
голова моей ветки ушла на `4fecbb8b` (DS127-129), поэтому текущий `c154-quality.json` больше не
проходит собственный freshness-gate harness. После этого OK Codex должен выпустить **новый**
receipt с текущей головой ревью; перепин старого качества недопустим.

## 1. Код и пара

| Объект | Значение | Как проверено |
| --- | --- | --- |
| Код | `307037bc08050d39f26310dc6cc7071dddddf4a6` | `ls-remote` ветки и HEAD `QA/build/ds076-code` совпадают, рабочее дерево чистое |
| CI | `40919291234` `completed/success`, `event=push`, `run_attempt=1`, head `307…` | живой GitHub API, 30.10 |
| Артефакт | `12917623035`, 14 185 253 B, digest `sha256:2003a25a…49553a`, `expired=false`, до 2026-11-05 | живой API + `restored-ci-artifact.json` |
| Клиент | `d4a08fbd3216088a21125127a4cc87d4a4ab4d0f7347c98ce259abe2a35d7fe8` | пересчитано на диске, совпало с `sha256.txt`, quality и provenance |
| Сервер | `bdd084af320a7bd5892f521a895742c1f0c2b07421ac468e1dff21d110303226` | то же |
| Джобы | unit `123607844493` success, client+server `123607844840` success | живой API + `restored-ci-jobs.json` |

`ci-provenance.json` честно помечен как восстановленный (`warsbots-recovered-ci-provenance/1`):
поля качества восстановлены из закоммиченного `C154_307_READY_HELD.json`, «original byte
serialization not asserted». Восстановленные и живые значения совпали.

## 2. Данные V5 и манифест-гард

`build/c154-route-data-v5` (пересчитано):

| Файл | SHA256 |
| --- | --- |
| `Shore.navmesh` | `b60178d7090fd8403b4ba0f4407d6a722c0057afd559a0024148ef0645930609` |
| `Shore.links.txt` | `0230a0c3e59bb818888684012291ee39efaddb542917ac0bc5e02c8e13d32dcb` |
| `Shore.spots.txt` | `d95e1e11fa06194e44e1b1d3887fb00334d7f0ccc9528c9a1b439d7ba78b548b` |
| `Shore.manifest.json` | `a95a0503813e394161bf961f6556e26db672b46041a03079baac99d8191b13e2` |

Манифест канонический LF (CRLF = 0). `build/c154-307-manifest/v5/result.json`
(`9a958ea78289d0c7ccd38aa9e1f9ee7931eb5d25f0feae1e3a208041657660d8` — совпал с
`manifest_guard_receipt_sha256` в quality): `source=307…`, `actual_source_sha256=21df800a…`,
`actual_exit=0`, `tamper_and_crlf_controls=PASS`, `game_linked=false`. Я пересчитал
`BotNavMesh.cpp` из рабочего дерева 307 — `21df800a1fbb292489dcb3d46357fb7303085589f0cb64072f47d3c14eb7c00a`,
совпало.

Сам caller — не копия: я извлёк из `BotNavMesh.cpp@307` тела `Fnv64` (305 символов),
`ManifestFnv` (482) и `ManifestOk` (726) и нашёл их **дословно** в `build/c154-307-manifest/v5/caller.cpp`.
`run.log`: `Actual ManifestOk navmesh=1 / links=1 / spots=1`, rawFnv и manifest FNV совпадают.

## 3. Harness и analyzer

`Tools/Invoke-WarsBotsC154Route.ps1`: SHA256 `87c3f5ee79c5507e1952367edbf5b53d3ee3c211eb8de5614b67fef2ae24fbf4`,
коммит `e973b905` (его blob `5f9c3746…` совпадает с файлом в QA-ветке — поле `harness` в quality
указывает именно на коммит, а не на blob).

Гейты, которые я проверил по коду:

- Требуется ровно 40-hex `-CodeSha`; `-PrepareOnly` ничего не запускает.
- Замороженный рецепт: манифест `4a065c8d…` и побайтовые хеши payload; helper
  `Launch-WarsBotsShowcase.ps1` = `d2a7cb30…`, AST-парсинг и извлечение только белого списка функций.
- Данные: жёстко `build/c154-route-data-v5` и четыре ожидаемых хеша.
- Только loopback: `+sv_bind 127.0.0.1 +sv_lanonly 1`, `+sv_gamerules PowerStruggle +map multiplayer/ps/Shore`.
- Receipt: `task=C154`, `harness_scope=c154-lower-route-v2`, `source==CodeSha`, `ci_success`,
  `ci_source_match`, `review_ok`, непустой `review_branch_head`, совпадение `harness_sha256`/`analyzer_sha256`,
  `manifest_guard_ok`, совпадение `data_hashes`.
- Свежесть: `Assert-ReviewFresh` сверяет `origin/warsbots/research/deepseek` с `review_branch_head` и
  требует `RUNTIME_PIN: <CodeSha>` в пункте C-154; вызывается перед подготовкой, перед прогоном и
  каждые 15 с во время прогона.
- Безопасность: отказ при активных игровых процессах, занятом порту и повторной приёмке;
  readiness-гейты (RULES INFO PowerStruggle, `gate=1 levelReady=1`, `ENTITY_EVENT_DONE`, `bots=1 server=1`,
  `NAVMESH loaded … Shore: … 2 links`, `links ON`, `#2 revive result … sameEntity=1 alive=1 health=100`);
  проверка effective-пресета; бюджет готовности 60 с и прогона 300 с; `+quit` нет; `Restore-Data` в `finally`.

`Tools/Diagnostics/Analyze-C154Route.py`: SHA256
`2a642aebcfff2d10e119746b42f8e50f48a004d732f7aee106602b3e6fff88f7`. Критерии маршрута:

- ровно один сетап `DEBUG REVIVE … spawnId=545 manual=1 nativeKill=1 zOffset=0.200 hooks=0`, один
  `#2 created EntityId=` и `revive result … sameEntity=1 alive=1 health=100`;
- после сетапа любой `DEAD/RESPAWNING/revive/removed/ENTITY_EVENT_DONE`, `DEBUG REVIVE|TELEPORT`,
  `STATE CONFLICT`, retry-гвард, TRAP-спасение, `TEST GRANT` → FAIL;
- `NAV fallback`/`LINK failed`/`LADDER failed` → FAIL; PS CAPTURE только на объявленный target 654;
- одна попытка лестницы, без cancel; `native_attached` → `exiting=1` → `done nativeOn=0 exiting=0 grounded=1`
  с z в 98.8…100.7;
- Strength только после `ladder_done`;
- трейс: монотонное время, живой бот, нижняя площадка z 89.5…91.0 у (2114.5, 1887.875) ±3 м;
- финал: после воды, не вплавь, grounded, z > 100.5, в 40 м от цели и > 100 м от (2110, 1880), сухо ≥ 7 с.

Это и есть требуемая цепочка lower545 → штатная лестница → физический выход/верхний пол → Strength →
вода → сухой берег, без телепорта и без «успеха по таймеру».

## 4. Что изменилось после DS126 и почему ревью устарело

- DS127/DS128/DS129 касались R4 (`BotBrain.cpp`, тест, CI) и OFF-линковки (`ScriptBind_GameRules.cpp`) —
  ни один из этих файлов не входит в набор C154; `claude-sprint` по-прежнему на `307037bc`.
- `c154-quality.json` содержит `review=4c710aca…`, `review_branch_head=4c710aca…`, `review_ok=true`.
  Текущая голова `research/deepseek` — `4fecbb8b` (эта публикация), поэтому `Assert-ReviewFresh`
  **сейчас обязан отказать**. Это корректное поведение, а не дефект: нужен новый receipt.
- Новый receipt должен пиновать голову ревью после DS130 и сохранить `RUNTIME_PIN` = `307037bc`.
  Перепин `4c710aca` без нового ревью недопустим.

## 5. Пробелы доказательств (честно)

1. `build/c154-ds124-check/accepted-consumed-control`, на который ссылался DS126, **отсутствует на
   диске** (вероятно, удалён при гигиене C155/C156). Тот же сценарий покрыт закоммиченным
   `Tests/test_bot_ladder_exit_retry_caller.py@307` (строки 249, 307: один bounded retry только после
   consumed/default, rejected `SetInput` тоже считается израсходованной попыткой), который гоняется в CI.
   Это сужает доказательство до CI-оракула, но не отменяет его.
2. Оригинальные per-attempt снимки Memory/Navmesh четырёх попыток и часть временных квитанций C154
   не восстановлены (C156). Они не подделывались и здесь не заявляются восстановленными.
3. `client_visual=NOT TESTED`; визуальная корректность выхода по-прежнему требует отдельного наблюдения.
4. Проверка `PrepareOnly` и самих гейтов — по коду и хешам; harness не запускался.

## 6. Рекомендация

Одобрить набор для **одного** ограниченного приёмочного прогона после того, как Codex:

1. выпустит новый receipt с `review_branch_head` = голова после DS130 и `review_ok=true`;
2. подтвердит, что игра/сервер не запущены и порт 46164 свободен;
3. выполнит ровно один прогон lower545 → лестница → физический выход/пол → Strength → вода → сухой берег,
   затем архив/демо/восстановление и независимый аудит.

Нового игрового кода не требуется. Форс, телепорт, ослабление физических гейтов и повтор неизменного
провала не допускаются. R4-кандидат и его приёмка в область этой задачи не входят.
