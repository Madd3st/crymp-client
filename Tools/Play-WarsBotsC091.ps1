# C102 revision of C091: approved QoL build, threat scan OFF; no GUI in PrepareOnly.
[CmdletBinding()]
param(
 [string]$BuildDirectory='build/batch-40bd9f60',
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [ValidateRange(1,15)][int]$BotCount=8,
 [ValidateRange(1024,49151)][int]$Port=46050,
 [switch]$PrepareOnly
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$m=Get-Content (Join-Path $repo "$BuildDirectory/claude-build.json") -Raw|ConvertFrom-Json
$quality=Get-Content (Join-Path $repo 'build/c097-on15-7179ec36/combat-by-skill.json') -Raw|ConvertFrom-Json
if(-not $quality.all_gates_pass -or $quality.source -notlike '7179ec36*' -or $m.sha -ne '40bd9f609efdafbfeb554b5d98a2bdce900dbe1a'){throw 'C102 requires approved40bd9f60 plus acceptedgreenC093 evidence'}
if(-not $m.owner_mark_command -or $m.threat_scan_default -ne 0){throw 'C102 QoL build with threatscanOFF required'}
if($m.corners_default -ne 1 -or $m.jump_sanity_default -ne 1){throw 'Promoted defaults required; no explicit override permitted'}
foreach($field in @('difficulty_default','finish_default','spectator_aim_default','path_progress_default')){if($m.PSObject.Properties.Name -notcontains $field){throw "Missing build capability $field"}}
$base=& (Join-Path $PSScriptRoot 'Play-WarsBotsSprint.ps1') -BuildDirectory $BuildDirectory -BotCount $BotCount -PrepareOnly
$source=[IO.File]::ReadAllText($base)
$gate='+wb_maxbots $BotCount';$client='+wb_bot_client_smooth 1'
if(-not $source.Contains($gate) -or -not $source.Contains($client)){throw 'Generated launcher anchors changed'}
$source=$source.Replace($gate,'+wb_bot_difficulty 1 +wb_bot_finish 1 +wb_bot_spectator_aim 1 +wb_bot_path_progress 0 +wb_bot_threat_scan 0 '+$gate)
$source=$source.Replace($client,'+wb_bot_spectator_aim 1 +bind f8 `"wb_mark owner`" '+$client)
$source=$source.Replace('feature_gates=@(','feature_gates=@("wb_bot_difficulty","wb_bot_finish",')
$source=$source.Replace('client_smooth=1;','client_smooth=1;spectator_aim=1;path_progress=0;threat_scan=0;owner_mark_key="F8";corners_default=1;jump_sanity_default=1;')
if($source -match '\+wb_bot_(corners|jump_sanity)\s+[01]'){throw 'C091 must use promoted defaults'}
$label='C091-'+$m.sha.Substring(0,8);$script=Join-Path $repo ('build/owner-tools/Play-'+$label+'Recording.ps1')
[IO.File]::WriteAllText($script,$source,[Text.UTF8Encoding]::new($false))
$tokens=$null;$parseErrors=$null;[System.Management.Automation.Language.Parser]::ParseFile($script,[ref]$tokens,[ref]$parseErrors)|Out-Null
if($parseErrors){throw 'Generated launcher syntax invalid'}
if($PrepareOnly){Write-Output $script;return}
& $script -GameDirectory $GameDirectory -BuildDirectory $BuildDirectory -SessionLabel $label -BotCount $BotCount -SkillMix -BotPace 1 -NoOverlay -Port $Port
