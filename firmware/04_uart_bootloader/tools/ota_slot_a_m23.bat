@echo off
REM ota_slot_a_m23.bat
REM Milestone 23 — Full OTA flow: erase Slot A, upload app, set pending metadata.
REM Uses generic --target a flag (M22/M23 firmware required).
REM
REM SAFETY: Only run this AFTER Slot B has been confirmed (confirmed_slot=B).
REM         If Slot A update fails, bootloader will rollback to Slot B.
REM         Do NOT run this when metadata is empty (default boot = A).
REM
REM After this completes:
REM   Reset the board WITHOUT pressing 'u'.
REM   Expected boot sequence:
REM     [BOOT] active_slot=A
REM     [BOOT] confirmed_slot=B
REM     [BOOT] update_state=PENDING
REM     [BOOT] selected_slot=A
REM     [APP]  slot_name=A
REM     [TEST13] app_confirm_image_check PASS
REM
REM   Reset again to confirm:
REM     [BOOT] confirmed_slot=A
REM     [BOOT] update_state=CONFIRMED

set COM_PORT=COM3
set BAUD=115200
set APP_BIN=..\app\SlotA_Release\app_slot_a.bin

echo ============================================================
echo  Project 04 - Milestone 23 - Full OTA Slot A
echo ============================================================
echo  port   = %COM_PORT%
echo  target = A
echo  file   = %APP_BIN%
echo ============================================================
echo.

if not exist "%APP_BIN%" (
    echo [ERROR] File not found: %APP_BIN%
    echo         Build the SlotA_Release configuration in STM32CubeIDE first.
    pause
    exit /b 1
)

echo SAFETY CHECK: Slot B must already be confirmed before updating Slot A.
echo               Run ota_slot_b_m23.bat and confirm Slot B first if not done yet.
echo.
echo STEP 1: Press the RESET button on the board NOW.
echo STEP 2: Then press any key here within 2 seconds to start the script.
echo.
pause

py -3 uart_packet_sender.py --port %COM_PORT% --baud %BAUD% --file "%APP_BIN%" --target a --set-pending

echo.
if %ERRORLEVEL% EQU 0 (
    echo [OK] OTA complete. Now reset the board WITHOUT pressing 'u'.
    echo      Board should boot Slot A and confirm the image.
) else (
    echo [NG] Script exited with error %ERRORLEVEL%.
)

pause
