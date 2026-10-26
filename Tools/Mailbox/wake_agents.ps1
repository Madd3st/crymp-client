# C101: current-user idle wake for the existing Codex thread; never bypasses approvals.
[CmdletBinding()] param([Parameter(Mandatory)][string]$CodexRepo,[Parameter(Mandatory)][ValidatePattern('^[0-9a-f-]{36}$')][string]$ThreadId,[int]$IdleMin=20,[int]$MinimumItem=112,[switch]$DryRun)
$ErrorActionPreference='Stop'
$dir=Join-Path $env:LOCALAPPDATA 'WarsBots/Mailbox';New-Item -ItemType Directory -Force $dir|Out-Null
$stateFile=Join-Path $dir 'wake-codex-state.json';$pidFile=Join-Path $dir 'wake-codex.pid';$log=Join-Path $dir 'wake.log'
function Log([string]$Message){[IO.File]::AppendAllText($log,([DateTime]::UtcNow.ToString('o')+' '+$Message+"`n"),[Text.UTF8Encoding]::new($false));Write-Output $Message}
function Hash-Text([string]$Text){$h=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($h.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','')}finally{$h.Dispose()}}
if(Get-Process CryMP-Server64,CryMP-Client64,MSBuild,cl -ErrorAction SilentlyContinue){Log 'codex: game/build active, skip';return}
if(Test-Path $pidFile){$old=Get-Content $pidFile -Raw;if($old.Trim() -match '^\d+$' -and (Get-Process -Id ([int]$old.Trim()) -ErrorAction SilentlyContinue)){Log 'codex: previous wake still alive, skip';return}}
$repo=(Resolve-Path -LiteralPath $CodexRepo).Path
$stamp=[long](git -C $repo log -1 --format=%ct);if($LASTEXITCODE){throw 'Invalid repo'}
$now=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds();$cut=[DateTime]::UtcNow.AddMinutes(-$IdleMin)
if($IdleMin -gt 0 -and $now-$stamp -lt $IdleMin*60){Log 'codex: recent commit, skip';return}
foreach($line in (git -C $repo status --porcelain)){if($line.Length -gt 3){$file=Join-Path $repo $line.Substring(3).Trim('"');if((Test-Path -LiteralPath $file) -and (Get-Item -LiteralPath $file).LastWriteTimeUtc -gt $cut){Log 'codex: recent working-tree edit, skip';return}}}
git -C $repo fetch -q origin '+refs/heads/warsbots/*:refs/remotes/origin/warsbots/*';if($LASTEXITCODE){throw 'Fetch failed'}
$inbox=(git -C $repo show origin/warsbots/research/steelmill-triage:Docs/Research/Coordination/MAILBOX/TO_CODEX.md)-join "`n"
$outbox=Get-Content (Join-Path $repo 'Docs/Coordination/TO_CLAUDE.md') -Raw
$states=@{}
foreach($header in [regex]::Matches($outbox,'(?m)^### X-[^\r\n]*')){foreach($m in [regex]::Matches($header.Value,'\b(DONE|BLOCKED|REJECTED|ACK|STARTED|PROGRESS)\s+C-?0*(\d+)\b')){$key=$m.Groups[2].Value;if(-not $states.ContainsKey($key)){$states[$key]=$m.Groups[1].Value}}}
$previous=$null;if(Test-Path $stateFile){$previous=Get-Content $stateFile -Raw|ConvertFrom-Json}
$sections=@{};$open=@()
$completion=$null;$completionFile=Join-Path $dir 'codex-last-completion.json'
if(Test-Path $completionFile){$completion=Get-Content $completionFile -Raw|ConvertFrom-Json;if($IdleMin -gt 0 -and [DateTime]::Parse($completion.finished).ToUniversalTime() -gt $cut){Log 'codex: recent wake completion, skip';return}}
foreach($m in [regex]::Matches($inbox,'(?ms)^### C-(\d+)[^\r\n]*\r?\n.*?(?=^### C-|\z)')){
 $id=[int]$m.Groups[1].Value;$key=[string]$id;$hash=Hash-Text $m.Value;$sections[$key]=$hash
 $oldHash=if($previous -and $previous.sections.PSObject.Properties.Name -contains $key){$previous.sections.$key}else{$null}
 $changed=$null -ne $oldHash -and $oldHash -ne $hash
 $pending=-not $states.ContainsKey($key) -or $states[$key] -notin @('DONE','BLOCKED','REJECTED')
 if(($id -ge $MinimumItem -and $id -ne 113 -and $pending) -or $changed){$open+=$id}
}
if($completion -and $completion.exit -ne 0 -and $previous.item){$open+=[int]$previous.item}
if(-not $open){Log 'codex: no pending/changed C-items, skip';return}
$item=($open|Sort-Object -Descending|Select-Object -First 1)
$cli=Get-Command codex.cmd -ErrorAction Stop
$worker=Join-Path $PSScriptRoot 'run_codex_wake.ps1'
if($DryRun){Log ("codex: DRY-RUN would resume thread=$ThreadId item=C-$item idleMin=$IdleMin repo=$repo");return}
$prompt='Continue WarsBots in this existing thread. Read latest owner instructions first. Fetch +refs/heads/warsbots/*; read mailbox and NOW. Work only unfinished named tasks under D-013 (Claude writes game code, Codex tooling/QA), no GUI/public servers/deletions outside build, never duplicate active runs or successful checks. Print and validate recipes, preserve memory, Save the research ref at task start; after reporting use Tools/Mailbox/checkpoint_mail.ps1 for that item with the original processed ref, then push/fetch. Never checkpoint newly edited unprocessed instructions. If blocked report and take next. Respect owner game and stop/limit requests. Existing heartbeat prompt/interval must remain unchanged. Do not repeat successful checks or completed fallback probes without a new named SHA/item. If nothing actionable remains, exit quietly.'
$promptFile=Join-Path $dir 'codex-wake-prompt.txt';[IO.File]::WriteAllText($promptFile,$prompt,[Text.UTF8Encoding]::new($false))
$args='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+$worker+'" -Repo "'+$repo+'" -ThreadId '+$ThreadId+' -CodexCmd "'+$cli.Source+'" -PromptFile "'+$promptFile+'"'
$p=Start-Process powershell.exe -ArgumentList $args -WindowStyle Hidden -PassThru
[IO.File]::WriteAllText($pidFile,[string]$p.Id)
@{sections=$sections;item=$item;pid=$p.Id;started=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 5|Set-Content $stateFile -Encoding UTF8
Log "codex: resumed C-$item existing thread=$ThreadId worker=$($p.Id)"
