param([switch]$SkipTests)

$ErrorActionPreference = 'Stop'
$toolRoot = $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    throw "未找到 Windows 内置 .NET Framework C# 编译器：$compiler"
}

$sourceRoot = Join-Path $toolRoot 'src'
$resultRoot = Join-Path $toolRoot '验证结果'
$protocolSource = Join-Path $sourceRoot 'Protocol.cs'
$programSource = Join-Path $sourceRoot 'Program.cs'
$testSource = Join-Path $sourceRoot 'ProtocolTests.cs'
$appPath = Join-Path $toolRoot 'CH584屏幕验证助手.exe'
$testPath = Join-Path $toolRoot 'ProtocolSelfTest.exe'
foreach ($source in @($protocolSource, $programSource, $testSource)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "缺少源文件：$source" }
}
[void][IO.Directory]::CreateDirectory($resultRoot)

$common = @('/nologo', '/langversion:5', '/optimize+', '/platform:anycpu', '/reference:System.dll', '/reference:System.Core.dll', '/reference:System.Xml.dll')
& $compiler @common '/target:winexe' "/out:$appPath" '/reference:System.Drawing.dll' '/reference:System.Windows.Forms.dll' $programSource $protocolSource
if ($LASTEXITCODE -ne 0) { throw "界面程序编译失败，退出码 $LASTEXITCODE" }

& $compiler @common '/target:exe' "/out:$testPath" '/main:Ch584ScreenVerifier.ProtocolTests' $testSource $protocolSource
if ($LASTEXITCODE -ne 0) { throw "协议自测程序编译失败，退出码 $LASTEXITCODE" }

if (-not $SkipTests) {
    $testOutput = @(& $testPath 2>&1)
    $testExit = $LASTEXITCODE
    $testLog = Join-Path $resultRoot '协议测试.txt'
    [IO.File]::WriteAllLines($testLog, [string[]]$testOutput, (New-Object Text.UTF8Encoding($true)))
    $testOutput | Write-Output
    if ($testExit -ne 0) { throw "协议自测失败，详见 $testLog" }
}
Write-Output "编译完成：$appPath"
Write-Output '构建与协议自测不会打开串口、烧录固件或操作硬件。'
