# DS-136 supplement — QA-профиль V2 `Shore-ed72987c-v2` (без Lua/таймеров)

Профиль: `E:/WarsBots/Visual/Shore-ed72987c-v2` (отдельный frozen-профиль; старый
`Shore-ed72987c` сохранён). Игровой код и бинарники не менялись: `ed72987c…`, CI `41086406959`,
PR `41086417565`, артефакт `12992223176`. DS135 (серверный whole-route PASS) остаётся в силе.

## Вердикт

**OK условно.** V2 корректно убирает единственную непроверенную часть V1 (Lua-префикс) и остаётся
fail-closed. Допуск к одной видимой попытке — при двух условиях: (1) `visual-quality.json` с головой
ревью после этого supplement; (2) инвентаризация UI показывает **безопасно адресуемый** интерфейс, а не
голый OS-терминал, и в игре реально наблюдается зритель до ручного `wb_revive_at 545 2`. Если UI не
подходит — попытки нет, никакого обхода и «визуального PASS» быть не может.

## 1. История провала V1 (сохраняется, но неполно)

- Сессия `E:/WarsBots/Visual/Shore-ed72987c/Sessions/C154-Visual-20261030-194627`: `phase=archived_restored`,
  `client_pid=null` — клиент не запускался, сервер остановлен, данные восстановлены. Позитивный маркер
  `C154_VISUAL_LUA_PREFLIGHT` не появился → гейт сработал как задумано (fail-closed).
- **Замечание:** в сессии остались только `session.json`, `preflight-arguments.txt`,
  `server-arguments.txt`, `client-arguments.txt` и restore-квитанции. Сам preflight-лог
  (`C154-Visual-20261030-194627-pre.log`) в сессию не попал и в `LOCALAPPDATA/WarsBots/Logs` уже
  отсутствует. Причина: `Start-CheckedServer` бросает исключение до копирования pre-лога (строка 110
  V1), а `Restore-Data` архивирует только `server_log`/`client_log`. Доказательство «маркера нет»
  сохранилось лишь косвенно (аргументы + отсутствие клиента). Рекомендация: в лаунчере копировать
  pre-лог в сессию и в ветке ошибки, иначе такие провалы теряют первичное доказательство.

## 2. V2: что проверено (хеши пересчитаны)

| Объект | SHA256 |
| --- | --- |
| `CryMP-Client64.exe` | `12870db9117d63054068ad18289f901801d7dae2c87db44366595fe698570bcf` |
| `CryMP-Server64.exe` | `05ecb35c3eee779aa1149c2fed492e69108461807b273722433607d650e24234` |
| `Data/Shore.navmesh` | `b60178d7090fd8403b4ba0f4407d6a722c0057afd559a0024148ef0645930609` |
| `Data/Shore.links.txt` | `0230a0c3e59bb818888684012291ee39efaddb542917ac0bc5e02c8e13d32dcb` |
| `Data/Shore.spots.txt` | `d95e1e11fa06194e44e1b1d3887fb00334d7f0ccc9528c9a1b439d7ba78b548b` |
| `Data/Shore.manifest.json` | `a95a0503813e394161bf961f6556e26db672b46041a03079baac99d8191b13e2` |
| `Run.ps1` | `999060751a402bc996c1f33c449d3411faee78b6ef832eee82b853ed63578847` |
| `preset.json` | `20291cccc19eec999c4e0e09ed7d0d5cc4068ca26e26d728bc760f03735da55f` |
| `package-manifest.json` | `8fd00090687bb9453343a960a1da641dbf2519f580fec118b5eeb6c2763f9082` |
| `build.json` | `3cbd7d215c0675b995e161ce679c57b92e6f434e61399ff62442c49299433f89` |
| `prepare-only.json` | `22c7ffa9e9eb99e7c987ed854d4b30ee7437e1030aac06924cc7d5feef833a72` |
| `sha256.txt` / `game-dir.txt` | `19f8e05d…` / `8341ca9f…` |

