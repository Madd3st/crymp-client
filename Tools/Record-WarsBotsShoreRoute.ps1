# C146: owner-launched native Shore route; exact green CI, zero bots, no binary/data installation.
[CmdletBinding()]
param([string]$GameDirectory='E:/Steam/steamapps/common/Crysis',[ValidateRange(1024,49151)][int]$Port=46157,[switch]$PrepareOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$sha='b06681eaa4365afe5bf7309994012dce13a7ac83'
$build=Join-Path $repo 'build/ci-b06681ea'
$quality=Get-Content (Join-Path $build 'native-route-quality.json') -Raw|ConvertFrom-Json
if($quality.source -ne $sha -or -not $quality.ci_success -or $quality.review -ne 'dd68f2ce'){throw 'Exact CI/review receipt required'}
foreach($name in @('CryMP-Client64.exe','CryMP-Server64.exe')){
 $row=Get-Content (Join-Path $build 'sha256.txt')|Where-Object{$_ -match [regex]::Escape($name)}
 if(@($row).Count -ne 1){throw 'Artifact manifest entry missing'}
 $expected=($row -split '\s+')[0]
 if((Get-FileHash -LiteralPath (Join-Path $build $name)).Hash -ne $expected){throw "Artifact hash mismatch: $name"}
}
if($PrepareOnly){Write-Output "READY exact $sha CI40649771503/DS081 OK; Shore PS, native player, zero bots, no map-data install";return}
if(Get-Process CryMP-Client64,CryMP-Server64,Crysis -ErrorAction SilentlyContinue){throw 'Existing game retained; no concurrent session'}
if(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue){throw 'Port occupied'}
if(Test-Path (Join-Path $GameDirectory 'Bin64/D3D9.dll')){throw 'Existing DX9 wrapper: use the established clean recording setup'}
$tag=[TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow,'Russian Standard Time').ToString('yyyyMMdd-HHmmss')
$sessionDir=Join-Path $repo "build/owner-playtests/C146-native-Shore-b06681ea-$tag"
$logs=Join-Path $env:LOCALAPPDATA 'WarsBots/Logs'
New-Item -ItemType Directory -Force $logs,$sessionDir|Out-Null
$serverLog=Join-Path $logs "C146-$tag-Server.log";$clientLog=Join-Path $logs "C146-$tag-Client.log"
$serverTrace=[IO.Path]::ChangeExtension($serverLog,'.bots.log');$clientTrace=[IO.Path]::ChangeExtension($clientLog,'.bots.log')
$memory=Join-Path $env:LOCALAPPDATA 'WarsBots/Memory';$snapshot=Join-Path $sessionDir 'memory-before'
$memoryExisted=Test-Path -LiteralPath $memory
New-Item -ItemType Directory $snapshot|Out-Null
$records=@(if($memoryExisted){foreach($f in Get-ChildItem -LiteralPath $memory -File){Copy-Item -LiteralPath $f.FullName -Destination $snapshot;@{name=$f.Name;sha256=(Get-FileHash $f.FullName).Hash}}})
ConvertTo-Json -InputObject $records|Set-Content (Join-Path $snapshot 'manifest.json') -Encoding UTF8
$serverExe=Join-Path $build 'CryMP-Server64.exe';$clientExe=Join-Path $build 'CryMP-Client64.exe'
$serverArgs="-dir `"$GameDirectory`" -headless -logfile `"$serverLog`" +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $Port +sv_gamerules PowerStruggle +map multiplayer/ps/Shore +wb_probe_local_test 1 +wb_maxbots 1 +wb_bot_navmesh 0 +wb_bot_jump_links 0 +wb_log 2 +wb_debug 1 +wb_move_trace 1 +wb_demo_record 1 +wb_demo_hz 20 +wb_rules_info +wb_list +wb_status"
$clientArgs="-dir `"$GameDirectory`" -dx9 -logfile `"$clientLog`" +r_displayInfo 0 +wb_log 2 +wb_move_trace 1 +wb_move_trace_hz 20 +bind f8 `"wb_mark route`" +connect 127.0.0.1:$Port"
foreach($pair in @(@($serverExe,$serverArgs),@($clientExe,$clientArgs))){if([Text.Encoding]::ASCII.GetByteCount('"'+$pair[0]+'" '+$pair[1]) -ge 2048){throw 'Engine command line too long'}}
$session=@{task='C146';source=$sha;ci=40649771503;review='dd68f2ce';bots=0;map='Shore';navmesh=0;jump_links=0;video='Owner screen recording optional; native logs/demo automatic';session_directory=$sessionDir;server_log=$serverLog;client_log=$clientLog;memory_before=$snapshot;phase='preparing';client_pid=$null;server_pid=$null}
$serverArgs|Set-Content (Join-Path $sessionDir 'server-arguments.txt');$clientArgs|Set-Content (Join-Path $sessionDir 'client-arguments.txt')
function Save-RouteSession{$session|ConvertTo-Json -Depth 5|Set-Content (Join-Path $sessionDir 'session.json') -Encoding UTF8;$session|ConvertTo-Json -Depth 5|Set-Content (Join-Path $repo 'build/c146-active-session.json') -Encoding UTF8}
$server=$null;$client=$null
try{
 if(Get-Process CryMP-Client64,CryMP-Server64,Crysis -ErrorAction SilentlyContinue){throw 'Game appeared before launch'}
 $server=Start-Process $serverExe -ArgumentList $serverArgs -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
 $session.server_pid=$server.Id;Save-RouteSession
 $deadline=[DateTime]::UtcNow.AddSeconds(60);$text=''
 do{Start-Sleep -Milliseconds 500;if($server.HasExited){throw 'Own server exited'};if(Get-Process CryMP-Client64,Crysis -ErrorAction SilentlyContinue){throw 'Owner game appeared during preparation'};if(Test-Path $serverLog){$text=Get-Content $serverLog -Raw}}while($text -notmatch 'RULES INFO class=PowerStruggle' -and [DateTime]::UtcNow -lt $deadline)
 if($text -notmatch 'RULES INFO class=PowerStruggle' -or $text -notmatch 'bots=0/1 defaultVerbose=1' -or $text -match '\[Error\]|Lua Error|Script Error'){throw 'Server readiness/zero-bot recipe failed'}
 if(Get-Process CryMP-Client64,Crysis -ErrorAction SilentlyContinue){throw 'Owner game appeared before client launch'}
 $client=Start-Process $clientExe -ArgumentList $clientArgs -WorkingDirectory $GameDirectory -PassThru
 $session.client_pid=$client.Id;$session.phase='owner_recording';Save-RouteSession
 Write-Host 'Choose US/black, spawn on the submarine, show your normal exit to shore. F8 marks key moments.'
 Write-Host 'No timer. Close the game normally when finished; keep this console open for archive/cleanup.'
 Write-Host "Session: $sessionDir"
 while(-not $client.HasExited){Start-Sleep -Seconds 2}
 $session.phase='owner_client_closed';Save-RouteSession
}finally{
 if(Get-Process CryMP-Client64,Crysis -ErrorAction SilentlyContinue){$session.phase='cleanup_deferred_client_active';Save-RouteSession;Write-Warning "Client retained; archive/restore deferred. Session $sessionDir"}
 else{
  if($server -and -not $server.HasExited){if($server.Path -ne $serverExe){throw 'Own server path mismatch'};Stop-Process -Id $server.Id;$null=$server.WaitForExit(10000)}
  foreach($f in @($serverLog,$serverTrace,$clientLog,$clientTrace)){if(Test-Path -LiteralPath $f){Copy-Item -LiteralPath $f -Destination $sessionDir}}
  $demoRoot=[IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'WarsBots/Demos'))+[IO.Path]::DirectorySeparatorChar
  $demos=@(foreach($trace in @($serverTrace,$clientTrace)){if(Test-Path $trace){foreach($m in [regex]::Matches((Get-Content $trace -Raw),'DEMO START path=(.*?) version=')){$path=[IO.Path]::GetFullPath($m.Groups[1].Value);if(-not $path.StartsWith($demoRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected demo path'};if(Test-Path -LiteralPath $path){Copy-Item -LiteralPath $path -Destination $sessionDir;$copy=Join-Path $sessionDir ([IO.Path]::GetFileName($path));if((Get-FileHash $path).Hash -ne (Get-FileHash $copy).Hash){throw 'Demo archive mismatch'};@{name=[IO.Path]::GetFileName($path);bytes=(Get-Item $copy).Length;sha256=(Get-FileHash $copy).Hash}}}}})
  ConvertTo-Json -InputObject $demos|Set-Content (Join-Path $sessionDir 'demo-archive.json') -Encoding UTF8
  if(Get-Process CryMP-Server64,CryMP-Client64,Crysis -ErrorAction SilentlyContinue){$session.phase='restore_deferred_other_game';Save-RouteSession;throw 'Other server retained; memory restore deferred'}
  if(Test-Path $memory){$after=Join-Path $sessionDir 'memory-after';New-Item -ItemType Directory $after|Out-Null;foreach($f in Get-ChildItem -LiteralPath $memory -File){Copy-Item -LiteralPath $f.FullName -Destination $after;if((Get-FileHash $f.FullName).Hash -ne (Get-FileHash (Join-Path $after $f.Name)).Hash){throw 'Memory archive hash mismatch'};if($f.Name -notin @($records|ForEach-Object{$_.name})){if($f.DirectoryName -ne (Resolve-Path -LiteralPath $memory).Path){throw 'Memory path escaped'};Remove-Item -LiteralPath $f.FullName}}}
  foreach($r in $records){Copy-Item -LiteralPath (Join-Path $snapshot $r.name) -Destination (Join-Path $memory $r.name) -Force;if((Get-FileHash (Join-Path $memory $r.name)).Hash -ne $r.sha256){throw 'Memory restore mismatch'}}
  if(-not $memoryExisted -and (Test-Path $memory) -and -not(Get-ChildItem -LiteralPath $memory)){Remove-Item -LiteralPath $memory}
  ConvertTo-Json -InputObject $records|Set-Content (Join-Path $sessionDir 'memory-restored.json') -Encoding UTF8
  $session.phase='archived_memory_restored';Save-RouteSession
  Write-Host "Archived and restored: $sessionDir"
 }
}