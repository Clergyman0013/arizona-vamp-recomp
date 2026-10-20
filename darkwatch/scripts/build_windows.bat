@echo off
rem ===========================================================================
rem  Build Darkwatch Recompiled on Windows (MSVC + D3D11)
rem
rem    scripts\build_windows.bat          configure + build (Release)
rem    scripts\build_windows.bat run      ... then launch the game
rem
rem  Needs: Visual Studio 2022 (Desktop C++ workload), CMake 3.20+.
rem  Run from an "x64 Native Tools Command Prompt for VS 2022" or let this
rem  script find vcvars64.bat itself.
rem ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0.."
set ROOT=%CD%

where cl >nul 2>nul
if errorlevel 1 (
    set VCVARS=
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat
    if defined VCVARS (
        echo == importing MSVC environment ==
        call "!VCVARS!"
    ) else (
        echo Could not find MSVC. Run this from a Developer Command Prompt. & exit /b 1
    )
)

echo == configure ==
cmake -S "%ROOT%" -B "%ROOT%\build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

echo == build ==
cmake --build "%ROOT%\build" --config Release --parallel
if errorlevel 1 exit /b 1

set BIN=%ROOT%\build\Release\darkwatch_recomp.exe
echo.
echo == built: %BIN% ==

if /i "%~1"=="run" (
    echo == running ==
    cd /d "%ROOT%"
    "%BIN%"
    exit /b %errorlevel%
)

echo.
echo Run it from the project root so the game data is found:
echo     cd /d "%ROOT%"
echo     build\Release\darkwatch_recomp.exe
echo.
echo Options live in darkwatch.ini (copy config\darkwatch.ini next to the
echo executable) or in the environment:
echo     set DARKWATCH_WIDESCREEN=1
echo     set DARKWATCH_RESOLUTION_SCALE=4
endlocal
