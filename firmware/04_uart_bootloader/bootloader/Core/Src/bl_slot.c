/*
 * bl_slot.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "stm32g0xx_hal.h"

#include "bl_slot.h"

#include "bl_flash_layout.h"

/* Private defines -----------------------------------------------------------*/

/* STM32G0 Flash must be programmed in 64-bit (8-byte) doubleword units. */
#define BL_SLOT_FLASH_DOUBLEWORD_SIZE    8UL

/* Private functions ---------------------------------------------------------*/

// Return the size of the given slot in bytes
static uint32_t BlSlot_GetSize(BlImageSlotId_t slot)
{
  (void)slot;

  /*
   * Both slots are the same size in this layout.
   * The cast silences unused-parameter warnings on compilers that do not
   * recognise (void)slot as a suppression.
   */
  return BL_SLOT_SIZE;
}

// Return 1 if [offset, offset+size) lies within the slot's Flash range
static uint8_t BlSlot_IsRangeValid(BlImageSlotId_t slot,
                                   uint32_t offset,
                                   uint32_t size)
{
  uint32_t slot_size;

  slot_size = BlSlot_GetSize(slot);

  if (size == 0UL) {
    return 0U;
  }

  if (offset >= slot_size) {
    return 0U;
  }

  if (size > (slot_size - offset)) {
    return 0U;
  }

  return 1U;
}

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

// Program data into the slot at byte offset; offset and size must be doubleword-aligned
uint8_t BlSlot_Write(BlImageSlotId_t slot,
                     uint32_t offset,
                     const uint8_t *data,
                     uint32_t size)
{
  HAL_StatusTypeDef status;
  uint32_t slot_base;
  uint32_t write_addr;
  uint32_t index;
  uint64_t double_word;

  if (data == NULL) {
    return 0U;
  }

  if (BlSlot_IsRangeValid(slot, offset, size) == 0U) {
    return 0U;
  }

  /*
   * Both offset and size must be multiples of 8 because STM32G0 Flash can
   * only be programmed in 64-bit doubleword units.
   */
  if (((offset % BL_SLOT_FLASH_DOUBLEWORD_SIZE) != 0UL) ||
      ((size   % BL_SLOT_FLASH_DOUBLEWORD_SIZE) != 0UL)) {
    return 0U;
  }

  slot_base = BlSlot_GetBaseAddress(slot);

  status = HAL_FLASH_Unlock();
  if (status != HAL_OK) {
    return 0U;
  }

  for (index = 0UL; index < size; index += BL_SLOT_FLASH_DOUBLEWORD_SIZE) {
    write_addr = slot_base + offset + index;

    memcpy(&double_word, &data[index], sizeof(double_word));

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                               write_addr,
                               double_word);

    if (status != HAL_OK) {
      break;
    }
  }

  (void)HAL_FLASH_Lock();

  if (status != HAL_OK) {
    return 0U;
  }

  return 1U;
}

// Compare Flash contents at [slot_base + offset] against data; returns 1 if all bytes match
uint8_t BlSlot_Verify(BlImageSlotId_t slot,
                      uint32_t offset,
                      const uint8_t *data,
                      uint32_t size)
{
  const uint8_t *flash_ptr;
  uint32_t slot_base;
  uint32_t index;

  if (data == NULL) {
    return 0U;
  }

  if (BlSlot_IsRangeValid(slot, offset, size) == 0U) {
    return 0U;
  }

  /*
   * Flash is memory-mapped on STM32, so a direct pointer comparison is enough.
   * No special read API is needed.
   */
  slot_base = BlSlot_GetBaseAddress(slot);
  flash_ptr = (const uint8_t *)(slot_base + offset);

  for (index = 0UL; index < size; index++) {
    if (flash_ptr[index] != data[index]) {
      return 0U;
    }
  }

  return 1U;
}
