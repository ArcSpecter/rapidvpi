@echo off
setlocal

set BUILD_DIR=cmake-build-release

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [ERROR] Build directory "%CD%\%BUILD_DIR%" is not a CMake build tree.
    echo         Run configure_win.bat for rtl_template first.
    exit /b 1
)

echo Running sim_questa_init in "%BUILD_DIR%"...
cmake --build "%BUILD_DIR%" --target sim_questa_init -j %NUMBER_OF_PROCESSORS%

endlocal
