/*
 * hid_keyboard_app.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "hid_keyboard_app.h"
#include "hid_keyboard_report.h"
#include "key_table.h"
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
 * One-shot test: wait 2 s, send key 'a' (keyLoc = 4) via key table, release after 50 ms.
 * State transitions are gated on SendReport returning true so no report is dropped
 * if the endpoint is still busy. Replaced in M5 with ring buffer drainer.
 */
void HID_Keyboard_App(void)
{
  uint32_t               now;
  HidKeyboardReport_t    report;
  const KeyTableEntry_t *entry;

  now = HAL_GetTick();

  switch (testState) {
  case 0U:
    /* Wait 2 s after power-on, then look up keyLoc 4 → 'a' and send key-down */
    if (now >= 2000U) {
      entry = KeyTable_Get(4U);

      if ((entry != NULL) && (entry->kind == KEY_KIND_NORMAL)) {
        HidKeyboardReport_SetKey(&report, entry->modifier, entry->usage);

        if (UsbHidTransport_SendReport(HidKeyboardReport_GetData(&report))) {
          testTick  = now;
          testState = 1U;
        }
      }
    }
    break;

  case 1U:
    /* Release all keys after 50 ms */
    if ((now - testTick) >= 50U) {
      HidKeyboardReport_Clear(&report);

      if (UsbHidTransport_SendReport(HidKeyboardReport_GetData(&report))) {
        testState = 2U;
      }
    }
    break;

  default:
    break;
  }
}
