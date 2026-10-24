# WarsBots: рецепт выгрузки геометрии карты из движка для Recast/Detour

Источник: `Madd3st/crymp-client`, ветка `warsbots/research/movement-params`, база `fb578d52`.
Только чтение кода, игра/серверы не запускались. Подготовка для C-019 (`wb_geom_export`) и решения
D-006 (Recast/Detour navmesh вместо текущего `BotNav`-графа). Все пути/номера строк — от корня репо.

## 1. Перечисление статических коллайдеров

**Точка входа:** `IPhysicalWorld::GetEntitiesInBox(ptmin, ptmax, IPhysicalEntity **&pList, objtypes, szListPrealloc)`
— `Code/CryCommon/CryPhysics/physinterface.h:2228`. `objtypes` — битовая маска `entity_query_flags`
(`physinterface.h:1816-1820`):

| Флаг | Значение | Что это |
| --- | --- | --- |
| `ent_static` | 1 | неподвижная геометрия (стены, здания, пропсы) |
| `ent_sleeping_rigid` | 2 | заснувший rigid (ящики, двери в покое) |
| `ent_rigid` | 4 | активный rigid |
| `ent_living` | 8 | живые акторы (для navmesh **не нужны**) |
| `ent_independent` | 16 | независимые |
| `ent_terrain` | 0x100 | ландшафт (heightfield) |
| `ent_water` | 0x200 | вода |
| `ent_ignore_noncolliding` | 0x10000 | пропустить non-colliding |
| `ent_all` | — | `static|sleeping_rigid|rigid|living|independent|terrain` |

Для navmesh берём: `ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain`
(живых — нет, воду обрабатываем отдельно как «непроходимо/проходимо»). Существующий пример уже в коде:
`BotManager::SpawnBoxCommand` — `BotManager.cpp:702-710` (`GetEntitiesInBox(center-radius, center+radius,
entities, ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid|ent_living)` + `GetStatus(&pe_status_pos)` +
`GetForeignData(PHYS_FOREIGN_ID_ENTITY)` + `GetType()`).

**Сопоставление с IEntity:** `IPhysicalEntity::GetForeignData(PHYS_FOREIGN_ID_ENTITY)` →
`static_cast<IEntity*>(...)` (`BotManager.cpp:707`) — даёт имя/класс/прокси (для фильтра дверей,
breakables, vegetation). `IPhysicalEntity::GetiForeignData()` (`physinterface.h:1750`) — тип foreign-data.

## 2. Части, матрицы, геометрия, треугольники

У сущности может быть несколько частей (parts). Порядок:

1. `pe_status_nparts nparts; int count = pEntity->GetStatus(&nparts);` — возвращает **число частей**
   (`physinterface.h:1039-1042`).
2. Для части `ipart`: `pe_status_pos pose; pose.ipart = ipart; pose.pMtx3x4 = &m; pEntity->GetStatus(&pose);`
   — даёт `pos`, `q`, `scale`, `pGeom`, `pGeomProxy` и **мировую матрицу** `pMtx3x4` (`physinterface.h:933-951`).
3. `pe_params_part pp; pp.ipart = ipart; pEntity->GetParams(&pp);` — даёт `pPhysGeom`, `flagsOR/flagsAND`,
   `flagsColliderOR/AND` (`physinterface.h:229-264`) — отсюда берём коллизионные флаги части.
4. `IGeometry *g = pose.pGeom;` → `int type = g->GetType();` (`physinterface.h:1573`).

**Типы геометрии** (`geomtypes`, `physinterface.h:1560-1562`):

| Тип | Примитив | Данные |
| --- | --- | --- |
| `GEOM_TRIMESH` | `triangle` | `const mesh_data* md = (const mesh_data*)g->GetData();` → `md->pVertices` (`strided_pointer<Vec3>`), `md->pIndices`, `md->nTris/nVertices`, `md->pMats` (`physinterface.h:1544-1556,1602`) |
| `GEOM_HEIGHTFIELD` | `heightfield` | `const heightfield* hf = (const heightfield*)g->GetData();` → `hf->getheight(ix,iy)`, `hf->gettype()`, `typehole`, `step/stepr`, `size` (`primitives.h:48-73`) |
| `GEOM_BOX` | `box` | `const box* b = ...` → `center`, `size`, `Basis`, `bOriented` (`primitives.h:15-21`) |
| `GEOM_CYLINDER` | `cylinder` | `center`, `axis`, `r`, `hh` (`primitives.h:87-92`) |
| `GEOM_CAPSULE` | `capsule` (extends cylinder) | то же (`primitives.h:94-96`) |
| `GEOM_SPHERE` | `sphere` | `center`, `r` (`primitives.h:81-85`) |

