@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem flash_wrong_slot_b.bat
rem
rem Wrong-slot test script for Project 04: STM32 Dual-Slot UART Bootloader V1
rem
rem This script intentionally writes app_slot_a.bin to the Slot B address.
rem
rem Expected result:
rem   [TEST4] slot_a_vector_check PASS
rem   [TEST5] slot_b_vector_check FAIL
rem   [TEST6] wrong_slot_b_reject_check PASS
rem ============================================================================

rem ----------------------------------------------------------------------------
rem Project paths
rem ----------------------------------------------------------------------------
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

rem ----------------------------------------------------------------------------
rem Build configurations
rem ----------------------------------------------------------------------------
set "BOOT_CONFIG=Release"
set "APP_SLOT_A_CONFIG=SlotA_Release"

rem ----------------------------------------------------------------------------
rem Flash addresses
rem ----------------------------------------------------------------------------
set "BOOT_ADDR=0x08000000"
set "SLOT_A_ADDR=0x08010000"
set "SLOT_B_ADDR=0x08040000"

rem ----------------------------------------------------------------------------
rem Binary paths
rem ----------------------------------------------------------------------------
set "BOOT_BIN=%PROJECT_ROOT%\bootloader\%BOOT_CONFIG%\bootloader.bin"
set "APP_A_BIN=%PROJECT_ROOT%\app\%APP_SLOT_A_CONFIG%\app_slot_a.bin"

rem ----------------------------------------------------------------------------
rem Find STM32CubeProgrammer CLI
rem ----------------------------------------------------------------------------
set "PROGRAMMER_CLI="

rem 1) Search from PATH
for /f "delims=" %%P in ('where STM32_Programmer_CLI.exe 2^>nul') do (
  set "PROGRAMMER_CLI=%%P"
  goto :programmer_found
)

rem 2) Standalone STM32CubeProgrammer default paths
if exist "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (
  set "PROGRAMMER_CLI=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
  goto :programmer_found
)

if exist "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (
  set "PROGRAMMER_CLI=C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
  goto :programmer_found
)

rem 3) STM32CubeIDE bundled CubeProgrammer plugin path
for /d %%D in ("C:\ST\STM32CubeIDE_*") do (
  for /d %%P in ("%%D\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_*") do (
    if exist "%%P\tools\bin\STM32_Programmer_CLI.exe" (
      set "PROGRAMMER_CLI=%%P\tools\bin\STM32_Programmer_CLI.exe"
      goto :programmer_found
    )
  )
)

:programmer_found

echo.
echo ===============================================================================
echo Project 04 wrong-slot flash started
echo ===============================================================================
echo PROJECT_ROOT = %PROJECT_ROOT%

if "%PROGRAMMER_CLI%"=="" (
  echo [ERROR] STM32_Programmer_CLI.exe was not found.
  exit /b 1
)

echo PROGRAMMER_CLI = %PROGRAMMER_CLI%
echo.
echo BOOT_BIN       = %BOOT_BIN%
echo APP_A_BIN      = %APP_A_BIN%
echo.
echo Flash plan:
echo   bootloader.bin -^> %BOOT_ADDR%
echo   app_slot_a.bin -^> %SLOT_A_ADDR%
echo   app_slot_a.bin -^> %SLOT_B_ADDR%   [INTENTIONAL WRONG-SLOT]
echo.

rem ----------------------------------------------------------------------------
rem Check binary files
rem ----------------------------------------------------------------------------
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

rem ----------------------------------------------------------------------------
rem Erase all Flash once before writing all regions
rem ----------------------------------------------------------------------------
echo.
echo [FLASH] Erase all Flash
"%PROGRAMMER_CLI%" -c port=SWD -e all

if errorlevel 1 (
  echo [ERROR] Flash erase failed.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Flash bootloader
rem ----------------------------------------------------------------------------
echo.
echo [FLASH] bootloader.bin -^> %BOOT_ADDR%
"%PROGRAMMER_CLI%" -c port=SWD -w "%BOOT_BIN%" %BOOT_ADDR% -v

if errorlevel 1 (
  echo [ERROR] Bootloader flash failed.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Flash app Slot A correctly
rem ----------------------------------------------------------------------------
echo.
echo [FLASH] app_slot_a.bin -^> %SLOT_A_ADDR%
"%PROGRAMMER_CLI%" -c port=SWD -w "%APP_A_BIN%" %SLOT_A_ADDR% -v

if errorlevel 1 (
  echo [ERROR] Slot A flash failed.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Flash app Slot A binary into Slot B address intentionally
rem ----------------------------------------------------------------------------
echo.
echo [FLASH] app_slot_a.bin -^> %SLOT_B_ADDR% [INTENTIONAL WRONG-SLOT]
"%PROGRAMMER_CLI%" -c port=SWD -w "%APP_A_BIN%" %SLOT_B_ADDR% -v

if errorlevel 1 (
  echo [ERROR] Wrong-slot flash failed.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Reset target
rem ----------------------------------------------------------------------------
echo.
echo [FLASH] Reset target
"%PROGRAMMER_CLI%" -c port=SWD -rst

if errorlevel 1 (
  echo [ERROR] Target reset failed.
  exit /b 1
)

echo.
echo ===============================================================================
echo Project 04 wrong-slot flash finished successfully
echo ===============================================================================
echo.
echo Expected UART result:
echo   [TEST4] slot_a_vector_check PASS
echo   [TEST5] slot_b_vector_check FAIL
echo   [TEST6] wrong_slot_b_reject_check PASS
echo.

exit /b 0
