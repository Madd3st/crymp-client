# Explicit offline export only. Refuses active sessions; no installed EXE changes.
[CmdletBinding()]
param(
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [Parameter(Mandatory)][string]$BuildDirectory,
 [string[]]$Maps=@('Steelmill','Quarry','Armada','Outpost'),
 [ValidateSet('IA','PS')][string]$MapFamily='IA',
 [string]$OutputDirectory='',
 [switch]$PlayAreaBox,
 [ValidateRange(1024,49151)][int]$Port=46060,
 [string]$Label=('C019-'+(Get-Date -Format yyyyMMdd-HHmmss)),
 [switch]$Resume
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=(Resolve-Path (Join-Path $repo $BuildDirectory)).Path
$manifest=Get-Content (Join-Path $build 'claude-build.json') -Raw|ConvertFrom-Json
$exe=Join-Path $build 'CryMP-Server64.exe'
if(-not $manifest.geometry_export -or (Get-FileHash $exe).Hash -ne $manifest.server){throw 'Frozen geometry-export build/hash required'}
if(Get-Process CryMP-Server64,CryMP-Client64 -ErrorAction SilentlyContinue){throw 'Active session retained; offline export deferred'}
if(Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue){throw 'Export port occupied'}
$dest=if($OutputDirectory){[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))}else{Join-Path $env:LOCALAPPDATA "WarsBots/Geometry/$Label"}
if((Test-Path $dest) -and -not $Resume){throw 'Use a fresh label or Resume existing completed bundles'}
New-Item -ItemType Directory -Force $dest|Out-Null
$python=Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$runs=@();$process=$null;$exportLogs=@()
if($PlayAreaBox -and ($MapFamily -ne 'PS' -or -not $manifest.geometry_export_box)){throw 'PlayAreaBox requires a compatible bounded PS exporter'}
$boxes=@{Shore='1431 1000 2591 2160';Plantation='602 867 1486 1751'}
$rules=if($MapFamily -eq 'PS'){'PowerStruggle'}else{'InstantAction'}
try{
 foreach($map in $Maps){
  $allowed=if($MapFamily -eq 'PS'){@('Shore','Plantation')}else{@('Steelmill','Quarry','Armada','Outpost')}
  if($map -notin $allowed){throw 'Unsupported map family/name'}
  if(Get-Process CryMP-Server64,CryMP-Client64 -ErrorAction SilentlyContinue){throw 'Another session appeared; no process touched'}
  $obj=Join-Path $dest "$map.obj";$meta=Join-Path $dest "$map.geometry.json"
  $regionArgs=if($PlayAreaBox){' '+$boxes[$map]}else{''}
  $log=Join-Path $env:LOCALAPPDATA "WarsBots/Logs/$Label-$map.log";$trace=$log -replace '\.log$','.bots.log';$exportLogs+=@($log,$trace)
  if(-not ($Resume -and (Test-Path $meta))){
  $args="-dir `"$GameDirectory`" -headless -logfile `"$log`" +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port $Port +sv_gamerules $rules +wb_probe_local_test 1 +wb_debug 1 +map multiplayer/$($MapFamily.ToLowerInvariant())/$map +sv_gamerules +wb_geom_export `"$obj`"$regionArgs"
  Write-Host "EXPORT START $map sha=$($manifest.sha)"
  $process=Start-Process $exe -ArgumentList $args -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
  $end=[DateTime]::UtcNow.AddMinutes(5);$text=''
  while([DateTime]::UtcNow -lt $end -and -not $process.HasExited){
   Start-Sleep -Seconds 2
   if(Test-Path $trace){$text=Get-Content $trace -Raw}
   if((Test-Path $meta) -or $text -match 'GEOM export (path=|FAILED|refused)'){break}
  }
  if(-not (Test-Path $meta)){throw "Export failed/timed out for $map; inspect $trace"}
  if(-not $process.HasExited){Stop-Process -Id $process.Id}
  $null=$process.WaitForExit(15000);$process.Dispose();$process=$null
  }
  $stored=Get-Content $meta -Raw|ConvertFrom-Json
  if($stored.sha -ne $manifest.sha){throw "Stored export source SHA mismatch: $map"}
  $result=& $python -B (Join-Path $PSScriptRoot 'Test-WarsBotsGeometry.py') $obj
  if($LASTEXITCODE){throw "Geometry validation failed: $map"}
  $data=$result|ConvertFrom-Json
  $data|Add-Member -NotePropertyName source_sha -NotePropertyValue $manifest.sha
  $data|Add-Member -NotePropertyName obj_sha256 -NotePropertyValue (Get-FileHash $obj).Hash
  $data|Add-Member -NotePropertyName map -NotePropertyValue $map
  $runs+=$data;$runs|ConvertTo-Json -Depth 5|Set-Content (Join-Path $dest 'exports.json') -Encoding UTF8
  Write-Host "EXPORT END $map triangles=$($data.triangles) bytes=$($data.bytes) unsupported=$($data.unsupported_parts)"
 }
}finally{
 if($process -and -not $process.HasExited){Stop-Process -Id $process.Id;$null=$process.WaitForExit(15000)}
 foreach($file in $exportLogs){if(Test-Path -LiteralPath $file){Copy-Item -LiteralPath $file -Destination $dest -Force}}
}
Write-Host "Export bundle: $dest"
