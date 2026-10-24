# Explicit owner recording launch; no auto-stop, installed EXE replacement or hidden client.
[CmdletBinding()]
param(
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [string]$BuildDirectory='build/play-8b9236f4',
 [ValidateRange(1024,49151)][int]$Port=46050
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=(Resolve-Path -LiteralPath (Join-Path $repo $BuildDirectory)).Path
$manifest=Get-Content (Join-Path $build 'claude-build.json') -Raw|ConvertFrom-Json
if($manifest.data_layout -ne 'local-v1' -or -not $manifest.move_trace){throw 'Matched local-data MOVE TRACE build required'}
foreach($kind in @('client','server')){
 $exe=Join-Path $build $(if($kind -eq 'client'){'CryMP-Client64.exe'}else{'CryMP-Server64.exe'})
 if((Get-FileHash $exe).Hash -ne $manifest.$kind){throw "EXE hash mismatch: $kind"}
}
if(Get-Process CryMP-Server64,CryMP-Client64 -ErrorAction SilentlyContinue){throw 'Existing session retained; close it explicitly first'}
if(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue){throw 'Recording port occupied'}
if(Test-Path (Join-Path $GameDirectory 'Bin64/D3D9.dll')){throw 'Local D3D9 wrapper present; clean DX9 required'}
$logs=Join-Path $env:LOCALAPPDATA 'WarsBots/Logs';New-Item -ItemType Directory -Force $logs|Out-Null
$tag=[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$sessionDir=Join-Path $repo "build/owner-playtests/move6-$tag";New-Item -ItemType Directory $sessionDir|Out-Null
$serverLog=Join-Path $logs "WarsBots-Move6-$tag-Server.log";$clientLog=Join-Path $logs "WarsBots-Move6-$tag-Client.log"
$args="-dir `"$GameDirectory`" -headless -logfile `"$serverLog`" +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $Port +sv_gamerules InstantAction +map multiplayer/ia/steelmill +wb_probe_local_test 1 +wb_maxbots 6 +wb_reachable_goals 0 +wb_sink_escape 0 +wb_hotspot_all_nodes 0 +wb_trap_recovery 1 +wb_physics_diagnostics 1 +wb_move_trace 1 +wb_debug 1 +wb_add 6 normal +wb_list"
$server=Start-Process (Join-Path $build 'CryMP-Server64.exe') -ArgumentList $args -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
$session=@{source=$manifest.sha;build=$build;server_pid=$server.Id;server_log=$serverLog;client_log=$clientLog;port=$Port;bots=6;skill='normal';move_trace=1;B=0;C=0;trap_recovery=1;physics_diagnostics=1}
$session|ConvertTo-Json|Set-Content (Join-Path $sessionDir 'session.json')
$trace=$serverLog -replace '\.log$','.bots.log';$ready=$false
for($i=0;$i -lt 40;$i++){
 if($server.HasExited){throw "Server exited; inspect $serverLog"}
 if((Test-Path $trace) -and (Get-Content $trace -Raw) -match '#6 created EntityId='){$ready=$true;break}
 Start-Sleep -Milliseconds 500
}
if(-not $ready){throw "Server retained PID$($server.Id), not ready; inspect $serverLog"}
$clientArgs="-dir `"$GameDirectory`" -dx9 -logfile `"$clientLog`" +r_displayInfo 1 +wb_move_trace 1 +connect 127.0.0.1:$Port"
$client=Start-Process (Join-Path $build 'CryMP-Client64.exe') -ArgumentList $clientArgs -WorkingDirectory $GameDirectory -PassThru
$session.client_pid=$client.Id;$session.client_arguments=$clientArgs
$session|ConvertTo-Json|Set-Content (Join-Path $sessionDir 'session.json')
Write-Host "Recording session:server$($server.Id),client$($client.Id),logs$logs"
