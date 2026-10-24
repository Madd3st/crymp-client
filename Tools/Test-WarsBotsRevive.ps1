# Explicit single-bot local IA diagnostics; never starts a client or replaces installed EXEs.
[CmdletBinding()]
param(
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [Parameter(Mandatory)][string]$BuildDirectory,
 [Parameter(Mandatory)][string]$ExpectedSource,
 [string]$OutputDirectory='build/revive-offset-qa',
 [ValidateSet('zero','legacy','native')][string[]]$Modes=@('zero','legacy','native'),
 [ValidateRange(0,3)][int]$Hooks=0
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$out=[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
if(Test-Path -LiteralPath $out){throw 'Fresh output directory required'}
function Servers {@(Get-Process CryMP-Server64 -ErrorAction SilentlyContinue)}
if((Servers).Count -or (Get-Process CryMP-Client64 -ErrorAction SilentlyContinue)){throw 'Owner/game session present'}
$port=64140;if(Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue){throw 'QA port busy'}
$profile=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games/Crysis'
$memory=Join-Path $profile 'WarsBots/Multiplayer_IA_Steelmill.mem'
$dir=(Resolve-Path -LiteralPath $BuildDirectory).Path
$manifest=Get-Content (Join-Path $dir 'claude-build.json') -Raw|ConvertFrom-Json
$exe=Join-Path $dir 'CryMP-Server64.exe'
if(-not $manifest.sha.StartsWith($ExpectedSource) -or (Get-FileHash $exe).Hash -ne $manifest.server){throw 'Build identity mismatch'}
New-Item -ItemType Directory $out|Out-Null
$backup=Join-Path $out 'initial-memory.mem';Copy-Item $memory $backup
$memoryHash=(Get-FileHash $backup).Hash
$runs=@();$p=$null;$joined=$false
try {
 foreach($mode in $Modes){
  if((Servers).Count){throw 'Server appeared; refusing shared memory mutation'}
  Copy-Item $backup $memory -Force
  $tag="revive195-$mode-h$Hooks";$name="WarsBots-$([IO.Path]::GetFileName($out))-$tag.log"
  $log=Join-Path $profile $name;$trace=$log -replace '\.log$','.bots.log'
  if(Test-Path $log){throw 'Existing evidence would be overwritten'}
  $offset=@{zero='0';legacy='0.2';native='-1'}[$mode]
  $commands=" +wb_probe_local_test 1 +wb_physics_diagnostics 1 +wb_debug 1 +wb_add 1 normal +wb_spawn_box 195 +wb_revive_z_test $offset +wb_revive_hooks_test $Hooks +wb_nav_export after 15"
  # 16s spacing gives the native summary's ~12s freeze detector complete windows.
  for($i=0;$i -lt 10;$i++){$commands+=" +wb_revive_at after $(5+16*$i) 195 1"}
  $args="-dir `"$GameDirectory`" -headless -logfile $name +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $port +sv_gamerules InstantAction +map multiplayer/ia/steelmill$commands"
  Write-Host "START $tag sha=$($manifest.sha) ten explicit scheduled revives"
  $p=Start-Process $exe -ArgumentList $args -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
  $end=[DateTime]::UtcNow.AddSeconds(170);$joined=$false;$early=$false;$captured=$false
  $nav=Join-Path $profile 'WarsBots/Multiplayer_IA_Steelmill.nav.json';$navStart=[DateTime]::UtcNow
  while([DateTime]::UtcNow -lt $end -and -not $p.HasExited){
   Start-Sleep -Seconds 5
   if(Test-Path $trace){$raw=Get-Content $trace -Raw;$joined=$raw -match 'clients=[1-9]'}
   if($joined){throw 'Real client joined; retaining server'}
   if(-not $captured -and (Test-Path $nav) -and (Get-Item $nav).LastWriteTimeUtc -gt $navStart){Copy-Item $nav (Join-Path $out "$tag-nav-1.json");$captured=$true}
  }
  $early=$p.HasExited
  if(-not $p.HasExited){Stop-Process -Id $p.Id};$null=$p.WaitForExit(15000);$p.Dispose();$p=$null
  Copy-Item $log,$trace $out
  $raw=Get-Content $trace -Raw;$engine=Get-Content $log -Raw
  $count=([regex]::Matches($raw,'DEBUG REVIVE actor=.*spawnId=195 manual=1')).Count
  $ok=-not $early -and $count -eq 10 -and $captured -and $engine -notmatch 'CRASH DETECTED|Lua Error|Script Error|\[Error\]'
  $run=[ordered]@{tag=$tag;log=[IO.Path]::GetFileName($trace);bots=1;seconds=170;mode=$mode;hooks=$Hooks;z_offset_requested=$offset;source_sha=$manifest.sha;server_sha256=$manifest.server;client_sha256=$manifest.client;memory_sha256=$memoryHash;manual_revives=$count;completed=$ok;style_mode='auto';physics_diagnostics_requested=1;roster=@(@{number=1;skill='normal';style='auto'})}
  $runs+=$run;$runs|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'runs.json')
  Write-Host "END $tag completed=$ok manualRevives=$count"
  if(-not $ok){throw "Probe fixture failed $tag; preserve partial logs"}
 }
} finally {
 if($p -and -not $p.HasExited -and -not $joined){Stop-Process -Id $p.Id;$null=$p.WaitForExit(15000)}
 if(-not (Servers).Count){Copy-Item $backup $memory -Force;if((Get-FileHash $memory).Hash -ne $memoryHash){throw 'Memory restore failed'}}
 if(Test-Path (Join-Path $out 'runs.json')){& (Join-Path $PSScriptRoot 'Invoke-WarsBotsTriage.ps1') -OutputDirectory $out}
}
