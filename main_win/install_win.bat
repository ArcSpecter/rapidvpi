@echo off
setlocal

echo Installing RapidVPI...

rem Go to the build directory next to this script
cd /d "%~dp0cmake-build-release"

cmake --install .

cd /d "%~dp0"
endlocal
