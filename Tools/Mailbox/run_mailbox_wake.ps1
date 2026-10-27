[CmdletBinding()] param([Parameter(Mandatory)][string]$ConfigFile,[Parameter(Mandatory)][ValidateSet('codex','deepseek')][string]$Agent,[Parameter(Mandatory)][string]$PromptFile,[switch]$Verify)
$ErrorActionPreference='Continue'
$cfg=Get-Content -LiteralPath $ConfigFile -Raw -Encoding UTF8|ConvertFrom-Json
$dir=Split-Path $ConfigFile -Parent;$dir=Split-Path $dir -Parent
$stamp=Get-Date -Format yyyyMMdd-HHmmss;$log=Join-Path $dir "$Agent-$stamp.jsonl";$exitValue=1
try {
 if(Get-Process CryMP-Client64,CryMP-Server64,MSBuild,cl -ErrorAction SilentlyContinue){throw 'DEFERRED game/build active before worker call'}
 $prompt=[IO.File]::ReadAllText($PromptFile,[Text.Encoding]::UTF8)
 if($Agent -eq 'codex'){
  Set-Location -LiteralPath $cfg.CodexRepo
  # Fresh nonpersistent job: no desktop-thread writer conflict, no extra saved chat, normal review.
  $prompt|& $cfg.CodexCmd --approve-for-me exec --ephemeral -C $cfg.CodexRepo --json - *> $log
 }else{
  Set-Location -LiteralPath $cfg.DeepSeekSessionDirectory
  # Continue only the explicitly identified DeepSeek session; inherit its model and normal permissions.
  & $cfg.OpenCodeCmd run --session $cfg.DeepSeekSession --format json $prompt *> $log
 }
 $exitValue=$LASTEXITCODE
}catch{$_|Out-String|Set-Content -LiteralPath ($log+'.err') -Encoding UTF8}
@{exit=$exitValue;agent=$Agent;log=$log;verify=[bool]$Verify;finished=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $dir "$Agent-last-completion-v2.json") -Encoding UTF8
if($Verify){
 $request=Join-Path $dir 'c122-deepseek-verification.started.json'
 if(Test-Path $request){$suffix=if($exitValue -eq 0){'done'}else{'failed'};Move-Item -LiteralPath $request -Destination (Join-Path $dir "c122-deepseek-verification.$suffix.json") -Force}
}
exit $exitValue
