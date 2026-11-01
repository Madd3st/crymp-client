#undefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedacceptedundefinedkeep-debugundefineddropundefineddropundefineddropundefineddrop | Ни в одном лаунчере, числится экспериментальным долгом ||||||||||||||||||||||||WarsBots

Сгенерировано из `BotConfig.cpp` на `warsbots/dev` (`dd1ac3fc`), 01.11.2026. Статус проставляет владелец.
Статусы: `accepted` (включить по умолчанию и удалить флаг), `trial` (в приёмке, максимум 5), `drop` (удалить вместе с кодом), `keep-debug` (отладка, остаётся).

| Флаг | Описание (начало) | Статус |
| --- | --- | --- |
| `wb_bot_high_ground` | Owner playtests 3-4 (nobody wants to go to the roofs; he fights from height): in a quiet m… | ? | accepted | Плейтест владельца прошёл, аудит советует включить |
| `wb_bot_pro_fire` | Owner playtest 4 (Elites sometimes stop shooting): backing off at close range and the low-… | ? | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_state_commit` | DS-034 (owner playtest 3: bots twitch): roam writes its state once per tick after the weap… | ? | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_link_failfast` | DS-048: a jump link whose take-off is blocked (railing/step, speed < 0.3 m/s within 2 m or… | ? | accepted | Прерывает заблокированные связи, включён в Steelmill-плейтесте |
| `wb_bot_withdraw_full` | DS-049 (C-121: 72/104 withdraw deaths; 10/25 at-cover deaths were low cover, 52/72 shooter… | ? | drop | Только разбор смертей, в игре не проверялся |
| `wb_bot_cloak_experimental` | Experimental bot cloak, defaults OFF… | ? | drop | Ни в одном лаунчере, числится экспериментальным долгом |
| `wb_bot_pace` | C018 native pace: stance speed,sprint40/20,Speed130/60,Strength70; default ON; set0 for le… | ? | accepted | Решение владельца принято, уже ON, убрать флаг |
| `wb_bot_look` | C-033 look policy: without a visible target look ~3 m ahead along the path; points of inte… | ? | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_turn_cap` | C-044 per-frame view turn-rate cap by skill (beginner 400 .. elite 900 deg/s) instead of 1… | ? | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_weapon_rank` | C-022 weapon value by range: pickups by value/distance, hold the best weapon for the fight… | ? | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_arbiter` | C-032 goal arbiter: fight/hunt/weapon/roam with commitment windows and hysteresis. 0 OFF d… | ? | keep-debug | Нет в свежих лаунчерах, аудит: оставить для диагностики |
| `wb_bot_perception` | C-037 perception: new targets need the 110 deg view cone and time to notice (skill reactio… | ? | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_aim_model` | C-038 aim model: flick (Fitts) + tracking with lead + filtered hand noise by skill, replac… | ? | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_suit_policy` | One nanosuit decision per tick: falling/under fire/fight Armor, Strength jumps with energy… | ? | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_client_smooth` | O-025 CLIENT cvar: remote WarsBot replicas turn in curves - replicated move direction (80 … | ? | accepted | Плейтест владельца прошёл, аудит советует включить |
| `wb_bot_navmesh` | P0 Detour navmesh from %LOCALAPPDATA%/WarsBots/Navmesh/<Level>.navmesh for paths, weapon/r… | ? | accepted | Основа навигации, во всех лаунчерах, аудит: испытание |
| `wb_bot_jump_links` | C-036 navmesh paths may use baked drop / jump / Strength-jump links (needs <Level>.navmesh… | ? | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_ladder` | C154/DS-107: navmesh paths may use a real UP-only NavLinkLadder edge (Shore long-shaft lad… | ? | trial | Shore пройден, другие лестницы не проверены |
| `wb_bot_weapon_handling` | Owner 25.10: LAM laser always on when attached (off while cloaked), iron sight/scope zoom … | ? | accepted | Запрос владельца, в Steelmill и трёх Shore-профилях |
| `wb_bot_name_profile` | Owner 25.10: new bots are named with skill and style, e.g. WarsBot_02 [Normal/Assault] (sp… | ? | keep-debug | Показывает навык и стиль в имени бота |
| `wb_bot_vantage` | Owner 25.10: Marksman/Defender bots holding a DSG1 or Gauss take, hold and rotate baked sn… | ? | trial | Цепочка крыш только смержена, плейтеста вехи нет |
| `wb_bot_tactics` | Fragmovie patterns: a target that broke line of sight is held from a nearby cover spot tha… | ? | accepted | Во всех лаунчерах, аудит держал на испытании |
| `wb_bot_path_progress` | A weapon goal counts as progressing while the remaining navmesh path shrinks (not the stra… | ? | trial | Нужен для снайперов, приёмки сценарием ещё нет |
| `wb_bot_jump_sanity` | Owner playtest 25.10 (364 Strength jumps, 58 on baked links): no random Strength hop while… | ? | accepted | Решение владельца принято, уже ON, убрать флаг |
| `wb_bot_threat_scan` | Owner 25.10 (camped roofs with a sniper rifle, no bot ever looked up): without a visible t… | ? | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_spectator_aim` | DS-013 / owner 25.10 (bot aim late and crooked in first-person spectator): the spectator c… | ? | accepted | Во всех лаунчерах, аудит советует включить |
| `wb_bot_difficulty` | DS-014 / owner 25.10 (difficulties felt the same): one table per skill sets reaction (0.9/… | ? | accepted | Плейтест владельца прошёл, аудит советует включить |
| `wb_bot_finish` | DS-015 / owner 25.10 (left an enemy at 4 HP): a target this bot has visibly damaged (>= 30… | ? | accepted | Плейтест владельца прошёл, аудит советует включить |
| `wb_bot_first_primary` | DS-044 #1 (audit: first primary p50 33.5 s, SOCOM held 35.7 %): on spawn go for the neares… | ? | trial | Данные сильные, но решение владельца ещё открыто |
| `wb_bot_withdraw_cover` | DS-044 #2 (audit: 80 % of withdraws die within 2.5 s): low HP backs off to the nearest cov… | ? | drop | 85 из 189 отмен, 69 % отходов гибнут |
| `wb_bot_finish_lock` | DS-044 #3 (audit: 22.4 % of target choices switch away from <=25 HP): keep a target at <=2… | ? | drop | Дублирует wb_bot_finish, случаев в прогонах не было |
| `wb_bot_sniper_reach` | DS-046 (C-117 X-157: 0/3 DSG1, 0/3 Gauss): a sniper-minded marksman searches the roof with… | ? | trial | Снайперки на крышах, нужен плейтест на dev |
| `wb_bot_corners` | Navmesh path corners v2 (BotCorners.h): each corner is aimed at 0.15 m to the outside of t… | ? | accepted | Решение владельца принято, уже ON, убрать флаг |

Рекомендации Claude от 01.11.2026 опираются на код, лаунчеры и старые отчёты; это не статусы, решает владелец. `drop` и `accepted` по косвенным данным проверить сценарием перед действием.

Всего 33 cvar `wb_bot_*`. По умолчанию включены `wb_bot_pace`, `wb_bot_jump_sanity` и `wb_bot_corners`, остальные 30 выключены.

## Команды консоли `wb_bot_*` (не флаги, статус не нужен)

`wb_bot_start`, `wb_bot_stop`, `wb_bot_status`, `wb_bot_verbose`, `wb_bot_jump`, `wb_bot_gait`, `wb_bot_strafe`, `wb_bot_suit`, `wb_bot_abilities`.

Имена только из тестового примера в `Tests/BotProbeRunnerTests.cpp`, в игре их нет: `wb_bot_goal`, `wb_bot_name`.

## Что лаунчеры включают сверх таблицы

Источник: `Run.ps1` и `preset.json` в `E:WarsBotsVisual*`; в самих `START.cmd` флагов нет. Все `wb_bot_*`, которые там включены, есть в таблице выше (16–24 штуки на лаунчер). Ниже — включённые значения с другими префиксами, в учёте их нет.

| Значение | Что это | Лаунчеры |
| --- | --- | --- |
| `wb_ps_capture`, `wb_ps_buy` | поведение в Power Struggle | все Shore |
| `wb_ps_doors` | поведение в Power Struggle | только `Shore-15-d5bef891` |
| `wb_trap_recovery` | поведение, экспериментальный | все |
| `wb_net_ext` | сеть | все |
| `wb_revive_guard`, `wb_spawn_exclude195` | безопасность, уже ON по умолчанию | Shore кроме `Shore-15-d5bef891` |
| `wb_log`, `wb_move_trace`, `wb_probe_local_test` | диагностика и допуск | все |
| `wb_demo_record`, `wb_demo_hz`, `wb_fire_diagnostics`, `wb_physics_diagnostics`, `wb_ps_events` | диагностика | Shore кроме `Shore-15-d5bef891` |
| `wb_maxbots` | настройка | Shore кроме `Shore-15-d5bef891` |

`wb_add`, `wb_remove`, `wb_select` в лаунчерах — команды, не флаги.
