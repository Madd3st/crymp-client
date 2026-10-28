# Команда и процесс

## Роли на 28.10.2026

| Участник | Ответственность |
| --- | --- |
| Владелец | Цели, плейтесты, видео и решения о поведении/defaults |
| Codex | Временная координация, интеграция, QA на ПК владельца, готовые профили и документация (D031/D035) |
| DeepSeek через OpenCode | Разбор сырых данных, патчи в своей ветке, независимое ревью перед игровыми пачками |
| Claude | Временно вне лимитов; возврат по сохранённому handoff, а не по памяти чата |

Новые параллельные агенты не запускаются без явного запроса. У общей интеграции один владелец. Отдельный автор патча не объявляет его интегрированным до SHA в целевой ветке.

## Память в Git

В triage: [PROTOCOL](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/PROTOCOL.md), [TO_CODEX](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/TO_CODEX.md), [TO_DEEPSEEK](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/TO_DEEPSEEK.md), [DECISIONS](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/DECISIONS.md), [CLAUDE_STATE](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/Coordination/MAILBOX/CLAUDE_STATE.md). C- — задача Codex, DS- — задача DeepSeek, D- — решение владельца. Ответ Codex — X- в `Docs/Coordination/TO_CLAUDE.md` на weapon-goals; ответ DeepSeek — в собственном outbox на deepseek.

Статусы ACK → STARTED → PROGRESS → DONE/BLOCKED. «Готово» сопровождается SHA; `NOW:` отражает текущую работу. Старый STOP ограничен своей задачей. Планы и ещё не принятые патчи не записываются как выполненное поведение.

## Согласованная работа

Странное поведение сначала анализируется совместно, исправления собираются в связную пачку. Проверки идут от дешёвых к дорогим; CI и свежее независимое ревью предшествуют игровому запуску. Локально game build не выполняется. Запуски не идут, пока владелец играет; чужие серверы/клиенты не останавливаются.

Ожидание почты — `Tools/Mailbox/wait_mail.ps1`; страховка на Windows — задача **WarsBots Mailbox Wake**, установленная версия в `%LOCALAPPDATA%\WarsBots\Mailbox\bin`. Она использует текущую сессию агента, не случайный последний чат. Долгие сессии перезапускаются с сохранением handoff по правилам протокола; идентификаторы сессий — эксплуатационные данные, не постоянная часть вики.

## Документация

Вики — вход для человека; mailbox — оперативные поручения; отчёты — проверяемые результаты. Исходник вики находится в `Docs/Wiki/` на triage, опубликованная копия — GitHub Wiki. При изменении поведения обновляется текущий статус и связанные страницы; исторические отчёты сохраняются с их ограничениями. Схема репозиториев — [здесь](Repository-Map).