Для тримеша треугольники получаются либо через `g->GetData()` → `mesh_data` (вершины+индексы целиком),
либо поштучно через `g->GetPrimitive(i, &triangle)` / `g->GetPrimitiveCount()` (`physinterface.h:1590,1601`).
Мировые координаты вершины = `pose.pMtx3x4 * localVertex` (с учётом `pose.scale`).

**«Игрок сталкивается»** (`geom_flags`, `physinterface.h:1262-1276`): часть блокирует игрока, если её
`flags` (каким типом она является) или `flagsCollider` (с чем сталкивается) содержит
`geom_colltype_player` (=`geom_colltype1`, `0x0002`) или `geom_colltype0` (`0x0001`, «всё по умолчанию»).
Практический фильтр для navmesh: **пропустить** части с `geom_colltype_foliage` (=`geom_colltype4`) и
`geom_proxy` (`0x20000`) — это vegetation/невидимые прокси, не блокирующие игрока; остальное включать.

## 3. Ландшафт (террейн)

Два пути:
- **Физическая сущность террейна** попадает в `GetEntitiesInBox` с флагом `ent_terrain`; её геометрия —
  `GEOM_HEIGHTFIELD`. Через `GetData()` → `primitives::heightfield` доступны `getheight(ix,iy)`,
  `gettype(ix,iy)` (тип поверхности + **дыры** через `typehole`/`typemask`), `step/stepr`, `size`
  (`primitives.h:48-73`). Это самый прямой путь для Recast (регулярная сетка высот).
- **Рендерный API**: `I3DEngine::GetTerrainElevation(x, y, bIncludeOutdoorVoxles)` (`I3DEngine.h:1500`),
  `GetHeightMapUnitSize()` (`:1529`, размер юнита высоты в метрах), `GetTerrainSize()` (`:1539`).
  `ITerrain` (`I3DEngine.h:893`) — для compiled-data/vegetation/ocean, прямого heightmap-запроса не даёт.

Рекомендация: идти через **физический heightfield** (единый источник истины с коллизиями), сверяя
`GetHeightMapUnitSize()` для перевода индексов в метры; дыры (`typehole`) исключать из навигации.

## 4. Скетч функции (~100 строк, markdown)

```cpp
// wb_geom_export  →  <memory>.navgeom.json  (вершины/индексы + маркеры поверхности)
struct Out { std::vector<Vec3> v; std::vector<int> idx; std::vector<unsigned char> surf; };

void ExportLevelGeometry(Out& out, const AABB& bounds)
{
    IPhysicalWorld* pw = gEnv->pPhysicalWorld;
    IPhysicalEntity** list = nullptr;
    int n = pw->GetEntitiesInBox(bounds.min, bounds.max, list,
        ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain | ent_ignore_noncolliding);

    for (int i = 0; i < n; ++i)
    {
        IPhysicalEntity* e = list[i];
        pe_status_nparts np; int parts = e->GetStatus(&np);       // число частей
        IEntity* ent = (IEntity*)e->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

        for (int ip = 0; ip < parts; ++ip)
        {
            Matrix34 m = Matrix34(IDENTITY);
            pe_status_pos pose; pose.ipart = ip; pose.pMtx3x4 = &m;
            if (!e->GetStatus(&pose)) continue;
            pe_params_part pp; pp.ipart = ip; e->GetParams(&pp);
            // фильтр: vegetation/proxy не блокируют игрока
            unsigned flags = pose.flagsOR; // (или pp.flagsOR/AND)
            if ((flags & geom_colltype_foliage) || (flags & geom_proxy)) continue;
            if (!(flags & (geom_colltype0 | geom_colltype_player))) continue;

            IGeometry* g = pose.pGeom; if (!g) continue;
            int base = (int)out.v.size();
            switch (g->GetType())
            {
            case GEOM_TRIMESH: {
                const mesh_data* md = (const mesh_data*)g->GetData();
                for (int k = 0; k < md->nVertices; ++k)
                    out.v.push_back(m.TransformPoint(md->pVertices[k]));
                for (int k = 0; k < md->nTris*3; ++k)
                    out.idx.push_back(base + md->pIndices[k]);
                if (md->pMats) out.surf.insert(out.surf.end(), md->pMats, md->pMats + md->nTris);
                break; }
            case GEOM_HEIGHTFIELD: {
                const primitives::heightfield* hf = (const primitives::heightfield*)g->GetData();
                // сетка hf->size, hf->step, getheight/gettype; пропуск gettype()==typehole
                for (int iy = 0; iy < hf->size.y; ++iy)
                    for (int ix = 0; ix < hf->size.x; ++ix) {
                        if (hf->gettype(ix,iy) == hf->typehole) continue;
                        out.v.push_back(Vec3(ix*hf->step.x, iy*hf->step.y, hf->getheight(ix,iy)));
                    }
                // два треугольника на клетку (пропуская клетки с дырой)
                break; }
            case GEOM_BOX: { const primitives::box* b = (const primitives::box*)g->GetData();
                // 8 углов + 12 индексов (или 24 с нормалями)
                break; }
            case GEOM_CYLINDER: case GEOM_CAPSULE: case GEOM_SPHERE: {
                // аппроксимация примитива как меша (окружность/сфера с фикс. сегментами)
                break; }
            default: break;
            }
        }
    }
}
```

