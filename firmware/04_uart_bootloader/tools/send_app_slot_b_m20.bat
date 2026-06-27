@echo off
REM send_app_slot_b_m20.bat
REM Milestone 20 — send real app_slot_b.bin to Slot B via UART updater.
REM
REM After this completes:
REM   1. Reset the board (do NOT press 'u')
REM   2. Verify: [TEST5] slot_b_vector_check PASS
REM   3. Bootloader still boots Slot A (metadata pending = Milestone 21)
REM
REM Change COM_PORT and APP_BIN below if needed.

set COM_PORT=COM3
set BAUD=115200
set APP_BIN=..\app\SlotB_Release\app_slot_b.bin

echo ============================================================
echo  Project 04 - Milestone 20 - Send real app to Slot B
echo ============================================================
echo  port = %COM_PORT%
echo  file = %APP_BIN%
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

py -3 uart_packet_sender.py --port %COM_PORT% --baud %BAUD% --file "%APP_BIN%"

echo.
if %ERRORLEVEL% EQU 0 (
    echo [OK] Slot B programmed. Now reset the board WITHOUT pressing 'u'.
    echo      Expected: [TEST5] slot_b_vector_check PASS
) else (
    echo [NG] Script exited with error %ERRORLEVEL%.
)

pause
