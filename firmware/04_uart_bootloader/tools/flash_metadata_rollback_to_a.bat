@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem flash_metadata_rollback_to_a.bat
rem
rem Generate metadata with active_slot=B, confirmed_slot=A, boot_count=3 and
rem flash it to Metadata Page 0.
rem
rem This simulates a pending update that has exceeded the maximum boot count.
rem The bootloader should detect the rollback condition and boot Slot A instead.
rem
rem Expected result:
rem   [TEST9]  pending_metadata_check PASS
rem   [TEST10] rollback_decision_check PASS
rem   - selected_slot=A
rem   - app Slot A boots
rem ============================================================================

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

set "METADATA_ADDR=0x0807F000"
set "METADATA_DIR=%PROJECT_ROOT%\tools\generated"
set "METADATA_BIN=%METADATA_DIR%\metadata_rollback_to_a.bin"

rem ----------------------------------------------------------------------------
rem Find Python
rem ----------------------------------------------------------------------------
set "PYTHON_CMD="

where py >nul 2>nul
if not errorlevel 1 (
  set "PYTHON_CMD=py -3"
  goto :python_found
)

where python >nul 2>nul
if not errorlevel 1 (
  set "PYTHON_CMD=python"
  goto :python_found
)

:python_found

if "%PYTHON_CMD%"=="" (
  echo [ERROR] Python was not found.
  echo Please install Python or add it to PATH.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Find STM32CubeProgrammer CLI
rem ----------------------------------------------------------------------------
set "PROGRAMMER_CLI="

for /f "delims=" %%P in ('where STM32_Programmer_CLI.exe 2^>nul') do (
  set "PROGRAMMER_CLI=%%P"
  goto :programmer_found
)

if exist "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (
  set "PROGRAMMER_CLI=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
  goto :programmer_found
)

if exist "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (
  set "PROGRAMMER_CLI=C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
  goto :programmer_found
)

for /d %%D in ("C:\ST\STM32CubeIDE_*") do (
  for /d %%P in ("%%D\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_*") do (
    if exist "%%P\tools\bin\STM32_Programmer_CLI.exe" (
      set "PROGRAMMER_CLI=%%P\tools\bin\STM32_Programmer_CLI.exe"
      goto :programmer_found
    )
  )
)

:programmer_found

if "%PROGRAMMER_CLI%"=="" (
  echo [ERROR] STM32_Programmer_CLI.exe was not found.
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Generate metadata
rem ----------------------------------------------------------------------------
if not exist "%METADATA_DIR%" (
  mkdir "%METADATA_DIR%"
)

echo.
echo ===============================================================================
echo Project 04 metadata rollback-to-A flash started
echo ===============================================================================
echo PROJECT_ROOT   = %PROJECT_ROOT%
echo PROGRAMMER_CLI = %PROGRAMMER_CLI%
echo METADATA_BIN   = %METADATA_BIN%
echo METADATA_ADDR  = %METADATA_ADDR%
echo.

%PYTHON_CMD% "%PROJECT_ROOT%\tools\generate_metadata.py" ^
  --active B ^
  --confirmed A ^
  --boot-count 3 ^
  -o "%METADATA_BIN%"

if errorlevel 1 (
  echo [ERROR] Metadata generation failed.
  exit /b 1
)

if not exist "%METADATA_BIN%" (
  echo [ERROR] Metadata binary was not generated:
  echo         %METADATA_BIN%
  exit /b 1
)

rem ----------------------------------------------------------------------------
rem Flash metadata only
rem ----------------------------------------------------------------------------
echo.
echo [FLASH] metadata_rollback_to_a.bin -^> %METADATA_ADDR%
"%PROGRAMMER_CLI%" -c port=SWD -w "%METADATA_BIN%" %METADATA_ADDR% -v

if errorlevel 1 (
  echo [ERROR] Metadata flash failed.
  exit /b 1
)

echo.
echo [FLASH] Reset target
"%PROGRAMMER_CLI%" -c port=SWD -rst

if errorlevel 1 (
  echo [ERROR] Target reset failed.
  exit /b 1
)

echo.
echo ===============================================================================
echo Project 04 metadata rollback-to-A flash finished successfully
echo ===============================================================================
echo.
echo Expected UART result:
echo   [BOOT] metadata_magic=OK
echo   [BOOT] active_slot=B
echo   [BOOT] confirmed_slot=A
echo   [BOOT] metadata_crc_check=OK
echo   [TEST8] metadata_crc_check PASS
echo   [BOOT] update_state=PENDING
echo   [BOOT] pending_slot=B
echo   [BOOT] last_confirmed_slot=A
echo   [BOOT] boot_count=3
echo   [BOOT] pending_boot_max=3
echo   [TEST9] pending_metadata_check PASS
echo   [BOOT] rollback_required=YES
echo   [BOOT] rollback_to_slot=A
echo   [TEST10] rollback_decision_check PASS
echo   [BOOT] selected_slot=A
echo   [APP] slot_name=A
echo   [TEST2] app_slot_a_boot_check PASS
echo.

exit /b 0
