# Project 03 - STM32 Flash EEPROM Emulation

This project implements a small EEPROM emulation layer on the internal Flash of an STM32G0B1RE.

The goal is not to build a production storage stack.  
The goal is to make the core idea measurable on real hardware:

- append-only records
- two Flash pages used in rotation
- page state markers
- latest-value lookup
- page transfer when the active page is full
- boot-time write offset recovery
- recovery behavior after software reset fault injection
- corrupt-record handling using CRC

This project is part of the `stm32g0b1re-firmware-debugging-lab` series.

```txt
firmware/03_flash_eeprom_emulation
```

---

## 1. Purpose

STM32G0B1RE does not have a dedicated EEPROM area.  
However, part of the internal Flash can be reserved and used as an EEPROM-like storage area.

This project uses two 2KB Flash pages in Bank 2:

```txt
Page A: 0x08040000 - 0x080407FF
Page B: 0x08040800 - 0x08040FFF
```

The implementation stores small configuration values as append-only records.

Instead of overwriting an old value, the firmware writes a new record.  
When reading, it scans backward and returns the latest valid record.

This project also verifies reset-sensitive behavior, such as:

```txt
- reset after a page is marked RECEIVE
- reset after latest records are copied
- reset after HAL_FLASH_Program() succeeds but before RAM write_offset is updated
```

---

## 2. Target

| Item | Value |
| --- | --- |
| Board | NUCLEO-G0B1RE |
| MCU | STM32G0B1RE |
| IDE | STM32CubeIDE |
| Clock | 48 MHz |
| Flash page size | 2048 bytes |
| EEPROM area | Bank 2, first 2 pages |
| Page A | `0x08040000` |
| Page B | `0x08040800` |
| UART log | USART2, 115200 bps |
| RTOS | Not used in this project |
| Logger | Simple blocking UART logger |

This project is intentionally bare-metal.  
FreeRTOS is not used so that Flash behavior, page transfer, and reset recovery can be observed without scheduling noise.

---

## 3. Relationship to Other Projects

Previous related projects:

```txt
00_base_bringup
01_uart_dma_ring_logger
02_freertos_firmware_skeleton
```

Project 01 provides the UART logging style used by this project.

Project 02 is intentionally not reused directly.  
This project focuses on Flash behavior and recovery logic, so the implementation stays bare-metal.

The related theory article is:

```txt
Flash STM32G0: Linker Script, EEPROM Emulation và Dual Bank
```

The article explains the concepts.  
This project implements and tests them on real hardware.

---

## 4. Scope

Included:

```txt
Append-only 8-byte records
Two-page EEPROM emulation
32-byte page header using marker slots
ACTIVE / VALID / RECEIVE / ERASING / ERASED states
CRC16 record validation
Latest-value read by backward scan
Write offset recovery by scanning Flash at boot
Page transfer when active page is full
Software reset fault injection using NVIC_SystemReset()
Corrupt record test
UART evidence logs
```

Not included:

```txt
FreeRTOS integration
Flash write task
Multi-region storage
Macro record format
Key table storage
Log storage
Bootloader
USB
OTA update slot
Hardware power-cut test
Production wear-leveling across many pages
```

---

## 5. Flash Memory Layout

The project reserves the first two pages of Bank 2.

```txt
Bank 1:
  0x08000000 - 0x0803FFFF
  Application code

Bank 2:
  0x08040000 - 0x0807FFFF
  Reserved Flash bank

EEPROM emulation:
  Page A: 0x08040000 - 0x080407FF
  Page B: 0x08040800 - 0x08040FFF
```

Expected constants:

```c
#define EE_PAGE_A_ADDR       0x08040000UL
#define EE_PAGE_B_ADDR       0x08040800UL
#define EE_PAGE_SIZE         2048U
#define EE_HEADER_SIZE       32U
#define EE_RECORD_SIZE       8U
#define EE_RECORDS_PER_PAGE  ((EE_PAGE_SIZE - EE_HEADER_SIZE) / EE_RECORD_SIZE)
```

Expected capacity:

```txt
(2048 - 32) / 8 = 252 records per page
```

Measured TEST0 result:

```txt
[TEST0] EE_PAGE_A_ADDR=0x08040000
[TEST0] EE_PAGE_B_ADDR=0x08040800
[TEST0] page_size=2048
[TEST0] header_size=32
[TEST0] record_size=8
[TEST0] records_per_page=252
[TEST0] system_clock_check=OK
[TEST0] PASS
```

