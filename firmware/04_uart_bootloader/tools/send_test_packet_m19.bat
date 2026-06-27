@echo off
REM send_test_packet_m19.bat
REM Milestone 19 — generate 1024-byte test file and send 4 packets to Slot B.
REM
REM Usage: double-click, or run from tools\ directory.
REM Change COM_PORT below to match your board.

set COM_PORT=COM3
set BAUD=115200
set OUT_FILE=generated\uart_multi_packet_test_slot_b.bin
set TEST_SIZE=1024

echo ============================================================
echo  Project 04 - Milestone 19 - UART multi-packet sender
echo ============================================================
echo  port      = %COM_PORT%
echo  baud      = %BAUD%
echo  file      = %OUT_FILE%
echo  test_size = %TEST_SIZE% bytes
echo ============================================================
echo.
echo STEP 1: Press the RESET button on the board NOW.
echo STEP 2: Then press any key here within 2 seconds to start the script.
echo         (Script will send 'u' immediately to catch the 3-second update window)
echo.
pause

py -3 uart_packet_sender.py --port %COM_PORT% --baud %BAUD% --file %OUT_FILE% --make-test-file --test-size %TEST_SIZE%

echo.
if %ERRORLEVEL% EQU 0 (
    echo [OK] Milestone 19 send finished. Check log above for TEST18 x4 PASS.
) else (
    echo [NG] Script exited with error %ERRORLEVEL%.
)

pause
