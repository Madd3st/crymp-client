# C154: one whole lower545 -> native ladder -> stairs -> upper Strength -> sea -> dry shore acceptance.
[CmdletBinding()]
param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$CodeSha,[switch]$PrepareOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$ci=Join-Path $repo ('build/ci-'+$CodeSha.Substring(0,8))
$qualityPath=Join-Path $ci 'c154-quality.json'
$profile='C:/Users/Magic8/WarsBotsBuild/WarsBots-Showcase-b8b444d6/Shore'
$packageManifest=Join-Path $profile 'package-manifest.json'
if((Get-FileHash $packageManifest).Hash -ne '4a065c8d200664014a40c03b7f65fba9f4ec3aa4b8fb78d8a6d12bf415eef00c'){throw 'Frozen recipe manifest changed'}
$package=Get-Content $packageManifest -Raw -Encoding UTF8|ConvertFrom-Json
foreach($record in $package.files){if((Get-FileHash (Join-Path $profile $record.name)).Hash -ne $record.sha256){throw 'Frozen recipe payload changed'}}
$preset=Get-Content (Join-Path $profile 'preset.json') -Raw -Encoding UTF8|ConvertFrom-Json
$preset.flags|Add-Member -NotePropertyName wb_bot_ladder -NotePropertyValue 1
$preset.critical|Add-Member -NotePropertyName wb_bot_ladder -NotePropertyValue 1
$game=(Resolve-Path -LiteralPath ((Get-Content (Join-Path $profile 'game-dir.txt') -Raw).Trim())).Path
$helpers=Join-Path $PSScriptRoot 'Launch-WarsBotsShowcase.ps1'
if((Get-FileHash $helpers).Hash -ne 'd2a7cb3065e393a6fa73c9135c868aee809a62cc782993663ed8f7ad38c9097e'){throw 'Reviewed recovery helper changed'}
$tokens=$null;$parseErrors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile($helpers,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Helper parse error'}
$wanted=@('Save-Json','Game-Processes','Quote-Arg','Stop-OwnServer','Save-Session','Snapshot-Data','Restore-Data')
foreach($f in $ast.EndBlock.Statements|Where-Object {$_ -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $_.Name -in $wanted}){. ([scriptblock]::Create($f.Extent.Text))}
$targets=@{memory=(Join-Path $env:LOCALAPPDATA 'WarsBots/Memory');nav=(Join-Path $env:LOCALAPPDATA 'WarsBots/Navmesh')}
$data=Join-Path $repo 'build/c154-route-data-v4'
$expectedData=@{'Shore.navmesh'='b60178d7090fd8403b4ba0f4407d6a722c0057afd559a0024148ef0645930609';'Shore.links.txt'='e7204de31a25482fd0d6d75feb55cb7e85255dc1e72f8a9bbf4f38f3191b7c2b';'Shore.spots.txt'='d95e1e11fa06194e44e1b1d3887fb00334d7f0ccc9528c9a1b439d7ba78b548b';'Shore.manifest.json'='c7e96b3d0458e93cab370989ca476126fe191ba54d5edff38f14db5fffd44747'}
foreach($name in $expectedData.Keys){if((Get-FileHash (Join-Path $data $name)).Hash -ne $expectedData[$name]){throw "Candidate mismatch: $name"}}
$port=46164
$out=Join-Path $repo ('build/c154-route-'+$CodeSha.Substring(0,8)+'-v1')
$logs=Join-Path $env:LOCALAPPDATA 'WarsBots/Logs'
$label='C154-'+$CodeSha.Substring(0,8)+'-v1'
$preLog=Join-Path $logs ($label+'-pre.log');$serverLog=Join-Path $logs ($label+'.log')
$trace=$serverLog -replace '\.log$','.bots.log'
$serverExe=[IO.Path]::GetFullPath((Join-Path $ci 'CryMP-Server64.exe'))
$python='C:/Users/Magic8/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$analyzer=Join-Path $PSScriptRoot 'Diagnostics/Analyze-C154Route.py'
$sets=' +wb_debug 1';foreach($p in $preset.flags.PSObject.Properties){$sets+=' +'+$p.Name+' '+[int]$p.Value}
$queries='';foreach($p in $preset.critical.PSObject.Properties){$queries+=' +'+$p.Name}
$base='-dir '+(Quote-Arg $game)+' -headless -logfile __LOG__ +sv_bind 127.0.0.1 +sv_lanonly 1 +sv_port '+$port+' +sv_gamerules PowerStruggle +map multiplayer/ps/Shore'+$sets+$queries+' +wb_rules_info'
$preArgs=$base.Replace('__LOG__',(Quote-Arg $preLog))+' +wb_list +wb_status'
# Existing diagnostic command calls native GameRules registered-spawn revive once. This is disclosed setup,
# not natural RNG coverage, an upper-spawn retry or a custom coordinate teleport. No intervention after it.
$runArgs=$base.Replace('__LOG__',(Quote-Arg $serverLog))+' +wb_add 2 normal +wb_remove 1 +wb_revive_at 545 2 +wb_list +wb_status'
$commandBytes=@(foreach($arguments in @($preArgs,$runArgs)){$bytes=[Text.Encoding]::UTF8.GetByteCount((Quote-Arg $serverExe)+' '+$arguments);if($bytes -ge 2048){throw "Native command buffer overflow $bytes"};$bytes})
if($PrepareOnly){@{source=$CodeSha;preflight_arguments=$preArgs;server_arguments=$runArgs;command_bytes=$commandBytes;runtime='NOT LAUNCHED';data_hashes='PASS'}|ConvertTo-Json;return}
$quality=Get-Content $qualityPath -Raw -Encoding UTF8|ConvertFrom-Json
if($quality.task -ne 'C154' -or $quality.harness_scope -ne 'c154-lower-route-v1' -or $quality.source -ne $CodeSha -or -not $quality.ci_success -or -not $quality.ci_source_match -or -not $quality.review_ok -or -not $quality.review_branch_head){throw 'Exact C154 CI/review receipt required'}
if($quality.harness_sha256 -ne (Get-FileHash $PSCommandPath).Hash -or $quality.analyzer_sha256 -ne (Get-FileHash $analyzer).Hash){throw 'Reviewed harness/analyzer changed'}
foreach($name in $expectedData.Keys){if($quality.data_hashes.$name -ne $expectedData[$name]){throw 'Reviewed data pin mismatch'}}
if(@(Game-Processes).Count){throw 'Owner game/server active; no test'}
if(Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue){throw 'Port occupied'}
if((Test-Path $out) -or (Test-Path $preLog) -or (Test-Path $serverLog)){throw 'Duplicate acceptance refused'}
$hashLines=@(Get-Content (Join-Path $ci 'sha256.txt')|Where-Object {$_.Trim()})
$hashNames=@(foreach($line in $hashLines){$hash,$name=$line -split '\s+',2;$name=$name.TrimStart('*');if($name -notin @('CryMP-Client64.exe','CryMP-Server64.exe') -or (Get-FileHash (Join-Path $ci $name)).Hash -ne $hash){throw 'CI payload mismatch'};$name})
if($hashNames.Count -ne 2 -or @($hashNames|Sort-Object -Unique).Count -ne 2){throw 'Exact paired client/server hashes required'}
function Assert-ReviewFresh {
 git -C $repo fetch -q origin '+refs/heads/warsbots/research/deepseek:refs/remotes/origin/warsbots/research/deepseek'
 if($LASTEXITCODE){throw 'Review fetch failed'}
 $head=(git -C $repo rev-parse origin/warsbots/research/deepseek).Trim()
 if($head -ne $quality.review_branch_head){throw 'Review head advanced; reread before game work'}
 git -C $repo fetch -q origin '+refs/heads/warsbots/research/steelmill-triage:refs/remotes/origin/warsbots/research/steelmill-triage'
 if($LASTEXITCODE){throw 'Runtime task fetch failed'}
 $taskText=git -C $repo show origin/warsbots/research/steelmill-triage:Docs/Research/Coordination/MAILBOX/TO_CODEX.md
 $taskText=[string]::Join("`n",$taskText)
 $item=[regex]::Match($taskText,'(?s)### C-154\b.*?(?=\n### C-|\z)').Value
 if($item -notmatch ('RUNTIME_PIN: '+$CodeSha+'\b')){throw 'Exact task runtime pin missing'}
 if(Test-Path $out){@{utc=[DateTime]::UtcNow.ToString('o');source=$CodeSha;approved=$quality.review_branch_head;observed=$head;verdict='MATCH'}|ConvertTo-Json -Compress|Add-Content (Join-Path $out 'review-checks.jsonl') -Encoding UTF8}
}
function Read-LiveLog([string]$Path){
 if(-not(Test-Path -LiteralPath $Path)){return ''}
 $pieces=@(Get-Content -LiteralPath $Path -Raw -Encoding UTF8)
 return [string]::Concat([object[]]$pieces)
}
Assert-ReviewFresh
New-Item -ItemType Directory $out|Out-Null
New-Item -ItemType Directory -Force $logs|Out-Null
Snapshot-Data $out
$state=@{source=$CodeSha;map='Shore';bots=1;phase='snapshot_complete';directory=$out;server_exe=$serverExe;server_pid=$null;server_started=$null;server_log=$preLog;client_log=$null;client_pid=$null}
Save-Session $state
Copy-Item -LiteralPath $qualityPath -Destination (Join-Path $out 'quality.json')
Copy-Item -LiteralPath (Join-Path $ci 'sha256.txt') -Destination $out
Save-Json @{logical_localappdata=$env:LOCALAPPDATA;namespace='Codex MSIX child process';targets=$targets;command_bytes=$commandBytes;setup='native registered545 once; synthetic setup death excluded';data_hashes=$expectedData} (Join-Path $out 'provenance.json')
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
  [string]$text=Read-LiveLog $log;[string]$side=Read-LiveLog ($log -replace '\.log$','.bots.log')
  if($text -match '\[Error\]|Lua Error|Script Error|RMI.*not a client' -or $side -match 'STATE CONFLICT|NAVMESH none|NAVMESH.*(?:mismatch|FAILED|refused|does not match its manifest)'){throw 'Readiness error'}
  $ready=$text -match 'RULES INFO class=PowerStruggle' -and $text -match 'gate=1 levelReady=1'
  if($bots){$ready=$ready -and $side -match '#1 ENTITY_EVENT_DONE' -and $side -match 'FRAME .*bots=1 server=1' -and $side -match 'NAVMESH loaded.*Shore: .*2 links' -and $side -match 'NAVMESH links ON:' -and $side -match '#2 revive result EntityId=\d+ sameEntity=1 alive=1 health=100'}else{$ready=$ready -and $text -match 'bots=0/2 defaultVerbose=1'}
 }while(-not $ready -and [DateTime]::UtcNow -lt $deadline)
 if(-not $ready){throw 'Readiness timeout'}
 $checks=@(foreach($property in $preset.critical.PSObject.Properties){$matches=[regex]::Matches($text,[regex]::Escape($property.Name)+'\s*=\s*(-?\d+)');$actual=if($matches.Count){[int]$matches[$matches.Count-1].Groups[1].Value}else{$null};@{name=$property.Name;expected=$property.Value;actual=$actual;pass=($null -ne $actual -and $actual -eq $property.Value)}})
 Save-Json @($checks) (Join-Path $out $(if($bots){'effective.json'}else{'preflight-effective.json'}))
 if(@($checks|Where-Object {-not $_.pass}).Count){throw 'Effective recipe mismatch'}
 return $p
}
try{
 Assert-ReviewFresh
 if(@(Game-Processes).Count){throw 'Game appeared before staging'}
 New-Item -ItemType Directory -Force $targets.nav|Out-Null
 foreach($name in $expectedData.Keys){Copy-Item (Join-Path $data $name) -Destination $targets.nav -Force}
 $preArgs|Set-Content (Join-Path $out 'preflight-arguments.txt');$runArgs|Set-Content (Join-Path $out 'server-arguments.txt')
 $null=Start-Phase $preArgs $preLog $false
 Stop-OwnServer $state
 foreach($file in @($preLog,($preLog -replace '\.log$','.bots.log'))){if(Test-Path $file){Copy-Item $file -Destination $out}}
 Assert-ReviewFresh
 $server=Start-Phase $runArgs $serverLog $true
 $start=[DateTime]::UtcNow;$reviewAt=$start
 do{
  Start-Sleep -Seconds 1
  if($server.HasExited){throw 'Server exited before result'}
  if(@(Game-Processes|Where-Object {$_.Id -ne $server.Id}).Count){throw 'Owner game appeared'}
  if(([DateTime]::UtcNow-$reviewAt).TotalSeconds -ge 15){Assert-ReviewFresh;$reviewAt=[DateTime]::UtcNow}
  [string]$text=Read-LiveLog $serverLog
  if($text -match '\[Error\]|Lua Error|Script Error|RMI.*not a client'){throw 'Runtime engine/script error'}
  $json=& $python -X utf8 $analyzer $trace --out (Join-Path $out 'live-evidence.json')
  if($LASTEXITCODE){throw 'Route analyzer failed; tooling invalid'}
  $runtimeEvidence=$json|ConvertFrom-Json
  if($runtimeEvidence.status -eq 'FAIL'){throw $runtimeEvidence.failure}
  $elapsed=([DateTime]::UtcNow-$start).TotalSeconds
  $passed=$runtimeEvidence.status -eq 'PASS'
 }while(-not $passed -and $elapsed -lt 300)
 if(-not $passed){throw 'Whole lower route incomplete by300s; no unchanged repeat'}
 Assert-ReviewFresh
 $qualityPass=$true
}catch{$failureReason=$_.Exception.Message;throw}finally{
 Save-Json @{task='C154';source=$CodeSha;runtime_criteria_pass=$passed;quality_pass=$qualityPass;failure=$failureReason;evidence=$runtimeEvidence;client_visual='NOT TESTED';acceptance='Runtime provisional; archive/demo/restore and independent audit still required'} (Join-Path $out 'runtime-result.json')
 Restore-Data $state
}
