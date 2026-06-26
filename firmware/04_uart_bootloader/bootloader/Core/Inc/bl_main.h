/*
 * bl_main.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_BL_MAIN_H_
#define INC_BL_MAIN_H_

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Function prototypes -------------------------------------------------------*/
void BlMain_ApplyVectorTable(void);
void BlMain_Init(UART_HandleTypeDef *debug_uart);
void BlMain_Run(void);

#endif /* INC_BL_MAIN_H_ */