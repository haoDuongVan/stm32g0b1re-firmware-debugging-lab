/*
 * vendor_cmd.h
 *
 *  Created on: Jul 5, 2026
 *      Author: haodu
 *
 * EP0 vendor request handler.
 * All requests use bmRequestType = 0xC0 (GET, device→host)
 * or 0x40 (SET, host→device) with recipient = Device.
 *
 * Request table:
 *   0x01  GET_FIRMWARE_INFO  GET  → 16-byte VendorFwInfo_t
 *   0x02  SET_REPEAT_ENABLE  SET  wValue: 0 = disable, 1 = enable key repeat
 *   0x03  SET_LED_MODE       SET  wValue: 0 = off, 1 = on, 2 = blink
 *   0x04  START_RAM_DUMP     GET  → 8-byte VendorRamDumpInfo_t (addr + size)
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

/* Exported types ------------------------------------------------------------*/

typedef enum
{
  LED_MODE_OFF   = 0U,
  LED_MODE_ON    = 1U,
  LED_MODE_BLINK = 2U,
} VendorLedMode_t;

/* GET_FIRMWARE_INFO response — 16 bytes */
typedef struct __attribute__((packed))
{
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint8_t reserved;
  char    name[12];   /* null-padded ASCII, e.g. "CompositeKbd" */
} VendorFwInfo_t;

/* START_RAM_DUMP response — 8 bytes */
typedef struct __attribute__((packed))
{
  uint32_t addr;
  uint32_t size;
} VendorRamDumpInfo_t;

/* Exported functions --------------------------------------------------------*/

// Reset state to defaults and drive LED off; call once from HID_Keyboard_Init
void VendorCmd_Init(void);

// Return true when key repeat events should be forwarded to HID
bool VendorCmd_GetRepeatEnable(void);

// Return the currently active LED mode
VendorLedMode_t VendorCmd_GetLedMode(void);

/*
 * Drive the LED pin to match the current LED mode.
 * BLINK: toggles every 250 ms; call from the main loop at high rate.
 * OFF / ON: sets the pin immediately; idempotent on repeated calls.
 */
void VendorCmd_UpdateLed(void);

/*
 * Handle one EP0 vendor Setup request.
 * Returns USBD_OK, or USBD_FAIL after calling USBD_CtlError for unknown requests.
 */
uint8_t VendorCmd_HandleSetup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

#endif /* INC_VENDOR_CMD_H_ */