Манифест: 11 файлов, все хеши совпадают (проверено). Пара совпадает с `sha256.txt`, DS135 quality и
артефактом `12992223176`. Данные — точный C154 V5. Пресет: `port=46169`, `polys=46830`,
`retained_bots=1`, `wb_bot_ladder=1`, `wb_maxbots=2`, `wb_probe_local_test=1`, `wb_log=2`,
label `C154-VisualV2`. `build.json` — тот же (source/CI/PR/artifact, `review=161b603a` информационно).

## 3. V2 `Run.ps1`: удаление Lua и ручной сценарий

- В файле **ноль** вхождений `#Script`, `ReloadScript`, `SetTimer`, `reveal.lua`, `preflight.lua`,
  `-headless`, `devmode`, `sv_cheats`. `wb_revive_at`/`wb_bot_start` встречаются только в тексте подсказки
  оператору (строка 115), не как исполняемые команды.
- Сервер запускается **без** `-headless` (нативная консоль), `Start-Process` по-прежнему
  `-WindowStyle Hidden`; клиент — `-WindowStyle Normal` и только после готовности сервера.
- AI не стартует: roster `+wb_add 2 normal +wb_remove 1 +wb_select 2 +wb_bot_stop +wb_list +wb_status`;
  готовность **отвергает** любой `#2 LADDER start` до подключения клиента (строка 94).
- Preflight-фаза (без ботов): `RULES INFO class=PowerStruggle`, `gate=1 levelReady=1`, `bots=0/2`;
  никаких Lua-маркеров больше нет.
- Реальная фаза: `NAVMESH loaded … 46830 polys`, `links ON`, `link kinds 2/2`, `#2 created`,
  `#1 ENTITY_EVENT_DONE`, `FRAME … bots=1 server=1`.
- Семь core-хелперов (`Snapshot-Data`, `Restore-Data`, `Stop-OwnServer`, `Save-Session`, `Save-Json`,
  `Game-Processes`, `Quote-Arg`) **побайтово идентичны** отревьюенному `Launch-WarsBotsShowcase.ps1`.
- `PrepareOnly` — до гейта, ничего не запускает; аргументы 1379/1431/349 байт (все <2048), без
  `#Script`/`-headless`.
- Гейт видимого профиля: `visual-quality.json` с `source`, `review_ok`, `launcher_sha256`,
  `manifest_sha256` и `review_head` == текущей голове `origin/warsbots/research/deepseek`. Сейчас файла
  нет — его создаёт Codex после этого supplement; после публикации голова сменится, поэтому профиль
  обязан быть перепинен.
- Очистка: снимок до установки данных, восстановление только после штатного закрытия клиента (иначе
  отложено с `RESTORE.cmd`), логи/демо архивируются с проверкой хешей, `RecoverLast` ограничен
  `Sessions`. Никаких `devmode`/security/anticheat-обходов; `+sv_bind 127.0.0.1 +sv_lanonly 1`.

## 4. Условия попытки (как в задаче, подтверждаю)

1. `visual-quality.json` создан на голове после DS136-supplement (иначе гейт 75-78 откажет).
2. **Инвентаризация UI**: если серверный UI — обычный OS-терминал (Windows Terminal / cmd / conhost),
   Computer Use его **не автоматизирует**; попытки нет. Если есть нормальный game-specific UI — только
   тогда ручные команды.
3. Сначала в игре реально наблюдается зритель (channel>0, in-game spectator), затем **один**
   `wb_revive_at 545 2` в нативной серверной консоли; затем клиентский `wb_watch 2`/F6; затем **один**
   `wb_bot_start`. Никаких таймеров, сил, телепортов, новых CVAR.
4. Клиент закрывается штатно; после этого — архив и точное восстановление. Одна попытка.

## 5. Некритичные замечания

- `-WindowStyle Hidden` при старте сервера может скрыть нативную консоль; если UI окажется недоступен —
  это и есть срабатывание условия 2 (попытки нет). Проверяется инвентаризацией, а не обходом.
- `build.json` по-прежнему содержит `review=161b603a` (информационно; гейт использует
  `visual-quality.json`).
- Preflight-лог V1 не сохранён в сессии — см. §1.

## 6. Что не доказано

- Что нативная консоль/UI окажется безопасно адресуемым.
- Что визуальная картинка выхода/анимации корректна — это и есть цель попытки.
- Другие карты/лестницы, полный PS — вне области.
