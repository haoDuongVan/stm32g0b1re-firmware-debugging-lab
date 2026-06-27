/*
 * bl_slot.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

#include "bl_slot.h"

#include "bl_flash_layout.h"

/* Private functions ---------------------------------------------------------*/

// Return the base Flash address for the given slot
static uint32_t BlSlot_GetBaseAddress(BlImageSlotId_t slot)
{
  if (slot == BL_IMAGE_SLOT_B) {
    return BL_SLOT_B_BASE_ADDR;
  }

  return BL_SLOT_A_BASE_ADDR;
}

// Return FLASH_BANK_1 or FLASH_BANK_2 for the given Flash address
static uint32_t BlSlot_GetFlashBank(uint32_t address)
{
#if defined(FLASH_DBANK_SUPPORT)
  if (address >= (FLASH_BASE + FLASH_BANK_SIZE)) {
    return FLASH_BANK_2;
  }
#else
  (void)address;
#endif

  return FLASH_BANK_1;
}

// Return the page number within its bank for the given Flash address
static uint32_t BlSlot_GetFlashPage(uint32_t address)
{
#if defined(FLASH_DBANK_SUPPORT)
  if (address >= (FLASH_BASE + FLASH_BANK_SIZE)) {
    return (uint32_t)((address - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE);
  }
#endif

  return (uint32_t)((address - FLASH_BASE) / FLASH_PAGE_SIZE);
}

/* Function definitions ------------------------------------------------------*/

// Erase all Flash pages occupied by the given slot; returns 1 on success
uint8_t BlSlot_Erase(BlImageSlotId_t slot)
{
  FLASH_EraseInitTypeDef erase_init;
  HAL_StatusTypeDef status;
  uint32_t page_error;
  uint32_t slot_base;
  uint32_t page_count;

  slot_base  = BlSlot_GetBaseAddress(slot);

  /*
   * Both slots are the same size (BL_SLOT_SIZE).
   * page_count is computed from BL_SLOT_SIZE, which is verified at compile
   * time to be a multiple of BL_PAGE_SIZE by bl_flash_layout.h.
   */
  page_count = BL_SLOT_SIZE / BL_PAGE_SIZE;

  status = HAL_FLASH_Unlock();
  if (status != HAL_OK) {
    return 0U;
  }

  page_error = 0xFFFFFFFFUL;

  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks     = BlSlot_GetFlashBank(slot_base);
  erase_init.Page      = BlSlot_GetFlashPage(slot_base);
  erase_init.NbPages   = page_count;

  status = HAL_FLASHEx_Erase(&erase_init, &page_error);

  (void)HAL_FLASH_Lock();

  if (status != HAL_OK) {
    return 0U;
  }

  return 1U;
}
