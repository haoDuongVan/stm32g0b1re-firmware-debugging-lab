/*
 * bl_image.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "stm32g0xx.h"
#include "stm32g0xx_hal.h"

#include "bl_image.h"

#include "bl_flash_layout.h"

/* Private defines -----------------------------------------------------------*/
#define BL_IMAGE_VECTOR_MSP_OFFSET             0x00UL
#define BL_IMAGE_VECTOR_RESET_OFFSET           0x04UL

#define BL_IMAGE_THUMB_BIT_MASK                0x00000001UL
#define BL_IMAGE_RESET_ADDR_MASK               0xFFFFFFFEUL

#define BL_IMAGE_MSP_ALIGN_MASK                0x00000007UL

/* Private typedef -----------------------------------------------------------*/

// Application Reset_Handler function pointer type.
typedef void (*BlImageEntryPoint_t)(void);

/* Private functions ---------------------------------------------------------*/

// Fill slot base/end address and name from slot_id into vector_info
static void BlImage_SetSlotInfo(BlImageSlotId_t slot_id,
                                BlImageVectorInfo_t *vector_info)
{
  if (slot_id == BL_IMAGE_SLOT_A) {
    vector_info->slot_name = "A";
    vector_info->slot_base = BL_SLOT_A_BASE_ADDR;
    vector_info->slot_end = BL_SLOT_A_END_ADDR;
  } else {
    vector_info->slot_name = "B";
    vector_info->slot_base = BL_SLOT_B_BASE_ADDR;
    vector_info->slot_end = BL_SLOT_B_END_ADDR;
  }
}

// Read initial MSP and Reset_Handler from the application vector table in Flash
static void BlImage_ReadVectorTable(BlImageVectorInfo_t *vector_info)
{
  uint32_t slot_base;

  slot_base = vector_info->slot_base;

  /*
   * The first two words of the application image are:
   *
   *   slot_base + 0 : Initial MSP
   *   slot_base + 4 : Reset_Handler
   */
  vector_info->initial_msp =
      *(const volatile uint32_t *)(slot_base + BL_IMAGE_VECTOR_MSP_OFFSET);

  vector_info->reset_handler_raw =
      *(const volatile uint32_t *)(slot_base + BL_IMAGE_VECTOR_RESET_OFFSET);

  /*
   * On Cortex-M, function addresses stored in the vector table have bit0 set
   * to indicate Thumb state. Clear bit0 before checking the Flash address range.
   */
  vector_info->reset_handler_addr =
      vector_info->reset_handler_raw & BL_IMAGE_RESET_ADDR_MASK;
}

// Check whether initial_msp falls inside SRAM and is 8-byte aligned; returns 1 if OK
static uint8_t BlImage_CheckMsp(uint32_t initial_msp)
{
  /*
   * Stack grows downward.
   *
   * A valid initial MSP normally points to the top of SRAM, so the upper bound
   * must allow BL_SRAM_LIMIT_ADDR.
   */
  if (initial_msp <= BL_SRAM_BASE_ADDR) {
    return 0U;
  }

  if (initial_msp > BL_SRAM_LIMIT_ADDR) {
    return 0U;
  }

  // The initial stack pointer should be 8-byte aligned
  if ((initial_msp & BL_IMAGE_MSP_ALIGN_MASK) != 0UL) {
    return 0U;
  }

  return 1U;
}

// Check whether Reset_Handler has the Thumb bit set (bit0 == 1); returns 1 if OK
static uint8_t BlImage_CheckResetThumbBit(uint32_t reset_handler_raw)
{
  if ((reset_handler_raw & BL_IMAGE_THUMB_BIT_MASK) == 0UL) {
    return 0U;
  }

  return 1U;
}

// Check whether Reset_Handler address (Thumb bit cleared) falls inside the slot; returns 1 if OK
static uint8_t BlImage_CheckResetRange(uint32_t reset_handler_addr,
                                       uint32_t slot_base,
                                       uint32_t slot_end)
{
  if (reset_handler_addr < slot_base) {
    return 0U;
  }

  if (reset_handler_addr > slot_end) {
    return 0U;
  }

  return 1U;
}

/* Function definitions ------------------------------------------------------*/

// Validate the application vector table in the given slot and fill vector_info with results
void BlImage_ValidateSlot(BlImageSlotId_t slot_id,
                          BlImageVectorInfo_t *vector_info)
{
  if (vector_info == NULL) {
    return;
  }

  BlImage_SetSlotInfo(slot_id, vector_info);
  BlImage_ReadVectorTable(vector_info);

  vector_info->msp_check =
      BlImage_CheckMsp(vector_info->initial_msp);

  vector_info->reset_thumb_check =
      BlImage_CheckResetThumbBit(vector_info->reset_handler_raw);

  vector_info->reset_range_check =
      BlImage_CheckResetRange(vector_info->reset_handler_addr,
                              vector_info->slot_base,
                              vector_info->slot_end);

  // vector_check is OK only when all three sub-checks pass
  if ((vector_info->msp_check != 0U) &&
      (vector_info->reset_thumb_check != 0U) &&
      (vector_info->reset_range_check != 0U)) {
    vector_info->vector_check = 1U;
  } else {
    vector_info->vector_check = 0U;
  }
}

// Tear down bootloader hardware state and jump to the application Reset_Handler
uint8_t BlImage_JumpToImage(const BlImageVectorInfo_t *vector_info)
{
  BlImageEntryPoint_t app_entry;

  if (vector_info == NULL) {
    return 0U;
  }

  /*
   * Do not jump if the vector table was not validated.
   *
   * The caller should call BlImage_ValidateSlot() first and print the detailed
   * validation result before reaching this point.
   */
  if (vector_info->vector_check == 0U) {
    return 0U;
  }

  app_entry = (BlImageEntryPoint_t)vector_info->reset_handler_raw;

  /*
   * Stop interrupts while changing the CPU execution context.
   *
   * Pending bootloader interrupts must not run after the application vector
   * table and MSP are installed.
   */
  __disable_irq();

  /*
   * Deinitialize HAL state used by the bootloader.
   *
   * The application will run its own SystemInit(), HAL_Init(), clock setup,
   * GPIO init and UART init again.
   */
  (void)HAL_DeInit();

  // Stop SysTick before handing off to the application
  SysTick->CTRL = 0UL;
  SysTick->LOAD = 0UL;
  SysTick->VAL  = 0UL;

  // Clear any pending SysTick and PendSV exceptions
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

  /*
   * Disable and clear all NVIC interrupts.
   *
   * STM32G0 is Cortex-M0+, so one ICER/ICPR register is enough for this target.
   */
  NVIC->ICER[0] = 0xFFFFFFFFUL;
  NVIC->ICPR[0] = 0xFFFFFFFFUL;

  // Install application vector table and stack pointer
  SCB->VTOR = vector_info->slot_base;
  __DSB();
  __ISB();

  __set_MSP(vector_info->initial_msp);

  /*
   * Re-enable interrupts before entering the application.
   *
   * After this point, interrupts will use the application vector table.
   */
  __enable_irq();

  /*
   * Jump to application Reset_Handler.
   *
   * reset_handler_raw keeps bit0 set, which is required for Thumb state.
   */
  app_entry();

  // Reset_Handler should never return
  while (1) {
  }
}
