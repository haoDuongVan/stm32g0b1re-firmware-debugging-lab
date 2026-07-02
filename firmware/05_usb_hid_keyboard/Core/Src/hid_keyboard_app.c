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
#include "matrix_scan.h"
#include "usbd_hid.h"
#include "stm32g0xx_hal.h"
#include <stddef.h>

/* Private variables ---------------------------------------------------------*/
static volatile UsbHidTxState_t gHidTxState = USB_HID_TX_IDLE;

static uint8_t  testState = 0U;
static uint8_t  testSent  = 0U;  /* guard: ON+OFF events pushed for current press */

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
}

/*
 * Main loop handler — M6 raw matrix scan test.
 * State 0: wait 2 s for USB enumeration.
 * State 1: scan keypad; track keyLoc 4 ('a') press/release; convert and send.
 */
void HID_Keyboard_App(void)
{
  uint16_t   rawState;
  KeyEvent_t event;

  switch (testState) {
  case 0U:
    /* Give USB host 2 s to enumerate before sending the first report */
    if (HAL_GetTick() >= 2000U)
    {
      testState = 1U;
    }
    break;

  case 1U:
    rawState = MatrixScan_ReadRaw();

    /* Watch keyLoc 4 (row 1, col 0 → bit 4) */
    if ((rawState & (1U << 4U)) != 0U)
    {
      /* Key is held down — push ON+OFF once per press */
      if (testSent == 0U)
      {
        event.type   = KEY_EVENT_ON;
        event.keyLoc = 4U;
        (void)KeyEventQueue_Push(&event);

        event.type   = KEY_EVENT_OFF;
        event.keyLoc = 4U;
        (void)KeyEventQueue_Push(&event);

        testSent = 1U;
      }
    }
    else
    {
      /* Key released — allow the next press to generate events */
      testSent = 0U;
    }

    HidKeyboardConvert_Run();

    HAL_Delay(5U);  /* ~5 ms pacing between scans */
    break;

  default:
    break;
  }
}
