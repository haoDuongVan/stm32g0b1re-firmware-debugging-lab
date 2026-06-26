/*
 * bl_main.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "bl_main.h"

#include "bl_flash_layout.h"
#include "bl_log.h"
#include "main.h"

/* Private defines -----------------------------------------------------------*/

// Define LED for heartbeat
#define BL_LED_GPIO_Port                 GPIOA
#define BL_LED_Pin                       GPIO_PIN_5

// Define heartbeat period
#define BL_HEARTBEAT_INTERVAL_MS         1000U   // ms

/* Private variables ---------------------------------------------------------*/
static uint8_t boot_check_done = 0U;

/* Private functions ---------------------------------------------------------*/

// Verify that all Flash layout addresses compile to the expected values; returns 1 if OK
static int BlMain_CheckFlashLayout(void)
{
  if (BL_BOOT_BASE_ADDR != 0x08000000UL) {
    return 0;
  }

  if (BL_BOOT_END_ADDR != 0x0800FFFFUL) {
    return 0;
  }

  if (BL_SLOT_A_BASE_ADDR != 0x08010000UL) {
    return 0;
  }

  if (BL_SLOT_A_END_ADDR != 0x0803FFFFUL) {
    return 0;
  }

  if (BL_SLOT_B_BASE_ADDR != 0x08040000UL) {
    return 0;
  }

  if (BL_SLOT_B_END_ADDR != 0x0806FFFFUL) {
    return 0;
  }

  if (BL_METADATA0_BASE_ADDR != 0x0807F000UL) {
    return 0;
  }

  if (BL_METADATA1_BASE_ADDR != 0x0807F800UL) {
    return 0;
  }

  if (BL_METADATA1_END_ADDR != 0x0807FFFFUL) {
    return 0;
  }

  return 1;
}

// Print project and board information
static void BlMain_PrintBootLog(void)
{
  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] STM32 Dual-Slot UART Bootloader V1\r\n");
  BlLog_Printf("[BOOT] Project: 04_uart_bootloader\r\n");
  BlLog_Printf("[BOOT] Board: NUCLEO-G0B1RE\r\n");
  BlLog_Printf("[BOOT] System clock: %lu Hz\r\n",
                (unsigned long)HAL_RCC_GetSysClockFreq());
}

// Print Flash layout addresses and sizes
static void BlMain_PrintFlashLayout(void)
{
  BlLog_Printf("[BOOT] boot_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_BOOT_BASE_ADDR,
                (unsigned long)(BL_BOOT_SIZE / 1024UL));

  BlLog_Printf("[BOOT] slot_a_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_SLOT_A_BASE_ADDR,
                (unsigned long)(BL_SLOT_SIZE / 1024UL));

  BlLog_Printf("[BOOT] slot_b_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_SLOT_B_BASE_ADDR,
                (unsigned long)(BL_SLOT_SIZE / 1024UL));

  BlLog_Printf("[BOOT] metadata0=0x%08lX\r\n",
                (unsigned long)BL_METADATA0_BASE_ADDR);

  BlLog_Printf("[BOOT] metadata1=0x%08lX\r\n",
                (unsigned long)BL_METADATA1_BASE_ADDR);
}

// Check VTOR and Flash layout once after boot; runs only on first call
static void BlMain_RunBootCheck(void)
{
  uint32_t expected_vtor;
  uint32_t actual_vtor;

  // Run boot check only once after boot
  if (boot_check_done != 0U) {
    return;
  }

  boot_check_done = 1U;

  expected_vtor = BL_BOOT_BASE_ADDR;
  actual_vtor = SCB->VTOR;

  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] expected_vtor=0x%08lX\r\n",
                (unsigned long)expected_vtor);
  BlLog_Printf("[BOOT] actual_vtor=0x%08lX\r\n",
                (unsigned long)actual_vtor);

  if (actual_vtor == expected_vtor) {
    BlLog_Printf("[BOOT] vtor_check=OK\r\n");
    BlLog_Printf("[TEST0] bootloader_boot_check PASS\r\n");
  } else {
    BlLog_Printf("[BOOT] vtor_check=NG\r\n");
    BlLog_Printf("[TEST0] bootloader_boot_check FAIL\r\n");
  }

  if (BlMain_CheckFlashLayout() != 0) {
    BlLog_Printf("[TEST1] flash_layout_check PASS\r\n");
  } else {
    BlLog_Printf("[TEST1] flash_layout_check FAIL\r\n");
  }
}

/* Function definitions ------------------------------------------------------*/

// Set VTOR to the bootloader base address before the scheduler or any interrupt fires
void BlMain_ApplyVectorTable(void)
{
  /*
   * The bootloader always starts from the beginning of Flash.
   *
   * Set VTOR explicitly so the interrupt vector table is known even if
   * generated SystemInit() does not relocate it.
   */
  SCB->VTOR = BL_BOOT_BASE_ADDR;
  __DSB();
  __ISB();
}

// Initialize the bootloader log and run the boot self-check
void BlMain_Init(UART_HandleTypeDef *debug_uart)
{
  BlLog_Init(debug_uart);

  BlMain_PrintBootLog();
  BlMain_PrintFlashLayout();
  BlMain_RunBootCheck();
}

// Toggle the LED at a fixed interval as a heartbeat indicator
void BlMain_Run(void)
{
  // Persists across calls to track when the LED was toggled last time
  static uint32_t last_tick = 0U;
  uint32_t now = HAL_GetTick();

  // uint32_t subtraction is wrap-safe, so this works correctly after HAL_GetTick() overflows
  if ((now - last_tick) >= BL_HEARTBEAT_INTERVAL_MS) {
    last_tick = now;

    // Toggle LED as a heartbeat indicator
    HAL_GPIO_TogglePin(BL_LED_GPIO_Port, BL_LED_Pin);
  }
}
