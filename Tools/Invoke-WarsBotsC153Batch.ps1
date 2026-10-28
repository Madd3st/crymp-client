# C153: one headless, result-gated goals/weapons/movement/combat acceptance. No client is launched or stopped.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$CodeSha,[switch]$PrepareOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$ci=Join-Path $repo ('build/ci-'+$CodeSha.Substring(0,8))
$qualityPath=Join-Path $ci 'c153-quality.json'
$profile='C:/Users/Magic8/WarsBotsBuild/WarsBots-Showcase-b8b444d6/Steelmill'
$packageManifest=Join-Path $profile 'package-manifest.json'
if((Get-FileHash $packageManifest).Hash -ne '593b3b5d386fa388143e557804d70b687c8fdf10dc4a642c947078c9518bd3a0'){throw 'Frozen recipe manifest changed'}
$package=Get-Content $packageManifest -Raw -Encoding UTF8|ConvertFrom-Json
foreach($record in $package.files){if((Get-FileHash (Join-Path $profile $record.name)).Hash -ne $record.sha256){throw 'Frozen recipe payload changed'}}
$preset=Get-Content (Join-Path $profile 'preset.json') -Raw -Encoding UTF8|ConvertFrom-Json
$game=(Get-Content (Join-Path $profile 'game-dir.txt') -Raw -Encoding UTF8).Trim()
$game=(Resolve-Path -LiteralPath $game).Path
$helpers=Join-Path $PSScriptRoot 'Launch-WarsBotsShowcase.ps1'
if((Get-FileHash $helpers).Hash -ne 'd2a7cb3065e393a6fa73c9135c868aee809a62cc782993663ed8f7ad38c9097e'){throw 'Reviewed recovery helper changed'}
$tokens=$null;$parseErrors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile($helpers,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Helper parse error'}
$wanted=@('Save-Json','Game-Processes','Quote-Arg','Stop-OwnServer','Save-Session','Snapshot-Data','Restore-Data')
foreach($f in $ast.EndBlock.Statements|Where-Object {$_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $_.Name -in $wanted}){. ([scriptblock]::Create($f.Extent.Text))}
$targets=@{memory=(Join-Path $env:LOCALAPPDATA 'WarsBots/Memory');nav=(Join-Path $env:LOCALAPPDATA 'WarsBots/Navmesh')}
$data=Join-Path $repo 'build/c119-canonical-data'
$expectedData=@{'Steelmill.navmesh'='cfc11810626c6fa0859a055592e6c5ab12d12777f40f52aeb8c6208eefaf503b';'Steelmill.links.txt'='7c384d12ff693bced574015a0e0067f8cb5c1a4a9a81881c1aff9088ec6b67e4';'Steelmill.spots.txt'='25c3eecf0699b9795bb214b322e9155b6bf5f207bc5e29e41479655a25cef4b5'}
foreach($name in $expectedData.Keys){if((Get-FileHash (Join-Path $data $name)).Hash -ne $expectedData[$name]){throw "Canonical data mismatch: $name"}}
$manifest=Get-Content (Join-Path $data 'Steelmill.manifest.json') -Raw -Encoding UTF8|ConvertFrom-Json
$manifestRecord=@($package.files|Where-Object {$_.name -eq 'Data/Steelmill.manifest.json'})
if($manifestRecord.Count -ne 1 -or (Get-FileHash (Join-Path $data 'Steelmill.manifest.json')).Hash -ne $manifestRecord[0].sha256){throw 'Canonical manifest mismatch'}
$port=46163
$out=Join-Path $repo ('build/c153-batch-'+$CodeSha.Substring(0,8))
$logs=Join-Path $env:LOCALAPPDATA 'WarsBots/Logs'
$label='C153-'+$CodeSha.Substring(0,8)
$preLog=Join-Path $logs ($label+'-pre.log');$serverLog=Join-Path $logs ($label+'.log')
$serverExe=[IO.Path]::GetFullPath((Join-Path $ci 'CryMP-Server64.exe'))
$sets=' +wb_debug 1';foreach($p in $preset.flags.PSObject.Properties){$sets+=' +'+$p.Name+' '+[int]$p.Value}
$queries='';foreach($p in $preset.critical.PSObject.Properties){$queries+=' +'+$p.Name}
$base='-dir '+(Quote-Arg $game)+' -headless -logfile __LOG__ +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port '+$port+' +sv_gamerules InstantAction +map multiplayer/ia/Steelmill'+$sets+$queries+' +wb_rules_info'
$preArgs=$base.Replace('__LOG__',(Quote-Arg $preLog))+' +wb_list +wb_status'
$runArgs=$base.Replace('__LOG__',(Quote-Arg $serverLog))+' +wb_add 8 normal +wb_list +wb_status'
foreach($argsText in @($preArgs,$runArgs)){if([Text.Encoding]::UTF8.GetByteCount((Quote-Arg $serverExe)+' '+$argsText) -ge 2048){throw 'Native command buffer overflow'}}
if($PrepareOnly){@{source=$CodeSha;preflight_arguments=$preArgs;server_arguments=$runArgs;runtime='NOT LAUNCHED';data_hashes='PASS'}|ConvertTo-Json;return}
$quality=Get-Content $qualityPath -Raw -Encoding UTF8|ConvertFrom-Json
if($quality.task -ne 'C153' -or $quality.source -ne $CodeSha -or -not $quality.ci_success -or -not $quality.ci_source_match -or -not $quality.review_ok -or -not $quality.review_branch_head){throw 'Exact C153 CI/review quality receipt required'}
if(@(Game-Processes).Count){throw 'Owner game/server active; no test'}
if(Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue){throw 'Port occupied'}
if((Test-Path $out) -or (Test-Path $preLog) -or (Test-Path $serverLog)){throw 'Duplicate shared acceptance refused'}
foreach($line in Get-Content (Join-Path $ci 'sha256.txt')){$hash,$name=$line -split '\s+',2;if((Get-FileHash (Join-Path $ci $name.TrimStart('*'))).Hash -ne $hash){throw 'CI payload hash mismatch'}}
function Assert-ReviewFresh {
 git -C $repo fetch -q origin '+refs/heads/warsbots/research/deepseek:refs/remotes/origin/warsbots/research/deepseek'
 if($LASTEXITCODE){throw 'Review fetch failed'}
 $head=(git -C $repo rev-parse origin/warsbots/research/deepseek).Trim()
 if($head -ne $quality.review_branch_head){throw 'Review head advanced; reread before further game work'}
 if(Test-Path $out){@{utc=[DateTime]::UtcNow.ToString('o');source=$CodeSha;approved=$quality.review_branch_head;observed=$head;verdict='MATCH'}|ConvertTo-Json -Compress|Add-Content (Join-Path $out 'review-checks.jsonl') -Encoding UTF8}
}
Assert-ReviewFresh
New-Item -ItemType Directory $out|Out-Null
New-Item -ItemType Directory -Force $logs|Out-Null
Snapshot-Data $out
$state=@{source=$CodeSha;map='Steelmill';bots=8;phase='snapshot_complete';directory=$out;server_exe=$serverExe;server_pid=$null;server_started=$null;server_log=$preLog;client_log=$null;client_pid=$null}
Save-Session $state
$passed=$false;$qualityPass=$false;$failureReason=$null;$runtimeEvidence=$null
function Start-Phase([string]$arguments,[string]$log,[bool]$bots){
 if(@(Game-Processes).Count){throw 'Game appeared before phase'}
 $p=Start-Process $serverExe -ArgumentList $arguments -WorkingDirectory $game -WindowStyle Hidden -PassThru
 $state.server_pid=$p.Id;$state.server_started=$p.StartTime.ToUniversalTime().ToString('o');$state.server_log=$log;Save-Session $state
 $deadline=[DateTime]::UtcNow.AddSeconds(60);$ready=$false
 do{
  Start-Sleep -Milliseconds 500
  if($p.HasExited){throw 'Own server exited during readiness'}
  if(@(Game-Processes|Where-Object {$_.Id -ne $p.Id}).Count){throw 'Owner game appeared; defer cleanup if needed'}
  $text=if(Test-Path $log){Get-Content $log -Raw}else{''};$trace=$log -replace '\.log$','.bots.log';$side=if(Test-Path $trace){Get-Content $trace -Raw}else{''}
  if($text -match '\[Error\]|Lua Error|Script Error|RMI.*not a client'){throw 'Readiness error'}
  $ready=$text -match 'RULES INFO class=InstantAction' -and $text -match 'gate=1 levelReady=1'
  if($bots){$ready=$ready -and $side -match '#8 created EntityId=' -and $side -match 'FRAME .*bots=8 server=1' -and $side -match 'NAVMESH loaded.*Steelmill: 9333 polys' -and $side -match 'NAVMESH links ON:'}else{$ready=$ready -and $text -match 'bots=0/8 defaultVerbose=1'}
 }while(-not $ready -and [DateTime]::UtcNow -lt $deadline)
 if(-not $ready){throw 'Readiness timeout'}
 $checks=@(foreach($property in $preset.critical.PSObject.Properties){$matches=[regex]::Matches($text,[regex]::Escape($property.Name)+'\s*=\s*(-?\d+)');$actual=if($matches.Count){[int]$matches[$matches.Count-1].Groups[1].Value}else{$null};@{name=$property.Name;expected=$property.Value;actual=$actual;pass=($null -ne $actual -and $actual -eq $property.Value)}})
 Save-Json @($checks) (Join-Path $out $(if($bots){'effective.json'}else{'preflight-effective.json'}))
 if(@($checks|Where-Object {-not $_.pass}).Count){throw 'Effective recipe mismatch'}
 return $p
}
try{
 Assert-ReviewFresh
 if(@(Game-Processes).Count){throw 'Game appeared before data staging'}
 New-Item -ItemType Directory -Force $targets.nav|Out-Null
 foreach($extension in @('navmesh','links.txt','spots.txt','manifest.json')){Copy-Item (Join-Path $data ('Steelmill.'+$extension)) -Destination $targets.nav -Force}
 $preArgs|Set-Content (Join-Path $out 'preflight-arguments.txt');$runArgs|Set-Content (Join-Path $out 'server-arguments.txt')
 $null=Start-Phase $preArgs $preLog $false
 Stop-OwnServer $state
 foreach($file in @($preLog,($preLog -replace '\.log$','.bots.log'))){if(Test-Path $file){Copy-Item $file -Destination $out}}
 Assert-ReviewFresh
 $server=Start-Phase $runArgs $serverLog $true
 $start=[DateTime]::UtcNow;$reviewAt=$start;$rosterAt=$start;$rosterSignature='';$trace=$serverLog -replace '\.log$','.bots.log'
 do{
  Start-Sleep -Seconds 1
  if($server.HasExited){throw 'Server exited before result'}
  if(@(Game-Processes|Where-Object {$_.Id -ne $server.Id}).Count){throw 'Owner game appeared during acceptance'}
  if(([DateTime]::UtcNow-$reviewAt).TotalSeconds -ge 15){Assert-ReviewFresh;$reviewAt=[DateTime]::UtcNow}
  $text=Get-Content $serverLog -Raw;$side=Get-Content $trace -Raw
  if($text -match '\[Error\]|Lua Error|Script Error|RMI.*not a client' -or $side -match 'STATE CONFLICT'){throw 'Runtime error/state conflict; archive for analysis'}
  $pickupActors=@([regex]::Matches($side,'REAL PICKUP actor=(\d+).*acquired=1 class=(SCAR|FY71|SMG|Shotgun|DSG1|GaussRifle|Hurricane)\b')|ForEach-Object {$_.Groups[1].Value}|Sort-Object -Unique)
  # Game reset can recreate the same eight slots with new EntityIds. Judge the latest roster, not all historic IDs.
  $created=[regex]::Matches($side,'#(\d+) created EntityId=(\d+)');$roster=@{};$lastCreate=0
  foreach($birth in $created){$roster[$birth.Groups[1].Value]=$birth.Groups[2].Value;$lastCreate=$birth.Index}
  $currentIds=@($roster.Values|Sort-Object -Unique);$signature=$currentIds -join ','
  if($signature -ne $rosterSignature){$rosterSignature=$signature;$rosterAt=[DateTime]::UtcNow}
  $currentPickups=@($currentIds|Where-Object {$_ -in $pickupActors})
  $currentSide=$side.Substring($lastCreate)
  $attacks=[regex]::Matches($currentSide,'AI state=ATTACKING').Count
  $revives=[regex]::Matches($currentSide,'revive result EntityId=\d+ sameEntity=1 alive=1').Count
  $elapsed=([DateTime]::UtcNow-$start).TotalSeconds
  $runtimeEvidence=@{observation_seconds=$elapsed;current_roster_ids=$currentIds;current_real_primary_pickup_ids=$currentPickups;historic_pickup_actor_ids=$pickupActors;attacking_states=$attacks;revive_log_events=$revives;test_grants=[regex]::Matches($side,'TEST GRANT SCAR').Count;client_visual='NOT TESTED'}
  $passed=$currentIds.Count -eq 8 -and $currentPickups.Count -eq 8 -and $attacks -gt 0 -and $revives -gt 0 -and $runtimeEvidence.test_grants -eq 0 -and $elapsed -ge 90 -and ([DateTime]::UtcNow-$rosterAt).TotalSeconds -ge 10
 }while(-not $passed -and $elapsed -lt 300)
 if(-not $passed){throw 'Shared goals/weapons acceptance incomplete by300s; no automatic repeat'}
 Assert-ReviewFresh
 $qualityPass=$true
}catch{$failureReason=$_.Exception.Message;throw}finally{
 Save-Json @{task='C153';source=$CodeSha;runtime_criteria_pass=$passed;quality_pass=$qualityPass;failure=$failureReason;evidence=$runtimeEvidence;client_visual='NOT TESTED'} (Join-Path $out 'runtime-result.json')
 Restore-Data $state
}
