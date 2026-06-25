/*
 * app_main.h
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

#ifndef INC_APP_MAIN_H_
#define INC_APP_MAIN_H_

/* Private includes ----------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Function prototypes -------------------------------------------------------*/
void App_Init(UART_HandleTypeDef *debug_uart);
void App_Run(void);

#endif /* INC_APP_MAIN_H_ */
