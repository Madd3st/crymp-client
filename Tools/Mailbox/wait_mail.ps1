<#
WarsBots mailbox: block until Claude leaves new mail for this agent, then print it.

  powershell -NoProfile -ExecutionPolicy Bypass -File wait_mail.ps1 -Agent codex -Repo C:\path\to\crymp-client
  -Agent      codex | opencode | deepseek
  -Repo       any clone of Madd3st/crymp-client (only `git fetch` + `git show`, never checkout/merge)
  -TimeoutMin how long to wait (default 9, below most agent shell timeouts; call again in a loop)
  -PollSec    fetch interval (default 60)
  -Peek       print open items and exit at once (no waiting, no state change)
  -StateDir   where the "last seen" counter lives (default %LOCALAPPDATA%\WarsBots\Mailbox; ASCII path)

Exit 0 = new mail printed (state advanced), 1 = timeout (nothing new), 2 = error.
State: <StateDir>\mail-<agent>.last (highest message ID already seen).
#>
param(
  [Parameter(Mandatory)][ValidateSet('codex','opencode','deepseek')][string]$Agent,
  [Parameter(Mandatory)][string]$Repo,
  [int]$TimeoutMin = 9,
  [int]$PollSec = 60,
  [switch]$Peek,
  [string]$StateDir = (Join-Path $env:LOCALAPPDATA 'WarsBots\Mailbox')
)
$ErrorActionPreference = 'Stop'
# git prints UTF-8; without this PowerShell 5.1 mangles Cyrillic paths and message text.
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$OutputEncoding = [Text.Encoding]::UTF8
$branch = 'warsbots/research/steelmill-triage'
$file   = switch ($Agent) { 'codex' { 'Docs/Research/Coordination/MAILBOX/TO_CODEX.md' } 'deepseek' { 'Docs/Research/Coordination/MAILBOX/TO_DEEPSEEK.md' } default { 'Docs/Research/Coordination/MAILBOX/TO_OPENCODE.md' } }
$prefix = switch ($Agent) { 'codex' { 'C' } 'deepseek' { 'DS' } default { 'OC' } }
git -C $Repo rev-parse --git-dir 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Output "wait_mail: $Repo is not a git repo"; exit 2 }
New-Item -ItemType Directory -Force -Path $StateDir | Out-Null
$state = Join-Path $StateDir "mail-$Agent.last"

function Get-Mail {
  # '+': a rewritten remote branch must still update the local ref (26.10: DeepSeek saw "state 21" with DS-026 open)
  $err = git -C $Repo fetch -q origin "+${branch}:refs/remotes/origin/$branch" 2>&1
  if ($LASTEXITCODE -ne 0) { Write-Output "wait_mail: fetch FAILED ($err) - mail may be stale" }
  $text = git -C $Repo show "refs/remotes/origin/${branch}:$file" 2>$null
  if (-not $text) { return @() }
  $items = @(); $cur = $null
  foreach ($line in ($text -split "`n")) {
    if ($line -match "^### $prefix-(\d+) ") {
      if ($cur) { $items += $cur }
      $cur = [pscustomobject]@{ Id = [int]$Matches[1]; Head = $line.TrimEnd(); Body = New-Object System.Collections.Generic.List[string] }
    } elseif ($cur) { $cur.Body.Add($line.TrimEnd()) }
  }
  if ($cur) { $items += $cur }
  return $items
}

function Show($items) {
  foreach ($i in ($items | Sort-Object Id)) { Write-Output $i.Head; $i.Body | ForEach-Object { Write-Output $_ } }
}

$last = 0
if (Test-Path $state) { [int]::TryParse((Get-Content $state -Raw).Trim(), [ref]$last) | Out-Null }

if ($Peek) {
  $all = @(Get-Mail)
  $open = @($all | Where-Object { $_.Id -gt $last })
  $top = ($all | Measure-Object Id -Maximum).Maximum
  Write-Output "wait_mail: newest on branch $prefix-$top, last seen $prefix-$last, $($open.Count) unseen"
  Show $open; exit 0
}

$deadline = (Get-Date).AddMinutes($TimeoutMin)
do {
  $items = Get-Mail
  $new = @($items | Where-Object { $_.Id -gt $last })
  if ($new.Count -gt 0) {
    $max = ($items | Measure-Object Id -Maximum).Maximum
    Set-Content -Path $state -Value $max -Encoding ascii
    Write-Output "wait_mail: NEW MAIL for $Agent ($($new.Count) item(s), up to $prefix-$max). Act on it now:"
    Show $new
    exit 0
  }
  if ((Get-Date).AddSeconds($PollSec) -gt $deadline) { break }
  Start-Sleep -Seconds $PollSec
} while ($true)
Write-Output "wait_mail: no new mail for $Agent in $TimeoutMin min (last seen $prefix-$last). Call again."
exit 1
