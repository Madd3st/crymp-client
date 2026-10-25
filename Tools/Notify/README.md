# WarsBots → Telegram

Пишет владельцу в Telegram, когда:
- ✅ агент закончил задачу (DONE) / 🔴 упёрся (BLOCKED, REJECTED);
- 🟡 агент в статусе «работаю» (STARTED/PROGRESS), но не пушил дольше 45 минут;
- 💤 агент свободен (в его строке NOW написано idle);
- ❓ в `DECISIONS.md` появилось решение, которое ждёт владельца.

Нагрузка — один `git fetch` трёх веток раз в 10 минут. Подходит для слабого сервера рядом с VPN.

## Установка (5 минут)

1. **Бот.** В Telegram напиши [@BotFather](https://t.me/BotFather) → `/newbot` → имя → получишь токен вида
   `123456:ABC...`. Напиши своему новому боту `/start`.
2. **Chat id.** Открой в браузере `https://api.telegram.org/bot<ТОКЕН>/getUpdates` — в ответе найди
   `"chat":{"id":123456789` — это твой id.
3. **Сервер** (нужны `git` и `python3`, пакеты ставить не надо):
   ```bash
   sudo mkdir -p /opt/warsbots-notify && cd /opt/warsbots-notify
   curl -fsSLO https://raw.githubusercontent.com/Madd3st/crymp-client/warsbots/research/steelmill-triage/Tools/Notify/notify.py
   printf 'TG_TOKEN=ТОКЕН\nTG_CHAT=ТВОЙ_ID\n' > notify.env && chmod 600 notify.env
   python3 notify.py          # первый запуск: пришлёт «уведомления подключены»
   ( crontab -l 2>/dev/null; echo '*/10 * * * * cd /opt/warsbots-notify && /usr/bin/python3 notify.py >> notify.log 2>&1' ) | crontab -
   ```
   Если репозиторий приватный — добавь в `notify.env` строку
   `REPO_URL=https://<read-only-token>@github.com/Madd3st/crymp-client.git` (fine-grained token, только чтение
   Contents этого репо).

Токен лежит только в `notify.env` на твоём сервере, в репозиторий не попадает.

## Настройки (`notify.env`)

| Ключ | По умолчанию | Что делает |
| --- | --- | --- |
| `SILENT_MIN` | 45 | через сколько минут без пуша считать агента «молчащим» |
| `REPO_URL` | публичный GitHub | адрес репозитория |

Выключить: `crontab -e` и удалить строку с `notify.py`.
