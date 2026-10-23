[CmdletBinding()]
param([switch]$Experimental, [int]$Jobs = 4, [string]$BuildDirectory = 'build/warsbots', [string[]]$NinjaArgs = @())
$ErrorActionPreference = 'Stop'
$portRoot = Split-Path $PSScriptRoot -Parent
$portVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$portVs = & $portVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $portVs) { throw 'MSVC C++ Build Tools are required.' }
& (Join-Path $portVs 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$env:VSLANG = '1033'
$portFlag = if ($Experimental) { 'ON' } else { 'OFF' }
Push-Location $portRoot
try {
    # config.h contains an absolute Pak.zip path; RC needs UTF-8 for Cyrillic paths.
    & cmake -S . -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_RC_FLAGS=/c65001 "-DWARSBOTS_EXPERIMENTAL_SPAWNER=$portFlag"
    if ($LASTEXITCODE -ne 0) { throw 'CryMP configure failed.' }
    if ($NinjaArgs.Count) { & cmake --build $BuildDirectory --parallel $Jobs -- @NinjaArgs } else { & cmake --build $BuildDirectory --parallel $Jobs }
    if ($LASTEXITCODE -ne 0) { throw 'CryMP build failed.' }
} finally { Pop-Location }
