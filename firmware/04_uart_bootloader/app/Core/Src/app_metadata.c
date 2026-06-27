/*
 * app_metadata.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "stm32g0xx_hal.h"

#include "app_metadata.h"

#include "bl_flash_layout.h"

/* Private defines -----------------------------------------------------------*/

#define APP_METADATA_CRC32_POLY             0xEDB88320UL
#define APP_METADATA_CRC32_INIT             0xFFFFFFFFUL
#define APP_METADATA_CRC32_XOROUT           0xFFFFFFFFUL

/*
 * CRC is calculated over all metadata fields except the crc32 field itself.
 */
#define APP_METADATA_CRC_DATA_SIZE          ((uint32_t)(sizeof(BlMetadata_t) - sizeof(uint32_t)))

/*
 * STM32G0 Flash must be written in 64-bit (8-byte) doublewords.
 * Round the struct size up to the next multiple of 8 and pad with 0xFF.
 */
#define APP_METADATA_FLASH_DOUBLEWORD_SIZE  8UL
#define APP_METADATA_FLASH_WRITE_SIZE       (((sizeof(BlMetadata_t) + \
                                               (APP_METADATA_FLASH_DOUBLEWORD_SIZE - 1UL)) / \
                                               APP_METADATA_FLASH_DOUBLEWORD_SIZE) * \
                                               APP_METADATA_FLASH_DOUBLEWORD_SIZE)

/* Private functions ---------------------------------------------------------*/

// Update CRC32 for one byte
static uint32_t AppMetadata_UpdateCrc32(uint32_t crc, uint8_t data)
{
  uint8_t bit;

  crc ^= (uint32_t)data;

  for (bit = 0U; bit < 8U; bit++) {
    if ((crc & 1UL) != 0UL) {
      crc = (crc >> 1U) ^ APP_METADATA_CRC32_POLY;
    } else {
      crc = crc >> 1U;
    }
  }

  return crc;
}

// Return FLASH_BANK_1 or FLASH_BANK_2 for the given Flash address
static uint32_t AppMetadata_GetFlashBank(uint32_t address)
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
static uint32_t AppMetadata_GetFlashPage(uint32_t address)
{
#if defined(FLASH_DBANK_SUPPORT)
  if (address >= (FLASH_BASE + FLASH_BANK_SIZE)) {
    return (uint32_t)((address - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE);
  }
#endif

  return (uint32_t)((address - FLASH_BASE) / FLASH_PAGE_SIZE);
}

/* Function definitions ------------------------------------------------------*/

// Copy metadata struct from Flash metadata page 0 into the caller's buffer
void AppMetadata_Read(BlMetadata_t *meta)
{
  const BlMetadata_t *flash_meta;

  if (meta == NULL) {
    return;
  }

  /*
   * Flash is memory-mapped on STM32.
   * Cast the metadata page address to a pointer and copy the struct directly.
   */
  flash_meta = (const BlMetadata_t *)BL_METADATA0_BASE_ADDR;
  *meta = *flash_meta;
}

// Return 1 if magic, version, and slot fields are valid; does not check CRC
uint8_t AppMetadata_IsHeaderValid(const BlMetadata_t *meta)
{
  if (meta == NULL) {
    return 0U;
  }

  if (meta->magic != BL_METADATA_MAGIC) {
    return 0U;
  }

  if (meta->version != BL_METADATA_VERSION) {
    return 0U;
  }

  if ((meta->active_slot != BL_METADATA_SLOT_A) &&
      (meta->active_slot != BL_METADATA_SLOT_B)) {
    return 0U;
  }

  if ((meta->confirmed_slot != BL_METADATA_SLOT_A) &&
      (meta->confirmed_slot != BL_METADATA_SLOT_B)) {
    return 0U;
  }

  return 1U;
}

// Calculate CRC32 over all metadata fields except the crc32 field itself
uint32_t AppMetadata_CalculateCrc(const BlMetadata_t *meta)
{
  const uint8_t *data;
  uint32_t crc;
  uint32_t i;

  if (meta == NULL) {
    return 0UL;
  }

  data = (const uint8_t *)meta;
  crc = APP_METADATA_CRC32_INIT;

  for (i = 0U; i < APP_METADATA_CRC_DATA_SIZE; i++) {
    crc = AppMetadata_UpdateCrc32(crc, data[i]);
  }

  return (crc ^ APP_METADATA_CRC32_XOROUT);
}

// Return 1 if the stored crc32 matches the recalculated value
uint8_t AppMetadata_IsCrcValid(const BlMetadata_t *meta)
{
  uint32_t calculated_crc;

  if (meta == NULL) {
    return 0U;
  }

  calculated_crc = AppMetadata_CalculateCrc(meta);

  if (meta->crc32 != calculated_crc) {
    return 0U;
  }

  return 1U;
}

// Return 1 if header fields are valid and CRC matches
uint8_t AppMetadata_IsValid(const BlMetadata_t *meta)
{
  if (AppMetadata_IsHeaderValid(meta) == 0U) {
    return 0U;
  }

  if (AppMetadata_IsCrcValid(meta) == 0U) {
    return 0U;
  }

  return 1U;
}

// Erase Metadata Page 0 and write the metadata struct with a recalculated CRC
uint8_t AppMetadata_Write(const BlMetadata_t *meta)
{
  BlMetadata_t write_meta;
  FLASH_EraseInitTypeDef erase_init;
  HAL_StatusTypeDef status;
  uint32_t page_error;
  uint32_t offset;
  uint64_t double_word;
  uint8_t write_buffer[APP_METADATA_FLASH_WRITE_SIZE];

  if (meta == NULL) {
    return 0U;
  }

  /*
   * Recalculate CRC before writing so the stored value is always consistent
   * with the fields, even if the caller did not call CalculateCrc first.
   */
  write_meta = *meta;
  write_meta.crc32 = AppMetadata_CalculateCrc(&write_meta);

  if (AppMetadata_IsValid(&write_meta) == 0U) {
    return 0U;
  }

  /*
   * Pad the write buffer to APP_METADATA_FLASH_WRITE_SIZE with 0xFF so that
   * the trailing bytes beyond the struct do not disturb the erased Flash state.
   */
  memset(write_buffer, 0xFF, sizeof(write_buffer));
  memcpy(write_buffer, &write_meta, sizeof(write_meta));

  status = HAL_FLASH_Unlock();
  if (status != HAL_OK) {
    return 0U;
  }

  page_error = 0xFFFFFFFFUL;

  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks     = AppMetadata_GetFlashBank(BL_METADATA0_BASE_ADDR);
  erase_init.Page      = AppMetadata_GetFlashPage(BL_METADATA0_BASE_ADDR);
  erase_init.NbPages   = 1U;

  status = HAL_FLASHEx_Erase(&erase_init, &page_error);

  if (status == HAL_OK) {
    for (offset = 0UL; offset < sizeof(write_buffer); offset += APP_METADATA_FLASH_DOUBLEWORD_SIZE) {
      memcpy(&double_word, &write_buffer[offset], sizeof(double_word));

      status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                 (uint32_t)(BL_METADATA0_BASE_ADDR + offset),
                                 double_word);

      if (status != HAL_OK) {
        break;
      }
    }
  }

  (void)HAL_FLASH_Lock();

  if (status != HAL_OK) {
    return 0U;
  }

  return 1U;
}
