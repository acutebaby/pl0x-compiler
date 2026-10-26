@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================================
echo    Building src\pl0x.exe
echo ============================================================

rem ---------- 1) try gcc (MinGW-w64) ----------
where gcc >nul 2>&1
if errorlevel 1 goto trycl
echo [1/2] gcc found, compiling...
gcc -O2 -o "src\pl0x.exe" "src\pl0x.c" -lm
if not errorlevel 1 if exist "src\pl0x.exe" goto ok

rem ---------- 2) try MSVC cl.exe directly ----------
:trycl
echo [2/2] looking for Visual Studio C++ compiler...

set "VSDIR="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if defined VSDIR goto gotvs
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community" set "VSDIR=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
if defined VSDIR goto gotvs
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools" set "VSDIR=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
if defined VSDIR goto gotvs
goto nocc

:gotvs
set "MSVCDIR="
if exist "!VSDIR!\VC\Tools\MSVC" for /d %%d in ("!VSDIR!\VC\Tools\MSVC\*") do set "MSVCDIR=%%d"
if not defined MSVCDIR goto nocc
if not exist "!MSVCDIR!\bin\Hostx64\x64\cl.exe" goto nocc

set "SDKVER="
set "SDKINC="
if exist "%ProgramFiles(x86)%\Windows Kits\10\Include" for /d %%d in ("%ProgramFiles(x86)%\Windows Kits\10\Include\*") do (
    set "SDKVER=%%~nxd"
    set "SDKINC=%ProgramFiles(x86)%\Windows Kits\10\Include\%%~nxd"
)
if not defined SDKINC goto nocc
set "SDKLIB=%ProgramFiles(x86)%\Windows Kits\10\Lib\!SDKVER!"
if not exist "!SDKLIB!\ucrt\x64" goto nocc

set "INCLUDE=!MSVCDIR!\include;!SDKINC!\ucrt;!SDKINC!\shared;!SDKINC!\um"
set "LIB=!MSVCDIR!\lib\x64;!SDKLIB!\ucrt\x64;!SDKLIB!\um\x64"

echo       VS  : !VSDIR!
echo       SDK : !SDKVER!
echo       compiling...
pushd src
"!MSVCDIR!\bin\Hostx64\x64\cl.exe" /nologo /utf-8 /O2 /W0 /D_CRT_SECURE_NO_WARNINGS /TC pl0x.c /Fe:pl0x.exe
set "CLRC=!ERRORLEVEL!"
popd
if not "!CLRC!"=="0" goto failed
if not exist "src\pl0x.exe" goto failed
if exist "src\pl0x.obj" del "src\pl0x.obj"
goto ok

:ok
echo.
echo [OK] built: src\pl0x.exe
if /i not "%~1"=="nopause" pause
exit /b 0

:failed
echo.
echo [!] Compile failed. See the messages above.
if /i not "%~1"=="nopause" pause
exit /b 1

:nocc
echo.
echo [!] No usable C compiler found on this machine.
echo     Install one of these, then run build.bat again:
echo       - MinGW-w64 (gcc)         https://winlibs.com
echo       - Visual Studio Installer -^> "Desktop development with C++"
echo.
echo     Not required for just running: src\pl0x.exe is already built,
echo     so you can double-click run.bat directly.
if /i not "%~1"=="nopause" pause
exit /b 2
