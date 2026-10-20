@echo off
setlocal
if not defined SCREEN_POWER_VCVARS set "SCREEN_POWER_VCVARS=D:\visual studio\visual studio2026\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%SCREEN_POWER_VCVARS%" (
  echo Missing vcvars64.bat. Set SCREEN_POWER_VCVARS to the installed MSVC environment script.
  exit /b 2
)
call "%SCREEN_POWER_VCVARS%" >nul
if errorlevel 1 exit /b 2
cd /d "%~dp0"
cl /nologo /TC /std:c11 /utf-8 /W4 /WX /I"%~dp0..\..\CH584_V1_0_1\APP\include" tests.c /Fe:screen_power_tests.exe >compile.log 2>&1
if errorlevel 1 (
  type compile.log
  exit /b 1
)
screen_power_tests.exe
exit /b %errorlevel%
