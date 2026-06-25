# Project 03 - STM32 Flash EEPROM Emulation

This project implements a small EEPROM emulation layer on the internal Flash of an STM32G0B1RE.

The goal is not to build a production storage stack.  
The goal is to make the core idea measurable:

- append-only records
- two Flash pages used in rotation
- page state markers
- latest-value lookup
- page transfer when the active page is full
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
| Logger | Reuse UART logger style from Project 01 |

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
This project is bare-metal so that Flash behavior, page transfer, and recovery can be tested without RTOS scheduling noise.

The theory article is:

```txt
Flash STM32G0: Linker Script, EEPROM Emulation và Dual Bank
```

The article explains the concepts.  
This project implements and tests them on hardware.

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
  EEPROM emulation and reserved area

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

---

## 6. Linker Script Requirement

The linker script should keep application code in Bank 1 and reserve a Flash area in Bank 2.

Example:

```ld
MEMORY
{
  FLASH     (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
  FLASH_EE  (r)   : ORIGIN = 0x08040000, LENGTH = 4K
  RAM       (xrw) : ORIGIN = 0x20000000, LENGTH = 144K
}
```

Optional reserved section:

```ld
.eeprom_store (NOLOAD) :
{
  . = ALIGN(2048);
  KEEP(*(.eeprom_store))
  . = ALIGN(2048);
} > FLASH_EE
```

The implementation may also use absolute addresses directly, but the linker script must still document and protect the reserved area.

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

This avoids overwriting the same double-word to change the page state.

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
    EE_PAGE_INVALID,
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
| `INVALID` | Unexpected marker combination or corrupted state |

State progression:

```txt
ERASED
  -> RECEIVE
  -> ACTIVE
  -> VALID
  -> ERASING
  -> ERASED
```

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
    EE_CRC_MISMATCH,
} EeStatus_t;

EeStatus_t Ee_Init(void);
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

---

## 12. Recovery Policy

This project uses a simple recovery policy.

If boot detects:

```txt
one page is ACTIVE or VALID
the other page is RECEIVE
```

Then the RECEIVE page is treated as an incomplete transfer.

Recovery action:

```txt
Erase RECEIVE page
Continue using the previous ACTIVE/VALID page
```

This policy intentionally treats RECEIVE as not trustworthy until it becomes ACTIVE.

This is simpler than resuming a transfer from a partially copied RECEIVE page and is easier to prove with logs.

---

## 13. Software Reset Fault Injection

This project uses `NVIC_SystemReset()` for controlled fault injection.

Fault injection points:

```txt
After writing RECEIVE marker
After copying records but before writing ACTIVE marker
After HAL_FLASH_Program() returns OK but before RAM write offset update
```

Important limitation:

```txt
Software reset fault injection is not the same as physical power loss.
```

It verifies that firmware handles intermediate states created by its own code.  
It does not fully prove behavior when power is physically removed during a Flash program/erase operation.

A physical power-cut test would require separate hardware setup.

---

## 14. Test Modes

Recommended compile-time test modes:

```c
#define APP_TEST_MODE_BOOT_CHECK              0U
#define APP_TEST_MODE_FORMAT_DEFAULT          1U
#define APP_TEST_MODE_WRITE_READBACK          2U
#define APP_TEST_MODE_APPEND_LATEST           3U
#define APP_TEST_MODE_PAGE_TRANSFER           4U
#define APP_TEST_MODE_FAULT_AFTER_RECEIVE     5U
#define APP_TEST_MODE_FAULT_AFTER_COPY        6U
#define APP_TEST_MODE_CORRUPT_RECORD          7U
#define APP_TEST_MODE_FAULT_AFTER_PROGRAM     8U
```

Each test should end with:

```txt
[TESTn] PASS
```

or:

```txt
[TESTn] FAIL: <reason>
```

---

## 15. Test 0 - Boot Check

Purpose:

```txt
Verify memory layout, DBANK assumption, page size, header size, record size and capacity.
```

Expected log:

