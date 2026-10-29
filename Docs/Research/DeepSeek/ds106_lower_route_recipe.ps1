# DS-106 offline recipe: Shore lower-deck DIAGNOSTIC subset bake + one real UP-only ladder link + NavMeshCore query.
# Read-only on the game/QA inputs; all outputs go to a private temp directory. No game, install or QA write.
# WARNING: the --zmin 90 below is a diagnostic subset only and is REJECTED for production (it admits the submerged
# sea floor). Production keeps global --zmin 99.5 --water 99.5 and uses Codex's --retain-box dry-interior exception
# (pending review/deployment). Do not copy this command as a map bake.
param(
    [string]$QaRoot = 'C:/Users/Magic8/WarsBotsBuild/crymp-multibot/build',
    [string]$OutDir = (Join-Path $env:TEMP 'ds106-lower-route')
)
$ErrorActionPreference = 'Stop'
$bake = Join-Path $QaRoot 'coordinator-c145/recast_bake-final.exe'
$obj  = Join-Path $QaRoot 'coordinator-c145/Shore.US.exact-subset.obj'
$pts  = Join-Path $QaRoot 'coordinator-c145/Shore.US.pts'
New-Item -ItemType Directory -Force $OutDir | Out-Null

# The validated native long-shaft ladder is at x=2116.125,y=1880.125, z 90.25..100.29.
# End points below are the baked lower floor and the nearest baked upper-deck poly at the shaft opening.
# Production edge is UP only (bidir=0); the reverse route is not physically verified.
$link = Join-Path $OutDir 'ladder.links.txt'
'2116.100 1880.600 90.168 2116.700 1879.800 99.768 0 ladder' | Set-Content -Encoding ascii $link

$common = @('--obj', $obj, '--points', $pts, '--zmin', '90', '--water', '99.5',
    '--cs', '0.1', '--ch', '0.1', '--radius', '0.3', '--height', '1.8', '--climb', '0.5',
    '--slope', '45', '--tile', '256', '--min-region', '2')

& $bake @common --out-bin (Join-Path $OutDir 'red.navmesh') > (Join-Path $OutDir 'red.json')
& $bake @common --links $link --out-bin (Join-Path $OutDir 'green.navmesh') > (Join-Path $OutDir 'green.json')

# Build the engine-free probe (same sources as CI: BotNavMeshCore + Detour).
$probeSrc = Join-Path $PSScriptRoot 'proto/ds106_nav_probe.cpp'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$detour = Join-Path $repo 'ThirdParty/recastnavigation/Detour'
$vs = & 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& (Join-Path $vs 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
Push-Location $OutDir
try {
    $sources = @($probeSrc, (Join-Path $repo 'Code/CryGame/Bots/BotNavMeshCore.cpp')) +
        (Get-ChildItem (Join-Path $detour 'Source') -Filter *.cpp | ForEach-Object { $_.FullName })
    & cl /nologo /std:c++17 /EHsc /I $repo /I (Join-Path $repo 'Code') /I (Join-Path $detour 'Include') `
        $sources /Fe:ds106_probe.exe
} finally { Pop-Location }

$probe = Join-Path $OutDir 'ds106_probe.exe'
Write-Host 'RED: 545 -> 99.768 (expect other_component)'
& $probe (Join-Path $OutDir 'red.navmesh') $link 2114.50 1887.88 90.38 2116.70 1879.80 99.768
Write-Host 'GREEN: 545 -> 99.768 with the UP-only ladder link (expect ok + 1 ladder link)'
& $probe (Join-Path $OutDir 'green.navmesh') $link 2114.50 1887.88 90.38 2116.70 1879.80 99.768
Write-Host 'E223 baseline: 545 off-mesh / 530 false +12 m'
$e223 = Join-Path $QaRoot 'coordinator-c145/Shore.eed85fb1.candidate.navmesh'
& $probe $e223 --project 2114.50 1887.88 90.38
& $probe $e223 --project 2113.62 1880.38 90.38
