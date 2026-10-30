# DS-136 — QA-ревью видимого dedicated-client профиля Shore `ed72987c`

Профиль: `E:/WarsBots/Visual/Shore-ed72987c` (standalone QA-пакет, не новый проект; frozen Showcases не
трогались). Игровой код не менялся: `ed72987c0c9fecb0c14e1cafdd3cc4162e7a13dd`, CI `41086406959`,
PR `41086417565`, артефакт `12992223176`.

Уровень доказательств: чтение лаунчера, Lua-скриптов, пресета, манифеста и данных, сверка хешей, проверка
API по исходникам SDK. Игра, сервер, клиент, сборка, живые данные и quality не запускались и не менялись.

## Вердикт

**OK условно.** QA-профиль корректен и безопасен: точные пины, read-only preflight с fail-closed для
непроверенного `#Lua`-префикса, отсутствие AI до зрителя, ограниченный reveal с ровно одним
select/revive/ChangeSpectatorMode и одним стартом после перепроверки, очистка/восстановление идентичны уже
отревьюенному харнессу. Условия допуска: `visual-quality.json` с головой ревью DS136 и хешами
launcher/reveal/manifest; ровно одна видимая попытка; вход зрителем (spectator).

## 1. Пины и хеши (пересчитаны)

| Объект | SHA256 |
| --- | --- |
| `CryMP-Client64.exe` | `12870db9117d63054068ad18289f901801d7dae2c87db44366595fe698570bcf` |
| `CryMP-Server64.exe` | `05ecb35c3eee779aa1149c2fed492e69108461807b273722433607d650e24234` |
| `Data/Shore.navmesh` | `b60178d7090fd8403b4ba0f4407d6a722c0057afd559a0024148ef0645930609` |
| `Data/Shore.links.txt` | `0230a0c3e59bb818888684012291ee39efaddb542917ac0bc5e02c8e13d32dcb` |
| `Data/Shore.spots.txt` | `d95e1e11fa06194e44e1b1d3887fb00334d7f0ccc9528c9a1b439d7ba78b548b` |
| `Data/Shore.manifest.json` | `a95a0503813e394161bf961f6556e26db672b46041a03079baac99d8191b13e2` |
| `Run.ps1` | `ace7b4aebd9ef6722654e97eec3bb1d0a141a41009c1d0dd73380ae85b840b73` |
| `reveal.lua` | `8ee5bec5d5b3802847e68307376979cc12693ff4d1b4a67de1fcc19ddaa2d59a` |
| `preflight.lua` | `23582e551a97868413374595f8987ead231cdcf0cea015b205035f3f6ec0df3c` |
| `preset.json` | `7d7b56a26a96d1f15e30ac6eb6a5df1f70d887af47d97dfb08c057af6571f62f` |
| `package-manifest.json` | `2e595b9f357adc9f57be09e3e86c48ee9f524450af7cbcbba6f9638035af614f` |
| `build.json` | `3cbd7d215c0675b995e161ce679c57b92e6f434e61399ff62442c49299433f89` |
| `prepare-only.json` | `8b6f632803a34a8ab5dbd7e4b4d0291c7b15ae2503b8ade59fafda0480b0a58f` |
| `sha256.txt` / `game-dir.txt` | `19f8e05d…` / `8341ca9f…` |

Пара совпадает с `sha256.txt`, quality DS135 и артефактом `12992223176` (digest `1c16a1f3…`). Данные —
**точный C154 V5**, не старый preset-nav. Пресет: `polys=46830`, `port=46168`, `retained_bots=1`,
`wb_bot_ladder=1`, `wb_maxbots=2`, `wb_probe_local_test=1`, `wb_log=2`.

## 2. Лаунчер `Run.ps1`

- **Хелперы**: `Snapshot-Data`, `Restore-Data`, `Stop-OwnServer`, `Save-Session`, `Save-Json`,
  `Game-Processes`, `Quote-Arg` — **побайтово идентичны** уже отревьюенному
  `Tools/Launch-WarsBotsShowcase.ps1` (7/7). Ядро снимка/восстановления не менялось.
- **Целостность**: манифест проверяется по хешам, пути без `..`/абсолютных (строки 53-54).
- **Пины**: `source==ed72987c`, `ci==41086406959`, `ci_success` (57); порт/карта из пресета; аргументы
  всех трёх процессов проверяются на `<2048` байт (73).
- **PrepareOnly** (74) выходит до проверки `visual-quality.json` и не запускает ничего; `prepare-only.json`
  подтверждает `package_hashes=PASS`, `runtime=NOT LAUNCHED`, аргументы в пределах буфера.
- **Гейт видимого профиля** (75-78): требует `visual-quality.json` с точным `source`, `review_ok`,
  совпадением хешей launcher/reveal/manifest и **равенством `review_head` текущей голове
  `origin/warsbots/research/deepseek`**. Сейчас файла ещё нет — это ожидаемо, его создаёт Codex после
  одобрения; после публикации этого отчёта голова сменится, поэтому профиль обязан быть перепинен.
- **Безопасность**: отказ при активных процессах и занятом порту; `+sv_bind 127.0.0.1 +sv_lanonly 1`;
  никакого devmode/security/anticheat-обхода; сервер `-headless`, клиент — обычное видимое окно `-dx9`.
