/*
 * uart_logger.h
 *
 *  Created on: Jun 23, 2026
 *      Author: haodu
 */

#ifndef INC_UART_LOGGER_H_
#define INC_UART_LOGGER_H_

/* Private includes ----------------------------------------------------------*/
#include "stm32g0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* Function prototypes -------------------------------------------------------*/
void     DebugLogger_Init (UART_HandleTypeDef *huart);
bool     DebugLogger_Write (const uint8_t *data, uint16_t len);
void     DebugLogger_Printf (const char *format, ...);
void     DebugLogger_TxCpltCallback (UART_HandleTypeDef *huart);
uint32_t DebugLogger_GetDroppedCount (void);
uint16_t DebugLogger_GetBufferedSize (void);

#endif /* INC_UART_LOGGER_H_ */
