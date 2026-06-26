/*
 * app_main.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_APP_MAIN_H_
#define INC_APP_MAIN_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Function prototypes -------------------------------------------------------*/
void AppMain_Init(UART_HandleTypeDef *debug_uart);
void AppMain_Run(void);

#endif /* INC_APP_MAIN_H_ */