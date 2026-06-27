@echo off
REM ota_slot_b_m23.bat
REM Milestone 23 — Full OTA flow: erase Slot B, upload app, set pending metadata.
REM Uses generic --target b flag (M22/M23 firmware required).
REM
REM After this completes:
REM   Reset the board WITHOUT pressing 'u'.
REM   Expected boot sequence:
REM     [BOOT] active_slot=B
REM     [BOOT] update_state=PENDING
REM     [BOOT] selected_slot=B
REM     [APP]  slot_name=B
REM     [TEST13] app_confirm_image_check PASS
REM
REM   Reset again to confirm:
REM     [BOOT] confirmed_slot=B
REM     [BOOT] update_state=CONFIRMED

set COM_PORT=COM3
set BAUD=115200
set APP_BIN=..\app\SlotB_Release\app_slot_b.bin

echo ============================================================
echo  Project 04 - Milestone 23 - Full OTA Slot B
echo ============================================================
echo  port   = %COM_PORT%
echo  target = B
echo  file   = %APP_BIN%
echo ============================================================
echo.

if not exist "%APP_BIN%" (
    echo [ERROR] File not found: %APP_BIN%
    echo         Build the SlotB_Release configuration in STM32CubeIDE first.
    pause
    exit /b 1
)

echo STEP 1: Press the RESET button on the board NOW.
echo STEP 2: Then press any key here within 2 seconds to start the script.
echo.
pause

py -3 uart_packet_sender.py --port %COM_PORT% --baud %BAUD% --file "%APP_BIN%" --target b --set-pending

echo.
if %ERRORLEVEL% EQU 0 (
    echo [OK] OTA complete. Now reset the board WITHOUT pressing 'u'.
    echo      Board should boot Slot B and confirm the image.
) else (
    echo [NG] Script exited with error %ERRORLEVEL%.
)

pause
