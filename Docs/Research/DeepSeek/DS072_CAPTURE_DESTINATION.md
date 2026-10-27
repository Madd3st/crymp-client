# DS-072: capture destination from the native area binding (local id + level XML)

Base: `2e0a6641` + `ds067d` + `ds070`; patch `patches/ds072_capture_destination.patch`. No build, no game.
Supersedes the earlier draft that treated `captureAreaId` as an EntityId.

## Why the first draft was wrong

`Properties.captureAreaId` is the **local area number** (`1`) on every Shore capturable. `GetEntity(EntityId(1))`
resolves entity 1, not shape 569/323/330. The verified map binds them explicitly in `mission_mission0.xml`:

```xml
<Entity Name="Shape9" Pos="..." Rotate="..." EntityClass="AreaShape" EntityId="629" ...>
  <Area Id="1" Height="38">
    <Points><Point Pos="0,0,0"/>...</Points>
    <Entities><Entity Id="654"/></Entities>
  </Area>
</Entity>
```

So a shape qualifies only when **both** `proxy.GetID() == captureAreaId` and the XML `<Entities>` list contains the
capturable entity id. One factory can bind several shapes (Shore 344 -> 323 + 330). The public `IEntityAreaProxy`
has `AddEntity/ClearEntities` but no target-list getter, so the XML is the only binding source.

## Runtime resolution (no absolute game path)

* `ILevelInfo::GetPath()` gives the mounted level path; read `<path>/mission_mission0.xml` through
  `gEnv->pCryPak->FOpen/FGetSize/FReadRawAll`, exactly like the game mounts the pak. Nothing is hardcoded.
* `ParseAreaShapes` keeps only host entities with `EntityClass="AreaShape"`: `EntityId` (map id), `<Area Id>`
  (local number), `Height`, `Pos`, `Rotate`, `<Points>`, `<Entities>` targets. Cached per level name, one read.
* For each capturable, `AreaDestination` takes only shapes where `CaptureShapeBinds` is true (local id **and** target),
  then verifies the live entity: class `AreaShape`, world position within 1 m of the XML `Pos`, area proxy exists,
  `proxy->GetID() == localAreaId`. A stale/mismatched id is skipped, never guessed.
* The polygon comes from the **live** `proxy->GetPoints()` and entity transform (authoritative geometry), with the
  XML polygon only as a diagnostic fallback. Samples every 1 m inside `[bottom, top]`; require
  `CaptureAreaInside`, native `IsPointWithin`, and (when the mesh is ready) `CurrentNavMesh()->Project` + a snapped
  point still inside. The most interior sample (max edge distance) wins across all bound shapes; a 0.25 m margin is
  required. The chosen **shape entity id** is stored in `PsCapturePoint::areaId`; the local number is kept separately
  in `areaLocalId` for logs.

## Hold and fail-closed

* `BotBrain::UpdatePSCapture` asks the live proxy of the chosen shape `IsPointWithin(feet)`; `BotPSCapture::Update`
  sets `plan.hold` only for the current target inside its native area. The 6 m pivot sphere is gone.
* No binding / no interior sample / too thin -> the point is skipped with
  `PS CAPTURE skip id=... area=... reason=no_binding|no_interior_sample|thin_area`, and another capturable is chosen.
  No fabricated destination, teleport, capture event or pivot link.
* `BasePoints` still caches once per level; the per-tick work is one `IsPointWithin` for the current target.

## Regression (`Tests/BotCaptureAreaTests.cpp`, CI `BotCaptureArea`)

Real Shore data (`Shore.area-shapes.json`, six AreaShapes, all `<Area Id="1">`):

| capturable | bound shape(s) | assertion |
| --- | --- | --- |
| 64 | 39 | only 39 matches; local-id-only would match all six |
| 316 | 291 | only 291 |
| 654 | 629 | only 629 |
| 344 | 323 + 330 | exactly two |
| 579 | 569 | only 569; pivot (2236.76,1325.84) is 13.3 m outside and `CaptureDestinationValid` is false |
| all | - | each real inside-area floor representative is inside its bound shape |

The test fails if the local id alone is used, if a pivot is accepted, or if a shape is bound to the wrong target.

## Unknowns

* XML `EntityId` -> live entity stability is verified by class, world position and proxy id; a mismatch skips the point
  (logged) rather than using a wrong area. No GUID fallback is implemented because the map id is stable on the
  verified build.
* If the navmesh is loaded but a shape's floor is not projectable, that shape is skipped; this is fail-closed by design.
* A modded level with a different XML layout yields `no_binding` and no capture goals (logged), never a pivot goal.

Apply check (no build): patch applies on the `2e0a6641 + 067d + 070` baseline.
