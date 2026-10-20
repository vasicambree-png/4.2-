param(
    [Parameter(Mandatory=$true)][string]$Repo,
    [string]$Task,
    [ValidateSet('ds','gpt')][string]$Provider = 'ds',
    [string]$Resume,
    [string]$Fork,
    [string]$Handoff,
    [string]$Prompt,
    [string]$Model = 'gpt-6-sol'
)
$ErrorActionPreference = 'Stop'
$bridgeArgs = @((Join-Path $PSScriptRoot 'bridge.py'), 'run', '--repo', $Repo, '--provider', $Provider)
if ($Provider -eq 'gpt') { $bridgeArgs += @('--model', $Model) }
if ($Task) { $bridgeArgs += @('--task', $Task) }
elseif ($Prompt) { $bridgeArgs += @('--prompt', $Prompt) }
else { throw 'Supply -Task or -Prompt.' }
if ($Resume) { $bridgeArgs += @('--resume', $Resume) }
if ($Fork) { $bridgeArgs += @('--fork', $Fork) }
if ($Handoff) { $bridgeArgs += @('--handoff', $Handoff) }
& python @bridgeArgs
exit $LASTEXITCODE
