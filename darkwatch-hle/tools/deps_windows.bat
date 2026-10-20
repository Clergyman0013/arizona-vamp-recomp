@echo off
rem deps_windows.bat: clone XboxXBERecomp's four CMake dependencies by hand and point the release preset at them.
rem Run once before build.bat on a PC where Git's shell scripts cannot fork (Mandatory ASLR on: every forked
rem child dies with 0xC0000142, git-for-windows #6349). FetchContent always runs `git submodule update`,
rem which is such a script; plain `git clone` / `fetch` are not, so they work. Tags match cmake/XbeRecomp.cmake.
setlocal
cd /d "%~dp0.."
set "DEPS=%CD%\out\deps"
if not exist "%DEPS%" mkdir "%DEPS%"

call :clone zydis        https://github.com/zyantific/zydis.git       v4.1.1        || goto :fail
call :clone tomlplusplus https://github.com/marzer/tomlplusplus.git   v3.4.0        || goto :fail
call :clone imgui        https://github.com/ocornut/imgui.git         v1.91.0       || goto :fail
call :clone SDL3         https://github.com/libsdl-org/SDL.git        release-3.4.16 || goto :fail

rem zydis's one submodule, at the commit its tree records
if not exist "%DEPS%\zydis\dependencies\zycore\CMakeLists.txt" (
    for /f "tokens=3" %%c in ('git -C "%DEPS%\zydis" ls-tree HEAD dependencies/zycore') do set "ZYCORE=%%c"
    git init -q "%DEPS%\zydis\dependencies\zycore" || goto :fail
    git -C "%DEPS%\zydis\dependencies\zycore" fetch -q --depth 1 https://github.com/zyantific/zycore-c.git %ZYCORE% || goto :fail
    git -C "%DEPS%\zydis\dependencies\zycore" -c advice.detachedHead=false checkout -q FETCH_HEAD || goto :fail
)

rem a failed FetchContent leaves half-populated _deps behind; the cache keeps the source dirs for build.bat
if exist out\build\release\_deps rd /s /q out\build\release\_deps
if exist out\build\release\CMakeCache.txt del /q out\build\release\CMakeCache.txt
set "D=%DEPS:\=/%"
cmake --preset release -DFETCHCONTENT_SOURCE_DIR_ZYDIS=%D%/zydis -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=%D%/tomlplusplus ^
    -DFETCHCONTENT_SOURCE_DIR_IMGUI=%D%/imgui -DFETCHCONTENT_SOURCE_DIR_SDL3=%D%/SDL3 >nul || goto :fail
echo deps ready in %DEPS%; now run build.bat
exit /b 0

:clone
if exist "%DEPS%\%1\.git" exit /b 0
git -c advice.detachedHead=false clone -q --depth 1 --branch %3 %2 "%DEPS%\%1"
exit /b %ERRORLEVEL%

:fail
echo deps_windows.bat FAILED
exit /b 1
