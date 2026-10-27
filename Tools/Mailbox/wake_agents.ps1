<#
WarsBots mailbox wake-up (safety net for sessions that stopped). Run by Task Scheduler every 10 min
(register_wake_task.ps1). For each configured agent: if Claude left mail newer than the last wake AND the
agent's own clone looks idle (no commit and no edited file for -IdleMin), start one non-interactive agent run
that reads the mailbox and works. Never starts a second run while one it started is still alive.

  wake_agents.ps1 -CodexRepo C:\...\crymp-client [-DeepSeekRepo C:\...\deepseek-clone] [-OpenCodeRepo ...] [-IdleMin 20] [-DryRun]

Commands (override if your CLI differs):
  -CodexCmd    default: codex exec --full-auto -C "<repo>" "<prompt>"
  -OpenCodeCmd default: opencode run "<prompt>"   (started in <repo>)
Log: %LOCALAPPDATA%\WarsBots\Mailbox\wake.log
#>
param(
  [string]$CodexRepo,
  [string]$OpenCodeRepo,
  [string]$DeepSeekRepo,
  [int]$IdleMin = 20,
  [string]$CodexCmd = 'codex',
  [string]$OpenCodeCmd = 'opencode',
  [switch]$DryRun
)
$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$logDir = Join-Path $env:LOCALAPPDATA 'WarsBots\Mailbox'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir 'wake.log'
function Log($m) { "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $m" | Add-Content -Path $log -Encoding utf8 }

$prompts = @{
  codex    = 'Mailbox wake-up from the WarsBots scheduler. Fetch warsbots/research/steelmill-triage, read Docs/Research/Coordination/MAILBOX/TO_CODEX.md, take the highest-priority open C- item, do it on your branch warsbots/feature/weapon-goals per PROTOCOL.md (ack/answer in Docs/Coordination/TO_CLAUDE.md, push). When done, run Tools/Mailbox/wait_mail.ps1 -Agent codex in a loop and continue with any new mail.'
  deepseek = 'Mailbox wake-up from the WarsBots scheduler. Fetch +refs/heads/warsbots/*:refs/remotes/origin/warsbots/*, read Docs/Research/Coordination/MAILBOX/TO_DEEPSEEK.md on warsbots/research/steelmill-triage, take the newest unanswered DS- item, do it per PROTOCOL.md in your branch warsbots/research/deepseek (reply in Docs/Coordination/DEEPSEEK_TO_CLAUDE.md, push). Never edit existing game code; patches only. When done, run Tools/Mailbox/wait_mail.ps1 -Agent deepseek in a loop and continue with any new mail.'
  opencode = 'Mailbox wake-up from the WarsBots scheduler. Fetch warsbots/research/steelmill-triage, read Docs/Research/Coordination/MAILBOX/TO_OPENCODE.md, take the next open OC- item, do it read-only per PROTOCOL.md in your branch warsbots/research/movement-params (reply in Docs/Coordination/OPENCODE_TO_CLAUDE.md, push). Never edit game code. When done, run Tools/Mailbox/wait_mail.ps1 -Agent opencode in a loop and continue with any new mail.'
}

function Is-Idle($repo) {
  $age = [int](git -C $repo log -1 --format=%ct 2>$null)
  $now = [int][double]::Parse((Get-Date -UFormat %s))
  if ($age -and ($now - $age) -lt $IdleMin * 60) { return $false }
  $cut = (Get-Date).AddMinutes(-$IdleMin)
  foreach ($l in (git -C $repo status --porcelain 2>$null)) {
    $p = Join-Path $repo ($l.Substring(3).Trim('"'))
    if ((Test-Path $p) -and (Get-Item $p).LastWriteTime -gt $cut) { return $false }
  }
  return $true
}

function Wake($agent, $repo) {
  if (-not $repo) { return }
  if (-not (Test-Path $repo)) { Log "$agent repo missing: $repo"; return }
  $pidFile = Join-Path $logDir "wake-$agent.pid"
  if (Test-Path $pidFile) {
    $old = (Get-Content $pidFile -Raw).Trim()
    if ($old -and (Get-Process -Id $old -ErrorAction SilentlyContinue)) { Log "${agent}: run $old still alive, skip"; return }
  }
  # New mail since last wake? (separate state from the agent's own wait_mail state)
  $wakeState = Join-Path $logDir "wake-$agent.last"
  $prefix = switch ($agent) { 'codex' { 'C' } 'deepseek' { 'DS' } default { 'OC' } }
  $file = switch ($agent) { 'codex' { 'TO_CODEX.md' } 'deepseek' { 'TO_DEEPSEEK.md' } default { 'TO_OPENCODE.md' } }
  git -C $repo fetch -q origin 'warsbots/research/steelmill-triage:refs/remotes/origin/warsbots/research/steelmill-triage' 2>$null | Out-Null
  $text = git -C $repo show "refs/remotes/origin/warsbots/research/steelmill-triage:Docs/Research/Coordination/MAILBOX/$file" 2>$null
  $ids = [regex]::Matches(($text -join "`n"), "(?m)^### $prefix-(\d+)\b") | ForEach-Object { [int]$_.Groups[1].Value }
  if (-not $ids) { Log "${agent}: no items"; return }
  $max = ($ids | Measure-Object -Maximum).Maximum
  $last = 0; if (Test-Path $wakeState) { [int]::TryParse((Get-Content $wakeState -Raw).Trim(), [ref]$last) | Out-Null }
  if ($max -le $last) { return }
  if (-not (Is-Idle $repo)) { Log "${agent}: new $prefix-$max but agent looks busy, retry later"; return }
  $cmd = if ($agent -eq 'codex') { $CodexCmd } else { $OpenCodeCmd } # DeepSeek runs inside OpenCode
  if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) { Log "${agent}: CLI '$cmd' not found, cannot wake"; return }
  $cliArgs = if ($agent -eq 'codex') { @('exec', '--full-auto', '-C', $repo, $prompts.codex) } else { @('run', $prompts[$agent]) }
  if ($DryRun) { Log "${agent}: DRY-RUN would start $cmd for $prefix-$max"; return }
  $out = Join-Path $logDir "$agent-$(Get-Date -Format yyyyMMdd-HHmmss).log"
  $p = Start-Process -FilePath $cmd -ArgumentList $cliArgs -WorkingDirectory $repo -WindowStyle Minimized `
       -RedirectStandardOutput $out -RedirectStandardError "$out.err" -PassThru
  Set-Content $pidFile $p.Id -Encoding ascii
  Set-Content $wakeState $max -Encoding ascii
  Log "${agent}: woke for $prefix-$max (pid $($p.Id), log $out)"
}

Wake 'codex' $CodexRepo
Wake 'opencode' $OpenCodeRepo
Wake 'deepseek' $DeepSeekRepo
