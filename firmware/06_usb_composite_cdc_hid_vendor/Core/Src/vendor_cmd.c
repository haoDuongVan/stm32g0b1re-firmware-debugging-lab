/*
 * vendor_cmd.c
 *
 *  Created on: Jul 5, 2026
 *      Author: haodu
 *
 * EP0 vendor request handler.
 * All commands use control IN (bmRequestType = 0xC0) and always return a
 * response struct so the host has an explicit success/failure signal.
 */

/* Includes ------------------------------------------------------------------*/
#include "vendor_cmd.h"
#include "usbd_composite.h"
#include "cdc_log.h"
#include "main.h"
#include "usbd_ctlreq.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define FW_VERSION_MAJOR   0U
#define FW_VERSION_MINOR   6U

/* SRAM1 base and maximum dumpable size (144 KB) */
#define RAM_DUMP_BASE      0x20000000UL
#define RAM_DUMP_MAX_SIZE  (144UL * 1024UL)

/* LED blink cadences */
#define LED_BLINK_SLOW_MS  500U
#define LED_BLINK_FAST_MS  125U

/* Private variables ---------------------------------------------------------*/
static bool            gRepeatEnable;
static VendorLedMode_t gLedMode;

/*
 * Static response buffers - must outlive the EP0 IN transfer, so they cannot
 * be stack-allocated.  Each command writes its response here before calling
 * USBD_CtlSendData.
 */
static FirmwareInfo_t       gFwInfo;
static VendorResponse_t     gVendorRsp;
static VendorDumpResponse_t gDumpRsp;

/*
 * RAM dump state machine.
 * gDumpActive  - set by START_RAM_DUMP, cleared once all bytes are sent.
 * gDumpAddr    - base address of the region being streamed.
 * gDumpTotal   - total bytes to send (validated acceptedLength).
 * gDumpOffset  - bytes queued so far; advanced in the DataIn callback.
 * gDumpDone    - set from ISR when the last chunk completes; cleared and
 *                logged by VendorDump_Run in the main loop.
 */
static bool           gDumpActive;
static uint32_t       gDumpAddr;
static uint32_t       gDumpTotal;
static uint32_t       gDumpOffset;
static volatile bool  gDumpDone;

/* Function definitions ------------------------------------------------------*/

void VendorCmd_Init(void)
{
  gRepeatEnable = true;
  gLedMode      = LED_MODE_OFF;
  gDumpActive   = false;
  gDumpAddr     = 0U;
  gDumpTotal    = 0U;
  gDumpOffset   = 0U;
  gDumpDone     = false;

  (void)memset(&gFwInfo, 0, sizeof(gFwInfo));
  gFwInfo.magic               = FW_INFO_MAGIC;
  gFwInfo.versionMajor        = FW_VERSION_MAJOR;
  gFwInfo.versionMinor        = FW_VERSION_MINOR;
  gFwInfo.featureFlags        = FW_FEATURE_HID_KEYBOARD
                              | FW_FEATURE_CDC_LOG
                              | FW_FEATURE_VENDOR_BULK
                              | FW_FEATURE_REPEAT_CONTROL;
  gFwInfo.hidInterface        = 0U;
  gFwInfo.cdcControlInterface = 1U;
  gFwInfo.cdcDataInterface    = 2U;
  gFwInfo.vendorInterface     = 3U;
  gFwInfo.hidInEp             = COMP_HID_EPIN_ADDR;
  gFwInfo.cdcLogInEp          = COMP_CDC_DATA_IN_EP_ADDR;
  gFwInfo.vendorBulkInEp      = COMP_VENDOR_DATA_IN_EP_ADDR;

  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);

  CdcLog_Printf("[BOOT] Project06 composite firmware started\r\n");
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
 * BLINK_SLOW toggles every 500 ms, BLINK_FAST every 125 ms.
 * OFF/ON write the pin immediately; idempotent on repeated calls.
 */
void VendorCmd_UpdateLed(void)
{
  static uint32_t gLastToggleTick = 0U;
  uint32_t period = 0U;

  switch (gLedMode)
  {
    case LED_MODE_OFF:
      HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
      return;

    case LED_MODE_ON:
      HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
      return;

    case LED_MODE_BLINK_SLOW:
      period = LED_BLINK_SLOW_MS;
      break;

    case LED_MODE_BLINK_FAST:
      period = LED_BLINK_FAST_MS;
      break;

    default:
      return;
  }

  uint32_t now = HAL_GetTick();
  if ((now - gLastToggleTick) >= period)
  {
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    gLastToggleTick = now;
  }
}

/*
 * Dispatch one EP0 vendor Setup request.
 * All commands return a response struct via USBD_CtlSendData.
 * Returns USBD_OK, or USBD_FAIL after USBD_CtlError for unknown requests.
 */