```txt
[BOOT] Project: 03_flash_eeprom_emulation
[TEST0] EE_PAGE_A_ADDR=0x08040000
[TEST0] EE_PAGE_B_ADDR=0x08040800
[TEST0] page_size=2048
[TEST0] header_size=32
[TEST0] record_size=8
[TEST0] records_per_page=252
[TEST0] PASS
```

---

## 16. Test 1 - Format and Default Load

Purpose:

```txt
Verify first-boot behavior.
```

Expected behavior:

```txt
Both pages erased
Ee_Init() formats Page A
Page A becomes ACTIVE
Read before write returns EE_NOT_FOUND
Write config value
Read back returns the written value
```

Expected log:

```txt
[TEST1] page_a_state=ERASED page_b_state=ERASED
[TEST1] after init: active_page=A state=ACTIVE
[TEST1] read before write: status=EE_NOT_FOUND
[TEST1] write CFG_BAUD_RATE=115200: status=EE_OK
[TEST1] read CFG_BAUD_RATE: status=EE_OK value=115200
[TEST1] PASS
```

---

## 17. Test 2 - Write and Read Back After Reset

Purpose:

```txt
Verify that records survive software reset.
```

Expected behavior:

```txt
Write config values
Write TEST_PHASE marker
Call NVIC_SystemReset()
After reset, Ee_Init() scans Flash again
Read config values
Mark test as done
```

Expected log:

```txt
[TEST2] write CFG_BAUD_RATE=9600
[TEST2] write CFG_TIMEOUT_MS=500
[TEST2] resetting now...
```

After reset:

```txt
[BOOT] Test mode: write_readback
[TEST2] post-reset phase detected
[TEST2] read CFG_BAUD_RATE: status=EE_OK value=9600
[TEST2] read CFG_TIMEOUT_MS: status=EE_OK value=500
[TEST2] PASS
```

---

## 18. Test 3 - Append and Latest Value

Purpose:

```txt
Verify that read returns the latest valid record.
```

Expected log:

```txt
[TEST3] write CFG_BAUD_RATE=9600
[TEST3] write CFG_BAUD_RATE=19200
[TEST3] write CFG_BAUD_RATE=115200
[TEST3] read CFG_BAUD_RATE: value=115200
[TEST3] record count for var_id=0x0001: 3
[TEST3] PASS
```

---

## 19. Test 4 - Page Transfer

Purpose:

```txt
Verify page transfer when the active page becomes full.
```

With 32-byte header:

```txt
record #1   offset = 32
record #252 offset = 2040
record #253 triggers transfer
```

Expected log:

```txt
[TEST4] writing records until page near full...
[TEST4] write #252: offset=2040
[TEST4] write #253 triggers transfer
[TRANSFER] new_page=B state=RECEIVE
[TRANSFER] copied latest values to page B
[TRANSFER] new_page=B state=ACTIVE
[TRANSFER] old_page=A state=ERASING
[TRANSFER] old_page=A erased
[TEST4] post-transfer read CFG_BAUD_RATE: status=EE_OK
[TEST4] PASS
```

---

## 20. Test 5A - Fault After RECEIVE Marker

Purpose:

```txt
Verify recovery when reset occurs after the new page is marked RECEIVE.
```

Expected log before reset:

```txt
[TEST5A] page A near full, triggering transfer
[TRANSFER] new_page=B state=RECEIVE
[FAULT] reset after RECEIVE marker
```

Expected log after reset:

```txt
[RECOVER] page_a_state=ACTIVE page_b_state=RECEIVE
[RECOVER] incomplete RECEIVE page detected
[RECOVER] erasing page B
[RECOVER] active_page=A
[TEST5A] read CFG_BAUD_RATE: status=EE_OK
[TEST5A] PASS
```

---

## 21. Test 5B - Fault After Copy Before ACTIVE Marker

Purpose:

```txt
Verify recovery when reset occurs after data copy but before ACTIVE marker.
```

Expected log before reset:

```txt
[TEST5B] page A near full, triggering transfer
[TRANSFER] new_page=B state=RECEIVE
[TRANSFER] copied latest values to page B
[FAULT] reset before ACTIVE marker
```

Expected log after reset:

