/*
 * uart_log.h
 *
 *  Created on: Jun 25, 2026
 *      Author: haodu
 */

#ifndef INC_UART_LOG_H_
#define INC_UART_LOG_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"
#include "cmsis_os.h"

/* Function prototypes -------------------------------------------------------*/
void UartLog_Init(UART_HandleTypeDef *huart, osMutexId_t mutex_id);
void UartLog_Printf(const char *format, ...);

#endif /* INC_UART_LOG_H_ */
