# Copies the offline-baked Detour meshes (Tools/RecastBake/meshes, recastnavigation v1.6.0, cs 0.1 / r 0.3, walk-only)
# to %LOCALAPPDATA%\WarsBots\Navmesh where wb_bot_navmesh=1 loads them (BotNavMesh.cpp). Read-only for the game tree.
[CmdletBinding()] param([string]$Source = (Join-Path $PSScriptRoot 'RecastBake/meshes'))
$ErrorActionPreference = 'Stop'
$target = Join-Path $env:LOCALAPPDATA 'WarsBots/Navmesh'
New-Item -ItemType Directory -Force -Path $target | Out-Null
Get-ChildItem -Path $Source -Filter *.navmesh | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $target $_.Name) -Force
  $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
  Write-Output ("{0} {1} bytes {2}" -f $_.Name, $_.Length, $hash)
}
Write-Output "Installed to $target"