- **Preflight-фаза** (110): сервер запускается с `+wb_list +wb_status +#Script.ReloadScript('<abs>/preflight.lua')`,
  готовность требует `RULES INFO class=PowerStruggle`, `gate=1 levelReady=1`, `bots=0/2` и **позитивного
  маркера `C154_VISUAL_LUA_PREFLIGHT`**. Если `#`-префикс в CE2007 не поддерживается — маркера нет →
  таймаут → throw → `finally` восстановление, клиент **не запускается**. Это и есть fail-closed для
  непроверенного префикса.
- **Реальная фаза**: роспись `+wb_add 2 normal +wb_remove 1 +wb_select 2 +wb_bot_stop` + reveal.lua;
  готовность требует `C154_VISUAL_BOOT`, `NAVMESH loaded … 46830 polys`, `links ON`, `link kinds 2/2`,
  `#2 created`, `#1 ENTITY_EVENT_DONE`, `FRAME … bots=1 server=1`, и **отвергает любой `#2 LADDER start`
  до подключения клиента** (94). То есть раннего маршрута/бана ссылки нет.
- **Клиент** запускается только после готовности сервера и при отсутствии чужих процессов (112-114).
- **Ожидание/очистка** (116, `finally`): скрипт ждёт штатного закрытия клиента, не завершает игру
  принудительно; восстановление — только когда клиент закрыт, иначе отложено с `RESTORE.cmd`; архивируются
  оба лога и демо с проверкой хешей; `RecoverLast` ограничен каталогом `Sessions` и сверяет receipt.

## 3. `reveal.lua` и `preflight.lua`

- `preflight.lua` — одна строка `System.Log("C154_VISUAL_LUA_PREFLIGHT")`, других эффектов нет.
- `reveal.lua` при загрузке логирует `C154_VISUAL_BOOT`; затем не более **180** проверок раз в секунду.
- Условие: ровно **один** участник с `channel>0`, `IsPlayerInGame(id)` и `GetSpectatorMode()>0`, и ровно
  **один** с `channel==0` и `not IsPlayer()`. Только тогда — один `wb_select 2`, один `wb_revive_at 545 2`
  и один нативный `ChangeSpectatorMode(viewer, 3, bot)`.
- Через 15 с перепроверка `IsPlayerInGame(viewer)` и `GetSpectatorMode()==3`; только тогда один
  `wb_select 2` + один `wb_bot_start`; иначе `C154_VISUAL_CANCELLED`. Таймеры используются только для
  опроса и отложенного старта — не для успеха/quit/позиции/импульса.

## 4. API — проверено по исходникам

- `game:GetPlayers()` возвращает **script-таблицы сущностей** (`ScriptBind_GameRules.cpp:504-550`), у них есть
  `.id` (используется стоковым Lua, `InstantAction.lua:521`) и `.actor` (используется проектом,
  `Pak/Scripts/WarsBots/InstantAction.lua:14`).
- `actor:GetChannel` (`ScriptBind_Actor.cpp:65,403`), `actor:IsPlayer` (66), `actor:GetSpectatorMode`
  (117,1048) — зарегистрированы.
- `game:IsPlayerInGame` (`ScriptBind_GameRules.cpp:106,553`) и `game:ChangeSpectatorMode(playerId, mode, targetId)`
  (140,1079) — зарегистрированы; C++ вызывает Lua `OnChangeSpectatorMode`; для `mode==3` с целью
  `PowerStruggle/InstantAction` делает `player.actor:SetSpectatorMode(3, targetId)` (`InstantAction.lua:595-605`).
  Режим 3 = `eASM_Follow` (`Actor.h:710-716`), что совпадает с перепроверкой `GetSpectatorMode()==3`.
- `GetPlayers` в экспериментальной сборке отдаёт участников матча, включая бота с channel 0 — именно это
  нужно для поиска «один бот».

## 5. Условия допуска

1. Codex создаёт `visual-quality.json` с `source=ed72987c`, `review_ok=true`, `review_head` = голова после
   DS136 и хешами launcher/reveal/manifest; иначе гейт (75-78) откажет.
2. Одна видимая попытка; вход — **зрителем** (spectator), иначе reveal истечёт по таймауту и AI не стартует
   (это безопасный исход, но не проверка).
3. Пересборка/новый CI не требуются: игровой код не менялся; достаточно уже проверенных CI/PR/пары.

## 6. Некритичные замечания

- `build.json` содержит `review=161b603a` (DS134) — информационно; гейт использует `visual-quality.json`.
  После DS136 это поле можно обновить, но на безопасность не влияет.
- `wb_debug 0` вместо `1`: нужные readiness-строки идут через `wb_log=2` (`FRAME` — WB_LOG(1),
  `NAVMESH link kinds` — WB_LOG(0)), поэтому подавления нет; при неожиданном отсутствии — таймаут и
  отказ, клиент не стартует.
- Единственная непроверенная часть — поддержка `#`-префикса в CE2007; она изолирована preflight-гейтом.

## 7. Что не доказано

- Что `#Script.ReloadScript` работает в CE2007 (проверит preflight).
- Что видимая картинка выхода/анимации выглядит корректно — это и есть цель попытки.
- Поведение на других картах/лестницах, полный PS — вне области.
