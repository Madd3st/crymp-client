# Карта репозитория

> 30.10.2026: текущий код307, QA-инструкция [START_HERE](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/START_HERE.md), [состояние](Status-and-Roadmap). Рабочие копии восстановлены; build нельзя удалять как кэш. `master` и локальная `warsbots/main` не являются текущим выпуском ботов. Старые C070/C071/combat-port/locomotion архивированы; ветки с уникальными коммитами не удалены.

Текущий проект — [Madd3st/crymp-client](https://github.com/Madd3st/crymp-client), порт WarsBots на CryMP. Старый репозиторий WarsBots и структура `Code/Mod/` относятся к прежнему этапу. Здесь игровой бот находится в `Code/CryGame/Bots/`.

## Сначала выбери ветку

| Ветка | Авторитетное содержимое | Для чего читать |
| --- | --- | --- |
| `warsbots/feature/claude-sprint` | Текущий игровой код и CI | C++, интеграция, фактические команды и значения |
| `warsbots/research/steelmill-triage` | Документация, исходники вики, исследования, mailbox | План, принятые решения, навигационные инструменты |
| `warsbots/feature/weapon-goals` | QA-инструменты, X-отчёты, JSON-результаты | Как запускали и что измерили; **игровой код здесь устарел** |
| `warsbots/research/deepseek` | Патчи, независимые ревью, анализ | Предложения и проверка; наличие патча не означает интеграцию |
| `warsbots/research/movement-params`, `warsbots/research/claude-modules` | Исторические исследования и модули | Только по ссылке из актуальной задачи |

[Рабочие checkout, артефакты и архивы](Artifacts-and-Storage) описывают физические пути, хранение наE и восстановление закрытых ревью.

Не делай вывод о текущем поведении по случайному локальному HEAD. Пакеты C150 и результат C149 зафиксированы на `b8b444d6`; ветка может продвинуться позже.

## Быстрый поиск по симптому

| Симптом / задача | Начало поиска в claude-sprint |
| --- | --- |
| Создать/убрать бота, команда консоли, общий тик | `BotManager.cpp`, `BotInstance.cpp`, `BotMatch.cpp` |
| Неверная цель, переключения поведения | `BotBrain.cpp`, `BotGoalArbiter.h`, `BotDecisionCommit.h` |
| Не замечает противника / плохо целится | `BotTargeting.cpp`, `BotPerception.h`, `BotAimModel.h`, `BotCombat.cpp` |
| Не выбирает оружие | `BotWeapons.cpp`, `BotWeaponRank.h` |
| Не доходит до цели | `BotMovement.cpp`, `BotNavMesh.cpp`, `BotNavMeshCore.cpp`, `BotReachability*` |
| Застревает на переходе / прыжке | `BotJumpLink.h`, `BotLocomotion.cpp`, `BotCorners.h` |
| Вода / выход на берег | `BotWaterPolicy.*`, `BotNavMesh*`, квалификация водной цели в `BotBrain.cpp` |
| Команда, спавн, покупка, захват | `BotGameMode.h`, `BotMatch.cpp`, `BotInstance.cpp`, `BotPSBuy.h`, `BotPSCapture*` |
| Техника | `BotBoatDriver.h` и вызовы из runtime; модуль не доказывает завершённый маршрут техники |
| Наблюдатель / реплика бота | `BotSpectator.h`, `BotReplicaSmooth.h`, интеграция `Actors/Player/` |
| Переключатель и его значение | `BotConfig.h/.cpp`; команды также в `BotManager.cpp` |
| Логи и запись | `BotLog.*`, `BotMoveTrace.*`, `BotDemoRecorder.*`, `BotDemoCodec.h` |

Все имена `Bot*` в таблице относятся к `Code/CryGame/Bots/`. Для точных ссылок и порядка чтения — [маршрут инженера](Engineering-Guide).

## Инструменты и документы

| Путь | Ветка / назначение |
| --- | --- |
| `.github/workflows/warsbots-ci.yml`, `Tests/` | claude-sprint: MSVC CI и проверки |
| `Tools/Launch-WarsBotsShowcase.ps1`, `Prepare-WarsBotsShowcase.py`, `Test-WarsBotsShowcase.ps1` | weapon-goals: исходники пакетов C150 |
| `Tools/SteelmillTriage/` | triage: разбор логов/демо; сначала README и `--help` |
| `Tools/RecastBake/` | triage: офлайн-запекание сетки и переходов |
| `Tools/OfflineCheck/` | triage: дешёвые проверки до игры |
| `Tools/PlaytestReport/` | triage: отчёт с привязкой к плейтесту |
| `Tools/Mailbox/` | Почта и пробуждение агентов; читать версию из актуального checkout |
| `Docs/Wiki/` | triage: источник опубликованной вики |
| `Docs/Research/STATUS.md` | triage: актуальный срез проекта и ссылки на доказательства |
| `Docs/Research/Coordination/MAILBOX/` | triage: задачи C-/DS-, решения D-, handoff и протокол |
| `Docs/Coordination/TO_CLAUDE.md` | weapon-goals: последние X-отчёты и NOW; старые — в архиве |
| `Docs/Coordination/DEEPSEEK_TO_CLAUDE.md` | deepseek: независимые ревью и патчи |
| `Docs/STATUS.md` | weapon-goals: журнал QA, не общий статус проекта |

## Данные не равны Git

`%LOCALAPPDATA%\WarsBots` содержит `Logs`, `Demos`, `Memory`, `Navmesh`, `Geometry`. Фактический путь зависит от окружения процесса: используй пути из журнала сессии. Результаты C149 и CI находятся также в локальном `build/`; это не гарантированно отслеживаемые Git-файлы. Большие геометрии и бинарники не следует добавлять в Git; в отчёте сохраняются хеши и расположение архива.
