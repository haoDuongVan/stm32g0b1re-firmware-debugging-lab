/*
 * bl_flash_layout.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef BL_FLASH_LAYOUT_H_
#define BL_FLASH_LAYOUT_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/
/*
 * STM32G0B1RE Flash layout for Project 04.
 *
 * Total Flash:
 *
 *   0x08000000 - 0x0807FFFF : 512KB
 *
 * Project layout:
 *
 *   0x08000000 - 0x0800FFFF : Bootloader, 64KB
 *   0x08010000 - 0x0803FFFF : Slot A, 192KB
 *   0x08040000 - 0x0806FFFF : Slot B, 192KB
 *   0x08070000 - 0x0807EFFF : Reserved, 60KB
 *   0x0807F000 - 0x0807F7FF : Metadata Page 0, 2KB
 *   0x0807F800 - 0x0807FFFF : Metadata Page 1, 2KB
 */
#define BL_FLASH_BASE_ADDR               0x08000000UL
#define BL_FLASH_SIZE                    (512UL * 1024UL)
#define BL_FLASH_END_ADDR                (BL_FLASH_BASE_ADDR + BL_FLASH_SIZE - 1UL)

#define BL_PAGE_SIZE                     2048UL

#define BL_BOOT_BASE_ADDR                0x08000000UL
#define BL_BOOT_SIZE                     (64UL * 1024UL)
#define BL_BOOT_END_ADDR                 (BL_BOOT_BASE_ADDR + BL_BOOT_SIZE - 1UL)

#define BL_SLOT_A_BASE_ADDR              0x08010000UL
#define BL_SLOT_B_BASE_ADDR              0x08040000UL
#define BL_SLOT_SIZE                     (192UL * 1024UL)

#define BL_SLOT_A_END_ADDR               (BL_SLOT_A_BASE_ADDR + BL_SLOT_SIZE - 1UL)
#define BL_SLOT_B_END_ADDR               (BL_SLOT_B_BASE_ADDR + BL_SLOT_SIZE - 1UL)

#define BL_RESERVED_BASE_ADDR            0x08070000UL
#define BL_RESERVED_SIZE                 (60UL * 1024UL)
#define BL_RESERVED_END_ADDR             (BL_RESERVED_BASE_ADDR + BL_RESERVED_SIZE - 1UL)

#define BL_METADATA0_BASE_ADDR           0x0807F000UL
#define BL_METADATA1_BASE_ADDR           0x0807F800UL
#define BL_METADATA_PAGE_SIZE            BL_PAGE_SIZE

#define BL_METADATA0_END_ADDR            (BL_METADATA0_BASE_ADDR + BL_METADATA_PAGE_SIZE - 1UL)
#define BL_METADATA1_END_ADDR            (BL_METADATA1_BASE_ADDR + BL_METADATA_PAGE_SIZE - 1UL)

/*
 * Compile-time layout checks.
 *
 * These checks intentionally fail the build if the layout is modified
 * inconsistently.
 */
#if (BL_BOOT_BASE_ADDR != BL_FLASH_BASE_ADDR)
#error "Bootloader must start at Flash base address"
#endif

#if (BL_SLOT_A_BASE_ADDR != (BL_BOOT_BASE_ADDR + BL_BOOT_SIZE))
#error "Slot A must start immediately after bootloader region"
#endif

#if (BL_SLOT_B_BASE_ADDR != (BL_SLOT_A_BASE_ADDR + BL_SLOT_SIZE))
#error "Slot B must start immediately after Slot A region"
#endif

#if ((BL_METADATA1_BASE_ADDR + BL_METADATA_PAGE_SIZE) != (BL_FLASH_BASE_ADDR + BL_FLASH_SIZE))
#error "Metadata page 1 must end at the end of Flash"
#endif

#endif /* BL_FLASH_LAYOUT_H_ */