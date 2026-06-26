# Project 04 - STM32 Dual-Slot UART Bootloader

This README is the design contract for Project 04.

The purpose of this project is to build a UART bootloader on STM32G0B1RE with:

- fixed bootloader area
- two application slots
- CubeIDE `.elf` to `.bin` export flow
- synchronous UART firmware update protocol
- vector table validation
- CRC verification
- pending / confirm / rollback state machine
- evidence-based tests

The project is intentionally split into V1 scope and V2 future work.

V1 is what this project will implement and test with logs.  
V2 is documented as future work only and must not be claimed as completed.

```txt
firmware/04_uart_bootloader
```

---

## 1. Project Goal

The goal is not to make a minimal tutorial bootloader.

The goal is to build a practical firmware update flow that can answer these questions with real logs:

```txt
Can the bootloader receive a firmware image over UART?
Can it write the image to an inactive slot?
Can it validate the image before booting it?
Can it reject a binary built for the wrong slot?
Can it boot a pending firmware and wait for confirmation?
Can it roll back if the new firmware does not confirm boot success?
Can it prove that Slot A is untouched while updating Slot B?
Can it verify that VTOR remains correct after the application starts?
```

---

## 2. Target

| Item | Value |
| --- | --- |
| Board | NUCLEO-G0B1RE |
| MCU | STM32G0B1RE |
| Flash size | 512 KB |
| Flash page size | 2048 bytes |
| Bank mode | Dual-bank |
| Bank 1 | `0x08000000 - 0x0803FFFF` |
| Bank 2 | `0x08040000 - 0x0807FFFF` |
| Debug UART | USART2, 115200 bps |
| RTOS | Not used in V1 |
| IDE | STM32CubeIDE |
| Payload format | `.bin` |
| Update protocol | UART, synchronous ACK/NACK |

---

## 3. V1 Scope

V1 implements:

```txt
Fixed bootloader in Bank 1
Application Slot A in Bank 1
Application Slot B in Bank 2
Two metadata pages at the end of Flash
Separate linker configuration for Slot A and Slot B
CubeIDE post-build export from .elf to .bin
Synchronous UART protocol
Erase/write inactive slot
Firmware image CRC verification
MSP validation
Reset_Handler range validation
Wrong-slot binary rejection
Bootloader VTOR + MSP jump
Application VTOR consistency log
Pending slot boot
Application boot confirmation
Rollback if pending firmware is not confirmed
Slot A untouched check after Slot B update
```

V1 does not claim:

```txt
Full execute-from-RAM Flash driver
UART continuous streaming during same-bank Flash erase/program
BFB2 option byte boot swap
Physical power-cut safe update
Cryptographic firmware signature
Anti-rollback security
Firmware encryption
Production OTA security model
```

---

## 4. V2 Future Work

V2 may add:

```txt
Register-level Flash primitive executed fully from RAM
objdump/map evidence proving the Flash critical path is in RAM
Continuous UART streaming while Flash operation is running
BFB2 / bank swap experiments
Physical power-cut test during erase/program
Firmware signing
Firmware anti-rollback version policy
Encrypted firmware payload
Multi-image metadata format
```

These features are not part of V1 acceptance criteria.

---

## 5. Flash Layout

Project 04 uses a fixed bootloader and two application slots.

```txt
STM32G0B1RE Flash 512 KB

Bank 1:
  0x08000000 - 0x0800FFFF : Bootloader, 64 KB
  0x08010000 - 0x0803FFFF : Slot A, 192 KB

Bank 2:
  0x08040000 - 0x0806FFFF : Slot B, 192 KB
  0x08070000 - 0x0807EFFF : Reserved, 60 KB
  0x0807F000 - 0x0807F7FF : Metadata Page 0, 2 KB
  0x0807F800 - 0x0807FFFF : Metadata Page 1, 2 KB
```

Constants:

```c
#define BL_BOOT_BASE_ADDR          0x08000000UL
#define BL_BOOT_SIZE               (64U * 1024U)

#define BL_SLOT_A_BASE_ADDR        0x08010000UL
#define BL_SLOT_A_SIZE             (192U * 1024U)

#define BL_SLOT_B_BASE_ADDR        0x08040000UL
#define BL_SLOT_B_SIZE             (192U * 1024U)

#define BL_METADATA_PAGE0_ADDR     0x0807F000UL
#define BL_METADATA_PAGE1_ADDR     0x0807F800UL
#define BL_METADATA_PAGE_SIZE      2048U
```

---

## 6. Dual-Bank Asymmetry

This layout is intentionally asymmetric.

The bootloader always runs from Bank 1.

