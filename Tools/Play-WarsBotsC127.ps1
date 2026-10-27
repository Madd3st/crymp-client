# C133/C127: owner-launched first-look Shore session; exact CI pair, no local build.
[CmdletBinding()]
param(
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [ValidateRange(1024,49151)][int]$Port=46050,
 [switch]$Smoke,
 [switch]$PrepareOnly
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$sha='4f3a1d72e1269a644c3e602701ca170b5b59d044'
$build=Join-Path $repo 'build/ci-4f3a1d72'
$root=Join-Path $repo 'build/c127-smoke-4f3a1d72'
$receiptPath=Join-Path $root 'receipt.json'
$expected=[ordered]@{wb_probe_local_test=1;wb_maxbots=6;wb_bot_navmesh=0;wb_bot_jump_links=0;wb_ps_capture=1;wb_ps_buy=1;wb_bot_look=1;wb_bot_turn_cap=1;wb_bot_weapon_rank=1;wb_bot_arbiter=1;wb_bot_perception=1;wb_bot_aim_model=1;wb_bot_suit_policy=1;wb_bot_weapon_handling=1;wb_bot_name_profile=1;wb_bot_vantage=0;wb_bot_tactics=1;wb_bot_difficulty=1;wb_bot_finish=1;wb_bot_spectator_aim=1;wb_bot_path_progress=0;wb_bot_pro_fire=1;wb_bot_high_ground=1;wb_bot_threat_scan=1;wb_bot_state_commit=1;wb_bot_first_primary=1;wb_bot_withdraw_cover=1;wb_fire_diagnostics=1;wb_net_ext=1;wb_bot_pace=1;wb_spawn_exclude195=1;wb_revive_guard=1;wb_reachable_goals=0;wb_hotspot_all_nodes=0;wb_trap_recovery=1;wb_physics_diagnostics=1;wb_move_trace=1;wb_demo_record=1;wb_demo_hz=20;wb_bot_corners=1;wb_bot_jump_sanity=1}
$recipeJson=ConvertTo-Json -InputObject $expected -Compress
function Hash-Text([string]$s){$h=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($h.ComputeHash([Text.Encoding]::UTF8.GetBytes($s)))).Replace('-','').ToLowerInvariant()}finally{$h.Dispose()}}
$recipeHash=Hash-Text $recipeJson
$hashes=@{client='F991D7DF54F47FDACCA5B1FA7900D7B96D8636E734E0A77B5E6C6D68635D144E';server='6AD1FD97A0FB89B567A2F3421140C8AE25D9FC61678B2C3613B361E72B976120'}
foreach($k in @('client','server')){$exe=Join-Path $build ('CryMP-'+$(if($k -eq 'client'){'Client'}else{'Server'})+'64.exe');if((Get-FileHash -LiteralPath $exe).Hash -ne $hashes[$k]){throw "Exact CI EXE mismatch: $k"}}
if(-not $Smoke){$quality=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json;if(-not $quality.pass -or $quality.source -ne $sha -or $quality.recipe_sha256 -ne $recipeHash){throw 'C127 requires exact source + matching green 120s smoke receipt'}}
if($PrepareOnly){Write-Output "READY $sha recipe=$recipeHash; Shore6 Normal; navmesh0/jump_links0, capture1/buy1; F8; DX9/no overlay";return}
if(Get-Process CryMP-Server64,CryMP-Client64 -ErrorAction SilentlyContinue){throw 'Existing game retained: no concurrent run'}
if(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue){throw 'Port occupied'}
if(Test-Path (Join-Path $GameDirectory 'Bin64/D3D9.dll')){throw 'Local D3D9 wrapper present; clean DX9 required'}
$logs=Join-Path $env:LOCALAPPDATA 'WarsBots/Logs';New-Item -ItemType Directory -Force $logs|Out-Null
$tag=[TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow,'Russian Standard Time').ToString('yyyyMMdd-HHmmss')
$sessionDir=if($Smoke){$root}else{Join-Path $repo "build/owner-playtests/C127-4f3a1d72-$tag"}
New-Item -ItemType Directory -Force $sessionDir|Out-Null
$snapshot=Join-Path $sessionDir 'memory-before';New-Item -ItemType Directory -Force $snapshot|Out-Null
$memory=Join-Path $env:LOCALAPPDATA 'WarsBots/Memory'
$records=@(foreach($f in Get-ChildItem -LiteralPath $memory -File){Copy-Item -LiteralPath $f.FullName -Destination $snapshot -Force;@{name=$f.Name;sha256=(Get-FileHash -LiteralPath $f.FullName).Hash}})
ConvertTo-Json -InputObject @($records)|Set-Content (Join-Path $snapshot 'manifest.json')
$recipeJson|Set-Content (Join-Path $sessionDir 'recipe.json')
$sets=' +wb_debug 1';$queries='';foreach($k in $expected.Keys){if($k -notin @('wb_bot_corners','wb_bot_jump_sanity')){$sets+=" +$k $($expected[$k])"};$queries+=" +$k"}
function Stop-Own($p){if($p -and -not $p.HasExited){if($p.Path -ne (Join-Path $build 'CryMP-Server64.exe')){throw 'Own server PID/path mismatch'};Stop-Process -Id $p.Id;$null=$p.WaitForExit(10000)}}
function Start-Server([string]$Phase,[bool]$Roster){
 $log=Join-Path $logs "C127-$tag-$Phase.log";$trace=$log -replace '\.log$','.bots.log'
 $add=if($Roster){' +wb_add 6 normal +wb_rules_info +wb_list'}else{' +wb_status'}
 $args="-dir `"$GameDirectory`" -headless -logfile `"$log`" +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $Port +sv_gamerules PowerStruggle +map multiplayer/ps/shore$sets$queries +wb_rules_info$add"
 $args|Set-Content (Join-Path $sessionDir "$Phase-arguments.txt")
 $p=Start-Process (Join-Path $build 'CryMP-Server64.exe') -ArgumentList $args -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
 $ready=$false;$deadline=[DateTime]::UtcNow.AddSeconds(45)
 try{
  do{Start-Sleep -Milliseconds 500;if($p.HasExited){throw "Server exited, inspect $log"};$text=if(Test-Path $log){Get-Content $log -Raw}else{''};$side=if(Test-Path $trace){Get-Content $trace -Raw}else{''};$ready=$text -match 'RULES INFO class=PowerStruggle' -and (-not $Roster -or $side -match '#6 created EntityId=')}while(-not $ready -and [DateTime]::UtcNow -lt $deadline)
  if(-not $ready){throw 'PS readiness missing'}
  $checks=@(foreach($k in $expected.Keys){$m=[regex]::Matches($text,[regex]::Escape($k)+'\s*=\s*(-?\d+)');$v=if($m.Count){[int]$m[$m.Count-1].Groups[1].Value}else{$null};@{name=$k;expected=$expected[$k];actual=$v;pass=($null -ne $v -and $v -eq $expected[$k])}})
  ConvertTo-Json -InputObject @($checks)|Set-Content (Join-Path $sessionDir "$Phase-effective.json")
  if(@($checks|Where-Object{-not $_.pass}).Count){throw 'Effective recipe mismatch'}
  if($text -match '\[Error\]|\[Script\]|Lua Error|Script Error|No such server channel|CRASH DETECTED'){throw 'PS startup errors'}
  return @{process=$p;log=$log;trace=$trace;arguments=$args}
 }catch{Stop-Own $p;throw}
}
# Actorless readiness/recipe check before the player session or QA roster.
$preflight=$null;$run=$null
try{
 $preflight=Start-Server 'preflight' $false
 Stop-Own $preflight.process
 foreach($f in @($preflight.log,$preflight.trace)){if(Test-Path $f){Copy-Item -LiteralPath $f -Destination $sessionDir -Force}}
 $run=Start-Server 'Server' $true
 if($Smoke){
  $end=[DateTime]::UtcNow.AddSeconds(120)
  while([DateTime]::UtcNow -lt $end){if($run.process.HasExited){throw 'PS smoke crashed'};Start-Sleep -Seconds 2}
  Stop-Own $run.process
  $eng=Get-Content $run.log -Raw;$side=Get-Content $run.trace -Raw
  $errors=@($eng -split "`n"|Where-Object{$_ -match '\[Error\]|\[Script\]|Lua Error|Script Error|No such server channel|CRASH DETECTED'})
  $deaths=([regex]::Matches($side,'death observed EntityId=')).Count;$waves=([regex]::Matches($side,'PS REVIVE SEEN.*source=wave')).Count
  $pass=$errors.Count -eq 0 -and $waves -ge 1
  $m=[regex]::Match($side,'DEMO START path=(.+?) version=');if($m.Success -and (Test-Path $m.Groups[1].Value)){Copy-Item -LiteralPath $m.Groups[1].Value -Destination $sessionDir -Force}
  @{source=$sha;ci_run=40526830694;recipe_sha256=$recipeHash;pass=$pass;seconds=120;deaths=$deaths;wave_seen=$waves;errors=@($errors);navmesh=0;jump_links=0;capture=1;buy=1;hashes=$hashes}|ConvertTo-Json -Depth 6|Set-Content $receiptPath
  if(-not $pass){throw 'C133 smoke gate FAIL: see receipt'}
  Write-Output "SMOKE PASS deaths=$deaths wave_seen=$waves errors=$($errors.Count) source=$sha"
 }else{
  $clientLog=Join-Path $logs "C127-$tag-Client.log"
  $clientArgs="-dir `"$GameDirectory`" -dx9 -logfile `"$clientLog`" +r_displayInfo 0 +wb_bot_pace 1 +wb_net_ext 1 +wb_bot_spectator_aim 1 +bind f8 `"wb_mark owner`" +wb_bot_client_smooth 1 +wb_move_trace 2 +connect 127.0.0.1:$Port"
  $session=@{label="C127-4f3a1d72-$tag";phase='owner_playing_wait_explicit_done';source=$sha;build=$build;server_pid=$run.process.Id;server_log=$run.log;server_trace=$run.trace;client_log=$clientLog;client_arguments=$clientArgs;session_directory=$sessionDir;memory_before=$snapshot;port=$Port;bots=6;skill='normal';teams='3+3';recipe_sha256=$recipeHash;recipe=$expected;navmesh=0;jump_links=0;demo_hz=20;owner_mark_key='F8';client_trace_mode=2;overlay=$false;client_team='choose fewer humans in standard PS team menu; equal allows either';archive_after='explicit owner DONE; retain session logs/demo/PS events, verify hashes, restore original memory after both processes stop'}
  $session|ConvertTo-Json -Depth 6|Set-Content (Join-Path $sessionDir 'session.json')
  $session|ConvertTo-Json -Depth 6|Set-Content (Join-Path $repo 'build/c127-active-session.json')
  $client=Start-Process (Join-Path $build 'CryMP-Client64.exe') -ArgumentList $clientArgs -WorkingDirectory $GameDirectory -PassThru
  $session.client_pid=$client.Id
  $session|ConvertTo-Json -Depth 6|Set-Content (Join-Path $sessionDir 'session.json')
  $session|ConvertTo-Json -Depth 6|Set-Content (Join-Path $repo 'build/c127-active-session.json')
  Write-Host "Shore ready: server $($run.process.Id), client $($client.Id). Join the team with fewer humans; F8 marks observations. Session $sessionDir"
 }
}finally{
 if($Smoke){
  if($run){Stop-Own $run.process};if($preflight){Stop-Own $preflight.process}
  foreach($f in @($(if($run){$run.log}),$(if($run){$run.trace}))){if($f -and (Test-Path $f)){Copy-Item -LiteralPath $f -Destination $sessionDir -Force}}
  if(Get-Process CryMP-Client64,CryMP-Server64 -ErrorAction SilentlyContinue){throw 'Game still active; memory restore deferred'}
  foreach($r in $records){Copy-Item -LiteralPath (Join-Path $snapshot $r.name) -Destination (Join-Path $memory $r.name) -Force;if((Get-FileHash (Join-Path $memory $r.name)).Hash -ne $r.sha256){throw 'Memory restore mismatch'}}
  ConvertTo-Json -InputObject @($records)|Set-Content (Join-Path $sessionDir 'memory-restored.json')
 }
}