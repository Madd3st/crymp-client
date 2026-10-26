# C106 owner playtest: exact green CI pair plus required smoke/wire receipt.
[CmdletBinding()]
param(
 [string]$BuildDirectory='build/ci-757f3ddc',
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [ValidateRange(1,15)][int]$BotCount=8,
 [ValidateRange(1024,49151)][int]$Port=46050,
 [switch]$PrepareOnly
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$m=Get-Content (Join-Path $repo "$BuildDirectory/claude-build.json") -Raw|ConvertFrom-Json
$quality=Get-Content (Join-Path $repo 'build/c106-final-757f3ddc/c106-result.json') -Raw|ConvertFrom-Json
if(-not $quality.pass -or $quality.source -ne $m.sha -or $m.sha -ne '757f3ddcff02b9f0ccc3e392911897441031b57f'){throw 'C106 requires exact757f3ddc plus green smoke and client-wire receipt'}
if(-not $m.net_compat){throw 'C106 wire-compatible client/server pair required'}
if(-not $m.owner_mark_command -or $m.threat_scan_default -ne 0){throw 'C102 QoL build with threatscanOFF required'}
if($m.corners_default -ne 1 -or $m.jump_sanity_default -ne 1){throw 'Promoted defaults required; no explicit override permitted'}
foreach($field in @('difficulty_default','finish_default','spectator_aim_default','path_progress_default')){if($m.PSObject.Properties.Name -notcontains $field){throw "Missing build capability $field"}}
$base=& (Join-Path $PSScriptRoot 'Play-WarsBotsSprint.ps1') -BuildDirectory $BuildDirectory -BotCount $BotCount -PrepareOnly
$source=[IO.File]::ReadAllText($base)
$gate='+wb_maxbots $BotCount';$client='+wb_bot_client_smooth 1'
if(-not $source.Contains($gate) -or -not $source.Contains($client)){throw 'Generated launcher anchors changed'}
$source=$source.Replace('+wb_bot_vantage 1','+wb_bot_vantage 0')
$source=$source.Replace('"wb_bot_vantage",','')
$source=$source.Replace($gate,'+wb_net_ext 1 +wb_bot_difficulty 1 +wb_bot_finish 1 +wb_bot_spectator_aim 1 +wb_bot_path_progress 0 +wb_bot_threat_scan 0 '+$gate)
$source=$source.Replace($client,'+wb_net_ext 1 +wb_bot_spectator_aim 1 +bind F8 `"wb_mark owner`" '+$client)
$source=$source.Replace('feature_gates=@(','feature_gates=@("wb_net_ext","wb_bot_difficulty","wb_bot_finish",')
$source=$source.Replace('client_smooth=1;','client_smooth=1;spectator_aim=1;vantage=0;client_net_ext=1;path_progress=0;threat_scan=0;owner_mark_key="F8";corners_default=1;jump_sanity_default=1;')
$memoryHook=@'
$warsBotsSnapshotRoot=Join-Path $sessionDir 'memory-before'
$warsBotsMemoryRoot=Join-Path $env:LOCALAPPDATA 'WarsBots/Memory'
New-Item -ItemType Directory -Force $warsBotsSnapshotRoot|Out-Null
$warsBotsSnapshotRecords=@(foreach($name in @('Multiplayer_IA_Steelmill.mem','Multiplayer_IA_Steelmill.nav.json')){
 $file=Join-Path $warsBotsMemoryRoot $name
 if(-not(Test-Path -LiteralPath $file)){throw 'Owner memory snapshot source missing'}
 Copy-Item -LiteralPath $file -Destination $warsBotsSnapshotRoot
 @{name=$name;sha256=(Get-FileHash $file).Hash}
})
ConvertTo-Json -InputObject $warsBotsSnapshotRecords|Set-Content (Join-Path $warsBotsSnapshotRoot 'manifest.json') -Encoding UTF8
$server=Start-Process
'@
if(-not $source.Contains('$server=Start-Process')){throw 'Owner snapshot anchor changed'}
$source=$source.Replace('$server=Start-Process',$memoryHook.TrimEnd())
$source=$source.Replace('$session=@{','$session=@{memory_before=$warsBotsSnapshotRoot;phase="owner_playing_wait_explicit_done";')
$sessionSave='$session|ConvertTo-Json|Set-Content (Join-Path $sessionDir ''session.json'')'
$activeSave='$session|ConvertTo-Json|Set-Content (Join-Path $repo ''build/c106-active-session.json'')'
if(-not $source.Contains($sessionSave)){throw 'Owner session-save anchor changed'}
$source=$source.Replace($sessionSave,$sessionSave+"`n"+$activeSave)
if($source -match '\+wb_bot_(corners|jump_sanity)\s+[01]'){throw 'C091 must use promoted defaults'}
$label='C106-'+$m.sha.Substring(0,8);$script=Join-Path $repo ('build/owner-tools/Play-'+$label+'Recording.ps1')
[IO.File]::WriteAllText($script,$source,[Text.UTF8Encoding]::new($false))
$tokens=$null;$parseErrors=$null;[System.Management.Automation.Language.Parser]::ParseFile($script,[ref]$tokens,[ref]$parseErrors)|Out-Null
if($parseErrors){throw 'Generated launcher syntax invalid'}
if($PrepareOnly){Write-Output $script;return}
& $script -GameDirectory $GameDirectory -BuildDirectory $BuildDirectory -SessionLabel $label -BotCount $BotCount -SkillMix -BotPace 1 -NoOverlay -Port $Port
