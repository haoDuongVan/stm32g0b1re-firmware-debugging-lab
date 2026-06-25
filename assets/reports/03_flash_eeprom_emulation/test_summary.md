# STM32 Flash EEPROM Emulation - Test Evidence Summary

Target board: NUCLEO-G0B1RE  
MCU: STM32G0B1RE  
Clock: 48 MHz  
EEPROM emulation area: Bank 2, 0x08040000 - 0x08040FFF  
Page size: 2048 bytes  
Header size: 32 bytes  
Record size: 8 bytes  
Records per page: 252  

## Test Results

| Test | Name | Result | Evidence |
|---|---|---|---|
| TEST0 | boot_check | PASS | test0_boot_check.txt |
| TEST1 | format_default | PASS | test1_format_default.txt |
| TEST2 | write_readback | PASS | test2_write_readback.txt |
| TEST3 | append_latest | PASS | test3_append_latest.txt |
| TEST4 | reboot_readback | PASS | test4_reboot_readback.txt |
| TEST5 | page_transfer | PASS | test5_page_transfer.txt |
| TEST6 | page_transfer_reboot | PASS | test6_page_transfer_reboot.txt |
| TEST7 | fault_after_receive | PASS | test7_fault_after_receive.txt |
| TEST8 | fault_after_copy | PASS | test8_fault_after_copy.txt |
| TEST9 | corrupt_record | PASS | test9_corrupt_record.txt |
| TEST10 | fault_after_program | PASS | test10_fault_after_program.txt |

## Key Evidence

### TEST0 - Layout check

- Page A: 0x08040000
- Page B: 0x08040800
- Page size: 2048 bytes
- Header size: 32 bytes
- Record size: 8 bytes
- Records per page: 252
- System clock: 48 MHz

### TEST5 - Page transfer

The active page was filled to offset 2048.  
A new write triggered transfer from Page A to Page B.

Final state:

- Page A: ERASED
- Page B: ACTIVE
- Active page: 0x08040800
- Write offset: 56
- Valid records in Page B: 3

### TEST6 - Page transfer after reset

After page transfer and software reset, `Ee_Init()` restored the state from Flash:

- Page A: ERASED
- Page B: ACTIVE
- Active page: 0x08040800
- Write offset: 64

### TEST7 - Reset after RECEIVE marker

A software reset was injected immediately after writing the RECEIVE marker.  
After reboot, the unfinished RECEIVE page was erased and the old ACTIVE page was kept.

Final state:

- Page A: ACTIVE
- Page B: ERASED
- Write offset: 2048

### TEST8 - Reset after copying latest records

A software reset was injected after copying latest records to the RECEIVE page.  
After reboot, the RECEIVE page was erased and the old ACTIVE page was kept.

Final state:

- Page A: ACTIVE
- Page B: ERASED
- Write offset: 2048

### TEST9 - Corrupt record

A corrupt record with an invalid CRC was written after a valid record.  
`Ee_Read()` skipped the corrupt latest record and returned the last valid value.

Result:

- Valid value: 115200
- Corrupt value: 230400
- Read result: 115200
- Valid record count: 1

### TEST10 - Reset after Flash program

A software reset was injected after `HAL_FLASH_Program()` succeeded but before the RAM write offset was updated.  
After reboot, `Ee_Init()` scanned Flash and restored the correct write offset.

Final state:

- Active page: 0x08040000
- Write offset: 48
- CFG_BAUD_RATE: 115200