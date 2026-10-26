# Existing-thread Windows Task Scheduler wake, separate from Codex heartbeat automation.
[CmdletBinding()] param([Parameter(Mandatory)][string]$CodexRepo,[string]$ThreadId=$env:CODEX_THREAD_ID,[ValidateRange(1,60)][int]$EveryMin=10,[ValidateRange(1,120)][int]$IdleMin=20,[int]$MinimumItem=112)
$ErrorActionPreference='Stop'
if($ThreadId -notmatch '^[0-9a-f-]{36}$'){throw 'Explicit existing thread ID required'}
$stable=Join-Path $env:LOCALAPPDATA 'WarsBots/Mailbox/bin';New-Item -ItemType Directory -Force $stable|Out-Null
foreach($name in @('wake_agents.ps1','run_codex_wake.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $stable $name) -Force}
$inbox=(git -C $CodexRepo show origin/warsbots/research/steelmill-triage:Docs/Research/Coordination/MAILBOX/TO_CODEX.md)-join "`n"
$sections=@{}
foreach($m in [regex]::Matches($inbox,'(?ms)^### C-(\d+)[^\r\n]*\r?\n.*?(?=^### C-|\z)')){
 $h=[Security.Cryptography.SHA256]::Create()
 try{$sections[$m.Groups[1].Value]=([BitConverter]::ToString($h.ComputeHash([Text.Encoding]::UTF8.GetBytes($m.Value)))).Replace('-','')}finally{$h.Dispose()}
}
$stateFile=Join-Path $env:LOCALAPPDATA 'WarsBots/Mailbox/wake-codex-state.json'
if(-not(Test-Path $stateFile)){@{sections=$sections;registered=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 5|Set-Content $stateFile -Encoding UTF8}
$script=Join-Path $stable 'wake_agents.ps1'
$args='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+$script+'" -CodexRepo "'+$CodexRepo+'" -ThreadId '+$ThreadId+' -IdleMin '+$IdleMin+' -MinimumItem '+$MinimumItem
$action=New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $args -WorkingDirectory $CodexRepo
$trigger=New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes($EveryMin) -RepetitionInterval (New-TimeSpan -Minutes $EveryMin)
$settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 5) -StartWhenAvailable
$principal=New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Limited
Register-ScheduledTask -TaskName 'WarsBots Mailbox Wake' -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Force|Out-Null
Write-Output "Registered WarsBots Mailbox Wake every${EveryMin}min idle${IdleMin}min thread=$ThreadId; Codex heartbeat unchanged"
