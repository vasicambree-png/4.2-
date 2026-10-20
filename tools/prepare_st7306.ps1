# Generate a complete local firmware working tree using the untouched 2.9-inch
# baseline plus ONLY the ST7306 overlay files maintained in this repository.
# The upstream repository is never written to or pushed.
param(
    [string]$BaseCommit = "f55ecc1042811ab410458be7223f19d4d9a5391e"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$workRoot = Join-Path $repoRoot ".work_st7306"
$upstream = Join-Path $workRoot "upstream_2_9"
$outRoot = Join-Path $workRoot "firmware"
$projectSrc = Join-Path $upstream "CH584_V1_0_1"
$projectDst = Join-Path $outRoot "CH584_V1_0_1"
$upstreamUrl = "https://github.com/vasicambree-png/2.9-.git"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git is required. Install Git for Windows and try again."
}
New-Item -ItemType Directory -Force -Path $workRoot | Out-Null
if (-not (Test-Path (Join-Path $upstream ".git"))) {
    Write-Host "[ST7306] Cloning untouched 2.9-inch baseline ..."
    & git clone --quiet $upstreamUrl $upstream
    if ($LASTEXITCODE -ne 0) { throw "Unable to clone upstream repository." }
}
Write-Host "[ST7306] Checking out pinned baseline: $BaseCommit"
& git -C $upstream fetch --quiet origin
if ($LASTEXITCODE -ne 0) { throw "Unable to fetch upstream history." }
& git -C $upstream checkout --quiet --detach $BaseCommit
if ($LASTEXITCODE -ne 0) { throw "Pinned baseline commit was not found." }

if (-not (Test-Path $projectSrc)) { throw "Firmware directory missing in pinned baseline." }
if (Test-Path $outRoot) { Remove-Item -LiteralPath $outRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null
Copy-Item -LiteralPath $projectSrc -Destination $projectDst -Recurse -Force

$files = @(
    "CH584_V1_0_1/APP/include/yuying_TFT.h",
    "CH584_V1_0_1/APP/yuying_TFT.c",
    "CH584_V1_0_1/APP/Usart3_task.c",
    "CH584_V1_0_1/u8g2/u8x8_d_st7305.c",
    "CH584_V1_0_1/u8g2/u8g2_d_memory.c",
    "CH584_V1_0_1/tools/ST7306_42_BRINGUP.md"
)
foreach ($f in $files) {
    $rel = $f.Replace("/", [IO.Path]::DirectorySeparatorChar)
    $overlayFile = Join-Path $repoRoot $rel
    $target = Join-Path $outRoot $rel
    if (-not (Test-Path -LiteralPath $overlayFile)) { throw "Missing overlay file: $f" }
    $parent = Split-Path $target -Parent
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Copy-Item -LiteralPath $overlayFile -Destination $target -Force
    Write-Host "[ST7306] Applied $f"
}
$projectFile = Join-Path $projectDst "CH584M_TFT_HB.wvproj"
if (-not (Test-Path -LiteralPath $projectFile)) { throw "MounRiver project file missing." }
Write-Host "[ST7306] Firmware working tree ready:"
Write-Host $projectFile
Write-Host "[ST7306] Not compiled or flashed. Open the project in MounRiver Studio."
