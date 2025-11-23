@echo off
setlocal

rem =============================
rem  CONFIGURE rtl_template (Win)
rem =============================

rem Pick a build dir name; I'll use cmake-build-release
set BUILD_DIR=cmake-build-release

if exist "%BUILD_DIR%" (
    echo Removing existing "%BUILD_DIR%"...
    rmdir /s /q "%BUILD_DIR%"
)

echo Configuring rtl_template (Release, Ninja) into "%BUILD_DIR%"...
cmake -S . -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration FAILED.
    exit /b 1
)

echo.
echo ==========================================
echo CMake configuration for rtl_template OK
echo ==========================================
endlocal
