# Флаги WarsBots

Сгенерировано из `BotConfig.cpp` на `warsbots/dev` (`4b882c6f`), 04.11.2026. Статусы проставил владелец 04.11.2026 (D-036–D-038 в [DECISIONS.md](DECISIONS.md)); колонка Default — значение в коде после PR смены defaults (06.11): 20 флагов `accepted` переведены в ON, `wb_bot_state_commit` = 1, три флага D-037 удалены. Выключатели `accepted` удаляются следующим PR.
Статусы: `accepted` (включить по умолчанию и удалить флаг), `trial` (в приёмке, максимум 5), `drop` (удалить вместе с кодом), `keep-debug` (отладка, остаётся).

| Флаг | Описание (начало) | Default | Статус | Рекомендация | Причина |
| --- | --- | --- | --- | --- | --- |
| `wb_bot_high_ground` | Owner playtests 3-4 (nobody wants to go to the roofs; he fights from height): in a quiet m… | ON (D-036) | accepted | accepted | Плейтест владельца прошёл, аудит советует включить |
| `wb_bot_pro_fire` | Owner playtest 4 (Elites sometimes stop shooting): backing off at close range and the low-… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_state_commit` | DS-034 (owner playtest 3: bots twitch): roam writes its state once per tick after the weap… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_link_failfast` | DS-048: a jump link whose take-off is blocked (railing/step, speed < 0.3 m/s within 2 m or… | ON (D-036) | accepted | accepted | Прерывает заблокированные связи, включён в Steelmill-плейтесте |
| `wb_bot_withdraw_full` | DS-049 (C-121: 72/104 withdraw deaths; 10/25 at-cover deaths were low cover, 52/72 shooter… | удалён (D-037) | drop | drop | Только разбор смертей, в игре не проверялся |
| `wb_bot_cloak_experimental` | Experimental bot cloak, defaults OFF | удалён (D-037) | drop (только флаг) | drop | Ни в одном лаунчере; удалять флаг, код Elite оставить |
| `wb_bot_pace` | C018 native pace: stance speed,sprint40/20,Speed130/60,Strength70; default ON; set0 for le… | ON | accepted | accepted | Решение владельца принято, уже ON, убрать флаг |
| `wb_bot_look` | C-033 look policy: without a visible target look ~3 m ahead along the path; points of inte… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_turn_cap` | C-044 per-frame view turn-rate cap by skill (beginner 400 .. elite 900 deg/s) instead of 1… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_weapon_rank` | C-022 weapon value by range: pickups by value/distance, hold the best weapon for the fight… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_arbiter` | C-032 goal arbiter: fight/hunt/weapon/roam with commitment windows and hysteresis. 0 OFF d… | OFF | keep-debug | keep-debug | Включён в пресетах Shore, аудит: оставить для диагностики |
| `wb_bot_perception` | C-037 perception: new targets need the 110 deg view cone and time to notice (skill reactio… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_aim_model` | C-038 aim model: flick (Fitts) + tracking with lead + filtered hand noise by skill, replac… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_suit_policy` | One nanosuit decision per tick: falling/under fire/fight Armor, Strength jumps with energy… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_client_smooth` | O-025 CLIENT cvar: remote WarsBot replicas turn in curves - replicated move direction (80 … | ON (D-036) | accepted | accepted | Плейтест владельца прошёл, аудит советует включить |
| `wb_bot_navmesh` | P0 Detour navmesh from %LOCALAPPDATA%/WarsBots/Navmesh/<Level>.navmesh for paths, weapon/r… | ON (D-036) | accepted | accepted | Основа навигации, во всех лаунчерах, аудит: испытание |
| `wb_bot_jump_links` | C-036 navmesh paths may use baked drop / jump / Strength-jump links (needs <Level>.navmesh… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_ladder` | C154/DS-107: navmesh paths may use a real UP-only NavLinkLadder edge (Shore long-shaft lad… | OFF | trial | trial | Shore пройден, другие лестницы не проверены |
| `wb_bot_weapon_handling` | Owner 25.10: LAM laser always on when attached (off while cloaked), iron sight/scope zoom … | ON (D-036) | accepted | accepted | Запрос владельца, в Steelmill и трёх Shore-профилях |
| `wb_bot_name_profile` | Owner 25.10: new bots are named with skill and style, e.g. WarsBot_02 [Normal/Assault] (sp… | OFF | keep-debug | keep-debug | Показывает навык и стиль в имени бота |
| `wb_bot_vantage` | Owner 25.10: Marksman/Defender bots holding a DSG1 or Gauss take, hold and rotate baked sn… | OFF | trial | trial | Цепочка крыш только смержена, плейтеста вехи нет |
| `wb_bot_tactics` | Fragmovie patterns: a target that broke line of sight is held from a nearby cover spot tha… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_path_progress` | A weapon goal counts as progressing while the remaining navmesh path shrinks (not the stra… | OFF | trial | trial | Нужен для снайперов, приёмки сценарием ещё нет |
| `wb_bot_jump_sanity` | Owner playtest 25.10 (364 Strength jumps, 58 on baked links): no random Strength hop while… | ON | accepted | accepted | Решение владельца принято, уже ON, убрать флаг |
| `wb_bot_threat_scan` | Owner 25.10 (camped roofs with a sniper rifle, no bot ever looked up): without a visible t… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_spectator_aim` | DS-013 / owner 25.10 (bot aim late and crooked in first-person spectator): the spectator c… | ON (D-036) | accepted | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_difficulty` | DS-014 / owner 25.10 (difficulties felt the same): one table per skill sets reaction (0.9/… | ON (D-036) | accepted | accepted | Плейтест владельца прошёл, аудит советует включить |
| `wb_bot_finish` | DS-015 / owner 25.10 (left an enemy at 4 HP): a target this bot has visibly damaged (>= 30… | ON (D-036) | accepted | accepted | Плейтест владельца прошёл, аудит советует включить |
| `wb_bot_first_primary` | DS-044 #1 (audit: first primary p50 33.5 s, SOCOM held 35.7 %): on spawn go for the neares… | ON (D-036) | accepted | trial | 01.11: данные сильные, решение было открыто; решено D-036 (accepted) |
| `wb_bot_withdraw_cover` | DS-044 #2 (audit: 80 % of withdraws die within 2.5 s): low HP backs off to the nearest cov… | OFF | keep-debug | keep-debug | Включён в Shore, данные смешанные, пользы не доказано |
| `wb_bot_finish_lock` | DS-044 #3 (audit: 22.4 % of target choices switch away from <=25 HP): keep a target at <=2… | удалён (D-037) | drop | drop | Дублирует wb_bot_finish, случаев в прогонах не было |
| `wb_bot_sniper_reach` | DS-046 (C-117 X-157: 0/3 DSG1, 0/3 Gauss): a sniper-minded marksman searches the roof with… | OFF | trial | trial | Снайперки на крышах, нужен плейтест на dev |
| `wb_bot_corners` | Navmesh path corners v2 (BotCorners.h): each corner is aimed at 0.15 m to the outside of t… | ON | accepted | accepted | Решение владельца принято, уже ON, убрать флаг |

Рекомендации Claude от 01.11.2026 сохранены для истории. Владелец принял их все и добавил `wb_bot_first_primary` в `accepted`. С 01.11 набор `wb_bot_*` не менялся; у `wb_bot_pro_fire` и `wb_bot_tactics` дополнено описание (PR #16).

Всего 33 cvar `wb_bot_*`: 23 `accepted` (из них `wb_bot_pace`, `wb_bot_jump_sanity` и `wb_bot_corners` уже ON, остальные 20 переводятся в ON отдельным PR, выключатели удаляются следующим), 4 `trial`, 3 `keep-debug`, 2 `drop` с кодом, у `wb_bot_cloak_experimental` удаляется только флаг.

## Команды консоли `wb_bot_*` (не флаги, статус не нужен)

`wb_bot_start`, `wb_bot_stop`, `wb_bot_status`, `wb_bot_verbose`, `wb_bot_jump`, `wb_bot_gait`, `wb_bot_strafe`, `wb_bot_suit`, `wb_bot_abilities`.

Имена только из тестового примера в `Tests/BotProbeRunnerTests.cpp`, в игре их нет: `wb_bot_goal`, `wb_bot_name`.

## Вне учёта

Cvar из того же `BotConfig.cpp` без префикса `wb_bot_`: в таблицу выше и в лимит `trial` они не входят, но часть из них меняет поведение ботов. Источник по лаунчерам: `Run.ps1` и `preset.json` в `E:/WarsBots/Visual/*` (10 лаунчеров, 04.11). Рекомендации Claude, решает владелец.

### Поведение (лаунчеры включают)

| Флаг | Default | Лаунчеры | Рекомендация | Причина |
| --- | --- | --- | --- | --- |
| `wb_ps_capture` | OFF | все Shore (7) | trial | Основа Power Struggle. 04.11 вся команда идёт к одной точке одним маршрутом и застревает на склонах Shore; принимать после фикса навмеша |
| `wb_ps_buy` | OFF | все Shore (7) | trial | Покупка FY71 от 125 PP (D-032) в игре отдельно не подтверждена |
| `wb_ps_doors` | OFF | все Shore (7) | trial | После PR #15 сервер Shore чистый (ворота 930 закрыты, двери 104/107 открываются); плейтеста нет |
| `wb_trap_recovery` | OFF | все (10) | trial, не принимать | 04.11: 0 срабатываний при застреваниях до 96 с — порог 0,5 м за 8 с сбрасывается колебаниями 1–2 м. Нужен фикс порога |
| `wb_net_ext` | -1 (авто) | все (10), значение 1 | keep | Совместимость сети, не поведение. Лаунчеры ставят 1 (принудительно); авто достаточно для LAN |
| `wb_revive_guard` | ON | 3 Shore | keep | Защита возрождения, уже ON по умолчанию; в лаунчере лишний |
| `wb_spawn_exclude195` | ON | 3 Shore | keep | Защита спавна, уже ON по умолчанию; в лаунчере лишний |

### Лаунчеры явно держат выключенными

| Флаг | Лаунчеры | Рекомендация | Причина |
| --- | --- | --- | --- |
| `wb_ps_vehicles` | 6, значение 0 | keep OFF | Только посадка в машину на 3 с, без вождения. Переделать по спеке техники в PS |
| `wb_reachable_goals` | 3, значение 0 | trial-кандидат | 04.11 боты выбирали цели в недостижимых частях навмеша (`no_route`); фильтр это отсекает. Сначала серверный прогон |
| `wb_hotspot_all_nodes` | 3, значение 0 | keep-debug | Полный перебор узлов; данных о пользе нет |

### Диагностика, допуск и настройки (статус не нужен)

| Флаг | Default | Лаунчеры | Назначение |
| --- | --- | --- | --- |
| `wb_probe_local_test` | OFF | все | Допуск WarsBots (127.0.0.1, LAN, IA) |
| `wb_log` | 1 | все | Уровень лога |
| `wb_maxbots` | 6 | все | Число ботов |
| `wb_move_trace` | OFF | все | Трасса позиций |
| `wb_demo_record`, `wb_demo_hz` | OFF, 20 | 3 Shore | Запись демо |
| `wb_fire_diagnostics`, `wb_physics_diagnostics`, `wb_ps_events` | OFF | 3 Shore | Диагностика |
| `wb_probe_runner`, `wb_qa_socom_only`, `wb_move_trace_hz`, `wb_skill` | OFF, OFF, 4, 1 | нет | Сценарии и QA |

`wb_add`, `wb_remove`, `wb_select`, `wb_list`, `wb_status`, `wb_watch`, `wb_mark`, `wb_debug`, `wb_rules_info`, `wb_revive_at` в лаунчерах — команды, не флаги.
