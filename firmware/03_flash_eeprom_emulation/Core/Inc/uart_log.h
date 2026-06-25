/*
 * uart_log.h
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

#ifndef INC_UART_LOG_H_
#define INC_UART_LOG_H_

/* Private includes ----------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Function prototypes -------------------------------------------------------*/
void UartLog_Init(UART_HandleTypeDef *debug_uart);
void UartLog_Printf(const char *format, ...);

#endif /* INC_UART_LOG_H_ */
