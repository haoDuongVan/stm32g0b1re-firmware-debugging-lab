/*
 * bl_metadata.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "bl_metadata.h"

#include "bl_flash_layout.h"

/* Private defines -----------------------------------------------------------*/

#define BL_METADATA_CRC32_POLY           0xEDB88320UL
#define BL_METADATA_CRC32_INIT           0xFFFFFFFFUL
#define BL_METADATA_CRC32_XOROUT         0xFFFFFFFFUL

/*
 * CRC is calculated over all metadata fields except the crc32 field itself.
 */
#define BL_METADATA_CRC_DATA_SIZE        ((uint32_t)(sizeof(BlMetadata_t) - sizeof(uint32_t)))

/* Function definitions ------------------------------------------------------*/

// Update CRC32 for one byte
static uint32_t BlMetadata_UpdateCrc32(uint32_t crc, uint8_t data)
{
  uint8_t bit;

  crc ^= (uint32_t)data;

  for (bit = 0U; bit < 8U; bit++) {
    if ((crc & 1UL) != 0UL) {
      crc = (crc >> 1U) ^ BL_METADATA_CRC32_POLY;
    } else {
      crc = crc >> 1U;
    }
  }

  return crc;
}

// Copy metadata struct from Flash metadata page 0 into the caller's buffer
void BlMetadata_Read(BlMetadata_t *meta)
{
  const BlMetadata_t *flash_meta;

  if (meta == NULL) {
    return;
  }

  /*
   * Cast the metadata page base address to a pointer and copy the struct.
   * Flash is memory-mapped, so no special read API is needed.
   */
  flash_meta = (const BlMetadata_t *)BL_METADATA0_BASE_ADDR;
  *meta = *flash_meta;
}

// Return 1 if magic, version, and slot fields are valid; does not check CRC
uint8_t BlMetadata_IsHeaderValid(const BlMetadata_t *meta)
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
uint32_t BlMetadata_CalculateCrc(const BlMetadata_t *meta)
{
  const uint8_t *data;
  uint32_t crc;
  uint32_t i;

  if (meta == NULL) {
    return 0UL;
  }

  data = (const uint8_t *)meta;
  crc = BL_METADATA_CRC32_INIT;

  for (i = 0U; i < BL_METADATA_CRC_DATA_SIZE; i++) {
    crc = BlMetadata_UpdateCrc32(crc, data[i]);
  }

  return (crc ^ BL_METADATA_CRC32_XOROUT);
}

// Return 1 if the stored crc32 matches the recalculated value
uint8_t BlMetadata_IsCrcValid(const BlMetadata_t *meta)
{
  uint32_t calculated_crc;

  if (meta == NULL) {
    return 0U;
  }

  calculated_crc = BlMetadata_CalculateCrc(meta);

  if (meta->crc32 != calculated_crc) {
    return 0U;
  }

  return 1U;
}

// Return 1 if header fields are valid and CRC matches
uint8_t BlMetadata_IsValid(const BlMetadata_t *meta)
{
  if (BlMetadata_IsHeaderValid(meta) == 0U) {
    return 0U;
  }

  if (BlMetadata_IsCrcValid(meta) == 0U) {
    return 0U;
  }

  return 1U;
}
