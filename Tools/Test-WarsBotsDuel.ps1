[CmdletBinding()]
param([Parameter(Mandatory)][string]$ServerDirectory,[Parameter(Mandatory)][string]$ExpectedSha,
 [string]$GameDirectory='E:\Steam\steamapps\common\Crysis',
 [ValidateRange(45,180)][int]$Seconds=60,[ValidateRange(1,4)][int]$Repeats=3,
 [ValidateSet('SOCOM','Mixed')][string]$WeaponMode='SOCOM',
 [string]$OutputDirectory='build/p0-socom-duel')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$out=[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
if(Test-Path -LiteralPath $out){throw 'Use a new evidence directory'}
$dir=(Resolve-Path -LiteralPath $ServerDirectory).Path
$m=Get-Content -LiteralPath (Join-Path $dir 'claude-build.json') -Raw | ConvertFrom-Json
$exe=Join-Path $dir 'CryMP-Server64.exe'
if(-not ([string]$m.sha).StartsWith($ExpectedSha) -or (Get-FileHash $exe).Hash -ne $m.server){throw 'Source/EXE mismatch'}
function Servers { @(Get-CimInstance Win32_Process -Filter "Name='CryMP-Server64.exe'") }
if((Servers).Count -or (Get-CimInstance Win32_Process -Filter "Name='CryMP-Client64.exe'")){throw 'Existing game prevents shared-profile QA'}
if(Get-NetUDPEndpoint -LocalPort 64160 -ErrorAction SilentlyContinue){throw 'QA port busy'}
New-Item -ItemType Directory -Path $out | Out-Null
$profile=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games/Crysis'
$memory=Join-Path $profile 'WarsBots/Multiplayer_IA_Steelmill.mem'
$backup=Join-Path $out 'initial-memory.mem'
Copy-Item -LiteralPath $memory -Destination $backup
$memoryHash=(Get-FileHash $backup).Hash
$session=(Split-Path $out -Leaf) -replace '[^A-Za-z0-9_-]','_'
$runs=@();$p=$null;$joined=$false
try {
 for($trial=1;$trial -le $Repeats;$trial++) {
  if((Servers).Count){throw 'Server appeared'}
  Copy-Item -LiteralPath $backup -Destination $memory -Force
  if((Get-FileHash $memory).Hash -ne $memoryHash){throw 'Memory restore failed'}
  $name="WarsBots-$session-r$trial.log";$log=Join-Path $profile $name;$trace=$log -replace '\.log$','.bots.log'
  if(Test-Path -LiteralPath $log){throw 'Evidence log exists'}
  # Coordinates measured during the prior native pilot, not invented roof/ground points.
  $qaOnly=if($WeaponMode -eq 'SOCOM'){1}else{0}
  $args="-dir `"$GameDirectory`" -headless -logfile $name +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port 64160 +sv_gamerules InstantAction +map multiplayer/ia/steelmill +wb_probe_local_test 1 +wb_maxbots 2 +wb_skill 3 +wb_qa_socom_only $qaOnly +wb_fire_diagnostics 1 +wb_debug 1 +wb_probe_spawn 1224.98 1331.85 136.2 +wb_bot_start +wb_probe_spawn 1242.72 1331.76 136.2 +wb_bot_start +wb_list"
  $p=Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
  Write-Host "START trial=$trial pid=$($p.Id) source=$($m.sha)"
  $deadline=[DateTime]::UtcNow.AddSeconds($Seconds);$raw='';$joined=$false
  while([DateTime]::UtcNow -lt $deadline -and -not $p.HasExited){
   Start-Sleep -Seconds 5
   if(Test-Path -LiteralPath $trace){$raw=Get-Content -LiteralPath $trace -Raw}
   $joined=$raw -match 'clients=[1-9]'
   if($joined){throw 'Human joined; leaving this server running'}
  }
  $early=$p.HasExited
  if(-not $p.HasExited){Stop-Process -Id $p.Id}
  $null=$p.WaitForExit(15000);$p.Dispose();$p=$null;Start-Sleep -Milliseconds 300
  foreach($f in @($log,$trace)){if(Test-Path -LiteralPath $f){Copy-Item -LiteralPath $f -Destination $out}}
  $raw=Get-Content -LiteralPath $trace -Raw
  $ids=@([regex]::Matches($raw,'#(\d+) created EntityId=') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
  $classes=@([regex]::Matches($raw,'FIRE SHOT .*class=(\w+)') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
  $ok=-not $early -and $ids.Count -eq 2 -and $raw -match 'NAV ready' -and (($WeaponMode -eq 'Mixed' -and $classes.Count -gt 0) -or ($classes.Count -eq 1 -and $classes[0] -eq 'SOCOM'))
  $runs+=@{tag="candidate-r$trial-n2";session=$session;variant='candidate';repeat=$trial;bots=2;seconds=$Seconds;source_sha=$m.sha;server_sha256=$m.server;memory_sha256=$memoryHash;log=[IO.Path]::GetFileName($trace);completed=$ok;roster=@(@{number=1;style='universal';skill='elite'},@{number=2;style=$(if($WeaponMode -eq 'SOCOM'){'universal'}else{'assault'});skill='elite'});fixture=$WeaponMode+'; native ammo/armor; free movement; subsequent respawns native random'}
  $runs | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $out 'runs.json') -Encoding utf8
  Write-Host "END trial=$trial completed=$ok"
  if(-not $ok){throw 'Duel fixture check failed'}
 }
} finally {
 if($p -and -not $p.HasExited -and -not $joined){Stop-Process -Id $p.Id;$null=$p.WaitForExit(15000)}
 if((Servers).Count -eq 0){Copy-Item -LiteralPath $backup -Destination $memory -Force}
}