---

## 6. Linker Script Requirement

The linker script keeps application code in Bank 1 and reserves the EEPROM emulation area in Bank 2.

Example memory layout:

```ld
MEMORY
{
  RAM      (xrw) : ORIGIN = 0x20000000, LENGTH = 144K
  FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
  FLASH_EE (r)   : ORIGIN = 0x08040000, LENGTH = 4K
}
```

`FLASH_EE` is marked as read-only from the linker point of view.  
Runtime Flash writing is still done by the Flash controller through:

```c
HAL_FLASH_Unlock();
HAL_FLASH_Program(...);
HAL_FLASH_Lock();
```

The linker script protects the reserved area from accidentally being used by application code.

---

## 7. Page Layout

Each page begins with a 32-byte header.

```txt
Offset +0x00 : RECEIVE marker slot
Offset +0x08 : ACTIVE marker slot
Offset +0x10 : VALID marker slot
Offset +0x18 : ERASING marker slot
Offset +0x20 : First record
```

Each marker slot is 8 bytes and is programmed only once after a page erase.

This avoids overwriting the same Flash double-word to change the page state.

---

## 8. Page States

Page states:

```c
typedef enum
{
  EE_PAGE_RECEIVE = 0,
  EE_PAGE_ACTIVE,
  EE_PAGE_VALID,
  EE_PAGE_ERASING,
  EE_PAGE_ERASED,
  EE_PAGE_INVALID
} EePageState_t;
```

State meaning:

| State | Meaning |
| --- | --- |
| `ERASED` | All marker slots are still `0xFFFFFFFFFFFFFFFF` |
| `RECEIVE` | New page is receiving copied records during transfer |
| `ACTIVE` | Current read/write page |
| `VALID` | Old page still has valid data but is no longer the main write page |
| `ERASING` | Page is obsolete and can be erased |
| `INVALID` | Unexpected state |

Marker priority is checked from newest state to oldest state:

```txt
ERASING -> VALID -> ACTIVE -> RECEIVE -> ERASED
```

This is important because a page may contain multiple programmed marker slots.

Example:

```txt
RECEIVE slot programmed
ACTIVE slot programmed later
```

The current state should be interpreted as `ACTIVE`, not `RECEIVE`.

---

## 9. Record Format

Each record is exactly 8 bytes.

```c
typedef struct __attribute__((packed))
{
  uint16_t var_id;
  uint16_t crc;
  uint32_t value;
} EeRecord_t;
```

Field meaning:

| Field | Size | Meaning |
| --- | --- | --- |
| `var_id` | 2 bytes | Virtual ID of the stored variable |
| `crc` | 2 bytes | CRC16 over `var_id + value` |
| `value` | 4 bytes | Stored value |

Reserved IDs:

```c
#define EE_VAR_ID_INVALID     0x0000U
#define EE_VAR_ID_ERASED      0xFFFFU
#define EE_VAR_ID_TEST_PHASE  0x7F00U
```

Example application variable IDs:

```c
#define CFG_BAUD_RATE         0x0001U
#define CFG_TIMEOUT_MS        0x0002U
#define CFG_MODE              0x0003U
```

---

## 10. API

Public API:

```c
typedef enum
{
  EE_OK = 0,
  EE_NOT_INIT,
  EE_NOT_FOUND,
  EE_INVALID_PARAM,
  EE_WRITE_ERROR,
  EE_ERASE_ERROR,
  EE_NO_ACTIVE_PAGE,
  EE_NO_FREE_PAGE,
  EE_TRANSFER_ERROR,
  EE_CRC_MISMATCH
} EeStatus_t;

EeStatus_t Ee_Init(void);
EeStatus_t Ee_Format(void);
EeStatus_t Ee_Read(uint16_t var_id, uint32_t *value);
EeStatus_t Ee_Write(uint16_t var_id, uint32_t value);
```

Debug/test API:

```c
uint32_t      Ee_GetActivePageAddr(void);
uint32_t      Ee_GetWriteOffset(void);
EePageState_t Ee_GetPageState(uint32_t page_addr);
uint32_t      Ee_CountValidRecords(uint32_t page_addr);
uint32_t      Ee_CountRecordsForVar(uint16_t var_id);
EeStatus_t    Ee_TestWriteCorruptRecord(uint16_t var_id, uint32_t value);
```

