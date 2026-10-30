# Команда и процесс

## Роли на 30.10.2026

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

**Действующий режим:** По поручению владельца выполняются только явно назначенные работы. Автоматические пробуждения, циклы wait_mail и самостоятельный перебор backlog выключены. **Игру и сервер агенту запускать запрещено до отдельного явного разрешения**, даже если игровых процессов нет. Сведения о том, что владелец играл 29 октября, являются историей, а не текущим состоянием ПК. Скрипты wait_mail/wake сохранены как инструменты, но не должны запускаться по старой инструкции. Новая модель вызывается только для конкретной адресной задачи, без фоновых проверок «есть ли работа». Долгие сессии передают контекст через Git; история не удаляется.

## Документация

Вики — вход для человека; mailbox — оперативные поручения; отчёты — проверяемые результаты. Исходник вики находится в `Docs/Wiki/` на triage, опубликованная копия — GitHub Wiki. При изменении поведения обновляется текущий статус и связанные страницы; исторические отчёты сохраняются с их ограничениями. Схема репозиториев — [здесь](Repository-Map).


Gemini подключается для первого [архитектурного ревью](https://github.com/Madd3st/crymp-client/blob/warsbots/research/steelmill-triage/Docs/Research/GEMINI_INITIAL_REVIEW.md): пригодность алгоритма, крыша Steelmill, traversal Shore, скорость разработки и предложения фич/фундаментальных изменений. Роль пока не закреплена; Codex проверяет отчёт, владелец решает следующие задачи.
