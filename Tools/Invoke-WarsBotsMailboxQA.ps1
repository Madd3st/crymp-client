param([Parameter(Mandatory)][string]$Task,[Parameter(Mandatory)][string]$Sha,[ValidateSet('Shore','Steelmill')][string]$Map='Shore',[int]$Seconds=300)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent;Set-Location $repo
$short=$Sha.Substring(0,8);$exe=Join-Path $repo "build/ci-$short/CryMP-Server64.exe"
$out=Join-Path $repo "build/$Task-$Map-$short";if(Test-Path $out){throw 'Run directory already exists; no duplicate run'}
$artifact=Get-Content (Join-Path (Split-Path $exe) 'sha256.txt');foreach($line in $artifact){$hash,$name=$line -split '\s+',2;if((Get-FileHash (Join-Path (Split-Path $exe) $name.TrimStart('*'))).Hash -ne $hash){throw 'Artifact hash mismatch'}}
if(Get-Process CryMP-Server64,CryMP-Client64,MSBuild,cl -ErrorAction SilentlyContinue){throw 'Existing game/build'}
New-Item -ItemType Directory $out|Out-Null
$memory=Join-Path $env:LOCALAPPDATA 'WarsBots/Memory';$snap=Join-Path $out 'memory-before';New-Item -ItemType Directory $snap|Out-Null
$records=@(foreach($f in Get-ChildItem $memory -File){Copy-Item -LiteralPath $f.FullName -Destination $snap;@{name=$f.Name;sha256=(Get-FileHash $f.FullName).Hash}})
ConvertTo-Json -InputObject @($records)|Set-Content (Join-Path $snap 'manifest.json')
$expected=Get-Content build/c127-smoke-4f3a1d72/recipe.json -Raw|ConvertFrom-Json
if($Map -eq 'Steelmill'){$expected.wb_bot_navmesh=1;$expected.wb_bot_jump_links=1;$expected.wb_ps_capture=0;$expected.wb_ps_buy=0;$expected.wb_bot_first_primary=0;$expected.wb_bot_withdraw_cover=0}
$expected|ConvertTo-Json|Set-Content (Join-Path $out 'recipe.json')
$set=' +wb_debug 1';$queries='';foreach($prop in $expected.PSObject.Properties){if($prop.Name -notin @('wb_bot_corners','wb_bot_jump_sanity')){$set+=' +'+$prop.Name+' '+$prop.Value};$queries+=' +'+$prop.Name}
$rules=if($Map -eq 'Shore'){'PowerStruggle'}else{'InstantAction'};$path=if($Map -eq 'Shore'){'ps/Shore'}else{'ia/Steelmill'}
try{
 foreach($phase in @('preflight','run')){
  if(Get-Process CryMP-Server64,CryMP-Client64 -ErrorAction SilentlyContinue){throw 'Existing game before phase'}
  $tag="$Task-$short-$phase";$log=Join-Path $env:LOCALAPPDATA "WarsBots/Logs/$tag.log";$trace=[IO.Path]::ChangeExtension($log,'.bots.log');if(Test-Path $log){throw 'Log already exists'}
  $setup=' +wb_rules_info +wb_list';if($phase -eq 'run'){$setup=' +wb_rules_info +wb_add 6 normal +wb_rules_info +wb_list';if($Map -eq 'Shore'){$setup+=' +wb_ps_summary'}}
  $args="-dir E:/Steam/steamapps/common/Crysis -headless -logfile `"$log`" +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port 46150 +sv_gamerules $rules +map multiplayer/$path$set$queries$setup"
  $args|Set-Content (Join-Path $out "$phase-arguments.txt");$p=$null
  try{
   $p=Start-Process $exe -ArgumentList $args -WorkingDirectory 'E:/Steam/steamapps/common/Crysis' -WindowStyle Hidden -PassThru
   $p.Id|Set-Content (Join-Path $out "$phase-pid.txt")
   $deadline=[DateTime]::UtcNow.AddSeconds(50)
   do{Start-Sleep -Milliseconds 500;if($p.HasExited){throw 'Startup exited'};$text=if(Test-Path $log){Get-Content $log -Raw}else{''}}while($text -notmatch "RULES INFO class=$rules" -and [DateTime]::UtcNow -lt $deadline)
   if($text -notmatch "RULES INFO class=$rules"){throw 'Rules readiness timeout'}
   $checks=@(foreach($prop in $expected.PSObject.Properties){$m=[regex]::Matches($text,[regex]::Escape($prop.Name)+'\s*=\s*(-?\d+)');$v=if($m.Count){[int]$m[$m.Count-1].Groups[1].Value}else{$null};@{name=$prop.Name;expected=$prop.Value;actual=$v;pass=($null -ne $v -and $v -eq $prop.Value)}})
   $checks|ConvertTo-Json|Set-Content (Join-Path $out "$phase-effective.json");if(@($checks|Where-Object{-not $_.pass}).Count){throw 'Recipe mismatch'}
   if($Map -eq 'Steelmill' -and $phase -eq 'run'){
    $navDeadline=[DateTime]::UtcNow.AddSeconds(10)
    while($text -notmatch 'NAVMESH loaded' -and [DateTime]::UtcNow -lt $navDeadline){Start-Sleep -Milliseconds 500;$text=Get-Content $log -Raw}
    if($text -notmatch 'NAVMESH loaded'){throw 'Navmesh not loaded'}
   }
   if($phase -eq 'preflight' -and $text -match '\[Error\]|\[Script\]|Lua Error|Script Error'){throw 'Startup Lua error before roster'}
   if($phase -eq 'run'){
    $end=[DateTime]::UtcNow.AddSeconds($Seconds)
    while([DateTime]::UtcNow -lt $end){if($p.HasExited){throw 'Runtime crashed/exited'};if(Get-Process CryMP-Client64 -ErrorAction SilentlyContinue){throw 'Owner client appeared; defer QA'};Start-Sleep -Seconds 2}
    @{task=$Task;sha=$Sha;map=$Map;seconds=$Seconds;complete=$true}|ConvertTo-Json|Set-Content (Join-Path $out 'run-complete.json')
   }
  }finally{
   if($p -and -not $p.HasExited){if($p.Path -ne $exe){throw 'Own PID/path mismatch'};Stop-Process -Id $p.Id;$null=$p.WaitForExit(10000)}
   foreach($f in @($log,$trace)){if(Test-Path $f){Copy-Item -LiteralPath $f -Destination $out}}
   foreach($f in Get-ChildItem (Join-Path $env:LOCALAPPDATA 'WarsBots/Logs') -Filter '*.wbdemo'|Where-Object{$_.LastWriteTimeUtc -ge (Get-Item $log).CreationTimeUtc}){Copy-Item -LiteralPath $f.FullName -Destination $out}
   $after=Join-Path $out "memory-after-$phase";New-Item -ItemType Directory $after|Out-Null;Get-ChildItem $memory -File|Copy-Item -Destination $after
   # New files belong to this isolated run and are archived above. Restore absent-file state for paired QA.
   foreach($f in Get-ChildItem $memory -File){if($f.Name -notin @($records|ForEach-Object{$_.name})){
    if($f.DirectoryName -ne (Resolve-Path -LiteralPath $memory).Path){throw 'Memory path escaped target directory'}
    if((Get-FileHash -LiteralPath $f.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $after $f.Name)).Hash){throw 'New memory archive hash mismatch'}
    Remove-Item -LiteralPath $f.FullName
   }}
   foreach($r in $records){Copy-Item -LiteralPath (Join-Path $snap $r.name) -Destination (Join-Path $memory $r.name) -Force;if((Get-FileHash (Join-Path $memory $r.name)).Hash -ne $r.sha256){throw 'Actual memory restore hash mismatch'}}
   ConvertTo-Json -InputObject @($records)|Set-Content (Join-Path $out 'memory-restored.json')
  }
 }
}catch{$_|Out-String|Set-Content (Join-Path $out 'error.txt');throw}
Write-Output "DONE $out"