```txt
[RECOVER] page_a_state=ACTIVE page_b_state=RECEIVE
[RECOVER] RECEIVE page is not trusted
[RECOVER] erasing page B
[RECOVER] active_page=A
[TEST5B] read CFG_BAUD_RATE: status=EE_OK
[TEST5B] PASS
```

Test 5A and Test 5B intentionally produce the same recovery decision.

---

## 22. Test 6 - Corrupt Record

Purpose:

```txt
Verify that Ee_Read() skips a record with wrong CRC.
```

Expected behavior:

```txt
Write valid record #1
Force-write corrupt record #2 with wrong CRC
Write valid record #3
Read must return record #3
```

Expected log:

```txt
[TEST6] write record #1: CFG_BAUD_RATE=9600
[TEST6] force-write corrupt record #2: value=0xBAADF00D crc=0x0000
[TEST6] write record #3: CFG_BAUD_RATE=115200
[EE_READ] var_id=0x0001 CRC mismatch, skipping
[TEST6] read CFG_BAUD_RATE: value=115200
[TEST6] PASS
```

Note:

```txt
This test simulates the result of a corrupted record.
It does not simulate the physical cause of power loss during double-word program.
```

---

## 23. Test 7 - Fault After Program Before Offset Update

Purpose:

```txt
Verify that Ee_Init() rebuilds write offset from Flash, not from RAM.
```

Expected log before reset:

```txt
[TEST7] write CFG_BAUD_RATE=9600
[FLASH_PROGRAM] writing CFG_TIMEOUT_MS=500 at offset=40
[FAULT] Flash program returned OK, resetting before offset update
```

Expected log after reset:

```txt
[RECOVER] scanning active page for next write offset
[RECOVER] found next_write_offset=48
[TEST7] read CFG_TIMEOUT_MS: status=EE_OK value=500
[TEST7] write CFG_BAUD_RATE=19200 at offset=48
[TEST7] verify CFG_TIMEOUT_MS still =500
[TEST7] PASS
```

---

## 24. Source Structure

Expected application files:

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
| `app_main.c` | test dispatcher and main loop |
| `ee_format.c` | CRC, record validation, marker parsing, offset scanning |
| `ee_storage.c` | init, read, write, transfer, recovery |
| `ee_fault_inject.c` | reset injection points and test phase helper |
| `uart_log.c` | UART log output |

---

## 25. Evidence Files

Recommended evidence outputs:

```txt
assets/logs/03_flash_eeprom_emulation_boot_check.txt
assets/logs/03_flash_eeprom_emulation_format_default.txt
assets/logs/03_flash_eeprom_emulation_write_readback.txt
assets/logs/03_flash_eeprom_emulation_append_latest.txt
assets/logs/03_flash_eeprom_emulation_page_transfer.txt
assets/logs/03_flash_eeprom_emulation_fault_receive.txt
assets/logs/03_flash_eeprom_emulation_fault_copy.txt
assets/logs/03_flash_eeprom_emulation_corrupt_record.txt
assets/logs/03_flash_eeprom_emulation_fault_after_program.txt
assets/logs/03_flash_eeprom_emulation_test_analysis.md
```

Recommended screenshots:

```txt
assets/screenshots/03_flash_eeprom_emulation_build_success.png
assets/screenshots/03_flash_eeprom_emulation_page_transfer.png
assets/screenshots/03_flash_eeprom_emulation_fault_recovery.png
assets/screenshots/03_flash_eeprom_emulation_corrupt_record.png
```

---

## 26. Expected Boot Log

```txt
[BOOT] STM32G0B1RE Firmware Debugging Lab
[BOOT] Project: 03_flash_eeprom_emulation
[BOOT] Board: NUCLEO-G0B1RE
[BOOT] System clock: 48 MHz
[BOOT] UART logger initialized
[BOOT] Test mode: <test name>
```

---

## 27. Final Acceptance Criteria

The project is considered complete when:

```txt
Test 0 passes
Test 1 passes
Test 2 passes after software reset
Test 3 proves latest-value lookup
Test 4 proves page transfer
Test 5A and 5B prove RECEIVE recovery policy
Test 6 proves corrupt record is skipped
Test 7 proves write offset recovery after reset
All logs are saved under assets/logs
README and test analysis are committed
```
