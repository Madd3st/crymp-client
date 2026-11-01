# Frozen C031 headless probes. Diagnostic-only; no build, install, or client launch.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$BuildDirectory,
 [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{8,40}$')][string]$ExpectedSource,
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [string]$Manifest=(Join-Path $PSScriptRoot 'Probes/c031-v1.json'),
 [string[]]$Cases=@(),
 [ValidateSet('Steelmill','Quarry','Armada','Outpost')][string[]]$Maps=@('Steelmill','Quarry','Armada','Outpost'),
 [string]$OutputDirectory='build/c031-probes',
 [ValidateRange(1024,49151)][int]$Port=46040,
 [string]$PythonExecutable=(Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe')
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=(Resolve-Path (Join-Path $repo $BuildDirectory)).Path
$m=Get-Content (Join-Path $build 'claude-build.json') -Raw|ConvertFrom-Json
$exe=Join-Path $build 'CryMP-Server64.exe'
if($m.data_layout -ne 'local-v1' -or -not $m.sha.StartsWith($ExpectedSource) -or (Get-FileHash $exe).Hash -ne $m.server){throw 'Frozen source/EXE mismatch'}
function GameActive {Get-Process CryMP-Server64,CryMP-Client64 -ErrorAction SilentlyContinue}
if(GameActive){throw 'Owner/game active: probes deferred'}
if(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue){throw 'Probe port busy'}
& $PythonExecutable -B (Join-Path $PSScriptRoot 'warsbots_probe.py') --manifest $Manifest
if($LASTEXITCODE){throw 'Probe manifest invalid'}
$fixture=Get-Content $Manifest -Raw|ConvertFrom-Json
foreach($id in $Cases){if($id -notin $fixture.cases.id){throw "Unknown scenario $id"}}
$selected=@($fixture.cases|Where-Object {$_.map -in $Maps -and (-not $Cases.Count -or $_.id -in $Cases)})
if(-not $selected.Count){throw 'No scenarios selected'}
$out=[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
if(Test-Path $out){throw 'Fresh output directory required'}
New-Item -ItemType Directory $out|Out-Null
Copy-Item $Manifest (Join-Path $out 'manifest.json')
$data=Join-Path $env:LOCALAPPDATA 'WarsBots';$memory=Join-Path $data 'Memory'
$backup=Join-Path $out 'initial-memory';New-Item -ItemType Directory $backup|Out-Null
$states=@();$p=$null;$joined=$false;$runs=@()
foreach($map in @($selected.map|Sort-Object -Unique)){
 foreach($ext in @('mem','nav.json')){
  $name="Multiplayer_IA_$map.$ext";$path=Join-Path $memory $name;$exists=Test-Path $path
  $hash=if($exists){(Get-FileHash $path).Hash}else{$null}
  if($exists){Copy-Item $path (Join-Path $backup $name)}
  $states+=@{name=$name;path=$path;existed=$exists;hash=$hash}
 }
}
function Restore-Memory {
 if(GameActive){throw 'Cannot restore memory while a game is active'}
 foreach($s in $states){
  if($s.existed){Copy-Item (Join-Path $backup $s.name) $s.path -Force;if((Get-FileHash $s.path).Hash -ne $s.hash){throw 'Memory restore mismatch'}}
  elseif(Test-Path -LiteralPath $s.path){
   # Exact generated file only, verified to be within the named data directory.
   if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($s.path)) -ne [IO.Path]::GetFullPath($memory)){throw 'Unsafe restore target'}
   Remove-Item -LiteralPath $s.path
  }
 }
}
try{
 foreach($c in $selected){
  Restore-Memory
  $caseDir=Join-Path $out $c.id;New-Item -ItemType Directory $caseDir|Out-Null
  $name='C031-'+(Split-Path $out -Leaf)+'-'+$c.id+'.log'
  $log=Join-Path $data ('Logs/'+$name);$trace=$log -replace '\.log$','.bots.log'
  if((Test-Path $log) -or (Test-Path $trace)){throw 'Evidence log exists'}
  $commands=''
  foreach($b in $c.bots){
   $xyz=(@($b.position)|ForEach-Object {([double]$_).ToString('R',[Globalization.CultureInfo]::InvariantCulture)}) -join ' '
   $commands+=" +wb_probe_spawn $xyz +wb_bot_start"
  }
  if($c.kind -eq 'jump'){$commands+=" +wb_select 1 +wb_bot_jump $($c.jump_mode) 2"}
  $password=[guid]::NewGuid().ToString('N')
  $args="-dir `"$GameDirectory`" -headless -logfile `"$log`" +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_password $password +sv_port $Port +sv_gamerules InstantAction +map multiplayer/ia/$($c.map) +wb_probe_local_test 1 +wb_demo_record 1 +wb_demo_hz 20 +wb_bot_pace 1 +wb_spawn_exclude195 1 +wb_revive_guard 1 +wb_reachable_goals 0 +wb_trap_recovery 1 +wb_physics_diagnostics 1 +wb_move_trace 1 +wb_debug 1 +wb_maxbots 2$commands +wb_list"
  Write-Host "PROBE START $($c.id) sha=$($m.sha)"
  $p=Start-Process $exe -ArgumentList $args -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
  $deadline=[DateTime]::UtcNow.AddSeconds(60);$text='';$ready=$false
  while([DateTime]::UtcNow -lt $deadline -and -not $p.HasExited){
   Start-Sleep -Milliseconds 500
   if(Test-Path $trace){$text=Get-Content $trace -Raw}
   if($text -match 'clients=[1-9]'){$joined=$true;throw 'Real client joined: retaining server/memory'}
   if($text -match 'NAV ready' -and ([regex]::Matches($text,'created EntityId=')).Count -eq $c.bots.Count -and $text -match 'DEMO START'){$ready=$true;break}
  }
  if(-not $ready){throw "Scenario did not become ready: $($c.id)"}
  $deadline=[DateTime]::UtcNow.AddSeconds($c.seconds)
  while([DateTime]::UtcNow -lt $deadline -and -not $p.HasExited){
   Start-Sleep -Seconds 2
   if((Get-Content $trace -Raw) -match 'clients=[1-9]'){$joined=$true;throw 'Real client joined: retaining server/memory'}
  }
  $early=$p.HasExited
  if(-not $p.HasExited){Stop-Process -Id $p.Id};$null=$p.WaitForExit(15000);$p.Dispose();$p=$null
  $text=Get-Content $trace -Raw;$engine=Get-Content $log -Raw
  Copy-Item $log (Join-Path $caseDir 'server.log');Copy-Item $trace (Join-Path $caseDir 'server.bots.log')
  $match=[regex]::Match($text,'DEMO START path=(.+?) version=2 hz=20')
  if(-not $match.Success){throw 'Expected demo path missing'}
  $demoPath=$match.Groups[1].Value
  Copy-Item -LiteralPath $demoPath -Destination (Join-Path $caseDir 'run.wbdemo')
  & $PythonExecutable -B (Join-Path $PSScriptRoot 'warsbots_probe.py') --manifest $Manifest --case $c.id --demo (Join-Path $caseDir 'run.wbdemo') --out (Join-Path $caseDir 'result.json')
  if($LASTEXITCODE){throw "Demo measurements failed: $($c.id)"}
  $result=Get-Content (Join-Path $caseDir 'result.json') -Raw|ConvertFrom-Json
  if($result.source_sha -ne $m.sha){throw 'Recorded demo source mismatch'}
  $errors=@([regex]::Matches($engine,'(?m)^.*(?:CRASH DETECTED|Lua Error|Script Error|\[Error\]|Unknown command: wb_).*$')|ForEach-Object {$_.Value})
  $run=[ordered]@{id=$c.id;map=$c.map;source_sha=$m.sha;server_sha256=$m.server;fixture_sha256=(Get-FileHash $Manifest).Hash;seconds=$c.seconds;runtime_ok=($result.runtime_ok -and -not $early -and -not $errors.Count);outcome=$result.outcome;engine_errors=$errors;demo_original=$demoPath;result="$($c.id)/result.json";memory=@($states|Where-Object {$_.name -like "Multiplayer_IA_$($c.map).*"});kind=$c.kind;roster='normal/universal';flags='pace1,guard1,exclude1951,B0,trap1,trace1,demo1/hz20; default other flags'}
  $runs+=$run
  @{schema='warsbots-probe-batch/1';runs=@($runs)}|ConvertTo-Json -Depth 9|Set-Content -Encoding utf8 (Join-Path $out 'runs.json')
  Write-Host "PROBE END $($c.id) runtime=$($run.runtime_ok) outcome=$($result.outcome)"
  if(-not $run.runtime_ok){throw 'Probe runtime failure; evidence retained'}
 }
}finally{
 if($p -and -not $p.HasExited -and -not $joined){Stop-Process -Id $p.Id;$null=$p.WaitForExit(15000)}
 if(-not (GameActive)){Restore-Memory;@{restored=$true;files=$states}|ConvertTo-Json -Depth 5|Set-Content -Encoding utf8 (Join-Path $out 'memory-restored.json')}
}
