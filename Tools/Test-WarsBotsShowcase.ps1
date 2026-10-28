# Filesystem-only recovery controls. Never imports the launcher's top-level startup or starts a game.
[CmdletBinding()]param()
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$launcher=Join-Path $PSScriptRoot 'Launch-WarsBotsShowcase.ps1'
$tokens=$null;$errors=$null;$ast=[System.Management.Automation.Language.Parser]::ParseFile($launcher,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Launcher AST failed'}
$wantedFunctions=@('Save-Json','Stop-OwnServer','Save-Session','Snapshot-Data','Restore-Data')
foreach($function in $ast.EndBlock.Statements|Where-Object {$_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $_.Name -in $wantedFunctions}){. ([scriptblock]::Create($function.Extent.Text))}
function Game-Processes { @() }
$fixture=Join-Path $repo ('build/c150-fs-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $fixture|Out-Null
$results=@()
foreach($existing in @($true,$false)){
 $case=Join-Path $fixture $(if($existing){'existing'}else{'absent'});$session=Join-Path $case 'Session'
 New-Item -ItemType Directory $session|Out-Null
 $targets=@{memory=(Join-Path $case 'Memory');nav=(Join-Path $case 'Navmesh')}
 $beforeHashes=@{}
 if($existing){foreach($kind in $targets.Keys){New-Item -ItemType Directory $targets[$kind]|Out-Null;$file=Join-Path $targets[$kind] 'before.dat';[IO.File]::WriteAllText($file,'ORIGINAL-'+$kind);$beforeHashes[$kind]=(Get-FileHash $file).Hash}}
 Snapshot-Data $session
 foreach($kind in $targets.Keys){New-Item -ItemType Directory -Force $targets[$kind]|Out-Null;[IO.File]::WriteAllText((Join-Path $targets[$kind] 'extra.dat'),'NEW');if($existing){[IO.File]::WriteAllText((Join-Path $targets[$kind] 'before.dat'),'CHANGED')}}
 $state=@{directory=$session;phase='test';server_pid=$null;server_log=(Join-Path $session 'absent-server.log');client_log=(Join-Path $session 'absent-client.log')}
 Restore-Data $state
 if($state.phase -ne 'archived_restored'){throw 'Fixture did not restore'}
 foreach($kind in $targets.Keys){if($existing){if((Get-FileHash (Join-Path $targets[$kind] 'before.dat')).Hash -ne $beforeHashes[$kind]){throw 'Original bytes not restored'};if(Test-Path (Join-Path $targets[$kind] 'extra.dat')){throw 'Temporary file retained'}}elseif(Test-Path $targets[$kind]){throw 'Originally absent directory retained'}}
 $results+=@{case=$(if($existing){'existing_bytes_and_names'}else{'absent_directories'});pass=$true}
}
# An active client must defer all mutation and must not reach process-stop logic.
$case=Join-Path $fixture 'active-client';$session=Join-Path $case 'Session';New-Item -ItemType Directory $session|Out-Null
$targets=@{memory=(Join-Path $case 'Memory');nav=(Join-Path $case 'Navmesh')}
New-Item -ItemType Directory $targets.memory|Out-Null;$held=Join-Path $targets.memory 'held.dat';[IO.File]::WriteAllText($held,'KEEP')
function Game-Processes { [pscustomobject]@{ProcessName='CryMP-Client64';Id=0} }
function Stop-OwnServer { throw 'Process stop must not be called while client active' }
$state=@{directory=$session;phase='test';server_pid=0;server_log='';client_log=''}
Restore-Data $state
if($state.phase -ne 'restore_deferred_client_active' -or [IO.File]::ReadAllText($held) -ne 'KEEP'){throw 'Active-client guard failed'}
$results+=@{case='active_client_defers_without_stopping_or_writing_data';pass=$true}
@{fixture=$fixture;game_started=$false;results=$results}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $fixture 'result.json') -Encoding UTF8
Write-Host 'Showcase filesystem controls PASS; no game started'