When updating Slot B:

```txt
Bootloader instruction fetch: Bank 1
Flash erase/program target:  Bank 2
```

This can benefit from dual-bank read-while-write behavior.

This is the same underlying dual-bank idea used in Project 03, where Flash behavior was tested through EEPROM emulation and page transfer logs.

When updating Slot A:

```txt
Bootloader instruction fetch: Bank 1
Flash erase/program target:  Bank 1
```

This may have same-bank Flash blocking behavior.

This is not a bug in the design.  
It is a physical consequence of placing both bootloader and Slot A in Bank 1.

V1 handles this by using a synchronous UART protocol:

```txt
Host sends one command or chunk.
Bootloader performs erase/program/verify.
Bootloader sends ACK or NACK.
Host sends the next command only after ACK.
```

V1 does not claim that UART interrupt continues to run during same-bank Flash erase/program.

---

## 7. Application Slot Builds

The same application source can be used for Slot A and Slot B, but the two binaries must be built with different linker configuration.

Slot A build:

```txt
FLASH ORIGIN      = 0x08010000
FLASH LENGTH      = 192K
VECT_TAB_OFFSET   = 0x00010000
APP_SLOT_BASE     = 0x08010000
APP_SLOT_ID       = A
```

Slot B build:

```txt
FLASH ORIGIN      = 0x08040000
FLASH LENGTH      = 192K
VECT_TAB_OFFSET   = 0x00040000
APP_SLOT_BASE     = 0x08040000
APP_SLOT_ID       = B
```

A `.bin` built for Slot A must not be accepted as a valid Slot B image.  
A `.bin` built for Slot B must not be accepted as a valid Slot A image.

---

## 8. ELF to BIN Export

CubeIDE builds an `.elf` file.

The UART bootloader payload is a raw `.bin` file.

Post-build command example:

```bash
arm-none-eabi-objcopy -O binary "${BuildArtifactFileName}" "${BuildArtifactFileBaseName}.bin"
```

Expected outputs:

```txt
bootloader.elf
bootloader.bin

app_slot_a.elf
app_slot_a.bin

app_slot_b.elf
app_slot_b.bin
```

The `.bin` file does not contain address metadata.  
Therefore, the bootloader must know which slot the host intends to write.

---

## 9. Firmware Image Validation

Before booting a slot, the bootloader validates:

```txt
Initial MSP
Reset_Handler address range
Image size
Image CRC
Slot state in metadata
```

MSP validation:

```txt
Initial MSP must be inside SRAM.
```

Reset_Handler validation:

```txt
Slot A image:
  Reset_Handler must be in 0x08010000 - 0x0803FFFF

Slot B image:
  Reset_Handler must be in 0x08040000 - 0x0806FFFF
```

This catches wrong-slot binaries.

Image size validation:

```txt
The host must declare the image size in BEGIN_UPDATE.
The bootloader checks image_size <= target_slot_size before erasing or writing anything.
If image_size is larger than the selected slot, the update is rejected immediately.
```

This prevents a too-large `.bin` from overflowing into the reserved area, metadata pages, or another slot.

Example failure:

```txt
app_slot_a.bin is written into Slot B.
The binary CRC may still match.
But Reset_Handler points to 0x08010xxx.
Slot B requires Reset_Handler in 0x08040xxx - 0x0806xxxx.
Bootloader rejects the image.
```

---

## 10. VTOR Contract

Both bootloader and application participate in vector table setup.

Bootloader responsibility before jump:

```txt
1. Read Initial MSP from slot_base + 0
2. Read Reset_Handler from slot_base + 4
3. Validate Initial MSP
4. Validate Reset_Handler range
5. Disable interrupts
6. Set SCB->VTOR = slot_base
7. Set MSP
8. Jump to Reset_Handler
```

Application responsibility:

```txt
1. Build with the correct slot linker script
2. Set SCB->VTOR again in SystemInit() using VECT_TAB_OFFSET
3. Initialize UART
4. Print final SCB->VTOR after startup
```

Expected Slot A application log:

```txt
[APP] Slot=A
[APP] VTOR=0x08010000
[APP] Boot OK
```

Expected Slot B application log:

```txt
[APP] Slot=B
[APP] VTOR=0x08040000
[APP] Boot OK
```

The final VTOR log is important because `SystemInit()` runs after bootloader jump.  
If the application was built with a wrong `VECT_TAB_OFFSET`, it may overwrite `SCB->VTOR` with an incorrect value even after the bootloader set it correctly.

TEST5 must not rely only on visual inspection.  
The test log must print expected and actual VTOR values and end with `PASS` or `FAIL`.

