param(
    [string]$MakePath = 'D:\11\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe',
    [string]$CompilerBin = 'D:\11\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$firmware = Join-Path $repo 'CH584_V1_0_1'
$templates = Join-Path $firmware 'obj'
$output = Join-Path $PSScriptRoot 'output'
if (!(Test-Path -LiteralPath $MakePath) -or
    !(Test-Path -LiteralPath (Join-Path $CompilerBin 'riscv-wch-elf-gcc.exe'))) {
    throw 'Supply the installed MounRiver MakePath and WCH GCC12 CompilerBin.'
}
if (!(Test-Path -LiteralPath (Join-Path $templates 'makefile'))) {
    throw 'Build once in MounRiver to generate obj/makefile before running this script.'
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
# Reuse the actual IDE build rules in a separate output directory. Neither the
# IDE obj files nor the other chat's UI preview artifacts are overwritten.
$prefix = $firmware.Replace('\', '/') + '/'
foreach ($file in Get-ChildItem -LiteralPath $templates -File -Recurse |
         Where-Object { $_.Name -eq 'makefile' -or $_.Extension -eq '.mk' }) {
    $relative = $file.FullName.Substring($templates.Length + 1)
    $target = Join-Path $output $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    $rules = [IO.File]::ReadAllText($file.FullName).Replace('../', $prefix)
    [IO.File]::WriteAllText($target, $rules, [Text.UTF8Encoding]::new($false))
}
function Get-SourceHashes {
    $hashes = @{}
    foreach ($dir in 'APP', 'HAL', 'LIB', 'Ld', 'RVMSIS', 'Startup', 'StdPeriphDriver', 'u8g2', 'src') {
        foreach ($file in Get-ChildItem -LiteralPath (Join-Path $firmware $dir) -File -Recurse) {
            $hashes[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
    }
    return $hashes
}
$before = Get-SourceHashes
$before | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'source_hashes.json') -Encoding utf8
$oldPath = $env:Path
try {
    $env:Path = $CompilerBin + ';' + (Split-Path -Parent $MakePath) + ';' + $oldPath
    & $MakePath -C $output -j2 all 2>&1 | Tee-Object -FilePath (Join-Path $output 'build.log')
    $buildExit = $LASTEXITCODE
} finally {
    $env:Path = $oldPath
}
if ($buildExit -ne 0) { throw "Firmware build failed: exit $buildExit" }
$after = Get-SourceHashes
$changed = @($before.Keys | Where-Object { !$after.ContainsKey($_) -or $before[$_] -ne $after[$_] })
$added = @($after.Keys | Where-Object { !$before.ContainsKey($_) })
if ($changed.Count -or $added.Count) {
    throw 'Source changed during compilation. Run again after those edits finish.'
}
Write-Output "Build succeeded with stable source hashes. HEX: $output\CH584M_TFT_HB.hex"
