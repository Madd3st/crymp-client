# C154: owner live preview; one disclosed delayed registered lower-spawn revive. Game resources are read from the owner's Crysis installation.
[CmdletBinding()]
param([string]$ProfileDirectory,[string]$GameDirectory,[switch]$PrepareOnly,[switch]$RecoverLast)
$ErrorActionPreference='Stop'
if(-not $ProfileDirectory){$ProfileDirectory=$PSScriptRoot}
$root=(Resolve-Path -LiteralPath $ProfileDirectory).Path
$source='abac72a9a1eb02793a2f4ab2b1a72f7c243dd72e'
$targets=@{memory=(Join-Path $env:LOCALAPPDATA 'WarsBots/Memory');nav=(Join-Path $env:LOCALAPPDATA 'WarsBots/Navmesh')}
function Save-Json($value,[string]$path){ConvertTo-Json -InputObject $value -Depth 12|Set-Content -LiteralPath $path -Encoding UTF8}
function Game-Processes {@(Get-Process|Where-Object {$_.ProcessName -match '^(CryMP-(Client|Server)64|Crysis.*)$'})}
function Quote-Arg([string]$value){if($value -match '["\r\n]'){throw 'Invalid path argument'};return '"'+$value+'"'}
function Stop-OwnServer($state){
 if(-not $state.server_pid){return};$proc=Get-Process -Id $state.server_pid -ErrorAction SilentlyContinue
 if($proc){if($proc.Path -ne $state.server_exe -or $proc.StartTime.ToUniversalTime().ToString('o') -ne $state.server_started){throw 'PID ownership changed; no process stopped'};Stop-Process -Id $proc.Id;$null=$proc.WaitForExit(10000)}
}
function Save-Session($state){Save-Json $state (Join-Path $state.directory 'session.json')}
function Snapshot-Data([string]$session){
 foreach($kind in $targets.Keys){$dir=$targets[$kind];$snap=Join-Path $session "$kind-before";New-Item -ItemType Directory $snap|Out-Null
  $existed=Test-Path -LiteralPath $dir;$files=@(if($existed){foreach($file in Get-ChildItem -LiteralPath $dir -File){Copy-Item -LiteralPath $file.FullName -Destination $snap;$hash=(Get-FileHash $file.FullName).Hash;if((Get-FileHash (Join-Path $snap $file.Name)).Hash -ne $hash){throw 'Snapshot hash mismatch'};@{name=$file.Name;sha256=$hash}}})
  Save-Json @{directory_existed=$existed;files=@($files)} (Join-Path $snap 'manifest.json')
 }
}
function Restore-Data($state){
 if(@(Game-Processes|Where-Object {$_.ProcessName -notmatch 'Server'}).Count){$state.phase='restore_deferred_client_active';Save-Session $state;Write-Warning 'Close the game, then run RESTORE.cmd. Client retained.';return}
 Stop-OwnServer $state
 if(@(Game-Processes).Count){$state.phase='restore_deferred_other_game';Save-Session $state;Write-Warning 'Other game retained. Run RESTORE.cmd after it closes.';return}
 foreach($file in @($state.server_log,($state.server_log -replace '\.log$','.bots.log'),$state.client_log,($state.client_log -replace '\.log$','.bots.log'))){if($file -and(Test-Path -LiteralPath $file)){Copy-Item -LiteralPath $file -Destination $state.directory -Force}}
 $demoRoot=[IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'WarsBots/Demos'))+[IO.Path]::DirectorySeparatorChar
 $demos=@(foreach($trace in @(($state.server_log -replace '\.log$','.bots.log'),($state.client_log -replace '\.log$','.bots.log'))){if($trace -and(Test-Path $trace)){foreach($match in [regex]::Matches((Get-Content $trace -Raw),'DEMO START path=(.*?) version=')){$file=[IO.Path]::GetFullPath($match.Groups[1].Value);if(-not $file.StartsWith($demoRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected demo path'};if(Test-Path -LiteralPath $file){Copy-Item -LiteralPath $file -Destination $state.directory -Force;$copy=Join-Path $state.directory ([IO.Path]::GetFileName($file));$hash=(Get-FileHash $file).Hash;if((Get-FileHash $copy).Hash -ne $hash){throw 'Demo copy mismatch'};@{name=[IO.Path]::GetFileName($file);sha256=$hash;bytes=(Get-Item $copy).Length}}}}})
 Save-Json @($demos) (Join-Path $state.directory 'demo-archive.json')
 foreach($kind in $targets.Keys){
  $dir=$targets[$kind];$snap=Join-Path $state.directory "$kind-before";$before=Get-Content (Join-Path $snap 'manifest.json') -Raw -Encoding UTF8|ConvertFrom-Json
  $after=Join-Path $state.directory "$kind-after";New-Item -ItemType Directory -Force $after|Out-Null
  if(Test-Path $dir){foreach($file in Get-ChildItem -LiteralPath $dir -File){Copy-Item -LiteralPath $file.FullName -Destination $after -Force;if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $after $file.Name)).Hash){throw 'After archive mismatch'}}}
  foreach($record in @($before.files)){if([IO.Path]::GetFileName($record.name) -ne $record.name){throw 'Invalid snapshot filename'};if((Get-FileHash (Join-Path $snap $record.name)).Hash -ne $record.sha256){throw 'Before snapshot corrupt'}}
  if(Test-Path $dir){foreach($file in Get-ChildItem -LiteralPath $dir -File){if($file.Name -notin @($before.files|ForEach-Object name)){if($file.DirectoryName -ne (Resolve-Path $dir).Path){throw 'Restore escaped target'};Remove-Item -LiteralPath $file.FullName}}}
  if(@($before.files).Count -and -not(Test-Path $dir)){New-Item -ItemType Directory -Force $dir|Out-Null}
  foreach($record in @($before.files)){Copy-Item -LiteralPath (Join-Path $snap $record.name) -Destination (Join-Path $dir $record.name) -Force;if((Get-FileHash (Join-Path $dir $record.name)).Hash -ne $record.sha256){throw 'Restore hash mismatch'}}
  $actual=@(if(Test-Path $dir){Get-ChildItem -LiteralPath $dir -File|ForEach-Object Name});$wanted=@($before.files|ForEach-Object name)
  if($actual.Count -ne $wanted.Count -or @($actual|Where-Object {$_ -notin $wanted}).Count){throw 'Restored file set mismatch'}
  if(-not $before.directory_existed -and(Test-Path $dir) -and -not(Get-ChildItem -LiteralPath $dir -Force)){Remove-Item -LiteralPath $dir}
  Save-Json @{exact_names_and_hashes=$true;files=@($before.files);directory_existed=$before.directory_existed} (Join-Path $state.directory "$kind-restored.json")
 }
 $state.phase='archived_restored';Save-Session $state;Write-Host "Archived and restored: $($state.directory)" -ForegroundColor Green
}
if($RecoverLast){
 $last=Get-Content (Join-Path $root 'last-session.txt') -Raw -Encoding UTF8;$last=[IO.Path]::GetFullPath($last.Trim());$allowed=[IO.Path]::GetFullPath((Join-Path $root 'Sessions'))+[IO.Path]::DirectorySeparatorChar
 if(-not $last.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Recovery path outside this profile'}
 $state=Get-Content (Join-Path $last 'session.json') -Raw -Encoding UTF8|ConvertFrom-Json
 if($state.source -ne $source -or $state.directory -ne $last){throw 'Recovery receipt mismatch'}
 if($state.phase -eq 'archived_restored'){Write-Host 'Already restored';return};Restore-Data $state;return
}
$manifest=Get-Content (Join-Path $root 'package-manifest.json') -Raw -Encoding UTF8|ConvertFrom-Json
foreach($file in $manifest.files){if($file.name -match '(^|[\\/])\.\.([\\/]|$)' -or [IO.Path]::IsPathRooted($file.name)){throw 'Invalid package path'};if((Get-FileHash -LiteralPath (Join-Path $root $file.name)).Hash -ne $file.sha256){throw "Package integrity failed: $($file.name)"}}
$build=Get-Content (Join-Path $root 'build.json') -Raw -Encoding UTF8|ConvertFrom-Json
$preset=Get-Content (Join-Path $root 'preset.json') -Raw -Encoding UTF8|ConvertFrom-Json
if($build.source -ne $source -or $build.ci -ne 40890940918 -or -not $build.ci_success){throw 'Exact approved CI build required'}
if($preset.map -notin @('Steelmill','Shore')){throw 'Unknown profile'}
if(-not $GameDirectory){$GameDirectory=(Get-Content (Join-Path $root 'game-dir.txt') -Raw -Encoding UTF8).Trim()}
$GameDirectory=(Resolve-Path -LiteralPath $GameDirectory).Path
$serverExe=Join-Path $root 'CryMP-Server64.exe';$clientExe=Join-Path $root 'CryMP-Client64.exe'
$tag=Get-Date -Format 'yyyyMMdd-HHmmss';$logs=Join-Path $env:LOCALAPPDATA 'WarsBots/Logs';$label=$preset.label
$serverLog=Join-Path $logs "$label-$tag.log";$clientLog=Join-Path $logs "$label-$tag-client.log";$preLog=Join-Path $logs "$label-$tag-pre.log"
$sets=' +wb_debug 0';foreach($prop in $preset.flags.PSObject.Properties){if($prop.Name -notmatch '^wb_[a-z0-9_]+$'){throw 'Invalid flag name'};$sets+=' +'+$prop.Name+' '+[int]$prop.Value}
$queries='';foreach($prop in $preset.critical.PSObject.Properties){$queries+=' +'+$prop.Name}
$baseArgs='-dir '+(Quote-Arg $GameDirectory)+' -headless -logfile __SHOWCASE_LOG__ +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port '+$preset.port+' +sv_gamerules '+$preset.mode+' +map '+$preset.level+$sets+$queries+' +wb_rules_info'
$preArgs=$baseArgs.Replace('__SHOWCASE_LOG__',(Quote-Arg $preLog))+' +wb_list +wb_status'
$roster=if($preset.map -eq 'Shore'){' +wb_add 2 normal +wb_remove 1 +wb_revive_at after 60 545 2 +wb_list +wb_status'}else{' +wb_add 8 normal +wb_list +wb_status'}
$serverArgs=$baseArgs.Replace('__SHOWCASE_LOG__',(Quote-Arg $serverLog))+$roster
$clientArgs='-dir '+(Quote-Arg $GameDirectory)+' -dx9 -logfile '+(Quote-Arg $clientLog)+' +r_displayInfo 0 +wb_log 2 +wb_net_ext 1 +wb_bot_client_smooth 1 +wb_bot_spectator_aim 1 +wb_move_trace 1 +bind f8 "wb_mark showcase"'
if($preset.map -in @('Steelmill','Shore')){$clientArgs+=' +bind f6 "wb_watch next" +bind f7 "wb_watch prev"'}
$clientArgs+=' +connect 127.0.0.1:'+$preset.port
foreach($pair in @(@($serverExe,$preArgs),@($serverExe,$serverArgs),@($clientExe,$clientArgs))){$bytes=[Text.Encoding]::UTF8.GetByteCount((Quote-Arg $pair[0])+' '+$pair[1]);if($bytes -ge 2048){throw "Command exceeds native buffer ($bytes bytes)"}}
if($PrepareOnly){@{source=$source;map=$preset.map;bots=$preset.retained_bots;server_arguments=$serverArgs;client_arguments=$clientArgs;preflight_arguments=$preArgs;package_hashes='PASS';runtime='NOT LAUNCHED'}|ConvertTo-Json -Depth 5;return}
if(@(Game-Processes).Count){throw 'Existing game retained; close it before starting another profile'}
if(Get-NetUDPEndpoint -LocalPort $preset.port -ErrorAction SilentlyContinue){throw 'Profile port occupied'}
New-Item -ItemType Directory -Force $logs|Out-Null
$sessionDir=Join-Path $root "Sessions/$label-$tag";New-Item -ItemType Directory $sessionDir|Out-Null
Snapshot-Data $sessionDir
$state=@{source=$source;map=$preset.map;bots=$preset.retained_bots;phase='snapshot_complete';directory=$sessionDir;server_exe=$serverExe;server_pid=$null;server_started=$null;server_log=$serverLog;client_log=$clientLog;client_pid=$null}
Save-Session $state;[IO.File]::WriteAllText((Join-Path $root 'last-session.txt'),$sessionDir,[Text.UTF8Encoding]::new($false))
$server=$null;$client=$null
function Start-CheckedServer([string]$ServerArguments,[string]$Log,[bool]$WithBots){
 if(@(Game-Processes).Count){throw 'Existing game retained before server launch'}
 $proc=Start-Process $serverExe -ArgumentList $ServerArguments -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
 $state.server_pid=$proc.Id;$state.server_started=$proc.StartTime.ToUniversalTime().ToString('o');Save-Session $state
 $deadline=[DateTime]::UtcNow.AddSeconds(60);$ready=$false
 do{Start-Sleep -Milliseconds 400;if($proc.HasExited){throw "Server exited; inspect $Log"};if(@(Game-Processes|Where-Object {$_.Id -ne $proc.Id}).Count){throw 'Other game appeared; retained'};$text=if(Test-Path $Log){Get-Content $Log -Raw}else{''};$trace=[IO.Path]::ChangeExtension($Log,'.bots.log');$side=if(Test-Path $trace){Get-Content $trace -Raw}else{''}
  if($text -match '\[Error\]|Lua Error|Script Error|RMI.*not a client' -or $side -match 'NAVMESH none|NAVMESH.*(?:mismatch|FAILED|refused|does not match its manifest)'){throw 'Server startup error; inspect session logs'}
  $ready=$text -match ('RULES INFO class='+$preset.mode) -and $text -match 'gate=1 levelReady=1'
  if($WithBots){$ready=$ready -and $side -match ('NAVMESH loaded.*'+$preset.map+': '+$preset.polys+' polys') -and $side -match 'NAVMESH links ON:'
   if($preset.map -eq 'Shore'){$ready=$ready -and $side -match 'NAVMESH link kinds 2/2' -and $side -match '#2 created EntityId=' -and $side -match '#1 ENTITY_EVENT_DONE' -and $side -match 'FRAME .*bots=1 server=1'}
   else{$ready=$ready -and $side -match '#8 created EntityId='}
  }else{$ready=$ready -and $text -match ('bots=0/'+$preset.flags.wb_maxbots)}
 }while(-not $ready -and [DateTime]::UtcNow -lt $deadline)
 if(-not $ready){throw 'Readiness timeout; game client not launched'}
 $checks=@(foreach($prop in $preset.critical.PSObject.Properties){$hits=[regex]::Matches($text,[regex]::Escape($prop.Name)+'\s*=\s*(-?\d+)');$value=if($hits.Count){[int]$hits[$hits.Count-1].Groups[1].Value}else{$null};@{name=$prop.Name;expected=$prop.Value;actual=$value;pass=($null -ne $value -and $value -eq $prop.Value)}})
 Save-Json @($checks) (Join-Path $sessionDir $(if($WithBots){'effective.json'}else{'preflight-effective.json'}));if(@($checks|Where-Object{-not $_.pass}).Count){throw 'Effective preset mismatch'}
 return $proc
}
try{
 if(@(Game-Processes).Count){throw 'Game appeared before temporary data install'}
 foreach($file in Get-ChildItem (Join-Path $root 'Data') -File){New-Item -ItemType Directory -Force $targets.nav|Out-Null;Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $targets.nav $file.Name) -Force}
 $preArgs|Set-Content (Join-Path $sessionDir 'preflight-arguments.txt');$serverArgs|Set-Content (Join-Path $sessionDir 'server-arguments.txt');$clientArgs|Set-Content (Join-Path $sessionDir 'client-arguments.txt')
 Write-Host "Preparing $($preset.map)..." -ForegroundColor Cyan
 $server=Start-CheckedServer $preArgs $preLog $false;Stop-OwnServer $state;foreach($file in @($preLog,($preLog -replace '\.log$','.bots.log'))){if(Test-Path $file){Copy-Item -LiteralPath $file -Destination $sessionDir}}
 $server=Start-CheckedServer $serverArgs $serverLog $true
 if(@(Game-Processes|Where-Object {$_.Id -ne $server.Id}).Count){throw 'Other game retained; no client launch'}
 $client=Start-Process $clientExe -ArgumentList $clientArgs -WorkingDirectory $GameDirectory -PassThru
 $state.client_pid=$client.Id;$state.phase='owner_playing';Save-Session $state
 Write-Host 'Lower spawn 545 starts once, 60 seconds after server loading. Spectator: F6 follows bot; F1 toggles view.'; Write-Host "$($preset.map): $($preset.retained_bots) bots. Close the game normally when finished; keep this window open." -ForegroundColor Green
 $state.logical_localappdata=$env:LOCALAPPDATA;Save-Session $state; $warned=$false;while(-not $client.HasExited){Start-Sleep -Seconds 2;if($server.HasExited -and -not $warned){Write-Warning 'Server exited. Close the client, then cleanup will run.';$warned=$true}}
}finally{Restore-Data $state}