---

## 11. Boot Behavior

`Ee_Init()` must not trust RAM state.

At boot, it must:

```txt
1. Read page marker slots
2. Decide active page
3. Handle incomplete transfer state
4. Scan the active page from offset 0x20
5. Find the first erased double-word
6. Rebuild the write offset from Flash contents
```

This is required because reset may occur after a record has been programmed but before the RAM write offset is updated.

TEST4 and TEST10 verify this behavior.

---

## 12. Write Behavior

`Ee_Write()` appends a new record to the current active page.

Normal write flow:

```txt
1. Check initialized state
2. Check var_id
3. Check active page
4. Check free space
5. Build record with CRC
6. Program one Flash double-word
7. Verify written record
8. Update RAM write_offset
```

If the active page is full, `Ee_Write()` triggers page transfer.

---

## 13. Read Behavior

`Ee_Read()` scans the active page backward from the current write offset.

Read flow:

```txt
1. Start from write_offset
2. Move backward record by record
3. Find matching var_id
4. Validate CRC
5. Return the latest valid value
6. Skip corrupt records
```

This is why the newest valid record is returned even when older records with the same `var_id` still exist.

---

## 14. Page Transfer

When the active page is full, the firmware transfers the latest valid values to the other page.

Transfer flow:

```txt
1. Choose the other page
2. Erase the other page if needed
3. Set new page to RECEIVE
4. Scan old page backward
5. Copy only the latest valid value for each virtual ID
6. Write the new trigger record
7. Set new page to ACTIVE
8. Mark old page VALID
9. Mark old page ERASING
10. Erase old page
11. Update active_page_addr and write_offset
```

TEST5 verifies page transfer in the same boot.  
TEST6 verifies page transfer persistence after reset.

---

## 15. Recovery Policy

This project uses a simple recovery policy.

If boot detects:

```txt
one page is ACTIVE
the other page is RECEIVE
```

Then the RECEIVE page is treated as an incomplete transfer.

Recovery action:

```txt
Erase RECEIVE page
Continue using the previous ACTIVE page
```

Example:

```txt
Before recovery:
  Page A = ACTIVE
  Page B = RECEIVE

After recovery:
  Page A = ACTIVE
  Page B = ERASED
```

This policy intentionally treats RECEIVE as not trustworthy until it becomes ACTIVE.

This is simpler than resuming a transfer from a partially copied RECEIVE page and is easier to prove with logs.

TEST7 and TEST8 verify this behavior.

---

## 16. Software Reset Fault Injection

This project uses `NVIC_SystemReset()` for controlled fault injection.

Fault injection points:

```txt
After writing RECEIVE marker
After copying latest records but before writing trigger record
After HAL_FLASH_Program() returns OK but before RAM write_offset update
```

Important limitation:

```txt
Software reset fault injection is not the same as physical power loss.
```

It verifies that firmware handles intermediate states created by its own code.  
It does not fully prove behavior when power is physically removed during a Flash program or erase operation.

A physical power-cut test would require separate hardware setup.

---

## 17. Test Modes

Actual compile-time test modes:

```c
#define APP_TEST_MODE_BOOT_CHECK              0
#define APP_TEST_MODE_FORMAT_DEFAULT          1
#define APP_TEST_MODE_WRITE_READBACK          2
#define APP_TEST_MODE_APPEND_LATEST           3
#define APP_TEST_MODE_REBOOT_READBACK         4
#define APP_TEST_MODE_PAGE_TRANSFER           5
#define APP_TEST_MODE_PAGE_TRANSFER_REBOOT    6
#define APP_TEST_MODE_FAULT_AFTER_RECEIVE     7
#define APP_TEST_MODE_FAULT_AFTER_COPY        8
#define APP_TEST_MODE_CORRUPT_RECORD          9
#define APP_TEST_MODE_FAULT_AFTER_PROGRAM     10
```

Inactive test code is guarded by preprocessor selectors to avoid unused static function and unused variable warnings.

Each test ends with:

```txt
[TESTn] PASS
```

or:

```txt
[TESTn] FAIL
```

---

## 18. Test Result Summary

