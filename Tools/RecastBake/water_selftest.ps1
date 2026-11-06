param(
  [Parameter(Mandatory)][string]$Tool,
  [string]$Fixtures = (Join-Path $PSScriptRoot 'water_fixtures')
)
$ErrorActionPreference = 'Stop'
function Run([string]$obj, [bool]$water, [string[]]$extra = @()) {
  $a = @('--obj', $obj, '--cs', '0.1', '--ch', '0.1', '--radius', '0.3', '--height', '1.8', '--climb', '0.5',
         '--slope', '45', '--tile', '256', '--min-region', '2', '--zmin', '100')
  if ($water) { $a += @('--water', '99.5') }
  $a += $extra
  $text = & $Tool @a 2>$null | Out-String
  if ($LASTEXITCODE) { throw "tool failed for $obj (exit $LASTEXITCODE)" }
  return $text | ConvertFrom-Json
}
# DS-067: assert the emitted water footprints, not a count that can hide an overlap with solid ground.
function Dump([string]$obj, [string]$dump) {
  if (Test-Path $dump) { Remove-Item -LiteralPath $dump }
  Run $obj $true @('--water-dump', $dump) | Out-Null
  if (-not (Test-Path $dump)) { throw "no water dump for $obj" }
  return @(Get-Content -LiteralPath $dump | ForEach-Object { , @($_ -split '\s+' | ForEach-Object { [double]$_ }) })
}
function RectHit($q, [double]$x0, [double]$y0, [double]$x1, [double]$y1) {
  return ($q[0] -lt $x1) -and ($q[2] -gt $x0) -and ($q[1] -lt $y1) -and ($q[3] -gt $y0)
}
$P = { param($n) Join-Path $Fixtures $n }

# 1. no-water baseline unchanged: two shores stay disconnected.
$x = Run (& $P 'two-shores.obj') $false @('--points', (& $P 'two-shores.points'))
if ($x.mesh.components -ne 2 -or $x.summary.items_reachable -ne 0) { throw 'no-water baseline changed' }

# 2. water connects two shores; real point fixtures, not an empty points list.
$x = Run (& $P 'two-shores.obj') $true @('--points', (& $P 'two-shores.points'))
if ($x.mesh.components -ne 1 -or $x.summary.items_reachable -lt 1) { throw 'two shores are not connected by water' }
if ($x.water.cells -le 0 -or $x.water.surface_tris -ne 2 * $x.water.cells) { throw 'water cell/triangle count mismatch' }
$openCells = $x.water.cells

# 3. roof at 105 over the gap: no water under it. The roof itself is a third, walkable component (C145 measured 3).
$x = Run (& $P 'roof.obj') $true
if ($x.water.cells -ne 0) { throw 'water appeared under a roof' }
if ($x.mesh.components -ne 3) { throw "roof topology changed (components=$($x.mesh.components), expected 3)" }

# 4. island: water around it, never on its footprint.
$x = Run (& $P 'island.obj') $true
if ($x.water.cells -le 0 -or $x.water.cells -ge $openCells) { throw 'water covers the island or no water around it' }
$q = Dump (& $P 'island.obj') (Join-Path $env:TEMP 'ds067-island.dump')
foreach ($r in $q) { if (RectHit $r 18.1 3.1 21.9 6.9) { throw 'water quad overlaps the island footprint' } }

# 5. void gap: unknown is not water.
$x = Run (& $P 'void.obj') $true
if ($x.water.cells -ne 0 -or $x.mesh.components -ne 2) { throw 'void gap was filled by a global waterplane' }

# 6. C145 mask-oracle RED case: a tiny solid island inside one cell must block that cell's quad.
$x = Run (& $P 'tiny-island.obj') $true
if ($x.water.cells -le 0) { throw 'tiny-island fixture produced no water at all' }
$q = Dump (& $P 'tiny-island.obj') (Join-Path $env:TEMP 'ds067-tiny.dump')
foreach ($r in $q) { if (RectHit $r 20.1 4.1 20.5 4.5) { throw 'water quad overlaps the tiny island (C145 RED)' } }

# 7. gentle shoreline: water reaches the waterline and the beach is reachable.
$x = Run (& $P 'shore-gentle.obj') $true @('--points', (& $P 'shore-gentle.points'))
if ($x.water.cells -le 0) { throw 'no water at the gentle shore' }
if ($x.mesh.components -ne 1 -or $x.summary.items_reachable -lt 1) { throw 'gentle shore water is not connected to land' }

# 8. true area-cost oracle on one bake: land detour at cost 4, water shortcut at cost 1.
$x = Run (& $P 'detour.obj') $true @('--water-cost', '4', '--path-from', '5', '5', '100', '--path-to', '35', '5', '100')
if ($x.path.found -ne $true -or $x.path.water_polys -ne 0) { throw 'cost 4 did not prefer the land detour' }
$x = Run (& $P 'detour.obj') $true @('--water-cost', '1', '--path-from', '5', '5', '100', '--path-to', '35', '5', '100')
if ($x.path.found -ne $true -or $x.path.water_polys -le 0) { throw 'cost 1 did not choose the water shortcut' }

# 9. Even four corners plus centre are insufficient: a tiny hole inside a known-seabed cell must stay unknown.
$q = Dump (& $P 'partial-void.obj') (Join-Path $env:TEMP 'c145-partial-void.dump')
foreach ($r in $q) { if (RectHit $r 4.1 4.1 4.5 4.5) { throw 'water fills an unknown hole in partial seabed coverage' } }

# 10. More than 256 path polygons: a partial route must never be sold as a complete route.
$x = Run (& $P 'long-corridor.obj') $false @('--tile', '16', '--path-from', '2', '2', '100', '--path-to', '498', '2', '100')
if ($x.path.found -ne $false -or $x.path.partial -ne $true -or $x.path.water_polys -ne 0 -or $x.path.length_m -ne 0) { throw 'partial route reported as a found path' }

'water_selftest: all cases passed'
