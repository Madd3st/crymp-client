# DS-129 — ревью точного LOCAL-кандидата `79c03389` (OFF-link фикс C-157/PR1)

Задача: DS-129, продолжение C-157/PR1. Кандидат
`79c03389a32103a674e0b60399c0e7b5c1c1e621`, родитель `bd8692cd`, ветка
`codex/r4-weapon-search-episode`, дерево `E:/WarsBots/Development/r4-weapon-search-episode`.
Код LOCAL, не запушен; полный CI и матрица PR на этом SHA ещё не запускались.
C154 остаётся на `307037bc`, runtime запрещён.

Уровень доказательств: чтение закоммиченного diff, теста, CMake и реальных вызывающих мест,
плюс собственная проверка дословного извлечения кода в логах. Игру, сервер, сборку проекта и
живые данные не использовал; кандидат не пересобирал (в среде ревью нет Python), вместо этого
сверил артефакты.

## Вердикт

**OK.** Правка закрывает единственную неохраняемую ссылку на `BotMatch` в TU, который
собирается и при OFF, поведение ON не меняет, штатная маршрутизация сообщений сохраняется.
Регрессий не найдено. Ниже — проверки, два некритичных замечания и границы доказательства.

## Что именно изменено

`git diff bd8692cd 79c03389`: 3 файла, +94/−0.

- `Code/CryGame/ScriptBind_GameRules.cpp` +4: `#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)` вокруг
  `#include "Bots/BotMatch.h"` и вокруг существующего блока подавления
  (`:1553-1557`). `channelId = pGameRules->GetChannelId(...)` остался **вне** охраны.
- `Tests/test_bot_text_message_gate_caller.py` +87: OFF/ON actual-caller тест.
- `.github/workflows/warsbots-ci.yml` +3: тест подключён после weapon-search caller.

CMake, PR-матрица, `IsPlayer`/канал, ladder и политика R4 не тронуты. Стаба `IsBot` нет.

## Причина сбоя подтверждена

- Реальный лог `pr-failed.log`: `ScriptBind_GameRules.cpp.obj : error LNK2019: unresolved external
  symbol "… WarsBots::BotMatch::IsBot …" referenced in function "… CScriptBind_GameRules::SendTextMessage …"`,
  затем `LNK1120: 1 unresolved externals` для `CryMP-Server64.exe` и `CryMP-Client64.exe`. Ровно один
  неразрешённый символ, что совпадает с одной неохраняемой ссылкой.
- `.github/workflows/pr.yml` действительно собирает четыре OFF-конфигурации: 32/64 ×
  RelWithDebInfo/Debug. `WARSBOTS_EXPERIMENTAL_SPAWNER` — `option(... OFF)` (`CMakeLists.txt:57`),
  PR его не включает.
- `BotMatch.cpp` добавляется только внутри `if(WARSBOTS_EXPERIMENTAL_SPAWNER)` (`CMakeLists.txt:1224-1259`),
  там же `target_compile_definitions(Base PUBLIC WARSBOTS_EXPERIMENTAL_SPAWNER)` (`:1258`), а
  `CryGame` линкуется с `Base` PUBLIC (`:1223`) — то есть в ON макрос виден TU `ScriptBind_GameRules.cpp`
  (`:1165`), в OFF его нет.
- Дефект **не от R4**: в `307037bc` и `bd8692cd` в этом файле ровно та же одна неохраняемая ссылка
  (`IsBot lines=1`), R4-коммит файл не трогал.

## Полнота охраны (весь OFF-набор)

Все ссылки на `BotMatch` вне `Code/CryGame/Bots/` (эти TU собираются и при OFF):

| Место | Состояние |
| --- | --- |
| `ScriptBind_Actor.cpp:1589` | уже охранялось (DS-050), тот же паттерн |
| `GameRules.cpp:1381,1387,1393,1400` | уже охранялось (блок `:1377-1396`, `:1399-1401`) |
| `GameRulesClientServer.cpp:302` (`LogScores`) | уже охранялось (`:300-303`) |
| `ScriptBind_GameRules.cpp:1554` | **единственное неохраняемое — теперь закрыто** |

