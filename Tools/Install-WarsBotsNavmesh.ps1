# Copies the offline-baked Detour meshes (Tools/RecastBake/meshes, recastnavigation v1.6.0, cs 0.1 / r 0.3, with off-mesh drop/jump/Strength links, C-036)
# to %LOCALAPPDATA%\WarsBots\Navmesh where wb_bot_navmesh=1 loads them (BotNavMesh.cpp); links are used only with wb_bot_jump_links=1. Read-only for the game tree.
[CmdletBinding()] param([string]$Source = (Join-Path $PSScriptRoot 'RecastBake/meshes'))
$ErrorActionPreference = 'Stop'
$target = Join-Path $env:LOCALAPPDATA 'WarsBots/Navmesh'
New-Item -ItemType Directory -Force -Path $target | Out-Null
Get-ChildItem -Path $Source -File | Where-Object { $_.Name -like '*.navmesh' -or $_.Name -like '*.links.txt' -or $_.Name -like '*.spots.txt' -or $_.Name -like '*.manifest.json' } | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $target $_.Name) -Force
  $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
  Write-Output ("{0} {1} bytes {2}" -f $_.Name, $_.Length, $hash)
}
Write-Output "Installed to $target"
