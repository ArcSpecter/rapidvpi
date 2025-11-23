@echo off
setlocal

echo Building RapidVPI...

rem Go into the build directory (created by configure_win.bat)
cd /d "%~dp0cmake-build-release"

rem Build the plugin; adjust target name if needed
rem For Linux it was: rapidvpi.vpi
rem For main_win CMake it is: rapidvpi_plugin
cmake --build . --target rapidvpi_plugin -j %NUMBER_OF_PROCESSORS%

cd /d "%~dp0"
endlocal
