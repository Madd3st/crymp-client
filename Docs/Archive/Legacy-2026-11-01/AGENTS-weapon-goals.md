# WarsBots: инструкции для агента

## Начало работы

1. Проверь `git status --short`, ветку и HEAD. Сохрани чужие изменения.
2. Обнови refs: `git fetch origin '+refs/heads/warsbots/*:refs/remotes/origin/warsbots/*'`.
3. Прочитай [START_HERE](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/START_HERE.md), затем свежие origin/warsbots/research/steelmill-triage: Docs/Research/STATUS.md, Docs/Research/Coordination/MAILBOX/CLAUDE_STATE.md и TO_CODEX.md. Источники на GitHub важнее старого чата и старой локальной копии.
4. Сначала rg, затем нужные функции. Не перечитывай весь SDK/архив.

## Ветки и рабочие копии

- Текущий проверенный Shore-кандидат: codex/shore-ladder-exit, E:/WarsBots/Development/r4-weapon-search-episode, ed72987c (PR2, не слит). Полный серверный маршрут и DS135 PASS; клиентский плейтест владельца PASS; видео отдельно не просмотрено. Основная claude-sprint/QA/build/ds076-code остаётся307 как история до PR.
- QA-инструменты и результаты: warsbots/feature/weapon-goals, C:/Users/Magic8/WarsBotsBuild/crymp-multibot. Его Code/ устарел.
- Задачи, решения, исходники вики: warsbots/research/steelmill-triage, QA/build/ds076-triage.
- Предложения/ревью: warsbots/research/deepseek. Не путать предложение с интегрированным кодом.
- master и старый warsbots/main не являются текущим выпуском ботов. Не сливать ветки автоматически. Не push upstream и не force-push.
- Документы/AGENTS внутри закреплённого старого игрового SHA описывают тот срез. Текущий режим владельца и очередь берутся из свежей передачи; не запускать старую сборку/задачу по историческому тексту.

## Действующий режим владельца

- Разрешена работа по явному поручению: разработка, офлайн-проверки, адресное ревью и CI. Владелец явно разрешил текущую разработку/проверки Shore. Каждый новый runtime требует свежего точного допуска code/data/harness/review/CI и свободной игры; не переносить разрешение на самостоятельные задачи backlog.
- Игровые сборки — только полный зелёный CI точного SHA и согласованная пара EXE. Локально игру не собирать. Engine-free тесты допустимы.
- Автономные wake/wait_mail-циклы и самостоятельный backlog выключены. Не включать Windows WarsBots Mailbox Wake. Не вызывать модель для проверки «есть ли работа».
- Не запускать параллельных агентов без запроса владельца. DeepSeek — только конкретная адресная задача в собственной ветке; один интегратор общих Player/GameRules/CMake/network изменений.
- Роль Codex/Claude — по текущей передаче, не по календарю или старому проценту лимитов.

## Инварианты

- Не менять defaults без решения владельца. Сохранять opt-in и ограничения точного проверяемого рецепта; наличие PS-кода не означает принятия полного Power Struggle.
- Никаких fake INetChannel/OnClientConnect(0), глобальных IsPlayer/channel изменений или anti-cheat обходов.
- Сохранять ownership/EntityId guards, очистку уровня, штатное оружие/физику. В spawn metadata до Init хранить только marker, не обращаться к GameObject.
- Согласованные клиент/сервер, штатная сериализация CryMP и optional groups. Нет новых engine binary patches без доказательств и точной проверки цели.
- Не заменять CryMP gameplay старой реализацией Crysis Wars. Сохранять лицензии.
- Не ослаблять физические критерии выхода ради зелёного теста; без SetPos/импульса/таймерного успеха.
- Код, unit, CI, серверный результат и визуальная приёмка — разные доказательства. Успех старой карты/верхнего спавна не закрывает нижний маршрут или весь PS.

## Сохранность и отчёты

- Не удалять build целиком: там Git-worktrees, данные и доказательства. Материалы восстановления: E:/WarsBots/Recovery-2026-10-30, ограничения — QA Docs/Coordination/C156_RECOVERY.md.
- Не подменять живые Memory/Navmesh и не смешивать namespaces LOCALAPPDATA. Чужую игру не останавливать, +quit/короткие игровые таймеры не использовать.
- В QA сохранить чужие Tools/Play-WarsBotsRecording.ps1 и Tools/Invoke-WarsBotsC126Probes.ps1.
- После существенного изменения обновить текущий state и относящийся к задаче результат. Не переписывать всю документацию и не добавлять бытовые отчёты об очистке диска.
- Исторические AGENT_HANDOFF/ROADMAP/ревью и архивные open/STOP не являются текущей очередью. Успешные проверки повторять только при изменениях или нерешённом вопросе.
