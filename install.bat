@echo off
REM Phob Ult Control Plus - Windows helper for tools/patch.py
REM
REM Drag your PhobGCC-SW folder onto this file, or just double-click it and
REM answer the prompt. This only patches the source tree; building the .uf2 is
REM a separate step and needs the Pico SDK toolchain. Most people should just
REM download a build from the Releases page instead.

setlocal
cd /d "%~dp0"

where python >nul 2>&1
if errorlevel 1 (
    echo.
    echo Python was not found on your PATH.
    echo Install it from https://www.python.org/downloads/ and tick
    echo "Add python.exe to PATH" during setup, then run this again.
    echo.
    pause
    exit /b 1
)

set "PHOB=%~1"
if "%PHOB%"=="" (
    echo.
    echo Phob Ult Control Plus installer
    echo ==============================
    echo.
    set /p "PHOB=Path to your PhobGCC-SW folder: "
)
if "%PHOB%"=="" goto :nopath

echo.
echo Which mods do you want?
echo.
echo   1. All three          (default)
echo   2. Tap Jump Off only
echo   3. Tilt Stick only
echo   4. Free Shield Tilt only
echo   5. Custom
echo   6. Remove the mods and restore stock PhobGCC-SW
echo.
set "CHOICE="
set /p "CHOICE=Choose 1-6 [1]: "

if "%CHOICE%"=="" set "MODS=tapjump,tiltstick,shieldtilt"
if "%CHOICE%"=="1" set "MODS=tapjump,tiltstick,shieldtilt"
if "%CHOICE%"=="2" set "MODS=tapjump"
if "%CHOICE%"=="3" set "MODS=tiltstick"
if "%CHOICE%"=="4" set "MODS=shieldtilt"
if "%CHOICE%"=="6" goto :revert
if "%CHOICE%"=="5" (
    echo.
    echo Enter the mods you want, comma separated, from:
    echo   tapjump  tiltstick  shieldtilt
    set /p "MODS=Mods: "
)
if "%MODS%"=="" goto :badchoice

echo.
python tools\patch.py "%PHOB%" --mods "%MODS%"
if errorlevel 1 goto :failed

echo.
echo Source tree patched. Now build it:
echo.
echo   cd "%PHOB%\PhobGCC\rp2040"
echo   mkdir build ^&^& cd build
echo   cmake -G Ninja .. ^&^& cmake --build .
echo.
echo See the README for toolchain setup, or download a prebuilt .uf2
echo from the Releases page.
echo.
pause
exit /b 0

:revert
echo.
python tools\patch.py "%PHOB%" --revert
if errorlevel 1 goto :failed
echo.
pause
exit /b 0

:nopath
echo No path given. Nothing was changed.
pause
exit /b 1

:badchoice
echo That was not one of the options. Nothing was changed.
pause
exit /b 1

:failed
echo.
echo The patch did not complete. Nothing should have been changed;
echo if it was, run this again and choose option 6 to restore.
echo.
pause
exit /b 1
