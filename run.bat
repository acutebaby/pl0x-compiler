@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

if not exist "src\pl0x.exe" (
    echo [!] src\pl0x.exe not found. Building it now...
    call build.bat nopause
)

if not exist "src\pl0x.exe" (
    echo [!] Build failed or compiler missing. Run build.bat to see details.
    pause
    exit /b 1
)

set "CASE=%~1"
if not defined CASE goto menu
goto runit

:menu
echo ============================================================
echo    PL/0 Compiler - Group 5 - Test Cases
echo ============================================================
echo    t1 ~ t12 are available.  Type a name or a number.
echo    Note: t4 reads numbers from the keyboard.
echo.
set "CASE="
set /p "CASE=Case (t10 or 10, q to quit): "
if not defined CASE goto menu
if /i "%CASE%"=="q" exit /b 0

:runit
if /i "%CASE:~0,1%"=="t" goto havecase
set "CASE=t%CASE%"
:havecase
set "SRCPATH=cases\%CASE%.pl0"
if not exist "%SRCPATH%" (
    echo [!] "%SRCPATH%" does not exist.
    echo.
    pause
    exit /b 1
)

if not exist "_out" mkdir "_out"
if not exist "_out\%CASE%" mkdir "_out\%CASE%"

echo.
echo ----- running %CASE% -----
if /i "%CASE%"=="t4" (
    echo [INPUT] line 1: how many numbers, then the numbers themselves.
    echo         for example:  5     then    3 17 9 42 8
    echo.
)

pushd "_out\%CASE%"
"..\..\src\pl0x.exe" "..\..\%SRCPATH%"
set "RC=%ERRORLEVEL%"
popd

echo.
echo ----- output files: _out\%CASE%\fa.tmp fas.tmp fa2.tmp f1.tmp -----
echo ----- reference    : run\%CASE%\  ^(compare with the four files^) -----
echo.
pause
exit /b %RC%
