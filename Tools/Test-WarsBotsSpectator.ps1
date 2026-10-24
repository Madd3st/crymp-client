[CmdletBinding()]
param([Parameter(Mandatory)][string]$GameDirectory,
      [Parameter(Mandatory)][string]$BuildDirectory,
      [string]$OutputDirectory='build/spectator-qa', [int]$Seconds=60)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$out=[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
if(Test-Path -LiteralPath $out){throw 'Use a fresh output directory.'}
$dir=(Resolve-Path -LiteralPath $BuildDirectory).Path
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$manifest=Get-Content (Join-Path $dir 'claude-build.json') -Raw | ConvertFrom-Json
$exe=Join-Path $dir 'CryMP-Server64.exe'
if((Get-FileHash $exe).Hash -ne $manifest.server){throw 'Build hash mismatch.'}
if(Get-Process -Name 'CryMP-Server64' -ErrorAction SilentlyContinue){throw 'An existing server is active.'}
if(Get-NetUDPEndpoint -LocalPort 64140 -ErrorAction SilentlyContinue){throw 'QA port occupied.'}
$profile=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games/Crysis'
$memory=Join-Path $profile 'WarsBots/Multiplayer_IA_Steelmill.mem'
New-Item -ItemType Directory -Path $out | Out-Null
$backup=Join-Path $out 'initial-memory.mem'
Copy-Item -LiteralPath $memory -Destination $backup
$before=(Get-FileHash $backup).Hash
$tag=(Split-Path $out -Leaf) -replace '[^A-Za-z0-9_-]','_'
$name="WarsBots-$tag.log"
$log=Join-Path $profile $name
$trace=$log -replace '\.log$','.bots.log'
if(Test-Path $log){throw 'Log already exists.'}
$p=$null;$joined=$false
try {
 # No observer is fabricated. Query the registered native binding against real bot entities.
 $commands='+wb_maxbots 6 +wb_watch +wb_watch_info +wb_watch_check +wb_add 3 elite +wb_watch_check +wb_remove 2 +wb_watch_check +wb_remove_all +wb_watch_check +wb_add 1 normal +wb_watch_check +wb_remove_all +wb_add 6 elite +wb_watch_check +wb_fire_diagnostics 1 +wb_debug 1'
 $args="-dir `"$game`" -headless -logfile $name +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port 64140 +sv_gamerules InstantAction +map multiplayer/ia/steelmill +wb_probe_local_test 1 $commands"
 $p=Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $game -WindowStyle Hidden -PassThru
 $end=[DateTime]::UtcNow.AddSeconds($Seconds)
 while([DateTime]::UtcNow -lt $end -and -not $p.HasExited){
  Start-Sleep -Seconds 5
  if((Test-Path $trace) -and (Get-Content $trace -Raw) -match 'clients=[1-9]'){$joined=$true;throw 'Real client joined; server left running.'}
 }
 if($p.HasExited){throw 'QA server exited early.'}
 Stop-Process -Id $p.Id
 if(-not $p.WaitForExit(15000)){throw 'QA server did not stop.'}
 Copy-Item -LiteralPath $log,$trace -Destination $out
 $text=Get-Content $trace -Raw
 $engine=Get-Content $log -Raw
 $passes=@([regex]::Matches($text,'WATCH CHECK PASS')).Count
 $ok=$passes -eq 6 -and $text -notmatch 'WATCH CHECK FAIL' -and $engine -notmatch 'CRASH DETECTED|Lua Error|Script Error|\[Error\]'
 @{source_sha=$manifest.sha;server_sha256=$manifest.server;seconds=$Seconds;native_query_passes=$passes;passed=$ok;memory_sha256=$before;client_tested=$false} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
 if(-not $ok){throw 'Native spectator QA failed; inspect saved logs.'}
 Write-Host "PASS: $passes native query batches; no client/camera test."
} finally {
 if(-not $joined){
  if($p -and -not $p.HasExited){Stop-Process -Id $p.Id;$p.WaitForExit(15000)|Out-Null}
  Copy-Item -LiteralPath $backup -Destination $memory -Force
  if((Get-FileHash $memory).Hash -ne $before){throw 'Memory restore failed.'}
 }
 if($p){$p.Dispose()}
}
