@echo off
setlocal enabledelayedexpansion

rem ============================
rem  CONFIGURE SCRIPT (WINDOWS)
rem  for vip_template
rem ============================

set BUILD_DIR=cmake-build-release

rem ---------------------------------
rem  Check VPI_INCLUDE_DIR is defined
rem ---------------------------------
if "%VPI_INCLUDE_DIR%"=="" (
    echo [ERROR] VPI_INCLUDE_DIR is NOT set.
    echo.
    echo   Please set it to your Questa include directory, for example:
    echo     setx VPI_INCLUDE_DIR "C:\temp_backup\eda\quartus_24_1_lite\questa_fse\include"
    exit /b 1
)

rem ---------------------------------
rem  Check RapidVPI CMake package dir
rem ---------------------------------
if "%RAPIDVPI_CMAKE_DIR%"=="" (
    echo [ERROR] RAPIDVPI_CMAKE_DIR is NOT set.
    echo.
    echo   It must point to the folder that contains rapidvpiConfig.cmake.
    echo   For example, if you installed RapidVPI into main_win\install, use:
    echo     setx RAPIDVPI_CMAKE_DIR "C:\temp_backup\msys2\home\arcspecter\tools\rapidvpi\main_win\install\lib\cmake\rapidvpi"
    exit /b 1
)

echo Using VPI_INCLUDE_DIR=%VPI_INCLUDE_DIR%
echo Using RAPIDVPI_CMAKE_DIR=%RAPIDVPI_CMAKE_DIR%

rem ---------------------------------
rem  Go to this script's directory
rem ---------------------------------
cd /d "%~dp0"

rem -------------------------------
rem  Remove old build directory
rem -------------------------------
if exist "%BUILD_DIR%" (
    echo Removing: %BUILD_DIR%
    rmdir /s /q "%BUILD_DIR%"
)

rem -------------------------------
rem  Recreate and configure CMake
rem -------------------------------
echo Configuring vip_template (Release, Ninja)...
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

cmake -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="%RAPIDVPI_CMAKE_DIR%" ^
  -Dvpi_include_dir="%VPI_INCLUDE_DIR%" ^
  ..

if %errorlevel% neq 0 (
    echo CMake configuration FAILED.
    endlocal
    exit /b 1
)

echo.
echo ======================================
echo CMake configuration for vip_template OK
echo ======================================

endlocal