Example Slot A TEST5 log:

```txt
[TEST5] expected_vtor=0x08010000
[TEST5] actual_vtor=0x08010000
[TEST5] vtor_check=OK
[TEST5] PASS
```

Example failure:

```txt
[TEST5] expected_vtor=0x08040000
[TEST5] actual_vtor=0x08010000
[TEST5] vtor_check=NG
[TEST5] FAIL
```

---

## 11. UART Protocol V1

V1 uses a synchronous command/response protocol.

The host must wait for ACK/NACK after every command or data chunk.

Example commands:

```txt
PING
GET_INFO
GET_METADATA
ERASE_SLOT
BEGIN_UPDATE
WRITE_CHUNK
END_UPDATE
VERIFY_SLOT
MARK_PENDING
BOOT_SLOT
CONFIRM_BOOT
```

V1 does not stream data continuously while Flash operations are in progress.

This keeps the design predictable when updating Slot A in the same bank as the bootloader.

---

## 12. Firmware Update Flow

Normal update to inactive slot:

```txt
1. Bootloader boots current confirmed slot.
2. Host sends BEGIN_UPDATE for inactive slot.
3. Bootloader erases the inactive slot.
4. Host sends firmware chunks.
5. Bootloader writes chunks to Flash.
6. Bootloader verifies image CRC.
7. Bootloader validates vector table.
8. Bootloader marks the slot as PENDING.
9. System resets.
10. Bootloader boots the pending slot.
11. Application starts and confirms boot.
12. Bootloader marks the slot as CONFIRMED.
```

Rollback flow:

```txt
1. Bootloader boots pending slot.
2. Pending application crashes or resets before confirmation.
3. Bootloader sees pending slot was not confirmed.
4. Bootloader marks pending slot invalid or failed.
5. Bootloader rolls back to previous confirmed slot.
```

---

## 13. Metadata State Machine

Metadata is stored in two Flash pages at the end of Flash.

The metadata design is inspired by Project 03:

```txt
Do not trust RAM state.
Use magic value.
Use sequence number.
Use metadata CRC.
Use two pages to avoid relying on one mutable location.
```

Suggested metadata fields:

```c
typedef enum
{
  BL_SLOT_A = 0,
  BL_SLOT_B = 1,
  BL_SLOT_NONE = 0xFFFFFFFFU
} BlSlotId_t;

typedef enum
{
  BL_SLOT_STATE_EMPTY = 0,
  BL_SLOT_STATE_CONFIRMED,
  BL_SLOT_STATE_PENDING,
  BL_SLOT_STATE_TESTING,
  BL_SLOT_STATE_FAILED,
  BL_SLOT_STATE_INVALID
} BlSlotState_t;

typedef struct __attribute__((packed))
{
  uint32_t magic;
  uint32_t sequence;

  uint32_t active_slot;
  uint32_t pending_slot;
  uint32_t previous_slot;

  uint32_t slot_a_state;
  uint32_t slot_a_base;
  uint32_t slot_a_size;
  uint32_t slot_a_crc;

  uint32_t slot_b_state;
  uint32_t slot_b_base;
  uint32_t slot_b_size;
  uint32_t slot_b_crc;

  uint32_t boot_attempt_count;
  uint32_t metadata_crc;
} BlMetadata_t;
```

The exact implementation can be simplified during coding, but the final project must not claim rollback safety without metadata evidence.

---

## 14. Safety Checks

The bootloader must check:

```txt
Slot base is valid
Slot size is within allowed range
Erase address is inside the selected slot
Write address is inside the selected slot
Chunk does not cross slot boundary
Image CRC matches host-provided CRC
Reset_Handler belongs to the target slot range
Metadata CRC is valid
Slot state allows boot
```

Boundary validation policy:

```txt
BEGIN_UPDATE:
  Check declared image_size <= target_slot_size before erasing or writing.

WRITE_CHUNK:
  Check current_offset + chunk_size <= declared_image_size.
  Check current_offset + chunk_size <= target_slot_size.
  Check write_address is still inside the selected slot.
```

The boundary check must run for every `WRITE_CHUNK`, not only once at `BEGIN_UPDATE`.

This prevents both accidental host bugs and malicious extra chunks after a valid BEGIN_UPDATE command.

Important evidence test:

```txt
Slot A CRC before Slot B update
Slot A CRC after Slot B update
The two CRC values must match
```

For this test, the CRC must be recalculated by scanning the actual Slot A Flash region before and after the Slot B update.

The test must not simply compare the `slot_a_crc` value stored in metadata, because metadata may remain unchanged even if a Flash erase/write bug accidentally corrupts Slot A.

