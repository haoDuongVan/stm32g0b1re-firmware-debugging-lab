/*
 * bl_log.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_BL_LOG_H_
#define INC_BL_LOG_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Function prototypes -------------------------------------------------------*/
void BlLog_Init(UART_HandleTypeDef *debug_uart);
void BlLog_Printf(const char *format, ...);

#endif /* INC_BL_LOG_H_ */