| Test | Name | Result | Main Purpose |
| --- | --- | --- | --- |
| TEST0 | boot_check | PASS | Check Flash layout constants and 48 MHz clock |
| TEST1 | format_default | PASS | Erase pages and set Page A ACTIVE |
| TEST2 | write_readback | PASS | Write and read one record |
| TEST3 | append_latest | PASS | Append multiple values and read latest |
| TEST4 | reboot_readback | PASS | Restore active page and write offset after reset |
| TEST5 | page_transfer | PASS | Transfer latest records to the other page |
| TEST6 | page_transfer_reboot | PASS | Verify page transfer result after reset |
| TEST7 | fault_after_receive | PASS | Recover reset after RECEIVE marker |
| TEST8 | fault_after_copy | PASS | Recover reset after copying latest records |
| TEST9 | corrupt_record | PASS | Skip corrupt latest record by CRC |
| TEST10 | fault_after_program | PASS | Restore write offset after reset before RAM update |

---

## 19. TEST0 - Boot Check

Purpose:

```txt
Verify memory layout, page size, header size, record size, record capacity, and system clock.
```

Measured log:

```txt
[BOOT] Project: 03_flash_eeprom_emulation
[BOOT] System clock: 48000000 Hz
[BOOT] Test mode: boot_check
[TEST0] EE_PAGE_A_ADDR=0x08040000
[TEST0] EE_PAGE_B_ADDR=0x08040800
[TEST0] page_size=2048
[TEST0] header_size=32
[TEST0] record_size=8
[TEST0] records_per_page=252
[TEST0] system_clock_check=OK
[TEST0] PASS
```

---

## 20. TEST1 - Format Default

Purpose:

```txt
Verify EEPROM format behavior.
```

Measured result:

```txt
[EE] format start
[EE] erase page A OK
[EE] erase page B OK
[EE] set page A ACTIVE OK
[EE] page A state=ACTIVE
[EE] page B state=ERASED
[EE] active_page=0x08040000
[EE] write_offset=32
[EE] format done
[TEST1] active_page_check=OK
[TEST1] write_offset_check=OK
[TEST1] PASS
```

---

## 21. TEST2 - Write Readback

Purpose:

```txt
Verify basic record write and read in the same boot.
```

Measured result:

```txt
[TEST2] write CFG_BAUD_RATE=115200 OK
[TEST2] read CFG_BAUD_RATE=115200 OK
[TEST2] write_offset=40
[TEST2] PASS
```

---

## 22. TEST3 - Append Latest

Purpose:

```txt
Verify that read returns the latest valid record.
```

Measured result:

```txt
[TEST3] write CFG_BAUD_RATE=9600 OK
[TEST3] write CFG_BAUD_RATE=115200 OK
[TEST3] write CFG_BAUD_RATE=230400 OK
[TEST3] read CFG_BAUD_RATE=230400 OK
[TEST3] record_count=3
[TEST3] write_offset=56
[TEST3] PASS
```

This confirms that `Ee_Read()` scans backward and returns the latest valid value.

---

## 23. TEST4 - Reboot Readback

Purpose:

```txt
Verify that records survive software reset and Ee_Init() rebuilds write_offset.
```

Measured result before reset:

```txt
[TEST4] phase=prepare
[TEST4] write TEST_PHASE=after_reset OK
[TEST4] write CFG_BAUD_RATE=230400 OK
[TEST4] write_offset_before_reset=48
[TEST4] trigger software reset
```

Measured result after reset:

```txt
[EE] page A state=ACTIVE
[EE] page B state=ERASED
[EE] active_page=0x08040000
[EE] write_offset=48
[TEST4] phase=after_reset
[TEST4] active_page=0x08040000
[TEST4] write_offset_after_reset=48
[TEST4] read CFG_BAUD_RATE=230400 OK
[TEST4] write TEST_PHASE=done OK
[TEST4] PASS
```

This confirms that the RAM write pointer is not trusted after reset.

---

## 24. TEST5 - Page Transfer

Purpose:

```txt
Verify page transfer when the active page becomes full.
```

Measured result:

