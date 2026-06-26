@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build_all.bat
rem
rem Build script for Project 04: STM32 Dual-Slot UART Bootloader V1
rem
rem This script builds:
rem   - bootloader
rem   - app Slot A
rem   - app Slot B
rem
rem Expected output:
rem   bootloader/<config>/bootloader.bin
rem   app/SlotA_Release/app_slot_a.bin
rem   app/SlotB_Release/app_slot_b.bin
rem ============================================================================

rem ----------------------------------------------------------------------------
rem Project paths
rem ----------------------------------------------------------------------------
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

rem ----------------------------------------------------------------------------
rem STM32CubeIDE command line path
rem
rem Change this path if your STM32CubeIDE is installed elsewhere.
rem ----------------------------------------------------------------------------
set "CUBEIDE_CLI=C:\ST\STM32CubeIDE_1.18.0\STM32CubeIDE\stm32cubeidec.exe"

rem ----------------------------------------------------------------------------
rem Build configurations
rem ----------------------------------------------------------------------------
set "BOOT_PROJECT=bootloader"
set "APP_PROJECT=app"

set "BOOT_CONFIG=Release"
set "APP_SLOT_A_CONFIG=SlotA_Release"
set "APP_SLOT_B_CONFIG=SlotB_Release"

rem ----------------------------------------------------------------------------
rem Temporary Eclipse workspace for headless build
rem ----------------------------------------------------------------------------
set "BUILD_WORKSPACE=%PROJECT_ROOT%\.cubeide_workspace"

echo.
echo ===============================================================================
echo Project 04 build started
echo ===============================================================================
echo PROJECT_ROOT     = %PROJECT_ROOT%
echo BUILD_WORKSPACE = %BUILD_WORKSPACE%
echo CUBEIDE_CLI      = %CUBEIDE_CLI%
echo.

if not exist "%CUBEIDE_CLI%" (
  echo [ERROR] STM32CubeIDE CLI not found:
  echo         %CUBEIDE_CLI%
  echo.
  echo Please edit CUBEIDE_CLI in this script.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Import projects into temporary workspace
rem ----------------------------------------------------------------------------
echo [BUILD] Import CubeIDE projects...
"%CUBEIDE_CLI%" ^
  -nosplash ^
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild ^
  -data "%BUILD_WORKSPACE%" ^
  -import "%PROJECT_ROOT%\bootloader" ^
  -import "%PROJECT_ROOT%\app"

if errorlevel 1 (
  echo [ERROR] Project import failed.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Build bootloader
rem ----------------------------------------------------------------------------
echo.
echo [BUILD] %BOOT_PROJECT%/%BOOT_CONFIG%
"%CUBEIDE_CLI%" ^
  -nosplash ^
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild ^
  -data "%BUILD_WORKSPACE%" ^
  -cleanBuild "%BOOT_PROJECT%/%BOOT_CONFIG%"

if errorlevel 1 (
  echo [ERROR] Bootloader build failed.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Build app Slot A
rem ----------------------------------------------------------------------------
echo.
echo [BUILD] %APP_PROJECT%/%APP_SLOT_A_CONFIG%
"%CUBEIDE_CLI%" ^
  -nosplash ^
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild ^
  -data "%BUILD_WORKSPACE%" ^
  -cleanBuild "%APP_PROJECT%/%APP_SLOT_A_CONFIG%"

if errorlevel 1 (
  echo [ERROR] App Slot A build failed.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Build app Slot B
rem ----------------------------------------------------------------------------
echo.
echo [BUILD] %APP_PROJECT%/%APP_SLOT_B_CONFIG%
"%CUBEIDE_CLI%" ^
  -nosplash ^
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild ^
  -data "%BUILD_WORKSPACE%" ^
  -cleanBuild "%APP_PROJECT%/%APP_SLOT_B_CONFIG%"

if errorlevel 1 (
  echo [ERROR] App Slot B build failed.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Check output binaries
rem ----------------------------------------------------------------------------
set "BOOT_BIN=%PROJECT_ROOT%\bootloader\%BOOT_CONFIG%\bootloader.bin"
set "APP_A_BIN=%PROJECT_ROOT%\app\%APP_SLOT_A_CONFIG%\app_slot_a.bin"
set "APP_B_BIN=%PROJECT_ROOT%\app\%APP_SLOT_B_CONFIG%\app_slot_b.bin"

echo.
echo [CHECK] Output binaries

if not exist "%BOOT_BIN%" (
  echo [ERROR] Missing bootloader binary:
  echo         %BOOT_BIN%
  exit /b 1
)

if not exist "%APP_A_BIN%" (
  echo [ERROR] Missing Slot A binary:
  echo         %APP_A_BIN%
  exit /b 1
)

if not exist "%APP_B_BIN%" (
  echo [ERROR] Missing Slot B binary:
  echo         %APP_B_BIN%
  exit /b 1
)

echo [OK] %BOOT_BIN%
echo [OK] %APP_A_BIN%
echo [OK] %APP_B_BIN%

echo.
echo ===============================================================================
echo Project 04 build finished successfully
echo ===============================================================================
exit /b 0