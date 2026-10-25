# Publishes Docs/Wiki from warsbots/research/steelmill-triage to the GitHub wiki (Madd3st/crymp-client.wiki).
# Usage (PowerShell): powershell -ExecutionPolicy Bypass -File Publish-WarsBotsWiki.ps1 [-Repo C:\path\to\crymp-client]
[CmdletBinding()] param([string]$Repo = 'C:\Users\Magic8\WarsBotsBuild\crymp-multibot')
$ErrorActionPreference = 'Stop'
$wiki = Join-Path $env:TEMP 'wb-wiki2'
if (-not (Test-Path (Join-Path $wiki '.git'))) { git clone https://github.com/Madd3st/crymp-client.wiki.git $wiki }
git -C $wiki pull --ff-only
git -C $Repo fetch origin warsbots/research/steelmill-triage
$zip = Join-Path $env:TEMP 'wbwiki.zip'; $out = Join-Path $env:TEMP 'wbwiki'
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
git -C $Repo archive --format=zip -o $zip FETCH_HEAD Docs/Wiki
Expand-Archive $zip -DestinationPath $out -Force
Copy-Item (Join-Path $out 'Docs\Wiki\*.md') $wiki -Force
git -C $wiki add -A
git -C $wiki diff --cached --quiet
if ($LASTEXITCODE -eq 0) { Write-Output 'Wiki already up to date.'; exit 0 }
git -C $wiki -c user.name='Madd3st' -c user.email='madd3st@gmail.com' commit -m ("WarsBots wiki " + (Get-Date -Format 'yyyy-MM-dd HH:mm'))
git -C $wiki push
Write-Output 'Published: https://github.com/Madd3st/crymp-client/wiki'
