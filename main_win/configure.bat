@echo off
setlocal enabledelayedexpansion

rem ============================
rem  CONFIGURE SCRIPT FOR WINDOWS
rem ============================

set BUILD_DIR=cmake-build-release

rem ---------------------------------
rem  Check VPI_INCLUDE_DIR / VPI_LIB_DIR
rem ---------------------------------
if "%VPI_INCLUDE_DIR%"=="" (
    echo [ERROR] VPI_INCLUDE_DIR is NOT set.
    echo   Example:
    echo     setx VPI_INCLUDE_DIR "C:\temp_backup\eda\quartus_24_1_lite\questa_fse\include"
    exit /b 1
)

if "%VPI_LIB_DIR%"=="" (
    echo [ERROR] VPI_LIB_DIR is NOT set.
    echo   Example:
    echo     setx VPI_LIB_DIR "C:\temp_backup\eda\quartus_24_1_lite\questa_fse\win64"
    exit /b 1
)

echo Using VPI_INCLUDE_DIR=%VPI_INCLUDE_DIR%
echo Using VPI_LIB_DIR=%VPI_LIB_DIR%

rem Local install base: <repo>/install
set INSTALL_PREFIX=%~dp0install
echo Using CMAKE_INSTALL_PREFIX=%INSTALL_PREFIX%

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
echo Configuring CMake (Release, Ninja)...
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

cmake -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
  -Dvpi_include_dir="%VPI_INCLUDE_DIR%" ^
  -Dvpi_lib_dir="%VPI_LIB_DIR%" ^
  ..

if %errorlevel% neq 0 (
    echo CMake configuration FAILED.
    endlocal
    exit /b 1
)

echo.
echo ================================
echo CMake configuration SUCCESSFUL!
echo ================================
endlocal
