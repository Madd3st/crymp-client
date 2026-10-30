# Текущая передача WarsBots — 30.10.2026

## Current outcome after owner finished Steelmill and resumed Shore

C154307 actual runtime FAIL: one controlled lower545 run reached native exiting, then blocked_top at15.63s z97.922; no physical upper exit/Strength/water/dry-shore. Archive/demo fulltail PASS; Codex MSIX physical Memory4/Navmesh16 exact restore PASS. No game active or unchanged retry. Next: source-grounded graph-selection/clip-availability investigation; root cause not proven, no physical fallback approved. [QA result](https://github.com/Madd3st/crymp-client/blob/55837e6d/Docs/Coordination/C154_307_RUNTIME_RESULT.json). C157 Steelmill finished361s, archived/restored; owner reports unused sniper roof. Autonomous wake loops remain OFF.

Earlier live-session/hold paragraphs below are historical, superseded by this outcome.

## Активная игровая проверка C157 — новое разрешение владельца 30.10

Владелец: «ну давай тестить и смотреть поведение». Запущен отдельный Steelmill на cde77823: 8ботов, по2каждойсложности, клиент и сервер активны для владельца. Не останавливать и не запускать параллельную игру. Демо/логи пишутся; launcher дождётся закрытия клиента, затем архивирует и восстановит SHA-проверенные снимки Memory/Navmesh. Срез QA9e70f494:112с,39bot pickups/652shots/218hits/22kills/18revives; текущие8/8 имели primary. Это LIVE PARTIAL, позднее ammo exhaustion R4 не подтверждено. [Квитанция](https://github.com/Madd3st/crymp-client/blob/9e70f494/Docs/Coordination/C157_RUNTIME_LIVE.json). C154 Shore pin307 и его отдельный preflight не менялись, автономные wake-циклы выключены. Старые запреты runtime ниже описывают состояние до этого разрешения; оно относится к текущему Steelmill.

По поручению владельца выполняются только явно назначенные работы. Автоматические пробуждения, циклы wait_mail и самостоятельный перебор backlog выключены. **Игру и сервер агенту запускать запрещено до отдельного явного разрешения**, даже если игровых процессов нет. Сведения о том, что владелец играл 29 октября, являются историей, а не текущим состоянием ПК.

## Кто и где работает

- Codex — временный координатор, интегратор и QA по D031/D035; DeepSeek — предложения и независимое ревью только в своей ветке. Роль не переключается автоматически по календарю или старому лимиту.
- Со слов владельца30.10, Claude ожидается примерно через два дня (ориентир01.11.2026). Подготовить понятную передачу в Git; фактический возврат и смена роли подтверждаются явно, не происходят автоматически по дате.
- Игровой код: claude-sprint `307037bc08050d39f26310dc6cc7071dddddf4a6`, QA/build/ds076-code. Рабочая копия detached HEAD на точном SHA; перед изменениями проверить ветку и status.
- Координация и исходники вики: steelmill-triage, QA/build/ds076-triage. QA-инструменты/результаты: weapon-goals, C:/Users/Magic8/WarsBotsBuild/crymp-multibot. Code/ QA-ветки устарел.
- DeepSeek: C:/Users/Magic8/AppData/Local/Temp/opencode/wb-review/deepseek, ветка research/deepseek. DS126 review head `4c710aca7a5543bc1c5a66005ef8a8e352792f6f`; не двигать его ради фонового backlog.
- Вики: QA/build/wiki-c085; публикация только из Docs/Wiki на triage. Стартовая инструкция: [START_HERE](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/START_HERE.md).

## Открыто и завершено

| Пункт | Состояние |
| --- | --- |
| C154 | Кандидат307/fullCI40919291234/DS126 code-data-harness OK. Физический результат307 НЕ проверен. Runtime запрещён владельцем |
| C155 | Завершён: E:/WarsBots/Showcases/Steelmill-Classic-abac72a9, IA8/2каждойсложности, abac/fullCI40890940918/DS121 |
| C156 | Восстановление завершено с указанными ниже историческими потерями; QA35112665 и последующий поиск QAffd2d87b |
| R4 / C157 | Кандидат `cde77823`, отдельная ветка codex/r4-weapon-search-episode: PR41039830200 SUCCESS (x86/x64 RelWithDebInfo/Debug + server_zip), WarsBots CI41039823887 SUCCESS (units + experimental x64 Release client/server); CI-пара12970885272 SHA PASS. DS128 R4 и DS129 OFF-guard OK; последующий workflow-only signing-key фикс проверен CI. [Draft PR№1](https://github.com/Madd3st/crymp-client/pull/1) не слит; runtime НЕ проверен, C154307 сохранён. |
| R9 | Подтверждён контрактный риск смены направления generic jump, конкретное падение не доказано |
| D032 | first_primary default — открытое решение владельца |

Следующий игровой шаг C154 — только после явного разрешения, свежих пинов/ревью/проверки процессов и НОВОГО snapshot живых данных: lower545 → штатная лестница → физический выход/верхний пол → Strength → вода → сухой берег; затем архив/демо/восстановление и независимый аудит. Ни нового прогона307, ни полного PS-принятия нет.

## Что восстановлено и что утрачено

C156: исходники, CI-пара307, V5, вики, исходные логи и демо четырёх попыток восстановлены и проверены; офлайн-проверки пройдены. Оригинальные per-attempt снимки Memory/Navmesh и часть временных квитанций C154 не найдены. Reviewed quality восстановлен по содержимому сохранённого Git-отчёта, исходная сериализация не заявляется восстановленной. Снимки C127/C155 не подставлялись вместо C154.

Заново пройдены oldsemanticRED/newGREEN, accepted-consumed/no-resend, route/climb/graph/motion, BotLadder/BotConfig. Результат actualManifestOk имеет тот же SHA256, что исходная квитанция. Новые офлайн-результаты не являются игровым доказательством.

Полный комплект: E:/WarsBots/Recovery-2026-10-30, проверенный bundle/restart-kit, CI-логи, геометрия Shore/Plantation на E. Дополнительный поиск:36ZIP/73937members и1368originalJSON,64snapshot-манифеста/268payloadSHA PASS,81restore-квитанция; шестьC155 receipt/before совпали. C127/C155 reference-снимки изолированы, оригиналов C154 не найдено. [C156](https://github.com/Madd3st/crymp-client/blob/warsbots/feature/weapon-goals/Docs/Coordination/C156_RECOVERY.md).

## Последняя выполненная разработка — C157

Кандидат `cde77823`, отдельная ветка codex/r4-weapon-search-episode: PR41039830200 SUCCESS (x86/x64 RelWithDebInfo/Debug + server_zip), WarsBots CI41039823887 SUCCESS (units + experimental x64 Release client/server); CI-пара12970885272 SHA PASS. DS128 R4 и DS129 OFF-guard OK; последующий workflow-only signing-key фикс проверен CI. [Draft PR№1](https://github.com/Madd3st/crymp-client/pull/1) не слит; runtime НЕ проверен, C154307 сохранён.

R4: clock начинается при новом ограниченном поиске; доступное оружие очищает устаревшую pickup-цель. Fallback once/life сохранён. Дополнительно исправлена обычная OFF линковка: BotMatch include/call под experimental macro. Подпись запускается только при наличии ключа; при настроенном ключе ошибка остаётся fatal. Набор сборок/тестов сохранён. bd8692cd имел только experimental CI green и OFF failure; 79c03389 исправил link, затем выявилась подпись без ключа. Это история, текущий результат — cde77823.

[QA f3c89e99: результат/ограничения](https://github.com/Madd3st/crymp-client/blob/f3c89e99/Docs/Coordination/C157_R4_RESULT.md), JSON рядом содержит source/run/artifact/digest/SHA. Рабочая копия E:/WarsBots/Development/r4-weapon-search-episode; квитанции E:/WarsBots/Development/C157-checks. Независимое DS129 — 4fecbb8b; оно не является новым runtime gate C154. Перед runtime нужны разрешение владельца и свежие code/data/harness/review проверки; старый quality не переписывать для обхода head check. Игра/сервер/локальная игровая сборка/установка/autowake не запускались.

## Правила продолжения

Не удалять build целиком: там живут worktree, данные и доказательства. Общий очиститель WarsBots уже исключает. Не возвращать устаревшие гигабайты на C без необходимости. Не подменять namespaces LOCALAPPDATA, живые Memory/Navmesh или чужие Recording/C126-файлы. Игровые сборки — только точный полный зелёный CI; никаких SetPos/импульсов/таймерного успеха/ослабления physical guards, IsPlayer/channel/defaults/wire ради C154. DS118v1 отозван, DS120 — исторический проект, текущий кандидат —307. Исторические open/STOP не новые задачи.