```txt
[TEST5] fill_records=250 OK
[TEST5] write_offset_before_transfer=2048
[EE] transfer start old=0x08040000 new=0x08040800
[EE] set new page RECEIVE OK
[EE] copied_records=2
[EE] write trigger record OK
[EE] set new page ACTIVE OK
[EE] set old page VALID OK
[EE] set old page ERASING OK
[EE] erase old page OK
[EE] page A state=ERASED
[EE] page B state=ACTIVE
[EE] transfer done active_page=0x08040800 write_offset=56
[TEST5] read CFG_BAUD_RATE=230400 OK
[TEST5] read CFG_TIMEOUT_MS=1000 OK
[TEST5] read CFG_MODE=3 OK
[TEST5] page A state=ERASED valid_count=0
[TEST5] page B state=ACTIVE valid_count=3
[TEST5] PASS
```

Explanation:

```txt
Page A was filled to offset 2048.
The next write triggered transfer to Page B.
Only the latest values were copied.
Page B became ACTIVE.
Page A was erased.
```

---

## 25. TEST6 - Page Transfer Reboot

Purpose:

```txt
Verify that page transfer result is still valid after software reset.
```

Measured result before reset:

```txt
[TEST6] fill_records=249 OK
[TEST6] write_offset_before_transfer=2048
[EE] transfer start old=0x08040000 new=0x08040800
[EE] copied_records=3
[EE] transfer done active_page=0x08040800 write_offset=64
[TEST6] active_page_before_reset=0x08040800
[TEST6] write_offset_before_reset=64
[TEST6] trigger software reset
```

Measured result after reset:

```txt
[EE] page A state=ERASED
[EE] page B state=ACTIVE
[EE] active_page=0x08040800
[EE] write_offset=64
[TEST6] phase=after_reset
[TEST6] active_page_after_reset=0x08040800
[TEST6] write_offset_after_reset=64
[TEST6] read CFG_BAUD_RATE=230400 OK
[TEST6] read CFG_TIMEOUT_MS=1000 OK
[TEST6] read CFG_MODE=3 OK
[TEST6] write TEST_PHASE=done OK
[TEST6] PASS
```

This proves that the page transfer result is persistent in Flash and not only stored in RAM.

---

## 26. TEST7 - Fault After RECEIVE Marker

Purpose:

```txt
Verify recovery when reset occurs immediately after the new page is marked RECEIVE.
```

Measured result before reset:

```txt
[TEST7] fill_records=249 OK
[TEST7] write_offset_before_fault=2048
[TEST7] trigger write CFG_BAUD_RATE=230400
[EE] transfer start old=0x08040000 new=0x08040800
[EE] set new page RECEIVE OK
```

Measured result after reset:

```txt
[EE] page A state=ACTIVE
[EE] page B state=RECEIVE
[EE] recovery erase RECEIVE page=0x08040800
[EE] recovery erase RECEIVE OK
[EE] page B state after recovery=ERASED
[EE] active_page=0x08040000
[EE] write_offset=2048
[TEST7] phase=after_reset
[TEST7] read CFG_BAUD_RATE=9848 OK
[TEST7] read CFG_TIMEOUT_MS=1000 OK
[TEST7] read CFG_MODE=3 OK
[TEST7] page A state=ACTIVE
[TEST7] page B state=ERASED
[TEST7] PASS
```

The trigger value `230400` is not written yet.  
The old latest value is preserved.

---

## 27. TEST8 - Fault After Copy

Purpose:

```txt
Verify recovery when reset occurs after copied records are written to the RECEIVE page but before the new page becomes ACTIVE.
```

Measured result before reset:

```txt
[TEST8] fill_records=249 OK
[TEST8] write_offset_before_fault=2048
[TEST8] trigger write CFG_BAUD_RATE=230400
[EE] transfer start old=0x08040000 new=0x08040800
[EE] set new page RECEIVE OK
[EE] copied_records=3
```

Measured result after reset:

```txt
[EE] page A state=ACTIVE
[EE] page B state=RECEIVE
[EE] recovery erase RECEIVE page=0x08040800
[EE] recovery erase RECEIVE OK
[EE] page B state after recovery=ERASED
[EE] active_page=0x08040000
[EE] write_offset=2048
[TEST8] phase=after_reset
[TEST8] read CFG_BAUD_RATE=9848 OK
[TEST8] read CFG_TIMEOUT_MS=1000 OK
[TEST8] read CFG_MODE=3 OK
[TEST8] page A state=ACTIVE
[TEST8] page B state=ERASED
[TEST8] PASS
```

TEST7 and TEST8 intentionally produce the same recovery decision:

```txt
The RECEIVE page is not trusted until it becomes ACTIVE.
```

---

