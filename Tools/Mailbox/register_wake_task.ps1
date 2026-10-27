# C122 current-user scheduler. Preserves the separate C090 app heartbeat unchanged.
[CmdletBinding()] param([Parameter(Mandatory)][string]$CodexRepo,[Parameter(Mandatory)][string]$DeepSeekRepo,[Parameter(Mandatory)][string]$DeepSeekSession,[Parameter(Mandatory)][string]$DeepSeekSessionDirectory,[Parameter(Mandatory)][string]$OpenCodeCmd,[Parameter(Mandatory)][string]$HostThreadFile,[int]$EveryMin=10,[int]$IdleMin=20,[string]$Python='C:/Users/Magic8/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe')
$ErrorActionPreference='Stop'
$CodexRepo=(Resolve-Path -LiteralPath $CodexRepo).Path;$DeepSeekRepo=(Resolve-Path -LiteralPath $DeepSeekRepo).Path
if((git -C $DeepSeekRepo branch --show-current).Trim() -ne 'warsbots/research/deepseek'){throw 'Wrong DeepSeek worktree'}
foreach($file in @($OpenCodeCmd,$Python,$HostThreadFile)){if(-not(Test-Path -LiteralPath $file)){throw "Missing file $file"}}
if(-not(Test-Path -LiteralPath $DeepSeekSessionDirectory)){throw 'DeepSeek original session directory missing'}
$codex=(Get-Command codex.cmd -ErrorAction Stop).Source
$stable=Join-Path $env:LOCALAPPDATA 'WarsBots/Mailbox/bin';New-Item -ItemType Directory -Force $stable|Out-Null
$cfg=@{CodexRepo=$CodexRepo;DeepSeekRepo=$DeepSeekRepo;DeepSeekSession=$DeepSeekSession;DeepSeekSessionDirectory=$DeepSeekSessionDirectory;OpenCodeCmd=$OpenCodeCmd;CodexCmd=$codex;Python=$Python;HostThreadFile=$HostThreadFile;EveryMin=$EveryMin;IdleMin=$IdleMin}
$cfgPath=Join-Path $stable 'wake-config.json';[IO.File]::WriteAllText($cfgPath,($cfg|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
$name='WarsBots Mailbox Wake';$old=Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue
if($old -and $old.State -eq 'Running'){throw 'Existing scheduler tick active; retry after it finishes'}
$oldXml=if($old){Export-ScheduledTask -TaskName $name}else{$null}
try{
 if($old){Disable-ScheduledTask -TaskName $name|Out-Null}
 # Exact triage copies first, then reviewed compatibility/safety integration.
 $fresh=Join-Path $CodexRepo 'build/c122-fresh'
 foreach($f in Get-ChildItem $fresh -Filter '*.ps1'){Copy-Item $f.FullName $stable -Force}
 foreach($f in @('wake_agents.ps1','run_mailbox_wake.ps1','register_wake_task.ps1','checkpoint_mail.ps1','session_state.py')){Copy-Item (Join-Path $PSScriptRoot $f) $stable -Force}
 $shim='@echo off'+"`r`n"+'"'+$OpenCodeCmd+'" %*'+"`r`n";[IO.File]::WriteAllText((Join-Path $stable 'opencode.cmd'),$shim,[Text.Encoding]::ASCII)
 $args='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $stable 'wake_agents.ps1')+'" -ConfigFile "'+$cfgPath+'"'
 $action=New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $args -WorkingDirectory $CodexRepo
 $trigger=New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Minutes $EveryMin)
 $settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit ([TimeSpan]::Zero) -StartWhenAvailable
 $principal=New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Limited
 Register-ScheduledTask -TaskName $name -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Force|Out-Null
 Enable-ScheduledTask -TaskName $name|Out-Null
 "Registered ${name}: every${EveryMin}min idle${IdleMin}min; C090 untouched"
}catch{
 $backup=Join-Path $CodexRepo 'build/c122-before/stable-bin'
 foreach($f in Get-ChildItem $backup -File){Copy-Item $f.FullName $stable -Force}
 if($oldXml){Register-ScheduledTask -TaskName $name -Xml $oldXml -Force|Out-Null;Enable-ScheduledTask -TaskName $name|Out-Null}
 throw
}
