@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem flash_metadata_slot_b.bat
rem
rem Generate metadata with active_slot=B and flash it to Metadata Page 0.
rem
rem Expected result:
rem   - metadata_magic=OK
rem   - active_slot=B
rem   - selected_slot=B
rem   - app Slot B boots
rem ============================================================================

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

set "METADATA_ADDR=0x0807F000"
set "METADATA_DIR=%PROJECT_ROOT%\tools\generated"
set "METADATA_BIN=%METADATA_DIR%\metadata_slot_b.bin"

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
echo Project 04 metadata Slot B flash started
echo ===============================================================================
echo PROJECT_ROOT   = %PROJECT_ROOT%
echo PROGRAMMER_CLI = %PROGRAMMER_CLI%
echo METADATA_BIN   = %METADATA_BIN%
echo METADATA_ADDR  = %METADATA_ADDR%
echo.

%PYTHON_CMD% "%PROJECT_ROOT%\tools\generate_metadata.py" ^
  --active B ^
  --confirmed B ^
  --boot-count 0 ^
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
echo [FLASH] metadata_slot_b.bin -^> %METADATA_ADDR%
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
echo Project 04 metadata Slot B flash finished successfully
echo ===============================================================================
echo.
echo Expected UART result:
echo   [BOOT] metadata_magic=OK
echo   [BOOT] active_slot=B
echo   [BOOT] selected_slot=B
echo   [APP] slot_name=B
echo   [TEST3] app_slot_b_boot_check PASS
echo.

exit /b 0