## 28. TEST9 - Corrupt Record

Purpose:

```txt
Verify that Ee_Read() skips a record with wrong CRC.
```

Measured result:

```txt
[TEST9] write valid CFG_BAUD_RATE=115200 OK
[TEST9] write corrupt CFG_BAUD_RATE=230400 OK
[TEST9] read CFG_BAUD_RATE=115200 OK
[TEST9] valid_record_count=1
[TEST9] write_offset=48
[TEST9] page A state=ACTIVE
[TEST9] page B state=ERASED
[TEST9] PASS
```

Explanation:

```txt
The latest physical record has the same var_id but invalid CRC.
Ee_Read() scans backward, sees the corrupt record, skips it, and returns the previous valid value.
```

This test simulates the result of a corrupted record.  
It does not simulate the physical cause of corruption during Flash programming.

---

## 29. TEST10 - Fault After Program Before Offset Update

Purpose:

```txt
Verify that Ee_Init() rebuilds write offset from Flash, not from RAM.
```

Fault injection point:

```txt
after HAL_FLASH_Program() succeeds
after record verification succeeds
before RAM write_offset is incremented
```

Measured result before reset:

```txt
[TEST10] write TEST_PHASE=after_reset OK
[TEST10] write_offset_before_fault=40
[TEST10] trigger write CFG_BAUD_RATE=115200
```

Measured result after reset:

```txt
[EE] page A state=ACTIVE
[EE] page B state=ERASED
[EE] active_page=0x08040000
[EE] write_offset=48
[TEST10] phase=after_reset
[TEST10] active_page_after_reset=0x08040000
[TEST10] write_offset_after_reset=48
[TEST10] read CFG_BAUD_RATE=115200 OK
[TEST10] write TEST_PHASE=done OK
[TEST10] page A state=ACTIVE
[TEST10] page B state=ERASED
[TEST10] PASS
```

This confirms that `Ee_Init()` scans Flash and rebuilds the write pointer from actual Flash contents.

---

## 30. Source Structure

Application files:

```txt
Core/Inc/
  app_main.h
  ee_format.h
  ee_storage.h
  ee_fault_inject.h
  uart_log.h

Core/Src/
  app_main.c
  ee_format.c
  ee_storage.c
  ee_fault_inject.c
  uart_log.c
```

Recommended responsibility:

| File | Responsibility |
| --- | --- |
| `app_main.c` | Test dispatcher and main loop |
| `ee_format.c` | CRC, record validation, marker parsing, offset scanning |
| `ee_storage.c` | Init, read, write, transfer, recovery |
| `ee_fault_inject.c` | Reset injection point control |
| `uart_log.c` | UART log output |

---

## 31. Evidence Files

Evidence logs:

```txt
assets/logs/03_flash_eeprom_emulation/test0_boot_check.txt
assets/logs/03_flash_eeprom_emulation/test1_format_default.txt
assets/logs/03_flash_eeprom_emulation/test2_write_readback.txt
assets/logs/03_flash_eeprom_emulation/test3_append_latest.txt
assets/logs/03_flash_eeprom_emulation/test4_reboot_readback.txt
assets/logs/03_flash_eeprom_emulation/test5_page_transfer.txt
assets/logs/03_flash_eeprom_emulation/test6_page_transfer_reboot.txt
assets/logs/03_flash_eeprom_emulation/test7_fault_after_receive.txt
assets/logs/03_flash_eeprom_emulation/test8_fault_after_copy.txt
assets/logs/03_flash_eeprom_emulation/test9_corrupt_record.txt
assets/logs/03_flash_eeprom_emulation/test10_fault_after_program.txt
```

Summary report:

```txt
assets/reports/03_flash_eeprom_emulation/test_summary.md
```

Screenshots:

```txt
assets/screenshots/03_flash_eeprom_emulation/
```

---

## 32. Final Acceptance Criteria

The project is considered complete when:

```txt
TEST0 passes
TEST1 passes
TEST2 proves write/readback
TEST3 proves append/latest lookup
TEST4 proves readback after reset
TEST5 proves page transfer
TEST6 proves page transfer persists after reset
TEST7 proves RECEIVE marker recovery
TEST8 proves recovery after copied records
TEST9 proves corrupt record is skipped
TEST10 proves write offset recovery after Flash program reset
All logs are saved under assets/logs
README and test summary are committed
```