uint8_t VendorCmd_HandleSetup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  uint16_t len;

  switch (req->bRequest)
  {
    /* ------------------------------------------------------------------ */
    case VENDOR_REQ_GET_FIRMWARE_INFO:
      len = (uint16_t)MIN(sizeof(gFwInfo), req->wLength);
      (void)USBD_CtlSendData(pdev, (uint8_t *)&gFwInfo, len);
      CdcLog_Printf("[VREQ] GET_FIRMWARE_INFO SUCCESS\r\n");
      break;

    /* ------------------------------------------------------------------ */
    case VENDOR_REQ_SET_REPEAT_ENABLE:
      gRepeatEnable          = (req->wValue != 0U);
      gVendorRsp.status      = VENDOR_STATUS_OK;
      gVendorRsp.request     = VENDOR_REQ_SET_REPEAT_ENABLE;
      len = (uint16_t)MIN(sizeof(gVendorRsp), req->wLength);
      (void)USBD_CtlSendData(pdev, (uint8_t *)&gVendorRsp, len);
      CdcLog_Printf("[VREQ] SET_REPEAT_ENABLE value=%u SUCCESS\r\n",
                    (unsigned int)gRepeatEnable);
      break;

    /* ------------------------------------------------------------------ */
    case VENDOR_REQ_SET_LED_MODE:
      if (req->wValue > (uint16_t)LED_MODE_BLINK_FAST)
      {
        gVendorRsp.status  = VENDOR_STATUS_ERROR;
        gVendorRsp.request = VENDOR_REQ_SET_LED_MODE;
        len = (uint16_t)MIN(sizeof(gVendorRsp), req->wLength);
        (void)USBD_CtlSendData(pdev, (uint8_t *)&gVendorRsp, len);
        CdcLog_Printf("[VREQ] SET_LED_MODE mode=%u FAILURE\r\n",
                      (unsigned int)req->wValue);
        break;
      }
      gLedMode               = (VendorLedMode_t)req->wValue;
      gVendorRsp.status      = VENDOR_STATUS_OK;
      gVendorRsp.request     = VENDOR_REQ_SET_LED_MODE;
      len = (uint16_t)MIN(sizeof(gVendorRsp), req->wLength);
      (void)USBD_CtlSendData(pdev, (uint8_t *)&gVendorRsp, len);
      CdcLog_Printf("[VREQ] SET_LED_MODE mode=%u SUCCESS\r\n",
                    (unsigned int)gLedMode);
      break;

    /* ------------------------------------------------------------------ */
    case VENDOR_REQ_START_RAM_DUMP:
    {
      if (gDumpActive)
      {
        gDumpRsp.status         = VENDOR_STATUS_ERROR;
        gDumpRsp.request        = VENDOR_REQ_START_RAM_DUMP;
        gDumpRsp.acceptedLength = 0U;
        len = (uint16_t)MIN(sizeof(gDumpRsp), req->wLength);
        (void)USBD_CtlSendData(pdev, (uint8_t *)&gDumpRsp, len);
        CdcLog_Printf("[VREQ] START_RAM_DUMP FAILURE (busy)\r\n");
        break;
      }

      gDumpAddr               = RAM_DUMP_BASE;
      gDumpTotal              = RAM_DUMP_MAX_SIZE;
      gDumpRsp.status         = VENDOR_STATUS_OK;
      gDumpRsp.request        = VENDOR_REQ_START_RAM_DUMP;
      gDumpRsp.acceptedLength = RAM_DUMP_MAX_SIZE;
      len = (uint16_t)MIN(sizeof(gDumpRsp), req->wLength);
      (void)USBD_CtlSendData(pdev, (uint8_t *)&gDumpRsp, len);

      /* Arm the bulk stream - VendorDump_Run kicks the first chunk */
      gDumpOffset = 0U;
      gDumpActive = true;

      CdcLog_Printf("[VREQ] START_RAM_DUMP len=%lu SUCCESS\r\n",
                    (unsigned long)RAM_DUMP_MAX_SIZE);
      break;
    }

    /* ------------------------------------------------------------------ */
    default:
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
  }

  return (uint8_t)USBD_OK;
}

/* Private helper - queue one 64-byte chunk; called from ISR or main loop */
static void VendorDump_SendNextChunk(USBD_HandleTypeDef *pdev)
{
  uint32_t remaining = gDumpTotal - gDumpOffset;
  uint16_t chunkLen  = (remaining > COMP_VENDOR_DATA_EP_SIZE)
                       ? (uint16_t)COMP_VENDOR_DATA_EP_SIZE
                       : (uint16_t)remaining;

  const uint8_t *src = (const uint8_t *)(uintptr_t)(gDumpAddr + gDumpOffset);

  if (USBD_COMPOSITE_VENDOR_Transmit(pdev, src, chunkLen) == (uint8_t)USBD_OK)
  {
    gDumpOffset += chunkLen;

    if (gDumpOffset >= gDumpTotal)
    {
      gDumpActive = false;
      gDumpDone   = true;  /* signal main loop - CDC log not safe from ISR */
    }
  }
}

/*
 * Called from Composite_DataIn (ISR) when EP4 IN transfer completes.
 * Immediately queues the next chunk to keep the pipeline full.
 */
void VendorDump_OnTxCplt(USBD_HandleTypeDef *pdev)
{
  if (gDumpActive)
  {
    VendorDump_SendNextChunk(pdev);
  }
}

/*
 * Called every main loop iteration.
 * Kicks the first chunk when a dump is freshly armed (gDumpOffset == 0),
 * and logs completion once the ISR signals gDumpDone.
 */
void VendorDump_Run(USBD_HandleTypeDef *pdev)
{
  if (gDumpDone)
  {
    gDumpDone = false;
    CdcLog_Printf("[BULK] RAM dump complete len=%lu\r\n",
                  (unsigned long)gDumpTotal);
    return;
  }

  if (gDumpActive && (gDumpOffset == 0U) && USBD_COMPOSITE_VENDOR_IsTxIdle(pdev))
  {
    CdcLog_Printf("[BULK] RAM dump started len=%lu\r\n",
                  (unsigned long)gDumpTotal);
    VendorDump_SendNextChunk(pdev);
  }
}
