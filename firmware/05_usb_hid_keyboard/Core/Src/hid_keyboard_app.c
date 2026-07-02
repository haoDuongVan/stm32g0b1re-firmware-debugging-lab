/*
 * hid_keyboard_app.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "hid_keyboard_app.h"
#include "hid_keyboard_convert.h"
#include "key_event_queue.h"
#include "usbd_hid.h"
#include "stm32g0xx_hal.h"
#include <stddef.h>

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
 * Initialise all keyboard subsystems and push the M5 test events.
 * Called once from main after MX_USB_Device_Init.
 */
void HID_Keyboard_Init(void)
{
  KeyEvent_t event;

  UsbHidTransport_Init();
  KeyEventQueue_Init();
  HidKeyboardConvert_Init();

  /* M5 test: push key-down and key-release for keyLoc 4 ('a') */
  event.type   = KEY_EVENT_ON;
  event.keyLoc = 4U;
  (void)KeyEventQueue_Push(&event);

  event.type   = KEY_EVENT_OFF;
  event.keyLoc = 4U;
  (void)KeyEventQueue_Push(&event);
}

/*
 * Main loop handler.
 * State 0: wait 2 s for USB enumeration before draining the queue.
 * State 1: drain one event per call via HidKeyboardConvert_Run.
 * Replaced in M6 when matrix scan pushes real events.
 */
void HID_Keyboard_App(void)
{
  switch (testState) {
  case 0U:
    /* Give USB host 2 s to enumerate before sending the first report */
    if (HAL_GetTick() >= 2000U)
    {
      testTick  = HAL_GetTick();
      testState = 1U;
    }
    break;

  case 1U:
    HidKeyboardConvert_Run();
    break;

  default:
    break;
  }
}
