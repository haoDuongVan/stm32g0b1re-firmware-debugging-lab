/*
 * bl_slot.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_BL_SLOT_H_
#define INC_BL_SLOT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "bl_image.h"

/* Function prototypes -------------------------------------------------------*/
uint8_t BlSlot_Erase(BlImageSlotId_t slot);

#ifdef __cplusplus
}
#endif

#endif /* INC_BL_SLOT_H_ */
