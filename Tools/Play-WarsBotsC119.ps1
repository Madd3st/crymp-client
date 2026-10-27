# C119 playtest 5: exact CI pair, canonical owner spots and fail-closed smoke receipt.
[CmdletBinding()]
param(
 [string]$BuildDirectory='build/ci-6e2713f9',
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [ValidateRange(1,15)][int]$BotCount=8,
 [ValidateRange(1024,49151)][int]$Port=46050,
 [switch]$PrepareOnly
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$m=Get-Content (Join-Path $repo "$BuildDirectory/claude-build.json") -Raw|ConvertFrom-Json
$quality=Get-Content (Join-Path $repo 'build/c119-smoke-6e2713f9/c119-result.json') -Raw|ConvertFrom-Json
if(-not $quality.pass -or $quality.source -ne $m.sha -or $m.sha -ne '6e2713f9762cb3771e8652397c9cda5d9008c9c7'){throw 'C119 requires exact6e2713f9 plus green server smoke receipt'}
if(-not $m.net_compat){throw 'C119 wire-compatible client/server pair required'}
if(-not $m.owner_mark_command -or $m.threat_scan_default -ne 0){throw 'C102 QoL build with threatscanOFF required'}
if($m.corners_default -ne 1 -or $m.jump_sanity_default -ne 1){throw 'Promoted defaults required; no explicit override permitted'}
foreach($field in @('difficulty_default','finish_default','spectator_aim_default','path_progress_default')){if($m.PSObject.Properties.Name -notcontains $field){throw "Missing build capability $field"}}
$base=& (Join-Path $PSScriptRoot 'Play-WarsBotsSprint.ps1') -BuildDirectory $BuildDirectory -BotCount $BotCount -PrepareOnly
$source=[IO.File]::ReadAllText($base)
$dataRoot=Join-Path $repo 'build/c119-canonical-data'
$spotsHash=(Get-FileHash -LiteralPath (Join-Path $dataRoot 'Steelmill.spots.txt')).Hash.ToLowerInvariant()
if($spotsHash -ne '25c3eecf0699b9795bb214b322e9155b6bf5f207bc5e29e41479655a25cef4b5'){throw 'Pinned C119 Steelmill spots changed'}
$linksHash=(Get-FileHash -LiteralPath (Join-Path $dataRoot 'Steelmill.links.txt')).Hash.ToLowerInvariant()
if($linksHash -ne '7c384d12ff693bced574015a0e0067f8cb5c1a4a9a81881c1aff9088ec6b67e4'){throw 'Pinned C119 Steelmill links changed'}
if(-not $source.Contains('& (Join-Path $repo ''Tools/Install-WarsBotsNavmesh.ps1'') | Out-Host')){throw 'C110 install anchor changed'}
$source=$source.Replace('& (Join-Path $repo ''Tools/Install-WarsBotsNavmesh.ps1'') | Out-Host','& (Join-Path $repo ''Tools/Install-WarsBotsNavmesh.ps1'') -Source (Join-Path $repo ''build/c119-canonical-data'') | Out-Host')
$stateCommit=[int]$quality.state_commit
if($stateCommit -notin @(1,2)){throw 'C119 missing paired verdict'}
$gate='+wb_maxbots $BotCount';$client='+wb_bot_client_smooth 1'
if(-not $source.Contains($gate) -or -not $source.Contains($client)){throw 'Generated launcher anchors changed'}
$source=$source.Replace('+wb_bot_vantage 1','+wb_bot_vantage 0')
$source=$source.Replace('"wb_bot_vantage",','')
$source=$source.Replace($gate,'+wb_net_ext 1 +wb_bot_difficulty 1 +wb_bot_finish 1 +wb_bot_spectator_aim 1 +wb_bot_path_progress 0 +wb_bot_pro_fire 1 +wb_bot_high_ground 1 +wb_bot_threat_scan 1 +wb_bot_jump_links 1 +wb_bot_state_commit __STATE__ '+$gate)
$source=$source.Replace('__STATE__',[string]$stateCommit)
$source=$source.Replace($client,'+wb_net_ext 1 +wb_bot_spectator_aim 1 +bind f8 `"wb_mark owner`" '+$client)
$source=$source.Replace('feature_gates=@(','feature_gates=@("wb_net_ext","wb_bot_difficulty","wb_bot_finish",')
$source=$source.Replace('client_smooth=1;','client_smooth=1;spectator_aim=1;vantage=0;state_commit=__STATE__;jump_links=1;client_net_ext=1;path_progress=0;threat_scan=1;pro_fire=1;high_ground=1;owner_mark_key="F8";corners_default=1;jump_sanity_default=1;')
$source=$source.Replace('__STATE__',[string]$stateCommit)
$memoryHook=@'
$warsBotsSnapshotRoot=Join-Path $sessionDir 'memory-before'
$warsBotsMemoryRoot=Join-Path $env:LOCALAPPDATA 'WarsBots/Memory'
New-Item -ItemType Directory -Force $warsBotsSnapshotRoot|Out-Null
$warsBotsMissingMemory=@()
$warsBotsSnapshotRecords=@(foreach($name in @('Multiplayer_IA_Steelmill.mem','Multiplayer_IA_Steelmill.nav.json')){
 $file=Join-Path $warsBotsMemoryRoot $name
 if(-not(Test-Path -LiteralPath $file -PathType Leaf)){
  $warsBotsMissingMemory+=@{name=$name;path=$file}
  continue
 }
 Copy-Item -LiteralPath $file -Destination $warsBotsSnapshotRoot -ErrorAction Stop
 @{name=$name;sha256=(Get-FileHash -LiteralPath $file -ErrorAction Stop).Hash}
})
$warsBotsUtf8=[Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $warsBotsSnapshotRoot 'manifest.json'),(ConvertTo-Json -InputObject @($warsBotsSnapshotRecords) -Depth 4),$warsBotsUtf8)
$warsBotsSnapshotInfo=@{source_root=$warsBotsMemoryRoot;missing=@($warsBotsMissingMemory);existing_count=$warsBotsSnapshotRecords.Count}
[IO.File]::WriteAllText((Join-Path $warsBotsSnapshotRoot 'source-info.json'),(ConvertTo-Json -InputObject $warsBotsSnapshotInfo -Depth 4),$warsBotsUtf8)
$server=Start-Process
'@
if(-not $source.Contains('$server=Start-Process')){throw 'Owner snapshot anchor changed'}
$source=$source.Replace('$server=Start-Process',$memoryHook.TrimEnd())
$source=$source.Replace('$session=@{','$session=@{memory_before=$warsBotsSnapshotRoot;phase="owner_playing_wait_explicit_done";')
$sessionSave='$session|ConvertTo-Json|Set-Content (Join-Path $sessionDir ''session.json'')'
$activeSave='$session|ConvertTo-Json|Set-Content (Join-Path $repo ''build/c119-active-session.json'')'
if(-not $source.Contains($sessionSave)){throw 'Owner session-save anchor changed'}
$source=$source.Replace($sessionSave,$sessionSave+"`n"+$activeSave)
if($source -match '\+wb_bot_(corners|jump_sanity)\s+[01]'){throw 'C091 must use promoted defaults'}
$label='C119-'+$m.sha.Substring(0,8);$script=Join-Path $repo ('build/owner-tools/Play-'+$label+'Recording.ps1')
[IO.File]::WriteAllText($script,$source,[Text.UTF8Encoding]::new($false))
$tokens=$null;$parseErrors=$null;[System.Management.Automation.Language.Parser]::ParseFile($script,[ref]$tokens,[ref]$parseErrors)|Out-Null
if($parseErrors){throw 'Generated launcher syntax invalid'}
if($PrepareOnly){Write-Output $script;return}
& $script -GameDirectory $GameDirectory -BuildDirectory $BuildDirectory -SessionLabel $label -BotCount $BotCount -SkillMix -BotPace 1 -NoOverlay -Port $Port
