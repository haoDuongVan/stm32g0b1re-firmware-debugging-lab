# Project 04 Build Setup

This document records the STM32CubeIDE build setup for Project 04: **STM32 Dual-Slot UART Bootloader V1**.

Current status:

```txt
- Bootloader project generated
- App project generated
- UART2 and GPIO LED enabled by CubeMX
- Bootloader Flash layout configured
- App Slot A / Slot B build configurations configured
- ELF to BIN post-build export configured
- Bootloader logic is not implemented yet
- UART update protocol is not implemented yet
- Metadata / rollback logic is not implemented yet
```

---

## 1. Project structure

```txt
firmware/04_uart_bootloader/
  bootloader/
    bootloader.ioc
    Core/
    Drivers/
    STM32G0B1RETX_FLASH.ld
    STM32G0B1RETX_RAM.ld

  app/
    app.ioc
    Core/
    Drivers/
    linker/
      STM32G0B1RETX_SLOT_A.ld
      STM32G0B1RETX_SLOT_B.ld
    STM32G0B1RETX_FLASH.ld
    STM32G0B1RETX_RAM.ld

  common/
  BUILD_SETUP.md
```

There are two STM32CubeIDE projects:

```txt
bootloader
app
```

The app project uses multiple build configurations instead of separate app projects.

---

## 2. Hardware target

```txt
MCU: STM32G0B1RE
Board: NUCLEO-G0B1RE
IDE: STM32CubeIDE
```

Basic peripheral setup from `.ioc`:

```txt
USART2:
  Mode: Asynchronous
  Baudrate: 115200
  PA2: USART2_TX
  PA3: USART2_RX

GPIO:
  PA5: LED_GREEN

SYS:
  Debug: Serial Wire
```

Phase 1 intentionally does not use:

```txt
- USB
- FreeRTOS
- UART DMA
- UART interrupt
- CRC peripheral
```

UART logging will initially use polling transmit.

---

## 3. Flash layout

Target Flash size:

```txt
STM32G0B1RE Flash: 512KB
Base address: 0x08000000
```

Project 04 layout:

```txt
0x08000000 - 0x0800FFFF : Bootloader, 64KB
0x08010000 - 0x0803FFFF : Slot A, 192KB
0x08040000 - 0x0806FFFF : Slot B, 192KB
0x08070000 - 0x0807EFFF : Reserved, 60KB
0x0807F000 - 0x0807F7FF : Metadata Page 0, 2KB
0x0807F800 - 0x0807FFFF : Metadata Page 1, 2KB
```

---

## 4. Bootloader linker setup

Bootloader linker file:

```txt
bootloader/STM32G0B1RETX_FLASH.ld
```

Flash region:

```ld
FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
```

Expected bootloader address range:

```txt
0x08000000 - 0x0800FFFF
```

The bootloader must not exceed 64KB.

---

## 5. App build configurations

The app project has two main build configurations:

```txt
SlotA_Debug
SlotB_Debug
```

The original `Debug` and `Release` configurations may exist temporarily, but the intended Project 04 app builds are:

```txt
SlotA_Debug
SlotB_Debug
```

---

## 6. Slot A build config

Build configuration:

```txt
app / SlotA_Debug
```

Preprocessor symbols:

```txt
APP_SLOT_A
USER_VECT_TAB_ADDRESS
VECT_TAB_OFFSET=0x00010000U
```

Linker script:

```txt
app/linker/STM32G0B1RETX_SLOT_A.ld
```

Flash region:

```ld
FLASH (rx) : ORIGIN = 0x08010000, LENGTH = 192K
```

Build artifact name:

```txt
app_slot_a
```

Expected output:

```txt
app/SlotA_Debug/app_slot_a.elf
app/SlotA_Debug/app_slot_a.bin
app/SlotA_Debug/app_slot_a.map
```

Expected map check:

```txt
.isr_vector 0x08010000
```

---

## 7. Slot B build config

Build configuration:

```txt
app / SlotB_Debug
```

Preprocessor symbols:

```txt
APP_SLOT_B
USER_VECT_TAB_ADDRESS
VECT_TAB_OFFSET=0x00040000U
```

Linker script:

```txt
app/linker/STM32G0B1RETX_SLOT_B.ld
```

Flash region:

