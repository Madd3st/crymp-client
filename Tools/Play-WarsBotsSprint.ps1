# Explicit owner playtest preset. Never installs EXEs or stops an existing session.
# Uses the tracked recording launcher, preserving a user's local launcher edits.
[CmdletBinding()]
param(
 [string]$BuildDirectory='build/play-0470fe7e',
 [string]$GameDirectory='E:/Steam/steamapps/common/Crysis',
 [ValidateRange(1,15)][int]$BotCount=6,
 [ValidateRange(1024,49151)][int]$Port=46050,
 [ValidateSet(1,2)][int]$ClientTraceMode=2,
 [switch]$ServerConsole,
 [switch]$PrepareOnly
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=(Resolve-Path (Join-Path $repo $BuildDirectory)).Path
$m=Get-Content (Join-Path $build 'claude-build.json') -Raw|ConvertFrom-Json
if(-not ($m.PSObject.Properties.Name -contains 'jump_links_default')){throw 'C061 matched client/server build required'}
if(-not $m.capped_view_pose_bit -or -not $m.demo_sprint_flags){throw 'Matched C050 capped-view/demo-gates build required'}
$source=(git -C $repo show HEAD:Tools/Play-WarsBotsRecording.ps1) -join "`n"
if($LASTEXITCODE){throw 'Cannot read tracked launcher'}
$repoAnchor='$repo=Split-Path $PSScriptRoot -Parent';$gateAnchor='+wb_maxbots $BotCount'
if(-not $source.Contains($repoAnchor) -or -not $source.Contains($gateAnchor)){throw 'Tracked launcher changed; review adapter before launching'}
$flags=@('wb_bot_look','wb_bot_turn_cap','wb_bot_weapon_rank','wb_bot_arbiter','wb_bot_navmesh','wb_bot_perception','wb_bot_aim_model','wb_bot_suit_policy','wb_bot_jump_links')
# New optional ammo fields require the matching C065 pair; older accepted builds retain their supported preset.
$newAmmoPose=($m.spectator_ammo_pose_fields -eq 2)
if($newAmmoPose){$flags+=@('wb_bot_weapon_handling','wb_bot_name_profile')}
$hasVantage=($m.PSObject.Properties.Name -contains 'vantage_default')
if($hasVantage){$flags+=@('wb_bot_vantage')}
$sessionLabel=if($hasVantage){'SprintC066'}elseif($newAmmoPose){'SprintC065'}else{'SprintC061'}
$gateArgs=($flags|ForEach-Object {"+$_ 1"}) -join ' '
$source=$source.Replace($repoAnchor,"`$repo='$($repo.Replace("'","''"))'")
$source=$source.Replace($gateAnchor,"$gateArgs +wb_demo_record 1 +wb_demo_hz 20 $gateAnchor")
$source=$source.Replace('+wb_move_trace 1 +connect',"+wb_bot_client_smooth 1 +wb_move_trace $ClientTraceMode +connect")
$source=$source.Replace('bots=$BotCount;roster=$roster;', 'bots=$BotCount;demo_record=1;demo_hz=20;feature_gates=@(__OWNER_FEATURE_GATES__);client_smooth=1;client_trace_mode=__CLIENT_TRACE_MODE__;roster=$roster;')
$source=$source.Replace('__OWNER_FEATURE_GATES__',(($flags|ForEach-Object {'"'+$_+'"'}) -join ','))
$source=$source.Replace('__CLIENT_TRACE_MODE__',[string]$ClientTraceMode)
$cache=Join-Path $repo 'build/owner-tools';New-Item -ItemType Directory -Force $cache|Out-Null
$script=Join-Path $cache ('Play-'+$sessionLabel+'Recording.ps1')
[IO.File]::WriteAllText($script,$source,[Text.UTF8Encoding]::new($false))
if($PrepareOnly){Write-Output $script;return}
& $script -GameDirectory $GameDirectory -BuildDirectory $BuildDirectory -SessionLabel $sessionLabel -BotCount $BotCount -SkillMix -BotPace 1 -NoOverlay -Port $Port -ServerConsole:$ServerConsole
