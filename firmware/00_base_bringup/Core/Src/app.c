/*
 * app.c
 *
 *  Created on: Jun 22, 2026
 *      Author: haodu
 */


/* Includes ------------------------------------------------------------------*/
#include "app.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Private macros ------------------------------------------------------------*/
#define APP_LED_GPIO_Port   GPIOA
#define APP_LED_Pin         GPIO_PIN_5

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *debug_uart = NULL;

/* Private functions ---------------------------------------------------------*/

// Send a raw string over the debug UART (blocking)
static void App_Log(const char *message)
{
    // Check if the debug UART is initialized and the message is not NULL
  if ((debug_uart == NULL) || (message == NULL)) {
    return;
  }

    // Transmit the message over the debug UART
  HAL_UART_Transmit(
    debug_uart,
    (uint8_t *)message,
    (uint16_t)strlen(message),
    100
  );
}

/* Function definitions ------------------------------------------------------*/

// Store the debug UART handle and print boot messages
void App_Init(UART_HandleTypeDef *uart)
{
  // Set the debug UART handle
  debug_uart = uart;

  // Log the initialization message
  App_Log("App initialized\r\n");
  App_Log("[BOOT] STM32G0B1RE Firmware Debugging Lab\r\n");
  App_Log("[BOOT] Project: 00_base_bringup\r\n");
  App_Log("[BOOT] UART debug initialized\r\n");
  App_Log("[BOOT] Starting main loop\r\n");
}

// Toggle LED and log tick every 1 second
void App_Run(void)
{
  static uint32_t last_tick = 0;
  uint32_t now = HAL_GetTick();

  if ((now - last_tick) >= 1000U)
  {
    char log_buffer[64];

    last_tick = now;

    // Toggle the LED as a heartbeat indicator
    HAL_GPIO_TogglePin(APP_LED_GPIO_Port, APP_LED_Pin);

        // Set the log message
    sprintf(log_buffer, "[MAIN] tick=%lu\r\n", (unsigned long)now);

        // Log the message
    App_Log(log_buffer);
  }
}
