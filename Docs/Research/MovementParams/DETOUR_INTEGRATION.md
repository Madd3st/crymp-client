# WarsBots: дизайн интеграции Recast/Detour (D-006)

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`.
Markdown-дизайн, код не меняется. Цель — заменить самописный `BotNav`-граф (3 м сетка) на Detour navmesh,
собранный из геометрии карты (рецепт — `ENGINE_GEOMETRY_EXPORT.md`, C-019 `wb_geom_export`).

## 1. Вендоринг recastnavigation (MIT)

- Кладём исходники под **`Code/ThirdParty/recastnavigation/`** (репо уже держит сторонние либы в
  `Code/ThirdParty`, по конвенции). Нужны модули: **Recast** (соло-меши), **Detour** (навигация),
  **DetourCrowd** (опционально), **DetourTileCache** (опционально).
- CMake: отдельная статическая цель `Recast`/`Detour` (или одна `recastnavigation`), сборка **MSVC x64
  Release**, `C++17` (как остальной CryMP). Подключаем `target_link_libraries(crygame PRIVATE Detour Recast)`
  в `Code/CryGame/CMakeLists.txt`; заголовки — `target_include_directories`.
- `Recast` (генерация) нужен только оффлайн/в `wb_geom_export`; в рантайм-сборку можно не тащить (см. §2 —
  нависеть запекается оффлайн), тащим только `Detour` + `DetourCrowd`.
- Модификации upstream: **не форкать** — vendored copy + при необходимости узкий патч (версия/опции),
  фиксируем commit в `Code/ThirdParty/recastnavigation/VERSION`.

## 2. Жизненный цикл navmesh

- **Сборка оффлайн** (предпочтительно) или **на загрузке уровня** (fallback для карт без кэша): из
  `wb_geom_export` (OBJ/тримеши + heightfield, см. `ENGINE_GEOMETRY_EXPORT.md`) → `rcCreateHeightfield` +
  `rcRasterizeTriangles` → `rcBuildNavMesh` → `dtNavMesh` → сериализация.
- **Формат/кэш**: бинарная сериализация Detour (`dtNavMesh::saveToBinary` / `initFromBinary` — нативный
  формат), файл `<level>.navmesh.bin`. Путь — рядом с текущей памятью карт:
  `C:\Users\Magic8\AppData\Local\WarsBots\Memory\<level>.navmesh.bin` (там же, где сейчас `map.mem` /
  `default.nav.json`, `Docs/STATUS.md` «C-008»).
- **Версионирование геометрией**: в файл пишем заголовок `{ geometryHash, agentRadius, agentHeight,
  cellSize, cellHeight }`; `geometryHash` — SHA/CRC по экспортированной геометрии (или по `level.pak`),
  вычисляется в `wb_geom_export`. Несовпадение → пересборка. `agentRadius/Height` из Recast-конфига игрока
  (см. `ENGINE_GEOMETRY_EXPORT.md` §6).
- Стриминг: для больших карт — DetourTileCache (`dtTileCache`) и тайлы по ~64 м; для IA-карт Steelmill/
  Quarry/Armada/Outpost достаточно монолитного `dtNavMesh`.

## 3. Runtime API для ботов (замена `BotNav`)

| Сейчас (`BotNav`) | Detour | Где вызывается (file:line) |
| --- | --- | --- |
| `FindPath(feet, goal, path, &trace)` | `dtNavMeshQuery::findPath` (полигоны) + `findStraightPath` (углы) | `BotBrain.cpp:97` (в `Steer`) |
| `Ready()` | `navMesh != null && navQuery != null` | `BotBrain.cpp:90` |
| `Nearest(pos)` (внутри `FindPath`) | `dtNavMeshQuery::findNearestPoly(pos, extents, ...)` | `BotBrain.cpp:97` (косвенно) |
| `NodeAt(origin)` | `findNearestPoly` | `BotWeapons.cpp:85` |
| `Component(node)` / `KnownUnreachable` / `GoalFilterEnabled` | **отпадают** — на нависети всё связно; проверка = «нашёлся полигон» | `BotWeapons.cpp:85,105,113` |
| `PickHotspot(...)` | отдельно (heat-map по полигонам) или сохранить текущий `BotHotspot` | `BotBrain.cpp:394` |
| `ReportStuck(pos, actor)` | пометить полигон временно непроходимым (`dtNavMeshQuery::setPolyCost`/флаг) или detour crowd avoidance | `BotBrain.cpp:423,538,558` |
| `InClosedComponent(p)` (sink escape) | **отпадает** — на нависети нет «закрытых компонент» | `BotBrain.cpp:589` |
| `Steer` (следует `m_path`/`m_pathIndex`) | следовать по `findStraightPath`-углам (тот же механизм: `m_path` углов + индекс) | `BotBrain.cpp:87-118` |
| `Begin/Step/Tick` (ленивая сборка) | заменяется загрузкой `.navmesh.bin` на `OnLoadingComplete` | `BotManager.cpp:413` |
| `ReportDeath/ReportKill` (hotspot-память) | сохранить как есть (навешено на `BotHotspot`, не на граф) | `BotManager.cpp:236,238` |
| `Explain/Export/Status` (диагностика) | `wb_nav_explain` → Detour raycast/`findNearestPoly`; `wb_nav_export` → дамп полигонов | `BotManager.cpp:796,813,671` |

`Steer` (`BotBrain.cpp:87-118`) остаётся точкой интеграции: вместо `nav.FindPath(...)` вызываем
`query.findPath(startPoly, endPoly, start, end, filter, polys, &npolys)` → `findStraightPath(...)` →
заполняем `m_path` углами; логика «advance past reached waypoints» (`:111-117`) и fallback
«straight line + local avoidance» (`:90,109`) сохраняются.

## 4. Off-mesh links для прыжков

- Recast off-mesh connection = `rcBuildOffMeshConnections`/Detour `dtOffMeshConnection` (bidir), радиус
  связи = `agentRadius`. Типы для Crysis-игрока:
  - **Normal jump** (высота 1.0 м, `BotJump::Normal`) — bidir off-mesh с радиусом ~1 м, оба конца в
    пределах `walkableClimb`+jump.
  - **Strength jump** (высота 4.7 м, `energy ≥ 70`, `BotJump::Strength`) — off-mesh с флагом
    «strength»; бот включает его только если `m_blackboard.energy >= 70` и `mode == NANOMODE_STRENGTH`
    (`BotLocomotion.cpp:108` в ef25395e).
  - **Drop** (уступ ≤ ~3 м, Armor поглощает) — односторонний off-mesh `dtOffMeshConnectionBidirectional = false`.
- **Исполнение**: `BotLocomotion::Execute` уже умеет `intent.jump = BotJump::Strength/Normal` →
  `request.SetJump()` (`BotLocomotion.cpp:111-128`). Боту в `Steer` при достижении off-mesh-конца выставлять
  `m_intent.jump` нужного типа; `JumpSpace` (`BotLocomotion.cpp:64-93`) заменяется проверкой link-флага.
- **Как owner MOVE TRACE-прыжки становятся линками**: MOVE TRACE даёт `jump=`, `jumpEdges=`, `takeoffs=`,
  `landEdges=`, `jumpTime/landTime`, `peakZ`, `mode=`, `energy=`. Пары «точка взлёта → точка приземления»
  с `peakZ`≥1 м кластеризуются в off-mesh-кандидаты; `mode=1` (Strength) и `energy` на взлёте помечают тип
  (strength/нормальный); повторяющиеся пары (owner проходит их регулярно) становятся линками. Отдельно
  рёбра «drop» — пары, где `peakZ`≈0 и `displacementZ` < −1 м.

## 5. Что станет мёртвым кодом (для рефактора)

- `Code/CryGame/Bots/BotNav.h/.cpp` — почти целиком: граф-сборка (`Begin/Step`, `Walkable` с knee-лучами,
  `FindPath`, `Nearest`), `Explain`, `Export` (замена — Detour-дамп), `Component`, `InClosedComponent`,
  `KnownUnreachable`, `GoalFilterEnabled`, `NodeAt`.
- `BotKneePolicy.h` — knee-audit (C-011 эксперимент) → выкинуть после миграции.
- `BotReachability.h` — SCC/component-анализ → мёртв.
- `BotGoalProjection.h` — проекция целей (для `KnownUnreachable`) → мёртв (Detour `findNearestPoly`).
- `BotHotspot.h` — оставить (heat-map), но перевесить с node-id на poly-id.
- `BotTrapWatchdog.h` + `BotBrain::RecoverTrap` (`BotBrain.cpp:577-630`) — уровни sink/component/obstacle
  становятся не нужны (на нависети нет синков); оставить только простой «stall → re-path» через Detour.
- cvar'ы: `wb_nav_knee_fix`, `wb_sink_escape`, `wb_reachable_goals`, `wb_hotspot_all_nodes` (заменить),
  `wb_nav_explain`/`wb_nav_export` (переписать на Detour).

## 6. Риски и 3-шаговый роллаут за флагом

Риски: (1) Detour navmesh может **не воспроизвести** owner-путь по лестницам/крышам хуже текущего графа —
нужна сверка с owner-видео (C-007) перед промоушном; (2) сериализация/версионирование геометрии — битая
геометрия даст «провалы» на нависети; (3) off-mesh-линки неправильного радиуса — боты «срезают» через
стены; (4) перфоманс `findPath` на 16 ботах 10 Гц — мерять (Detour быстр, но нужен бенч); (5) dead-code
удаление ломает QA-инструменты (`wb_nav_export`/`explain`, `steelmill_triage.py` graph) — мигрировать
инструменты синхронно.

Роллаут за флагом `wb_detour_nav` (default 0):
1. **Параллельный режим**: `wb_geom_export` + оффлайн Recast → `.navmesh.bin`; `wb_nav_explain/export` на
   Detour; боты ещё на `BotNav`. Сверить геометрию/пути с owner-видео, без ботов.
2. **Теневой режим**: боты идут по `BotNav`, но параллельно считают Detour-путь; логировать расхождения
   (длина/блокировки) в MOVE TRACE. Парный QA 15 ботов 660 с ×2, `summary.json` без регресса.
3. **Переключение**: `wb_detour_nav 1` → `Steer`/`BotWeapons`/`BotManager` ходят через Detour; парный soak;
   затем удаление мёртвого кода (п.5) отдельными коммитами (C-010 R1).
