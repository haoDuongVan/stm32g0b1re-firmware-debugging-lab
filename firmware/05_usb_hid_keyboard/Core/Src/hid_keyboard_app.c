/*
 * hid_keyboard_app.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "hid_keyboard_app.h"
#include "stm32g0xx_hal.h"

/* Private defines -----------------------------------------------------------*/
#define HID_KEYBOARD_REPORT_SIZE  8U

/* Private variables ---------------------------------------------------------*/
static uint8_t  testState = 0U;
static uint32_t testTick  = 0U;

/* External variables --------------------------------------------------------*/
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Private function prototypes -----------------------------------------------*/
static void HID_Keyboard_SendReport(const uint8_t report[HID_KEYBOARD_REPORT_SIZE]);

/* Private functions ---------------------------------------------------------*/

// Send one 8-byte HID keyboard report to the USB host
static void HID_Keyboard_SendReport(const uint8_t report[HID_KEYBOARD_REPORT_SIZE])
{
  (void)USBD_HID_SendReport(&hUsbDeviceFS, (uint8_t *)report, HID_KEYBOARD_REPORT_SIZE);
}

/* Function definitions ------------------------------------------------------*/

// Drive the keyboard test sequence; call from the main loop
void HID_Keyboard_App(void)
{
  uint32_t now;
  uint8_t  report[HID_KEYBOARD_REPORT_SIZE];

  now = HAL_GetTick();

  switch (testState) {
  case 0U:
    /* Wait 2 s after power-on, then press 'A' (HID usage 0x04) */
    if (now >= 2000U) {
      report[0] = 0x00U;
      report[1] = 0x00U;
      report[2] = 0x04U;  /* keycode: A */
      report[3] = 0x00U;
      report[4] = 0x00U;
      report[5] = 0x00U;
      report[6] = 0x00U;
      report[7] = 0x00U;

      HID_Keyboard_SendReport(report);

      testTick  = now;
      testState = 1U;
    }
    break;

  case 1U:
    /* Release the key after 50 ms */
    if ((now - testTick) >= 50U) {
      report[0] = 0x00U;
      report[1] = 0x00U;
      report[2] = 0x00U;
      report[3] = 0x00U;
      report[4] = 0x00U;
      report[5] = 0x00U;
      report[6] = 0x00U;
      report[7] = 0x00U;

      HID_Keyboard_SendReport(report);

      testState = 2U;
    }
    break;

  default:
    break;
  }
}
