/*
 * app_log.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_APP_LOG_H_
#define INC_APP_LOG_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Function prototypes -------------------------------------------------------*/
void AppLog_Init(UART_HandleTypeDef *debug_uart);
void AppLog_Printf(const char *format, ...);

#endif /* INC_APP_LOG_H_ */