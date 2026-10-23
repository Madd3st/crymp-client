# Claude QA helper: separate headless loopback IA server on port 64120 with N bots, no client.
# Runs for a bounded time, then stops ONLY the process it started (and only if no client joined).
param([int]$Bots = 2, [int]$Seconds = 120, [string]$Tag = 'm3')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$wt = Join-Path (Split-Path $repo -Parent) 'crymp-multibot'
$out = Join-Path $repo 'build\claude-qa'
New-Item -ItemType Directory -Force $out | Out-Null
$done = Join-Path $out "$Tag.done"
Remove-Item -LiteralPath $done -ErrorAction SilentlyContinue
$gameFile = Join-Path $PSScriptRoot 'game-dir.txt'
if (-not (Test-Path $gameFile)) { Set-Content $done 'NO_GAME_DIR (ClaudeQA\game-dir.txt)'; exit 1 }
$game = (Get-Content -LiteralPath $gameFile -Raw).Trim()
$exe = Get-ChildItem -Path (Join-Path $wt 'build\multibot-on') -Recurse -Filter 'CryMP-Server64.exe' | Select-Object -First 1
if (-not $exe) { Set-Content $done 'NO_SERVER_EXE'; exit 1 }
$name = "WarsBots-Claude-$Tag-$([DateTime]::Now.ToString('HHmmss')).log"
$gameLog = Join-Path ([Environment]::GetFolderPath('MyDocuments')) "My Games\Crysis\$name"
$qaArgs = "-dir `"$game`" -headless -logfile $name +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port 64120 +sv_gamerules InstantAction +map multiplayer/ia/steelmill +wb_probe_local_test 1 +wb_debug 1 +wb_maxbots $Bots +wb_add $Bots +wb_list +wb_nav"
Write-Host "QA $Tag : $Bots bots, $Seconds s, port 64120, log $name"
$p = Start-Process -FilePath $exe.FullName -ArgumentList $qaArgs -WorkingDirectory $game -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
$text = ''
while ([DateTime]::UtcNow -lt $deadline -and -not $p.HasExited) {
    Start-Sleep -Seconds 5
    if (Test-Path -LiteralPath $gameLog) { $text = Get-Content -LiteralPath $gameLog -Raw }
    if ($text -match 'CRASH DETECTED') { break }
    $kills = ([regex]::Matches($text, 'combat kill')).Count; $left = [int]($deadline - [DateTime]::UtcNow).TotalSeconds
    Write-Host ("   QA {0}: kills {1}, {2}s left" -f $Tag, $kills, $left)
}
$state = if ($p.HasExited) { "EXITED $($p.ExitCode)" } else { 'RUNNING' }
if (-not $p.HasExited) {
    if ($text -match 'clients=[1-9]') { $state = 'LEFT RUNNING (a client joined)' }
    else { Stop-Process -Id $p.Id; Wait-Process -Id $p.Id -Timeout 15 -ErrorAction SilentlyContinue; $state = 'STOPPED BY QA' }
}
if (Test-Path -LiteralPath $gameLog) { Copy-Item -LiteralPath $gameLog -Destination (Join-Path $out $name) -Force }
Set-Content -LiteralPath $done -Value "$state log=$name"
