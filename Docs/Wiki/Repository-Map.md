# Карта репозитория

Репозиторий: `Madd3st/crymp-client` (форк клиента CryMP). Всё, что касается ботов, — в этих местах:

| Путь | Что там |
| --- | --- |
| `Code/CryGame/Bots/` | код ботов (C++) — см. [Как устроен бот](Bot-Architecture) |
| `Code/CryGame/Actors/Player/` | игрок движка (движение, прыжки, лестницы) — бот использует его как есть |
| `Tools/*.ps1` | скрипты Windows: сборка (`Build-WarsBots.ps1`), тестовые прогоны (`Test-WarsBots*.ps1`), запуск игры с записью (`Play-WarsBotsRecording.ps1`), выгрузка геометрии |
| `Tools/SteelmillTriage/` | **Triage** — набор Python-инструментов анализа логов и демок (`steelmill_triage.py`) |
| `Tools/RecastBake/` | офлайн-запекание навигационной сетки Recast (C++), см. [Навигация](Navigation) |
| `Tools/Mailbox/` | скрипты почты между агентами (`wait_mail.ps1`) |
| `Docs/STATUS.md` | статус от Codex (игровой код) |
| `Docs/Research/STATUS.md` | общий статус проекта: цели, кто что делает, ошибки, тупики, риски, очередь |
| `Docs/Research/Coordination/MAILBOX/` | почта: задачи агентам и решения владельца |
| `Docs/Research/SteelmillTriage/` | вердикты и доказательства (картинки, JSON) |
| `Docs/Experiments/` | отчёты о тестовых прогонах |

## Ветки

| Ветка | Кто | Что |
| --- | --- | --- |
| `warsbots/feature/weapon-goals` | Codex | **рабочий игровой код** — то, что запускается в игре |
| `warsbots/research/steelmill-triage` | Claude | статусы, почта, вердикты, Triage, RecastBake |
| `warsbots/research/claude-modules` | Claude | новые модули логики с тестами (без движка) |
| `warsbots/research/movement-params` | OpenCode | исследования кода движка, ревью |

Смотреть историю ветки: `https://github.com/Madd3st/crymp-client/commits/<ветка>`.

## Данные на ПК владельца

- Логи: `%LOCALAPPDATA%\WarsBots\Logs` (у Codex — в песочнице приложения: `...\Packages\OpenAI.Codex_*\LocalCache\Local\WarsBots\`).
- Память карт и геометрия: соседние папки `Memory`, `Geometry`.
- Демки: `...\WarsBots\Demos\*.wbdemo`.
