/*
 * bl_update.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_BL_UPDATE_H_
#define INC_BL_UPDATE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "stm32g0xx_hal.h"

/* Function prototypes -------------------------------------------------------*/
void    BlUpdate_Init(UART_HandleTypeDef *uart);
uint8_t BlUpdate_WaitForEntry(void);
void    BlUpdate_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_BL_UPDATE_H_ */