Итог `out` скармливается Recast (`rcCreateHeightfield` + `rcRasterizeTriangles` для тримешей,
`rcMarkConvexPolyArea` не нужен — просто триангуляция), террейн — либо как тримеш из heightfield, либо
напрямую через `rcHeightfield`-заполнение высотами.

## 5. Подводные камни

- **Vegetation**: `geom_colltype_foliage`/`geom_colltype_foliage_proxy` (`physinterface.h:1273-1274`) —
  пропускать, иначе кусты/ветки дадут ложные стены.
- **Breakables/doors**: динамика `ent_rigid` с `geom_manually_breakable` (`0x800000`) — для navmesh
  брать **закрытое** состояние, но помечать как «проходимо после взлома» (Detour `off-mesh`/dynamic).
- **Water**: `ent_water` (`0x200`) — отдельный слой; по умолчанию вода = не-поверхность (или помечается
  как «глубоко, вплавь»). Не растеризовать как пол.
- **Масштаб**: `pose.scale` у террейна/частей не 1.0 — умножать локальные координаты перед `pMtx3x4`.
- **Размер**: тримеши уровня Crysis — миллионы треугольников; экспорт одним `GetData()` целиком может
  упереться в память — стримить по `GetEntitiesInBox`-тайлам (уровень разбивать на боксы по ~64 м).
- **`ent_sleeping_rigid`**: ящики в покое — включать, но они могут проснуться и сдвинуться; для статики
  это редкий шум, для дверей — отдельный динамический слой.
- **`pGeomProxy`**: у некоторых частей есть прокси-геометрия (упрощённая коллизия) — для navmesh брать
  `pGeom` (точную), а не прокси.
- **Не-коллидящие static**: части с `geom_no_coll_response` (`0x1000000`) — пропускать.

## 6. Параметры Recast для игрока Crysis

Из OC-001 (`PLAYER_MOVEMENT.md`) и `BotProfile`/`CPlayer`:

| Параметр Recast | Значение | Источник |
| --- | --- | --- |
| `agentRadius` (капсула) | 0.4 м (r=0.35–0.45; коллизия игрока ~0.4) | `CPlayer` capsule; `pe_player_dimensions` |
| `agentHeight` | 1.8 м (стойка ~1.75–1.9) | стойка игрока |
| `agentMaxClimb` | 0.75 м (`step_max`) | `BotNav.cpp` ground test (step ≤ 0.75) |
| `agentMaxSlope` | ~45° (normalZ ≥ 0.707) | `BotNav` walkable `normalZ > 0.7` |
| `cellSize` | 0.25 м | ~радиус/1.5, для IA-карт достаточно |
| `cellHeight` | 0.25 м | выравнивание |
| `walkableHeight` | 1.9 м | headroom из `BotNav` (1.9 м) |
| `walkableRadius` | 0.4 м | = agentRadius |
| `walkableClimb` | 0.75 м | = agentMaxClimb |
| `walkableSlopeAngle` | 45° | = agentMaxSlope |

Примечание: текущий `BotNav` строится на 3-метровой сетке с допуском шага 0.75 м; Recast с cell 0.25 м
даст заметно более точную геометрию и должен воспроизводить owner-путь по лестницам/крышам
(сейчас теряемым — `WALL_STARE.md`, C-007). Strength-jump-полки (+7 м) навислом не покрываются —
понадобятся off-mesh connections.
