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

/* Function definitions ------------------------------------------------------*/

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

// Return 1 if magic, version, and slot fields are valid; CRC is not checked in this milestone
uint8_t BlMetadata_IsValid(const BlMetadata_t *meta)
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

  /*
   * CRC32 is intentionally not checked in this milestone.
   * It will be added after the metadata read / slot selection flow is stable.
   */
  return 1U;
}
