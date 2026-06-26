/*
 * bl_image.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_BL_IMAGE_H_
#define INC_BL_IMAGE_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/

// Application slot identifier.
typedef enum
{
  BL_IMAGE_SLOT_A = 0,
  BL_IMAGE_SLOT_B
} BlImageSlotId_t;

// Application vector table validation result.
typedef struct
{
  const char *slot_name;

  uint32_t slot_base;
  uint32_t slot_end;

  uint32_t initial_msp;
  uint32_t reset_handler_raw;
  uint32_t reset_handler_addr;

  uint8_t msp_check;
  uint8_t reset_thumb_check;
  uint8_t reset_range_check;
  uint8_t vector_check;
} BlImageVectorInfo_t;

/* Function prototypes -------------------------------------------------------*/
void BlImage_ValidateSlot(BlImageSlotId_t slot_id,
                          BlImageVectorInfo_t *vector_info);

#endif /* INC_BL_IMAGE_H_ */