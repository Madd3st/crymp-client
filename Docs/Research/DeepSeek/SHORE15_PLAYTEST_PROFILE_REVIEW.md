# Shore 15-bot playtest profile — bounded review (`Shore-15-1ae65bb0`)

Профиль: `E:/WarsBots/Visual/Shore-15-1ae65bb0`. Источник `1ae65bb01770f9c7660d57fc2363a6144570482f`
(gemini/vantage-heard-priority, PR #3, база `ed72987c`). Проверка только по файлам и живому CI; игра, сервер,
клиент и код не запускались и не менялись.

## Вердикт

**Одобрено условно** — для одной owner-попытки «Shore 8 vs 7» (playtest, не приёмка маршрута). Условие:
Codex создаёт `visual-quality.json` на голове после этого ревью; до этого гейт (Run.ps1:75-78) откажет.

## 1. Пины и пара (проверено)

- `build.json`: source `1ae65bb0…`, CI `41119885189`, PR `41119972991`, artifact `13008402830`,
  pair client `321405a66df842dfde41c95028431f54e2233516202de2bd57d456372919bc1d`,
  server `10573ba1db96b091f37bafce721061779be0797b7f0c39c4f82f82b5345f5143`,
  scope: «Owner 15-bot Shore playtest, not full PS acceptance».
- Живой API: CI `41119885189` completed/success, head `1ae65bb0`; PR `41119972991` completed/success,
  head `1ae65bb0`; artifact `13008402830` (warsbots-1ae65bb0…, 14 193 699 B, digest `801e43a6…`, не истёк).
- `sha256.txt` в профиле совпадает с парой выше; данные — точный C154 V5
  (`b60178d7/0230a0c3/d95e1e11/a95a0503`).
- `package-manifest.json`: 11 файлов, все хеши пересчитаны и совпадают; `Run.ps1` `ab837e8b…`,
  `preset.json` `4ea52894…`, `build.json` `f4e43265…`, `sha256.txt` `435e20c6…`.

## 2. Пресет и аргументы

- `port=46170`, `retained_bots=15`, `wb_maxbots=15`, `polys=46830`, label `Shore15`, skill `normal`.
- Roster: `+wb_add 15 normal +wb_list +wb_status` — без `wb_remove`/`wb_select`/`wb_bot_stop`/`wb_revive_at`:
  боты стартуют нативно, ручного сетапа и остановки AI нет.
- Аргументы: roster короче, чем в одобренном V2 (1431 байт), поэтому <2048; проверка длины в Run.ps1:73
  сохранена.
- Preflight-фаза — тот же base без roster, требует `bots=0/15`.

## 3. Readiness и команды

- Run.ps1:96: `NAVMESH loaded … 46830 polys`, `links ON`, `link kinds 2/2`, `#15 created EntityId=`,
  `FRAME .*bots=15 server=1`; затем по логу `#N PS initial spawn team=` считается `team1` и `team2`
  и требуется **8 / 7**, иначе throw.
- Проверка источника: `TeamForSlot(slot) = (slot & 1) ? 1 : 2` (`BotTeamPlan.h:9-12`) — для слотов 1..15
  это ровно 8 (нечётные) и 7 (чётные). Строка `#%d PS initial spawn team=%d` пишется один раз на бота в
  `BotManager::AddWith` (`BotManager.cpp:395-399`, только при первом спавне и `spawnGroups`), поэтому счёт
  8/7 не задваивается респавнами.
- `#15 created EntityId=` — лог `BotInstance.cpp:97`, номера 1..15.
- Одиночная проверка «`#2 LADDER start` до клиента» **удалена** (Run.ps1:94) — как заявлено; для обычного
  15-ботового плейтеста это правильно: боты маршрутизируются с самого старта, клиент подключается позже.
- `ShowServerConsole` сохранён; клиент — `-WindowStyle Normal` после готовности сервера.

## 4. Очистка

Семь core-хелперов (`Snapshot-Data`, `Restore-Data`, `Stop-OwnServer`, `Save-Session`, `Save-Json`,
`Game-Processes`, `Quote-Arg`) **побайтово идентичны** отревьюенному `Launch-WarsBotsShowcase.ps1`.
Снимок данных до установки, восстановление только после штатного закрытия клиента (иначе отложено с
`RESTORE.cmd`), архивирование логов/демо с хешами, `RecoverLast` ограничен `Sessions`.

## 5. Условия и границы

1. Codex создаёт `visual-quality.json` с `source=1ae65bb0`, `review_ok=true`,
   `launcher_sha256=ab837e8b…`, `manifest_sha256=f157ccb6…` и `review_head` = голова после этого ревью.
2. Одна owner-попытка; 15 ботов, команды 8/7, normal; порт 46170.
3. Это **playtest**, а не приёмка: маршрут C154, клиентская визуальная корректность выхода, полный PS и
   другие карты этим прогоном не подтверждаются.
4. Штатное закрытие клиента → архив/восстановление.

## 6. Замечания (не блокируют)

- В профиле нет `prepare-only.json` и `visual-quality.json` — первый не обязателен, второй создаётся после
  ревью (см. условие 1).
- Если какой-то бот не получит team default spawn group, `AddWith` вернёт nullptr (`BotManager.cpp:400`),
  не будет `#15 created` → readiness timeout → клиент не запустится (fail closed).
- `build.json` минимальный (source/CI/PR/artifact/pair/scope), без harness/analyzer — для bounded playtest
  допустимо.
