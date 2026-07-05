/*
 * vendor_cmd.c
 *
 *  Created on: Jul 5, 2026
 *      Author: haodu
 *
 * EP0 vendor request handler.
 * Owns: firmware version string, key-repeat enable flag, LED mode.
 */

/* Includes ------------------------------------------------------------------*/
#include "vendor_cmd.h"
#include "cdc_log.h"
#include "main.h"
#include "usbd_ctlreq.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define FW_MAJOR  0U
#define FW_MINOR  6U
#define FW_PATCH  0U

/* SRAM1 region exposed as RAM dump target (144 KB) */
#define RAM_DUMP_ADDR  0x20000000UL
#define RAM_DUMP_SIZE  (144UL * 1024UL)

/* LED blink cadence */
#define LED_BLINK_PERIOD_MS  250U

/* Private variables ---------------------------------------------------------*/
static bool            gRepeatEnable;
static VendorLedMode_t gLedMode;

/*
 * Static response buffers: must stay alive while the EP0 IN transfer is in
 * progress, so they cannot be on the stack.
 */
static VendorFwInfo_t      gFwInfo;
static VendorRamDumpInfo_t gRamDumpInfo;

/* Function definitions ------------------------------------------------------*/

void VendorCmd_Init(void)
{
  gRepeatEnable = true;
  gLedMode      = LED_MODE_OFF;

  (void)memset(&gFwInfo, 0, sizeof(gFwInfo));
  gFwInfo.major = FW_MAJOR;
  gFwInfo.minor = FW_MINOR;
  gFwInfo.patch = FW_PATCH;
  (void)strncpy(gFwInfo.name, "CompositeKB", sizeof(gFwInfo.name));

  gRamDumpInfo.addr = RAM_DUMP_ADDR;
  gRamDumpInfo.size = RAM_DUMP_SIZE;

  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
}

bool VendorCmd_GetRepeatEnable(void)
{
  return gRepeatEnable;
}

VendorLedMode_t VendorCmd_GetLedMode(void)
{
  return gLedMode;
}

/*
 * Drive LED_GREEN to match gLedMode.
 * BLINK: toggles the pin when the 250 ms window elapses.
 * OFF/ON: writes the pin and returns immediately.
 */
void VendorCmd_UpdateLed(void)
{
  static uint32_t gLastToggleTick = 0U;

  switch (gLedMode)
  {
    case LED_MODE_OFF:
      HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
      break;

    case LED_MODE_ON:
      HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
      break;

    case LED_MODE_BLINK:
      uint32_t now = HAL_GetTick();

      if ((now - gLastToggleTick) >= LED_BLINK_PERIOD_MS)
      {
        HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
        gLastToggleTick = now;
      }
      break;

    default:
      break;
  }
}

/*
 * Dispatch one EP0 vendor Setup request.
 * GET requests send a response buffer via USBD_CtlSendData.
 * SET requests read their argument from req->wValue (wLength = 0).
 */
uint8_t VendorCmd_HandleSetup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  uint16_t len = 0U;
  const uint8_t *pbuf = NULL;

  switch (req->bRequest)
  {
    case VENDOR_REQ_GET_FIRMWARE_INFO:
      len = (uint16_t)MIN(sizeof(gFwInfo), req->wLength);
      pbuf = (const uint8_t *)&gFwInfo;
      (void)USBD_CtlSendData(pdev, (uint8_t *)pbuf, len);
      break;

    case VENDOR_REQ_SET_REPEAT_ENABLE:
      gRepeatEnable = (req->wValue != 0U);
      CdcLog_Printf("[VND] repeat=%u\r\n", (unsigned int)gRepeatEnable);
      break;

    case VENDOR_REQ_SET_LED_MODE:
      if (req->wValue <= (uint16_t)LED_MODE_BLINK)
      {
        gLedMode = (VendorLedMode_t)req->wValue;
        VendorCmd_UpdateLed();
        CdcLog_Printf("[VND] led_mode=%u\r\n", (unsigned int)gLedMode);
      }
      else
      {
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
      }
      break;

    case VENDOR_REQ_START_RAM_DUMP:
      len = (uint16_t)MIN(sizeof(gRamDumpInfo), req->wLength);
      pbuf = (const uint8_t *)&gRamDumpInfo;
      (void)USBD_CtlSendData(pdev, (uint8_t *)pbuf, len);
      CdcLog_Printf("[VND] ram_dump addr=0x%08lX size=%lu\r\n",
                    (unsigned long)gRamDumpInfo.addr,
                    (unsigned long)gRamDumpInfo.size);
      break;

    default:
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
  }

  return (uint8_t)USBD_OK;
}
