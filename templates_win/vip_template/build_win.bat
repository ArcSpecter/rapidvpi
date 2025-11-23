@echo off
setlocal

rem ============================
rem  BUILD SCRIPT (WINDOWS)
rem  for vip_template
rem ============================

rem Infer project name from current directory (same as build.sh)
for %%I in ("%CD%") do set PROJECT_NAME=%%~nI

echo Building %PROJECT_NAME%...

rem Go into build dir next to this script
cd /d "%~dp0cmake-build-release"

cmake --build . --target "%PROJECT_NAME%" -j %NUMBER_OF_PROCESSORS%

rem Go back to project root
cd /d "%~dp0"

endlocal
