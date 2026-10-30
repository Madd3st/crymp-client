# WarsBots: актуальный QA-статус — 30.10.2026

Начать с [START_HERE](../START_HERE.md). Игровой Code/ этой QA-ветки устарел; текущий код находится в claude-sprint, а задачи и решения — на steelmill-triage.

Owner finished C157 Steelmill and requested continuing Shore. C157 archived/restored. One fresh-reviewed C154307 run completed with blocked_top failure; no game active, no unchanged retry. Autonomous wake/wait_mail loops remain disabled.

## Текущая работа

| Пункт | Состояние | Следующий шаг |
| --- | --- | --- |
| C154: Shore | 307037bc first controlled run FAILED: blocked_top at15.63s, feet z97.922, nativeOn1/exiting1. DS130 preflight OK; demo/archive and exact Memory4/Nav16 restore PASS | No unchanged rerun. Investigate graph state selection; DS131 post-run audit pending. [Result](Coordination/C154_307_RUNTIME_RESULT.json) |
| C156: восстановление | Завершено с известными историческими потерями | [Результат и ограничения](Coordination/C156_RECOVERY.md) |
| R4 / C157 | Кандидат `cde77823`: PR41039830200 SUCCESS (x86/x64 RelWithDebInfo и Debug + server_zip); WarsBots CI41039823887 SUCCESS (MSVC units + experimental x64 Release client/server). DS128 R4 и DS129 OFF-guard OK; последующий workflow фикс проверен CI. CI-пара12970885272 скачана, SHA-256 PASS. PR№1 не слит; runtime НЕ проверен, C154307 сохранён. | [Draft PR №1](https://github.com/Madd3st/crymp-client/pull/1); [результат](Coordination/C157_R4_RESULT.md) |
| R9 | Подтверждён разрыв контракта направления generic jump; конкретное падение не доказано | Удержание проверенной траектории или повторная валидация |
| D032 | Решение о first_primary по умолчанию открыто | Решение владельца; defaults не менять самостоятельно |

## Текущий игровой срез C157

C157 completed: 361.2s server demo, 107 bot pickups / 2247 shots / 714 hits / 76 kills / 71 revives. Full demo tail and archive SHA PASS; original Memory4/Navmesh16 exact names/hashes restored and verified. Owner reports unused sniper roof positions; tracked for later diagnosis. R4 late-ammo exhaustion still not specifically proven. [Final receipt](Coordination/C157_RUNTIME_FINAL.json).

## Что подтверждено раньше

C155: готов классический Steelmill на E, `abac72a9` / CI40890940918 / DS121 `5c12d27b`, восемь ботов, по два каждой сложности. [Квитанция](Coordination/C155_CLASSIC_READY.json). Отдельной игровой приёмки смешанного состава ради упаковки не было.

C153: ограниченный серверный Steelmill на `5ef9ee22` / CI40790759287; это не клиентская приёмка и не полный матч PS. [Результат](Coordination/C153_GOAL_WEAPON_RESULT.md).

C154 на старом abac: подъём работал, верхний выход FAIL около97,92м. Сохранены логи/демо четырёх попыток и видео человека, успешно вышедшего из той же шахты. Старые отчёты о точном восстановлении данных остаются историческими: отсутствующие снимки нельзя заново проверить. [V4](Coordination/C154_ATTEMPT_V4_TOP_EXIT.json), [видео](Coordination/C154_OWNER_VIDEO_20261029.json).

## Восстановление и источники истины

C156: исходники, CI-пара307, V5, вики, исходные логи и демо четырёх попыток восстановлены и проверены; офлайн-проверки пройдены. Оригинальные per-attempt снимки Memory/Navmesh и часть временных квитанций C154 не найдены. Reviewed quality восстановлен по содержимому сохранённого Git-отчёта, исходная сериализация не заявляется восстановленной. Снимки C127/C155 не подставлялись вместо C154.

Дополнительный поиск E: 36ZIP/73937записей, 64snapshot-манифеста/268файлов SHA PASS,81restore-квитанция; оригиналов C154 не найдено. Полный комплект и индекс — E:/WarsBots/Recovery-2026-10-30/.

[Текущие задачи](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/TO_CODEX.md), [передача](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/CLAUDE_STATE.md), [решения](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/DECISIONS.md), [вики](https://github.com/Madd3st/crymp-client/wiki). Старые отчёты и архивные open/STOP не являются текущими поручениями. Чужие Recording/C126-изменения сохраняются.