```ld
FLASH (rx) : ORIGIN = 0x08040000, LENGTH = 192K
```

Build artifact name:

```txt
app_slot_b
```

Expected output:

```txt
app/SlotB_Debug/app_slot_b.elf
app/SlotB_Debug/app_slot_b.bin
app/SlotB_Debug/app_slot_b.map
```

Expected map check:

```txt
.isr_vector 0x08040000
```

---

## 8. Vector table setup

The generated `system_stm32g0xx.c` supports vector table relocation when this macro is defined:

```c
USER_VECT_TAB_ADDRESS
```

The generated code uses:

```c
SCB->VTOR = VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET;
```

For the app project:

```txt
Slot A:
  VECT_TAB_OFFSET=0x00010000U
  SCB->VTOR = 0x08010000

Slot B:
  VECT_TAB_OFFSET=0x00040000U
  SCB->VTOR = 0x08040000
```

Important note:

```txt
VECT_TAB_OFFSET only changes SCB->VTOR.
It does not change the link address of the firmware.
The linker script must also place the image at the correct slot address.
```

---

## 9. ELF to BIN post-build

Each build configuration exports a `.bin` file from the `.elf` file.

Post-build command:

```bash
arm-none-eabi-objcopy -O binary "${BuildArtifactFileName}" "${BuildArtifactFileBaseName}.bin"
```

Expected binaries:

```txt
bootloader.bin
app_slot_a.bin
app_slot_b.bin
```

The bootloader update protocol will use the app `.bin` file as the firmware payload.

---

## 10. Why separate Slot A and Slot B builds are needed

Slot A and Slot B have different link addresses.

```txt
Slot A base: 0x08010000
Slot B base: 0x08040000
```

Therefore, the same app source code must be built twice:

```txt
SlotA_Debug -> app_slot_a.bin
SlotB_Debug -> app_slot_b.bin
```

A binary linked for Slot A must not be accepted as a valid Slot B image.

Example:

```txt
app_slot_a.bin written to Slot B address:
  CRC may be correct
  But Reset_Handler still points to Slot A range
  Bootloader must reject it
```

This is one of the planned validation tests.

---

## 11. Files that should not be committed

Build output folders should not be committed:

```txt
Debug/
Release/
SlotA_Debug/
SlotB_Debug/
SlotA_Release/
SlotB_Release/
```

Generated binary/object files should not be committed:

```txt
*.elf
*.bin
*.hex
*.map
*.list
*.o
*.d
*.su
*.cyclo
```

STM32CubeIDE language index should not be committed:

```txt
.settings/language.settings.xml
```

Recommended `.gitignore` pattern:

```gitignore
**/.settings/language.settings.xml

**/Debug/
**/Release/
**/SlotA_Debug/
**/SlotB_Debug/
**/SlotA_Release/
**/SlotB_Release/

*.elf
*.bin
*.hex
*.map
*.list
*.o
*.d
*.su
*.cyclo
```

---

## 12. Current verification checklist

Bootloader:

```txt
[OK] FLASH ORIGIN = 0x08000000
[OK] FLASH LENGTH = 64K
[OK] Artifact name = bootloader
[OK] Post-build exports bootloader.bin
[OK] .isr_vector is located at 0x08000000
```

App Slot A:

```txt
[OK] Build config = SlotA_Debug
[OK] Define APP_SLOT_A
[OK] Define USER_VECT_TAB_ADDRESS
[OK] Define VECT_TAB_OFFSET=0x00010000U
[OK] Linker FLASH ORIGIN = 0x08010000
[OK] Linker FLASH LENGTH = 192K
[OK] Artifact name = app_slot_a
[OK] Post-build exports app_slot_a.bin
[OK] .isr_vector is located at 0x08010000
```

App Slot B:

```txt
[OK] Build config = SlotB_Debug
[OK] Define APP_SLOT_B
[OK] Define USER_VECT_TAB_ADDRESS
[OK] Define VECT_TAB_OFFSET=0x00040000U
[OK] Linker FLASH ORIGIN = 0x08040000
[OK] Linker FLASH LENGTH = 192K
[OK] Artifact name = app_slot_b
[OK] Post-build exports app_slot_b.bin
[OK] .isr_vector is located at 0x08040000
```
