@echo off
setlocal
set "PHASE=%~1"
if "%PHASE%"=="" set "PHASE=after"
if not "%PHASE%"=="before" if not "%PHASE%"=="after" exit /b 2
set "OUTPUT_ROOT=%~2"
if "%OUTPUT_ROOT%"=="" set "OUTPUT_ROOT=%~dp0output"
set "PREVIEW_SCOPE=%~3"
if "%PREVIEW_SCOPE%"=="" set "PREVIEW_SCOPE=home"
if not "%PREVIEW_SCOPE%"=="home" if not "%PREVIEW_SCOPE%"=="menus" if not "%PREVIEW_SCOPE%"=="unified" exit /b 2
if not defined UI_PREVIEW_VCVARS set "UI_PREVIEW_VCVARS=D:\visual studio\visual studio2026\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%UI_PREVIEW_VCVARS%" (
  echo Missing vcvars64.bat: set UI_PREVIEW_VCVARS to your installed MSVC environment script.
  exit /b 2
)
call "%UI_PREVIEW_VCVARS%" >nul
if errorlevel 1 exit /b 2
set "FW=%~dp0..\..\CH584_V1_0_1"
cd /d "%OUTPUT_ROOT%\%PHASE%"
if errorlevel 1 exit /b 2
cl /nologo /TC /utf-8 /std:c11 /Gy /O1 /D_CRT_SECURE_NO_WARNINGS /DUI_PREVIEW /DU8G2_USE_LARGE_FONTS /I. /I"%FW%\APP\include" /I"%FW%\u8g2\include" "%~dp0host_render.c" ui_under_test.c font_engine.c bitmap_engine.c box_engine.c local_fonts.c "%FW%\APP\u8g2_font_cn.c" "%FW%\u8g2\u8x8_8x8.c" "%FW%\u8g2\u8g2_line.c" "%FW%\u8g2\u8g2_circle.c" /Fe:host_render.exe /link /OPT:REF /OPT:ICF >compile.log 2>&1
if errorlevel 1 (
  type compile.log
  exit /b 1
)
host_render.exe "%PREVIEW_SCOPE%"
exit /b %errorlevel%
