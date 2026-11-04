# recast_bake — offline Recast/Detour bake for WarsBots

Offline research tool (not part of the game build). Reads a cropped collision OBJ(.gz) exported by the bots
(Codex C-019/C-028, Crysis frame, Z up), bakes a tiled Detour navmesh with the player capsule, optionally generates
drop / jump / Strength links with geometry clearance, and reports which spawns and items lie on the main part.

Build (Linux or Windows/MSVC, CMake ≥ 3.16, zlib):

    cmake -S Tools/RecastBake -B build-recast -DCMAKE_BUILD_TYPE=Release   # fetches recastnavigation v1.6.0
    cmake --build build-recast --config Release

Run:

    recast_bake --obj Steelmill.crop.obj.gz --points Steelmill.pts --zmax 190 --jumps \
        --links-out Steelmill.links.txt --out-obj Steelmill.recast.obj --out-bin Steelmill.navmesh > Steelmill.json
    python3 Tools/RecastBake/render_mesh.py Steelmill.recast.obj Steelmill.json Steelmill.links.txt Steelmill.png [x0,y0,x1,y1]

`--points`: lines `spawn|item|probe id x y z`. `--links`: extra links `x0 y0 z0 x1 y1 z1 bidir kind`.
Agent defaults r 0.4, h 1.8, climb 0.5, slope 45, cell 0.2/0.1, tile 128; `--motions 1|2|3` limits generated links to
drop / +jump / +Strength; `--vh` horizontal take-off speed (4.5 run, 6.75 sprint). Level ceilings: `--zmax`
(Steelmill 190), sea bed: `--zmin` (Armada 108).

`--retain-box xmin ymin zmin xmax ymax zmax` is an explicit dry-interior exception to the global `--zmin` cutoff.
It retains only whole original triangles whose three vertices lie inside the Crysis-space box; a large seabed triangle
merely intersecting the box is rejected. The global `--zmax` still applies. With no box the mesh behavior is unchanged.
The water mask always uses all original source triangles, including the seabed, independently of retention.
This preserves a verified below-sea dry room without making submerged floors walkable across the map. Record the box
and source geometry in the recipe; this option does not prove that a ladder or any other transition can be executed.

`ctest --test-dir <build-dir> -C Release` runs the actual-loader inside/outside/crossing/upper-cutoff controls. The checks
remain active in Release. The existing `water_selftest.ps1 -Tool <recast_bake.exe>` validates water masking separately.

Verdict and numbers: `Docs/Research/SteelmillTriage/RECAST-BAKE-2026-10-24.md`.

## Новая карта с нуля, по шагам

Бейк идёт на ПК владельца: исходная геометрия (`*.crop.obj.gz`, сотни МБ) лежит вне git (D-021).
`recast_bake.exe` не собирается локально, его собирает CI. Workflow `RecastBake`
(`.github/workflows/recast-bake.yml`) выкладывает artifact `recast-bake-<SHA>` с `recast_bake.exe`,
`zlib1.dll` и `sha256.txt`. Скачать можно один раз, для точного SHA; хэш сверить с `sha256.txt`.

1. **Экспорт геометрии.** Локальный сервер IA без актёров, или PowerStruggle без игроков. В консоли:
   `wb_geom_export [path.obj] [x0 y0 x1 y1]`. Прямоугольник — игровая зона, остальное обрезается.
   Получаются `<Map>.crop.obj.gz`, `<Map>.spawns.json` и `<Map>.items.json`. Положить их рядом и записать SHA сборки экспорта.
2. **Точки.** Из `spawns.json`/`items.json` сделать файл `<Map>.pts`: строки `spawn|item|probe id x y z`.
   Добавить `probe` для мест, которые обязаны быть достижимы: точки захвата, заводы, выходы.
3. **Бейк.** Начать с параметров соседней карты и подобрать срезы по высоте:
   `recast_bake --obj <Map>.crop.obj.gz --points <Map>.pts --cs 0.1 --ch 0.05 --radius 0.3 --height 1.8 --climb 0.5
   --slope 38 --tile 256 --min-region 2 [--zmin Z] [--zmax Z] [--water 99.5] [--retain-box ...] [--jumps]
   [--links extra.links.txt] --links-out <Map>.links.txt --out-obj <Map>.recast.obj --out-bin <Map>.navmesh > <Map>.json`.
   - `--slope 38`: физика игрока начинает скользить с 45° (`BasicActor.lua` `min_slide_angle`, `Actor.cpp`). На Shore
     боты застревали на путях со средним уклоном около 41° (04.11), поэтому порог бейка берётся с запасом ниже.
   - `--retain-box` нужен только для сухих помещений ниже среза `--zmin`, например подлодки Shore.
4. **Проверка.** В `<Map>.json` смотреть `summary`: сколько спавнов и предметов на главной части и сколько частей
   (`mesh.components`). Сравнить с прошлым бейком таблицей «было/стало». Каждая важная точка (спавны, точки
   захвата, выходы) должна быть достижима. Если что-то отвалилось, уклон для всей карты не поднимать: прикрыть
   место данными (ручной линк в `--links`, исключающий или удерживающий бокс). Картинка:
   `python Tools/RecastBake/render_mesh.py <Map>.recast.obj <Map>.json <Map>.links.txt <Map>.png [x0,y0,x1,y1]`.
5. **Тактические точки** (если нужны снайперские/укрытия): `tactical_spots` → `<Map>.spots.txt`.
6. **Манифест.** `python Tools/RecastBake/make_manifest.py Tools/RecastBake/meshes <Map> --mode PS|IA --source "<SHA экспорта>"
   --bake "<вся командная строка бейка>"`. Игра откажется от файлов, если хэши не совпали.
7. **Установка для прогона.** Живые данные Navmesh не подменять. Для серверного прогона запускать сервер с
   отдельным `LOCALAPPDATA`, где лежит `WarsBots/Navmesh/<Map>.*`. Делать замер «до/после» на одной и той же
   CI-сборке, меняться должен только навмеш. Установка в живые данные (`Tools/Install-WarsBotsNavmesh.ps1`) — только после
   приёмки и по решению владельца.
8. **В git.** `meshes/<Map>.{navmesh,links.txt,spots.txt,manifest.json}` и строка в `Docs/STATUS.md`. Исходный
   `crop.obj.gz` в git не кладётся, в манифесте — ссылка на него (путь и SHA экспорта).
