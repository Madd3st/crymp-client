<#
WarsBots mailbox: block until Claude leaves new mail for this agent, then print it.

  powershell -NoProfile -ExecutionPolicy Bypass -File wait_mail.ps1 -Agent codex -Repo C:\path\to\crymp-client
  -Agent      codex | opencode
  -Repo       any clone of Madd3st/crymp-client (only `git fetch` + `git show`, never checkout/merge)
  -TimeoutMin how long to wait (default 9, below most agent shell timeouts; call again in a loop)
  -PollSec    fetch interval (default 60)
  -Peek       print open items and exit at once (no waiting, no state change)

Exit 0 = new mail printed (state advanced), 1 = timeout (nothing new), 2 = error.
State: <repo>\.git\warsbots-mail-<agent>.last (highest message ID already seen).
#>
param(
  [Parameter(Mandatory)][ValidateSet('codex','opencode')][string]$Agent,
  [Parameter(Mandatory)][string]$Repo,
  [int]$TimeoutMin = 9,
  [int]$PollSec = 60,
  [switch]$Peek
)
$ErrorActionPreference = 'Stop'
$branch = 'warsbots/research/steelmill-triage'
$file   = if ($Agent -eq 'codex') { 'Docs/Research/Coordination/MAILBOX/TO_CODEX.md' } else { 'Docs/Research/Coordination/MAILBOX/TO_OPENCODE.md' }
$prefix = if ($Agent -eq 'codex') { 'C' } else { 'OC' }
$gitDir = (git -C $Repo rev-parse --git-dir 2>$null)
if (-not $gitDir) { Write-Output "wait_mail: $Repo is not a git repo"; exit 2 }
if (-not [IO.Path]::IsPathRooted($gitDir)) { $gitDir = Join-Path $Repo $gitDir }
$state = Join-Path $gitDir "warsbots-mail-$Agent.last"

function Get-Mail {
  git -C $Repo fetch -q origin "${branch}:refs/remotes/origin/$branch" 2>$null | Out-Null
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
  $open = Get-Mail | Where-Object { $_.Head -match '\bopen\b' }
  Write-Output "wait_mail: $($open.Count) open $prefix- item(s), last seen $prefix-$last"
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