Файлы `BotBrain/BotInstance/BotManager/BotMoveTrace/BotWaterPolicy/BotMatch.cpp` входят только в
экспериментальный список источников, поэтому в OFF не компилируются. Других ссылок нет —
после правки OFF-линковка не должна иметь неразрешённых `BotMatch`-символов.

## Поведение ON/OFF

- **ON**: условие и вызов идентичны прежним; бот (channelId 0) не получает per-client RMI,
  `GetChannelId` для него не вызывается. Тест проверяет: бот → `EndFunction`, `channels==0`.
- **ON, человек**: id 8 → `GetChannelId` вызван один раз, сообщение уходит (108).
- **ON, broadcast** (`to=0`) на bot id: подавления нет (условие требует `eRMI_ToClientChannel`) → 107.
- **OFF**: подавление отсутствует, штатная маршрутизация: id 7 → 107, `GetChannelId` вызван.
  В OFF ботов быть не может (спавнер выключен), поэтому потеря подавления не наблюдаема.

## Проверка оракула и логов

Тест извлекает **реальный** блок `if (pH->GetParamType(4) == svtPointer) { … }` из
`CScriptBind_GameRules::SendTextMessage`, подставляет в `Dispatch`, компилирует OFF без макроса и
ON с `/DWARSBOTS_EXPERIMENTAL_SPAWNER`. `BotMatch::IsBot` объявлен всегда, определён только под
макросом — как в реальной линковке.

Моя независимая сверка: блок из `bd8692cd` (484 символа, без охраны) присутствует дословно в
`OFF-link-before/OFF/caller.cpp`; блок из `79c03389` (534 символа, с охраной) — дословно в
`OFF-link-after/OFF/caller.cpp`. Копирования логики нет.

Логи:

- `OFF-link-before`: компиляция прошла (`compile.log` = имя файла), затем **LNK2019** на
  `WarsBots::BotMatch::IsBot` из `Dispatch` и `LNK1120` — воспроизведён именно тот сбой, не ошибка
  компиляции.
- `OFF-link-after`: OFF компилируется, линкуется **без реализации `BotMatch`**, `run.log` —
  `Actual SendTextMessage caller PASS`; ON — то же.
- `R4-after-link-fix`: `invariant_failures=0 episode_failures=0 late_failures=0`, `GREEN PASS` —
  правка не сломала оракул R4.
- Экспериментальный CI на `bd8692cd` (`40976171549`) был зелёным, то есть ON-сборка и подавление
  работали; падала только OFF-матрица PR.

## Некритичные замечания

1. **Тест проверяет извлечённый блок, а не распространение макроса из CMake.** ON-поведение
   подтверждается реальной ON-сборкой (CI `40976171549`) и существующим паттерном DS-050/C-126, но
   сам юнит-тест макрос не прокидывает. Если кто-то уберёт `PUBLIC` у
   `target_compile_definitions(Base ...)`, тест этого не поймает — поймает игровая сборка/поведение.
2. **Воспроизведение RED (`--expect-off-link-failure`) не входит в CI-прогон.** Регрессию это не
   ослабляет: при снятой охране OFF-линковка `caller.obj` падает, а тест превращает это в ошибку CI.
   Но сам факт «старый сбой воспроизводится» остаётся ручной проверкой.
3. Мелочь: `units/` не в `.gitignore`, после локального прогона в дереве остаётся `?? units/`
   (так же у остальных тестов).

## Что не доказано

- Полный CI и вся 4-конфигурационная PR-матрица на `79c03389` ещё не запускались: вывод о
  линковке OFF опирается на реальный диф, карту символов и воспроизведение точного LNK2019.
- Тест мокает движок; он не проверяет реальную отправку RMI по сети (это заявлено в docstring).
- C154 runtime не проверялся и запрещён; источник C154 остаётся `307037bc`.

## Рекомендация

Принять `79c03389` для полного CI и PR-матрицы на этом же SHA. Правка минимальна, совпадает с уже
принятым паттерном охраны, ON-поведение не меняет. В `claude-sprint` не вливать до зелёной матрицы
и стандартных гейтов; C154 не двигать.
