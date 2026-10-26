[CmdletBinding()]
param(
 [Parameter(Mandatory,Position=0)][string]$Probe,
 [Parameter(Mandatory)][Alias('Build')][string]$BuildDirectory,
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [string]$OutputDirectory='',
 [string]$FirstTrace='',
 [ValidateRange(1024,49151)][int]$Port=46120
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$spec=Get-Content -LiteralPath $Probe -Raw|ConvertFrom-Json
if($spec.schema -ne 1 -or $spec.map -ne 'multiplayer/ia/steelmill'){throw 'Unsupported probe schema/map'}
if($spec.duration_s -lt 30 -or $spec.duration_s -gt 180 -or $spec.repeat -lt 1 -or $spec.repeat -gt 10){throw 'Invalid probe bounds'}
if(Get-Process CryMP-Client64,CryMP-Server64 -ErrorAction SilentlyContinue|Where-Object{-not $_.HasExited}){throw 'Existing game; refusing shared profile QA'}
if(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue){throw 'Port in use'}
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$exe=Join-Path $build 'CryMP-Server64.exe'
$manifestPath=Join-Path $build 'claude-build.json'
if(-not(Test-Path $manifestPath)){$manifestPath=Join-Path $build 'ci-build.json'}
$manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json
if((Get-FileHash -LiteralPath $exe).Hash -ne $manifest.server){throw 'Frozen server hash mismatch'}
if($OutputDirectory){$out=[IO.Path]::GetFullPath($OutputDirectory)}else{$out=Join-Path $repo ('build/probes/'+$spec.id+'-'+(Get-Date -Format yyyyMMdd-HHmmss))}
if(Test-Path $out){throw 'Fresh output directory required'}
New-Item -ItemType Directory -Path $out -Force|Out-Null
$memory=Join-Path $env:LOCALAPPDATA 'WarsBots/Memory';$snapshot=Join-Path $out 'memory-before';New-Item -ItemType Directory $snapshot|Out-Null
$memoryManifest=@()
foreach($name in @('Multiplayer_IA_Steelmill.mem','Multiplayer_IA_Steelmill.nav.json')){
 $file=Join-Path $memory $name
 if(-not(Test-Path $file)){throw 'Existing memory/nav required'}
 Copy-Item -LiteralPath $file -Destination $snapshot
 $memoryManifest+=@{name=$name;sha256=(Get-FileHash $file).Hash}
}
$memoryManifest|ConvertTo-Json|Set-Content (Join-Path $snapshot 'manifest.json')
$levelItems=@{}
$pak=[IO.Compression.ZipFile]::OpenRead((Join-Path $GameDirectory 'Game/Levels/Multiplayer/IA/Steelmill/level.pak'))
try{
 $reader=[IO.StreamReader]::new($pak.GetEntry('mission_mission0.xml').Open())
 try{[xml]$levelXml=$reader.ReadToEnd()}finally{$reader.Dispose()}
 foreach($entity in $levelXml.SelectNodes('//Entity[@EntityId][@EntityClass]')){$levelItems[[int]$entity.EntityId]=[string]$entity.EntityClass}
}finally{$pak.Dispose()}
$expected=[ordered]@{wb_probe_local_test=1;wb_move_trace=1;wb_demo_record=1}
foreach($p in $spec.cvars.PSObject.Properties){
 if($p.Name -notmatch '^wb_[a-z0-9_]+$' -or [string]$p.Value -notmatch '^-?\d+(\.\d+)?$'){throw 'Unsafe CVar'}
 $expected[$p.Name]=$p.Value
}
$sets=' +wb_debug 1';$queries=''
foreach($key in $expected.Keys){$sets+=" +$key $($expected[$key])";$queries+=" +$key"}
$setup=''
foreach($line in $spec.setup){
 if($line -notmatch '^wb_(add|teleport|probe_item) [A-Za-z0-9_. -]+$'){throw 'Unsupported setup command'}
 $setup+=' +'+$line
}
$expected|ConvertTo-Json|Set-Content (Join-Path $out 'recipe.json')
function Restore-ProbeMemory {
 if(Get-Process CryMP-Client64,CryMP-Server64 -ErrorAction SilentlyContinue|Where-Object{-not $_.HasExited}){throw 'Live process: memory restore deferred'}
 foreach($f in $memoryManifest){$dest=Join-Path $memory $f.name;Copy-Item -LiteralPath (Join-Path $snapshot $f.name) -Destination $dest -Force;if((Get-FileHash $dest).Hash -ne $f.sha256){throw 'Memory hash restore failed'}}
}
function Invoke-ProbeProcess([string]$tag,[bool]$withSetup) {
 Restore-ProbeMemory
 if((Get-FileHash -LiteralPath $exe).Hash -ne $manifest.server){throw 'Frozen EXE changed'}
 ${unique}='Probe-'+$spec.id+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)+'-'+$tag
 $log=Join-Path $out ($unique+'.log');$trace=$log -replace '\.log$','.bots.log';$traceSource=Join-Path (Join-Path $env:LOCALAPPDATA 'WarsBots/Logs') ($unique+'.bots.log');$p=$null
 # CryEngine serializes +map before subsequent console commands, as Test-WarsBotsTactics does.
 # Wait for the loaded-level marker before timing; settings were validated in a separate no-actor process.
 $commands=$sets+$queries;if($withSetup){$commands+=$setup}
 $args="-dir `"$GameDirectory`" -headless -logfile `"$log`" +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $Port +sv_gamerules InstantAction +map $($spec.map)$commands"
 [IO.File]::WriteAllText((Join-Path $out ($tag+'-arguments.txt')),$args)
 try{
  $p=Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
  $deadline=[DateTime]::UtcNow.AddSeconds(40);$loaded=$false
  do{
   Start-Sleep -Milliseconds 500;$engine=if(Test-Path $log){Get-Content $log -Raw}else{''}
   $loaded=$engine -match '\*LOADING: Level multiplayer/ia/steelmill loading time'
   if($engine -match 'CRASH DETECTED|Failed initializing game|\[Error\]'){throw 'Engine startup error'}
  }while(-not $loaded -and -not $p.HasExited -and [DateTime]::UtcNow -lt $deadline)
  if(-not $loaded){throw 'Level not ready'}
  $checks=@()
  foreach($key in $expected.Keys){$matches=[regex]::Matches($engine,[regex]::Escape($key)+'\s*=\s*(-?\d+(?:\.\d+)?)');$actual=if($matches.Count){[double]$matches[$matches.Count-1].Groups[1].Value}else{$null};$checks+=[pscustomobject]@{name=$key;expected=$expected[$key];effective=$actual;pass=($null -ne $actual -and $actual -eq [double]$expected[$key])}}
  $checks|ConvertTo-Json|Set-Content (Join-Path $out ($tag+'-effective.json'))
  $checks|Format-Table name,expected,effective,pass|Out-Host
  if(@($checks|Where-Object{-not $_.pass}).Count){throw 'Effective recipe mismatch'}
  if($withSetup){
   $end=[DateTime]::UtcNow.AddSeconds([int]$spec.duration_s)
   while([DateTime]::UtcNow -lt $end){if($p.HasExited){throw 'Unexpected process exit'};Start-Sleep -Seconds 1}
  }
 }finally{
  if($p -and -not $p.HasExited){$actual=Get-Process -Id $p.Id -ErrorAction SilentlyContinue;if($actual.Path -ne $exe){throw 'PID/path mismatch'};Stop-Process -Id $p.Id;if(-not $p.WaitForExit(10000)){throw 'Server did not exit'};Start-Sleep -Milliseconds 200}
  Restore-ProbeMemory
 }
 if(Test-Path -LiteralPath $traceSource){Copy-Item -LiteralPath $traceSource -Destination $trace}
 return $trace
}
function Metrics-FromTrace([string]$file){
 $text=Get-Content -LiteralPath $file -Raw
 $m=[ordered]@{unreachable=([regex]::Matches($text,'weapon \d+ unreachable; skipping it')).Count;stalls=([regex]::Matches($text,'TRAP RECOVERY')).Count;glances=([regex]::Matches($text,'LOOK TRACE[^\n]*reason=glance')).Count}
 $start=[regex]::Match($text,'(?m)^([\d.]+).*AI state=SPAWNING');$t0=if($start.Success){[double]$start.Groups[1].Value}else{0}
 $ids=@{};foreach($entry in $levelItems.GetEnumerator()){$ids[$entry.Key]=$entry.Value}
 foreach($match in [regex]::Matches($text,'PROBE ITEM (\w+) id=(\d+)')){$ids[[int]$match.Groups[2].Value]=$match.Groups[1].Value}
 foreach($match in [regex]::Matches($text,'(?m)^([\d.]+).*REAL PICKUP actor=\d+ item=(\d+) acquired=1')){$class=$ids[[int]$match.Groups[2].Value];if($class -and -not $m.Contains('pickup:'+$class)){$m['pickup:'+$class]=[double]$match.Groups[1].Value-$t0}}
 foreach($match in [regex]::Matches($text,'#(\d+) combat kill')){$key='kills:'+$match.Groups[1].Value;if(-not $m.Contains($key)){$m[$key]=0};$m[$key]++}
 if($text -match 'PROBE ITEM refused|PROBE ITEM \w+ id=0 |Teleport refused|AI start refused|Unknown command|Usage: wb_probe_item'){throw 'Setup refused/unsupported'}
 return $m
}
$results=@();$failure=$null
try{
 # Same exact settings, no setup: effective CVars must pass before any bot is added.
 $null=Invoke-ProbeProcess 'preflight' $false
 for($repeat=1;$repeat -le $spec.repeat;$repeat++){
  if($repeat -eq 1 -and $FirstTrace){$trace=(Resolve-Path -LiteralPath $FirstTrace).Path}else{$trace=Invoke-ProbeProcess ('repeat-'+$repeat) $true}
  $m=Metrics-FromTrace $trace;$assertions=@()
  foreach($a in $spec.assert){
   $exists=$m.Contains($a.metric);$value=if($exists){$m[$a.metric]}elseif($a.metric -like 'kills:*'){0}else{$null};$pass=$false
   switch($a.op){
    'eq' {$pass=($null -ne $value -and $value -eq $a.value)}
    'le_seconds' {$pass=($null -ne $value -and $value -le $a.value)}
    'ge_ratio_of' {$other=if($m.Contains($a.other)){$m[$a.other]}else{0};$pass=($value -gt 0 -and $value -ge $a.value*$other)}
    default {throw 'Unsupported assertion operator'}
   }
   $assertions+=@{metric=$a.metric;op=$a.op;actual=$value;expected=$a.value;pass=$pass}
  }
  $results+=@{repeat=$repeat;metrics=$m;assertions=$assertions;pass=(@($assertions|Where-Object{-not $_.pass}).Count -eq 0);trace=$trace}
 }
}catch{$failure=$_.Exception.Message}finally{
 Restore-ProbeMemory
 $medians=@{};$keys=@($results|ForEach-Object{$_.metrics.Keys}|Sort-Object -Unique)
 foreach($key in $keys){$v=@($results|ForEach-Object{if($_.metrics.Contains($key)){$_.metrics[$key]}}|Sort-Object);if($v.Count){$medians[$key]=$v[[int][math]::Floor(($v.Count-1)/2)]}}
 $pass=($null -eq $failure -and $results.Count -eq $spec.repeat -and @($results|Where-Object{-not $_.pass}).Count -eq 0)
 $restoredHashes=@($memoryManifest|ForEach-Object{@{name=$_.name;sha256=(Get-FileHash (Join-Path $memory $_.name)).Hash;expected=$_.sha256}})
 @{memory_after=$restoredHashes;schema='warsbots-probe-result/1';id=$spec.id;source=$manifest.sha;server_sha256=$manifest.server;recipe=$expected;results=$results;reused_first_trace=$FirstTrace;medians=$medians;pass=$pass;error=$failure;memory_restored=$true;seed='native random_device, setup only is fixed'}|ConvertTo-Json -Depth 12|Set-Content (Join-Path $out 'probe-result.json')
}
if(-not $pass){Write-Error "Probe FAIL: $failure";exit 1}
Write-Host "Probe PASS: $out";exit 0
