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
#include "scan_scheduler.h"
#include "usbd_composite.h"
#include "cdc_log.h"
#include "vendor_cmd.h"
#include "stm32g0xx_hal.h"
#include <stddef.h>

/* Private variables ---------------------------------------------------------*/
static volatile UsbHidTxState_t gHidTxState = USB_HID_TX_IDLE;

/* External variables --------------------------------------------------------*/
extern USBD_HandleTypeDef hUsbDeviceFS;
extern TIM_HandleTypeDef  htim6;

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

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (gHidTxState != USB_HID_TX_IDLE)
  {
    if (primask == 0U) { __enable_irq(); }
    return false;
  }

  gHidTxState = USB_HID_TX_BUSY;

  if (primask == 0U) { __enable_irq(); }

  if (USBD_COMPOSITE_HID_SendReport(&hUsbDeviceFS,
                                    (uint8_t *)report,
                                    USB_HID_KEYBOARD_REPORT_SIZE) != USBD_OK)
  {
    uint32_t pm2 = __get_PRIMASK();
    __disable_irq();
    gHidTxState = USB_HID_TX_IDLE;
    if (pm2 == 0U) { __enable_irq(); }
    return false;
  }

  return true;
}

// Called from usbd_hid.c DataIn; marks the endpoint as free for the next report
void UsbHidTransport_TxCpltCallback(void)
{
  gHidTxState = USB_HID_TX_IDLE;
}

// Forward TIM6 period-elapsed ticks to the scan scheduler
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    ScanScheduler_OnTimerTick();
  }
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
  ScanScheduler_Init();
  CdcLog_Init();
  VendorCmd_Init();
  HAL_TIM_Base_Start_IT(&htim6);
}

/*
 * Main loop handler.
 * Drain all pending 5 ms timer ticks by running KeyDetect_Run once per tick,
 * then call HidKeyboardConvert_Run unconditionally so reports are sent to the
 * USB host as soon as the IN endpoint becomes idle — without waiting for the
 * next scan tick.
 */
void HID_Keyboard_App(void)
{
  while (ScanScheduler_TakeRequest() != 0U)
  {
    KeyDetect_Run();
  }

  HidKeyboardConvert_Run();
  CdcLog_Run();
  VendorDump_Run(&hUsbDeviceFS);
  VendorCmd_UpdateLed();
}
