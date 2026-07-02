/*
 * hid_keyboard_app.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "hid_keyboard_app.h"
#include "usbd_hid.h"
#include "stm32g0xx_hal.h"

/* Private variables ---------------------------------------------------------*/
static volatile UsbHidTxState_t gHidTxState = USB_HID_TX_IDLE;

static uint8_t  testState = 0U;
static uint32_t testTick  = 0U;

/* External variables --------------------------------------------------------*/
extern USBD_HandleTypeDef hUsbDeviceFS;

/* Function definitions ------------------------------------------------------*/

// Reset TX state to IDLE; call once after USB device init
void UsbHidTransport_Init(void)
{
  gHidTxState = USB_HID_TX_IDLE;
}

// Return true if the HID IN endpoint is ready to accept a new report
bool UsbHidTransport_IsIdle(void)
{
  return (gHidTxState == USB_HID_TX_IDLE);
}

/*
 * Send one 8-byte HID keyboard report.
 * Returns true  — report was accepted and queued for transfer.
 * Returns false — USB not configured, or a previous transfer is still in progress.
 *
 * State transition uses a critical section (__disable_irq / __enable_irq) so
 * the check-and-set is atomic on Cortex-M0+ (no BASEPRI register available).
 */
bool UsbHidTransport_SendReport(const uint8_t report[USB_HID_KEYBOARD_REPORT_SIZE])
{
  if (report == NULL)
  {
    return false;
  }

  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
  {
    return false;
  }

  __disable_irq();

  if (gHidTxState != USB_HID_TX_IDLE)
  {
    __enable_irq();
    return false;
  }

  gHidTxState = USB_HID_TX_BUSY;

  __enable_irq();

  if (USBD_HID_SendReport(&hUsbDeviceFS,
                          (uint8_t *)report,
                          USB_HID_KEYBOARD_REPORT_SIZE) != USBD_OK)
  {
    __disable_irq();
    gHidTxState = USB_HID_TX_IDLE;
    __enable_irq();
    return false;
  }

  return true;
}

// Called from usbd_hid.c DataIn; marks the endpoint as free for the next report
void UsbHidTransport_TxCpltCallback(void)
{
  gHidTxState = USB_HID_TX_IDLE;
}

/*
 * One-shot keyboard test sequence driven from the main loop.
 * State 0: wait 2 s, send key-down for 'A' (HID usage 0x04).
 * State 1: hold for 50 ms, send key-release (all zeros).
 * State 2: done — stays idle.
 *
 * State transitions are gated on SendReport returning true so no report is
 * lost if the endpoint is still busy from the previous call.
 */
void HID_Keyboard_App(void)
{
  uint32_t now;
  uint8_t  report[USB_HID_KEYBOARD_REPORT_SIZE];

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

      if (UsbHidTransport_SendReport(report)) {
        testTick  = now;
        testState = 1U;
      }
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

      if (UsbHidTransport_SendReport(report)) {
        testState = 2U;
      }
    }
    break;

  default:
    break;
  }
}
