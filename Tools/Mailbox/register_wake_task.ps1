<#
Registers the Windows scheduled task "WarsBots Mailbox Wake" (every 10 min, current user, only when logged on).
  register_wake_task.ps1 -CodexRepo C:\...\crymp-client -OpenCodeRepo C:\...\crymp-client-opencode
  register_wake_task.ps1 -Remove
Copy Tools\Mailbox\ to a stable folder first (e.g. %LOCALAPPDATA%\WarsBots\Mailbox\bin) so branch switches
do not move the script.
#>
param([string]$CodexRepo, [string]$OpenCodeRepo, [string]$DeepSeekRepo, [int]$EveryMin = 10, [int]$IdleMin = 20, [switch]$Remove)
$name = 'WarsBots Mailbox Wake'
if ($Remove) { Unregister-ScheduledTask -TaskName $name -Confirm:$false; "removed $name"; return }
$script = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'wake_agents.ps1'
$arg = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$script`" -IdleMin $IdleMin"
if ($CodexRepo)    { $arg += " -CodexRepo `"$CodexRepo`"" }
if ($OpenCodeRepo) { $arg += " -OpenCodeRepo `"$OpenCodeRepo`"" }
if ($DeepSeekRepo) { $arg += " -DeepSeekRepo `"$DeepSeekRepo`"" }
$action  = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $arg
$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Minutes $EveryMin)
$set     = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 5) -StartWhenAvailable
Register-ScheduledTask -TaskName $name -Action $action -Trigger $trigger -Settings $set -Force | Out-Null
"registered '$name' every $EveryMin min: powershell $arg"
