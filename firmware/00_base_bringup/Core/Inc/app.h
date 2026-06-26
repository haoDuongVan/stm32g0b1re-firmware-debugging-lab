/*
 * app.h
 *
 *  Created on: Jun 22, 2026
 *      Author: haodu
 */

#ifndef INC_APP_H_
#define INC_APP_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"


/* Function prototypes -------------------------------------------------------*/
void App_Init(UART_HandleTypeDef *debug_uart);
void App_Run(void);

#endif /* INC_APP_H_ */
