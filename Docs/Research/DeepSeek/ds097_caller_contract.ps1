# DS-097 caller contract: check the weapon-goal lifecycle in the REAL BotBrain.cpp text.
#   powershell -NoProfile -File ds097_caller_contract.ps1 -Source <path to BotBrain.cpp> -Label old|patched
# Red on b8b444d6 (unconditional candidate/timer reset in UpdateUpgrade), green on the DS-097 patch.
param(
  [Parameter(Mandatory)][string]$Source,
  [string]$Label = $(Split-Path -Leaf (Split-Path -Parent $Source))
)
$ErrorActionPreference = 'Stop'
$lines = Get-Content -LiteralPath $Source
function Count-Hits([string]$pattern) {
  @($lines | Select-String -Pattern $pattern -AllMatches | ForEach-Object { $_.LineNumber })
}
$fail = 0
function Report([string]$name, [bool]$ok, [string]$detail) {
  if ($ok) { Write-Output ("PASS {0}: {1}" -f $name, $detail) }
  else { Write-Output ("FAIL {0}: {1}" -f $name, $detail); $script:fail++ }
}

# 1. Exactly one creation site: UpgradeCandidate() is called only from the single writer.
$calls = @(Count-Hits 'UpgradeCandidate\s*\(' | Where-Object { $lines[$_ - 1] -notmatch 'BotBrain::UpgradeCandidate' })
Report 'single-candidate-call' ($calls.Count -eq 1) ("UpgradeCandidate calls at line(s) " + ($calls -join ','))

# 2. Both callers go through the single writer.
$writers = Count-Hits 'TryAcquireUpgradeGoal\s*\(\s*actor'
Report 'both-callers-use-writer' ($writers.Count -eq 2) ("TryAcquireUpgradeGoal(actor) at line(s) " + ($writers -join ','))

# 3. The total budget is written once, and only as the pursuit anchor (not on every call).
$stamps = Count-Hits 'm_upgradeSince\s*=\s*m_now'
$anchored = $false
if ($stamps.Count -eq 1) {
  $from = [Math]::Max(1, $stamps[0] - 6)
  $window = $lines[($from - 1)..($stamps[0] - 1)] -join "`n"
  $anchored = $window -match 'WeaponGoalPursues'
}
Report 'single-pursuit-anchor' ($stamps.Count -eq 1 -and $anchored) (
  "m_upgradeSince=m_now at line(s) " + ($stamps -join ',') + ", pursuit-anchored=" + $anchored)

Write-Output ("{0}: {1} check(s) failed" -f $Label, $fail)
exit ([int]($fail -gt 0))
