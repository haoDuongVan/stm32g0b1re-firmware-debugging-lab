/*
 * vendor_cmd.h
 *
 *  Created on: Jul 5, 2026
 *      Author: haodu
 *
 * EP0 vendor request handler.
 * All requests use bmRequestType = 0xC0 (control IN, device→host).
 * Parameters are carried in wValue / wIndex; firmware always returns a
 * response struct so the host knows immediately whether the command succeeded.
 *
 * Request table:
 *   bReq  Name               wValue          wIndex   Response
 *   0x01  GET_FIRMWARE_INFO  -               -        FirmwareInfo_t
 *   0x02  SET_REPEAT_ENABLE  0=off 1=on      -        VendorResponse_t
 *   0x03  SET_LED_MODE       0–3 (see enum)  -        VendorResponse_t
 *   0x04  START_RAM_DUMP     -               -        VendorDumpResponse_t
 */

#ifndef INC_VENDOR_CMD_H_
#define INC_VENDOR_CMD_H_

/* Includes ------------------------------------------------------------------*/
#include "usbd_ioreq.h"
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/

/* bRequest codes */
#define VENDOR_REQ_GET_FIRMWARE_INFO   0x01U
#define VENDOR_REQ_SET_REPEAT_ENABLE   0x02U
#define VENDOR_REQ_SET_LED_MODE        0x03U
#define VENDOR_REQ_START_RAM_DUMP      0x04U

/* Firmware feature flags reported in FirmwareInfo_t.featureFlags */
#define FW_FEATURE_HID_KEYBOARD        (1UL << 0)
#define FW_FEATURE_CDC_LOG             (1UL << 1)
#define FW_FEATURE_VENDOR_BULK         (1UL << 2)
#define FW_FEATURE_REPEAT_CONTROL      (1UL << 3)

/* Magic word embedded in FirmwareInfo_t to let the tool validate the response */
#define FW_INFO_MAGIC                  0x43363050UL  /* "P06C" little-endian */

/* Status codes returned in VendorResponse_t.status / VendorDumpResponse_t.status */
#define VENDOR_STATUS_OK               0U
#define VENDOR_STATUS_ERROR            1U

/* Exported types ------------------------------------------------------------*/

typedef enum
{
  LED_MODE_OFF        = 0U,
  LED_MODE_ON         = 1U,
  LED_MODE_BLINK_SLOW = 2U,
  LED_MODE_BLINK_FAST = 3U,
} VendorLedMode_t;

/*
 * Common response for SET commands (SET_REPEAT_ENABLE, SET_LED_MODE).
 * status: 0 = success, 1 = failure.
 * request: echoed bRequest so the host can match response to command.
 */
typedef struct __attribute__((packed))
{
  uint8_t status;
  uint8_t request;
} VendorResponse_t;

/*
 * Response for START_RAM_DUMP.
 * On success, acceptedLength tells the host exactly how many bytes to read
 * from the Vendor Bulk IN endpoint.  On failure, acceptedLength = 0.
 */
typedef struct __attribute__((packed))
{
  uint8_t  status;
  uint8_t  request;
  uint32_t acceptedLength;
} VendorDumpResponse_t;

/*
 * Response for GET_FIRMWARE_INFO - fixed-size struct, no length prefix.
 * The host validates the response by checking magic == FW_INFO_MAGIC.
 */
typedef struct __attribute__((packed))
{
  uint32_t magic;               /* FW_INFO_MAGIC */
  uint16_t versionMajor;
  uint16_t versionMinor;
  uint32_t featureFlags;        /* FW_FEATURE_* bitmask */
  uint8_t  hidInterface;        /* interface number: 0 */
  uint8_t  cdcControlInterface; /* interface number: 1 */
  uint8_t  cdcDataInterface;    /* interface number: 2 */
  uint8_t  vendorInterface;     /* interface number: 3 */
  uint8_t  hidInEp;             /* 0x81 */
  uint8_t  cdcLogInEp;          /* 0x83 */
  uint8_t  vendorBulkInEp;      /* 0x84 */
  uint8_t  reserved;
} FirmwareInfo_t;

/* Exported functions --------------------------------------------------------*/

/* Reset state to defaults and drive LED off; call once from HID_Keyboard_Init */
void VendorCmd_Init(void);

/* Return true when key repeat events should be forwarded to HID */
bool VendorCmd_GetRepeatEnable(void);

/* Return the currently active LED mode */
VendorLedMode_t VendorCmd_GetLedMode(void);

/*
 * Drive the LED pin to match the current LED mode.
 * BLINK_SLOW toggles every 500 ms, BLINK_FAST every 125 ms.
 * Call from the main loop at high rate; idempotent for OFF/ON.
 */
void VendorCmd_UpdateLed(void);

/*
 * Flush any pending CDC log event that was deferred by VendorCmd_HandleSetup.
 * Must be called from the main loop - not ISR-safe.
 */
void VendorCmd_FlushPendingLog(void);

/*
 * Handle one EP0 vendor Setup request.
 * Runs in USB stack context. Does not call CdcLog_Printf directly;
 * sets a pending log flag for VendorCmd_FlushPendingLog to consume.
 * Returns USBD_OK, or USBD_FAIL after calling USBD_CtlError for unknown requests.
 */
uint8_t VendorCmd_HandleSetup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

/*
 * Called from Composite_DataIn (ISR) when EP4 IN completes a transfer.
 * Queues the next chunk immediately to keep the bulk pipeline full.
 * Does not touch CDC log - ISR-safe.
 */
void VendorDump_OnTxCplt(USBD_HandleTypeDef *pdev);

/*
 * Called every iteration of HID_Keyboard_App (main loop).
 * Kicks the first chunk after START_RAM_DUMP arms the dump, and logs
 * completion once the ISR signals gDumpDone.
 */
void VendorDump_Run(USBD_HandleTypeDef *pdev);

#endif /* INC_VENDOR_CMD_H_ */
