# C122: reviewed triage wake integration; no GUI, no duplicate work, no approval bypass.
[CmdletBinding()] param([string]$ConfigFile=(Join-Path $PSScriptRoot 'wake-config.json'),[switch]$DryRun)
$ErrorActionPreference='Stop';[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);$OutputEncoding=[Console]::OutputEncoding
$cfg=Get-Content -LiteralPath $ConfigFile -Raw -Encoding UTF8|ConvertFrom-Json
$dir=Join-Path $env:LOCALAPPDATA 'WarsBots/Mailbox';$log=Join-Path $dir 'wake.log';$env:PATH=$PSScriptRoot+';'+$env:PATH
function Log($m){[IO.File]::AppendAllText($log,([DateTime]::UtcNow.ToString('o')+' '+$m+"`n"),[Text.UTF8Encoding]::new($false));Write-Output $m}
function Hash($s){$h=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($h.ComputeHash([Text.Encoding]::UTF8.GetBytes($s)))).Replace('-','')}finally{$h.Dispose()}}
function Recent-Repo($repo){
 $cut=[DateTime]::UtcNow.AddMinutes(-[int]$cfg.IdleMin);$age=[long](git -C $repo log -1 --format=%ct)
 if([DateTimeOffset]::UtcNow.ToUnixTimeSeconds()-$age -lt $cfg.IdleMin*60){return 'recent commit'}
 foreach($ln in (git -C $repo -c core.quotepath=false status --porcelain)){
  if($ln.Length -gt 3){$file=Join-Path $repo $ln.Substring(3).Trim('"');if((Test-Path -LiteralPath $file) -and (Get-Item -LiteralPath $file).LastWriteTimeUtc -gt $cut){return 'recent working-tree edit'}}
 }
 return $null
}
function Wake($agent,$repo,$prefix,$outbox){
 if(-not $repo){return}
 $stateFile=Join-Path $dir "wake-$agent-v2.json";$old=$null
 if(Test-Path $stateFile){$old=Get-Content $stateFile -Raw -Encoding UTF8|ConvertFrom-Json}
 if($old -and $old.worker){$p=Get-CimInstance Win32_Process -Filter "ProcessId=$($old.worker)";if($p -and $p.CommandLine -like '*run_mailbox_wake.ps1*' -and $p.CommandLine -like "* -Agent $agent *"){Log "${agent}: previous worker active, skip";return}}
 git -C $repo fetch -q origin '+refs/heads/warsbots/*:refs/remotes/origin/warsbots/*';if($LASTEXITCODE){throw 'Mailbox fetch failed'}
 $inbox=(git -C $repo show "origin/warsbots/research/steelmill-triage:Docs/Research/Coordination/MAILBOX/TO_$($agent.ToUpperInvariant()).md")-join "`n"
 $answers=Get-Content -LiteralPath (Join-Path $repo $outbox) -Raw -Encoding UTF8;$states=@{}
 foreach($head in [regex]::Matches($answers,'(?m)^### [^\r\n]+')){
  $idMatch=[regex]::Match($head.Value,"\b$prefix-?0*(\d+)\b");$status=[regex]::Match($head.Value,'\b(DONE|BLOCKED|REJECTED|ACK|STARTED|PROGRESS)\b')
  if($idMatch.Success -and $status.Success -and -not $states.ContainsKey($idMatch.Groups[1].Value)){$states[$idMatch.Groups[1].Value]=$status.Value}
 }
 $known=@{};if($old -and $old.sections){foreach($p in $old.sections.PSObject.Properties){$known[$p.Name]=$p.Value}}
 $pending=@();$all=@{};$seen=@{}
 foreach($section in [regex]::Matches($inbox,"(?ms)^### $prefix-(\d+)[^\r\n]*\r?\n.*?(?=^### $prefix-|\z)")){
  $id=[int]$section.Groups[1].Value;$key=[string]$id;if($seen.ContainsKey($key)){continue};$seen[$key]=$true;$body=$section.Value.Substring($section.Value.IndexOf("`n")+1).Trim();$hash=Hash $body;$all[$key]=$hash
  $historical=$agent -eq 'deepseek' -and $id -lt 47
  if($historical -and -not $known.ContainsKey($key)){$known[$key]=$hash} # migration baseline before already-done DS047; amendments remain detectable
  $terminal=$states[$key] -in @('DONE','BLOCKED','REJECTED') -or $section.Value.Split("`n")[0] -match '(?i)\bdone\b'
  if($terminal -and -not $known.ContainsKey($key)){$known[$key]=$hash}
  $changed=$known.ContainsKey($key) -and $known[$key] -ne $hash
  $skipMeta=$agent -eq 'codex' -and ($id -lt 112 -or $id -eq 113)
  if(-not $skipMeta -and ((-not $terminal -and (-not $known.ContainsKey($key) -or (-not $historical -and $states[$key] -in @('ACK','STARTED','PROGRESS')))) -or $changed)){$pending+=$id}
 }
 if($agent -eq 'codex'){Log ("codex: C-121="+$states['121']+' C-122='+$states['122']+' (outbox checked; no literal open-header requirement)')}
 @{sections=$known;checked=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 5|Set-Content $stateFile -Encoding UTF8
 $busy=Recent-Repo $repo
 $doneFile=Join-Path $dir "$agent-last-completion-v2.json"
 if(Test-Path $doneFile){$done=Get-Content $doneFile -Raw -Encoding UTF8|ConvertFrom-Json;if([DateTime]::Parse($done.finished).ToUniversalTime() -gt [DateTime]::UtcNow.AddMinutes(-[int]$cfg.IdleMin)){$busy='recent worker completion; cooldown'}}
 if($agent -eq 'codex' -and (Test-Path $cfg.HostThreadFile) -and (Get-Item $cfg.HostThreadFile).LastWriteTimeUtc -gt [DateTime]::UtcNow.AddMinutes(-[int]$cfg.IdleMin)){$busy='desktop thread recently active'}
 if($agent -eq 'deepseek'){
  $session=& $cfg.Python (Join-Path $PSScriptRoot 'session_state.py') --session $cfg.DeepSeekSession --latest|ConvertFrom-Json
  if($LASTEXITCODE){throw 'OpenCode session metadata lookup failed'}
  Log "deepseek: current session=$($session.session)"
  if(-not $session.idle -or $session.age_seconds -lt $cfg.IdleMin*60){$busy="session recent/active age=$([int]$session.age_seconds)s pending=$($session.pending)"}
 }
 $verify=$agent -eq 'deepseek' -and (Test-Path (Join-Path $dir 'c122-deepseek-verification.request.json'))
 if($busy){Log "${agent}: $busy; pending=$($pending -join ',') verify=$verify; skip";return}
 if(-not $pending -and -not $verify){@{sections=$known;checked=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 5|Set-Content $stateFile -Encoding UTF8;Log "${agent}: no unanswered/changed mail; skip (DS-047=$($states['47']))";return}
 $item=$pending|Sort-Object -Descending|Select-Object -First 1
 if($DryRun){Log "${agent}: DRY-RUN eligible item=$item verify=$verify";return}
 $prompt=if($verify){'C122 scheduler delivery receipt only. Reply exactly C122_WAKE_ACK and stop. No tools, no file changes, no game or builds. DS-047 is already DONE; do not repeat it.'}elseif($agent -eq 'codex'){
  'WarsBots mailbox worker. Latest owner instructions are recorded in Docs/STATUS.md and the NOW line. Fetch +refs/heads/warsbots/*:refs/remotes/origin/warsbots/*, read TO_CODEX.md and your outbox; do only unfinished named work in weapon-goals. Respect current stop/limit requests and active owner game. Completed-task historical STOP entries apply to their named task only; use the current NOW and STATUS for latest owner authorization, never let an archived C121 STOP override newer explicitly authorized mailbox work. D-013: Claude writes all gameplay code; you do tooling/QA only. No GUI without explicit owner permission, no duplicate or already successful runs, no force/main. Validate recipes before any QA and preserve/restore memory. Report and push each completed task. Exit quietly if nothing is actionable.'
 }else{"WarsBots mailbox wake. Task repo is $repo on warsbots/research/deepseek. Fetch all warsbots refs, read TO_DEEPSEEK.md and DEEPSEEK_TO_CLAUDE.md. Do only unanswered DS work under D-014: read-only gameplay, patches/reports only, no builds, game, servers, merges or feature-branch edits. DS-047 is already DONE; never repeat completed work. Respect latest owner stop/game requests. Report/push, then wait_mail -Agent deepseek as authorized. No other project changes."}
 $promptFile=Join-Path $dir "$agent-wake-prompt-v2.txt";[IO.File]::WriteAllText($promptFile,$prompt,[Text.UTF8Encoding]::new($false))
 $args='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $PSScriptRoot 'run_mailbox_wake.ps1')+'" -ConfigFile "'+$ConfigFile+'" -Agent '+$agent+' -PromptFile "'+$promptFile+'"'
 if($verify){$args+=' -Verify';Move-Item -LiteralPath (Join-Path $dir 'c122-deepseek-verification.request.json') -Destination (Join-Path $dir 'c122-deepseek-verification.started.json') -Force}
 $p=Start-Process powershell.exe -ArgumentList $args -WorkingDirectory $repo -WindowStyle Hidden -PassThru
 @{sections=$known;worker=$p.Id;item=$item;verify=$verify;started=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 5|Set-Content $stateFile -Encoding UTF8
 Log "${agent}: STARTED worker=$($p.Id) item=$item verify=$verify; normal permissions, existing DeepSeek session / ephemeral Codex"
}
if(Get-Process CryMP-Client64,CryMP-Server64,MSBuild,cl -ErrorAction SilentlyContinue){Log 'all: game/build active, skip before fetch or worker';return}
if(-not(Test-Path $cfg.CodexCmd) -or -not(Test-Path $cfg.OpenCodeCmd)){throw 'Pinned CLI missing; no installation or silent replacement'}
Wake 'codex' $cfg.CodexRepo 'C' 'Docs/Coordination/TO_CLAUDE.md'
Wake 'deepseek' $cfg.DeepSeekRepo 'DS' 'Docs/Coordination/DEEPSEEK_TO_CLAUDE.md'
