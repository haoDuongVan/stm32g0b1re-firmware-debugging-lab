/*
 * hid_keyboard_app.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "hid_keyboard_app.h"
#include "hid_keyboard_convert.h"
#include "key_detect.h"
#include "key_event_queue.h"
#include "matrix_scan.h"
#include "usbd_hid.h"
#include "stm32g0xx_hal.h"
#include <stddef.h>

/* Private variables ---------------------------------------------------------*/
static volatile UsbHidTxState_t gHidTxState = USB_HID_TX_IDLE;

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
 * Initialise all keyboard subsystems.
 * Called once from main after MX_USB_Device_Init.
 */
void HID_Keyboard_Init(void)
{
  UsbHidTransport_Init();
  KeyEventQueue_Init();
  HidKeyboardConvert_Init();
  MatrixScan_Init();
  KeyDetect_Init();
}

/*
 * Main loop handler.
 * Scan the matrix, debounce, push events, then drain one event per call.
 * HAL_Delay(5) paces the scan to ~5 ms per iteration; 2-buffer debounce
 * therefore requires 2 consecutive stable reads (~10 ms) before a key
 * on/off is accepted.
 */
void HID_Keyboard_App(void)
{
  KeyDetect_Run();
  HidKeyboardConvert_Run();
  HAL_Delay(5U);
}
