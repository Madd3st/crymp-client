# Команды wb

Консольные команды и переменные сервера (префикс `wb_`). Основные:

| Команда | Что делает |
| --- | --- |
| `wb_add N` | добавить N ботов |
| `wb_remove`, `wb_remove_all` | убрать бота / всех |
| `wb_list`, `wb_status` | список ботов и их состояние |
| `wb_maxbots` | предел ботов на сервере |
| `wb_skill` | уровень навыка (0 новичок … 3 элита) |
| `wb_select`, `wb_watch`, `wb_watch_info` | выбрать бота и следить за ним |
| `wb_bot_start`, `wb_bot_stop`, `wb_bot_status` | включить/выключить «мозг» бота |
| `wb_teleport`, `wb_revive_at` | телепорт / возрождение в точке (для тестов) |

Поведение (переключатели):

| Переменная | Смысл | По умолчанию |
| --- | --- | --- |
| `wb_bot_pace` | темп: бег/спринт/шаг по ситуации | 1 |
| `wb_bot_jump`, `wb_bot_strafe`, `wb_bot_suit`, `wb_bot_gait` | прыжки, стрейф, костюм, походка | см. код |
| `wb_trap_recovery`, `wb_revive_guard`, `wb_spawn_exclude195` | защиты от застревания | 1 |
| `wb_reachable_goals`, `wb_sink_escape` | экспериментальные, выключены | 0 |

Фичи спринта (все по умолчанию 0; в плейтесте владельца включены все):

| Переменная | Что включает |
| --- | --- |
| `wb_bot_look`, `wb_bot_turn_cap` | взгляд по ходу движения, плавный поворот |
| `wb_bot_weapon_rank`, `wb_bot_arbiter` | выбор оружия по дистанции, арбитр решений |
| `wb_bot_perception`, `wb_bot_aim_model` | восприятие и слух, человеческий прицел |
| `wb_bot_suit_policy` | политика костюма |
| `wb_bot_navmesh`, `wb_bot_jump_links` | навигация Detour, прыжки по связям меша |
| `wb_bot_corners` | следование углам пути v2.1: смещение наружу поворота, остановка у точки 0,2 м, лучи по наклону лестницы (D-016) |
| `wb_bot_jump_sanity` | без случайных прыжков в силе в пути и без прыжков на ступени при ходьбе по навмешу (D-016) |
| `wb_geom_export [path.obj] [x0 y0 x1 y1]` | выгрузка геометрии столкновений; для PS — рамка игровой зоны |
| `wb_bot_weapon_handling`, `wb_bot_vantage` | лазер и зум; снайперские точки |
| `wb_bot_name_profile` | имена с уровнем и стилем (для новых ботов) |
| `wb_bot_client_smooth` | **на клиенте**: сглаживание движения ботов |

Данные для навмеша/прыжков/точек ставит `Tools/Install-WarsBotsNavmesh.ps1` в `%LOCALAPPDATA%\WarsBots\Navmesh`.

Диагностика: `wb_debug`, `wb_log`, `wb_move_trace` (журнал позиций), `wb_nav`, `wb_nav_audit`, `wb_nav_explain`,
`wb_nav_export`, `wb_geom_export` (выгрузка геометрии), `wb_physics_diagnostics`, `wb_fire_diagnostics`,
`wb_probe_*` (сценарии-пробы), `wb_qa_time_scale` (ускорение времени — не принято, ломает поведение).

Полный список — поиск `"wb_` в `Code/CryGame/Bots/`.