Required TEST12 evidence:

```txt
[TEST12] slot_a_crc_before=<crc calculated from Slot A Flash>
[TEST12] update_slot_b=OK
[TEST12] slot_a_crc_after=<crc recalculated from Slot A Flash>
[TEST12] slot_a_untouched_check=OK
[TEST12] PASS
```

This proves that updating Slot B does not accidentally erase or write Slot A.

---

## 15. Test Plan

The full test plan is split into phases.

The phases are a roadmap, not a promise that every advanced test must be completed before the first useful result.

### Phase 1 - Boot and Slot Validation

```txt
TEST0 bootloader_boot_check
TEST1 flash_layout_check
TEST2 validate_empty_slots
TEST3 validate_slot_a_vector
TEST4 jump_to_slot_a
TEST5 verify_vtor_after_app_start_with_pass_fail
TEST6 reject_wrong_slot_binary
```

### Phase 2 - BIN Export and UART Update

```txt
TEST7 cubeide_export_bin_check
TEST8 uart_ping_command
TEST9 erase_inactive_slot
TEST10 write_firmware_chunks
TEST11 verify_slot_crc
TEST12 slot_a_untouched_after_slot_b_update
```

### Phase 3 - A/B Safe Update

```txt
TEST13 mark_pending_slot
TEST14 boot_pending_slot
TEST15 app_confirm_boot
TEST16 rollback_if_not_confirmed
TEST17 reject_invalid_crc
```

---

## 16. Evidence Requirements

Evidence logs should be stored under:

```txt
assets/logs/04_uart_bootloader/
```

Screenshots should be stored under:

```txt
assets/screenshots/04_uart_bootloader/
```

Reports should be stored under:

```txt
assets/reports/04_uart_bootloader/
```

Each test should produce its own evidence file following the same naming pattern.

Suggested important evidence files:

```txt
test0_bootloader_boot_check.log
test1_flash_layout_check.log
test5_verify_vtor_after_app_start.log
test6_reject_wrong_slot_binary.log
test12_slot_a_untouched_after_slot_b_update.log
test16_rollback_if_not_confirmed.log
test_summary.md
```

The list above highlights the most important logs, but it does not mean other tests can skip evidence.

---

## 17. Blog Alignment

The blog article for this project must match V1.

The blog should emphasize:

```txt
Dual-slot safe update
BIN payload generated from ELF
Slot-specific linker script
VTOR and MSP jump
Wrong-slot binary rejection
Synchronous UART protocol
Pending / confirm / rollback
```

The blog should not claim V1 has:

```txt
Full execute-from-RAM Flash driver
Continuous UART streaming during same-bank Flash erase
Physical power-cut safety
BFB2 bank swap
```

Execute-from-RAM should be written as an advanced note or V2 direction unless there is objdump/map evidence.

---

## 18. Acceptance Criteria for V1

The acceptance criteria are split into two levels.

This prevents the project from claiming the full safe-update state machine before the basic bootloader and UART update flow are proven.

### V1-Minimal Acceptance Criteria

V1-Minimal is considered complete when Phase 1 and Phase 2 are done.

```txt
Bootloader builds and runs from 0x08000000
Slot A app builds and runs from 0x08010000
Slot B app builds and runs from 0x08040000
CubeIDE exports .bin from .elf
Bootloader validates MSP and Reset_Handler range
Bootloader rejects wrong-slot binary
Bootloader jumps to app with correct VTOR and MSP
Application logs final SCB->VTOR
TEST5 verifies expected_vtor vs actual_vtor with PASS/FAIL
Bootloader uses synchronous UART PING/ACK flow
Bootloader rejects image_size > target_slot_size before erase/write
Bootloader checks WRITE_CHUNK boundary on every chunk
Bootloader writes firmware chunks to inactive slot
Bootloader verifies image CRC
Slot A remains unchanged after Slot B update
Evidence logs are saved for Phase 1 and Phase 2
README, blog, project page, and notes do not claim features outside V1-Minimal
```

### V1-Full Acceptance Criteria

V1-Full is considered complete when Phase 1, Phase 2, and Phase 3 are done.

```txt
All V1-Minimal criteria are satisfied
Metadata has magic, sequence, state, and CRC validation
Pending slot can boot
Application can confirm boot
Bootloader can mark a pending slot as confirmed
Bootloader can rollback when pending app does not confirm
Bootloader rejects invalid CRC before marking a slot pending
Evidence logs are saved for Phase 3
README, blog, project page, and notes clearly distinguish V1-Full from V2 future work
```
