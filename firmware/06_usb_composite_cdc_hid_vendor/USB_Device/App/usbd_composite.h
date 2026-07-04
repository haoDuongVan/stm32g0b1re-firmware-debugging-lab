/*
 * usbd_composite.h
 *
 *  Created on: Jul 3, 2026
 *      Author: haodu
 *
 * Composite USB class: HID Keyboard (Interface 0) + CDC ACM (Interfaces 1-2).
 *
 * Endpoint map:
 *   EP1 IN  0x81  HID Keyboard Interrupt IN   8 bytes  10 ms
 *   EP2 IN  0x82  CDC Notification  IN        8 bytes  16 ms
 *   EP3 OUT 0x03  CDC Data OUT               64 bytes
 *   EP3 IN  0x83  CDC Data IN                64 bytes
 */

#ifndef USB_DEVICE_APP_USBD_COMPOSITE_H_
#define USB_DEVICE_APP_USBD_COMPOSITE_H_

/* Includes ------------------------------------------------------------------*/
#include "usbd_ioreq.h"
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/

/* Configuration descriptor total length */
#define USBD_COMPOSITE_CFG_DESC_SIZE   100U

/* HID sub-class */
#define COMP_HID_EPIN_ADDR             0x81U
#define COMP_HID_EPIN_SIZE             0x08U
#define COMP_HID_FS_BINTERVAL          0x0AU   /* 10 ms */

/* CDC sub-class */
#define COMP_CDC_CMD_EPIN_ADDR         0x82U
#define COMP_CDC_CMD_EPIN_SIZE         0x08U
#define COMP_CDC_CMD_FS_BINTERVAL      0x10U   /* 16 ms */

#define COMP_CDC_DATA_OUT_EP_ADDR      0x03U
#define COMP_CDC_DATA_IN_EP_ADDR       0x83U
#define COMP_CDC_DATA_EP_SIZE          0x40U   /* 64 bytes */

/* CDC ACM class requests */
#define CDC_SEND_ENCAPSULATED_COMMAND  0x00U
#define CDC_GET_ENCAPSULATED_RESPONSE  0x01U
#define CDC_SET_COMM_FEATURE           0x02U
#define CDC_GET_COMM_FEATURE           0x03U
#define CDC_CLEAR_COMM_FEATURE         0x04U
#define CDC_SET_LINE_CODING            0x20U
#define CDC_GET_LINE_CODING            0x21U
#define CDC_SET_CONTROL_LINE_STATE     0x22U
#define CDC_SEND_BREAK                 0x23U

/* CDC line coding structure size */
#define CDC_LINE_CODING_SIZE           7U

/* Exported types ------------------------------------------------------------*/

/* CDC line coding (baud, stop bits, parity, data bits) */
typedef struct
{
  uint32_t baudRate;  /* Baud rate, e.g. 115200 */
  uint8_t  stopBits;  /* 0=1 stop bit, 1=1.5, 2=2 */
  uint8_t  parity;    /* 0=none, 1=odd, 2=even, 3=mark, 4=space */
  uint8_t  dataBits;  /* 5, 6, 7, 8, or 16 */
} USBD_CDC_LineCodingTypeDef;

typedef struct
{
  USBD_CDC_LineCodingTypeDef lineCoding;
  uint8_t                    controlLineState;   /* DTR/RTS bits */
  bool                       cdcHostConnected;   /* true after SET_CONTROL_LINE_STATE */

  /* CDC TX state */
  volatile bool              cdcTxBusy;
  uint8_t                    cdcTxBuf[COMP_CDC_DATA_EP_SIZE];
  uint16_t                   cdcTxLen;

  /* CDC RX */
  uint8_t                    cdcRxBuf[COMP_CDC_DATA_EP_SIZE];

  /* HID state */
  uint32_t                   hidProtocol;
  uint32_t                   hidIdleState;
  volatile bool              hidTxBusy;
} USBD_Composite_HandleTypeDef;

/* Exported variables --------------------------------------------------------*/
extern USBD_ClassTypeDef USBD_COMPOSITE;

/* Exported functions --------------------------------------------------------*/

/*
 * Send one HID keyboard report (8 bytes).
 * Returns USBD_OK if the transfer was accepted, USBD_BUSY if a previous
 * transfer is still in progress.
 */
uint8_t USBD_COMPOSITE_HID_SendReport(USBD_HandleTypeDef *pdev,
                                       uint8_t *report,
                                       uint16_t len);

/*
 * Transmit CDC data to the host.
 * Returns USBD_OK if accepted, USBD_BUSY if a previous TX is pending.
 */
uint8_t USBD_COMPOSITE_CDC_Transmit(USBD_HandleTypeDef *pdev,
                                     const uint8_t *buf,
                                     uint16_t len);

/* Return true when the CDC host has opened the virtual COM port */
bool USBD_COMPOSITE_CDC_IsHostConnected(USBD_HandleTypeDef *pdev);

/* Return true when the CDC IN endpoint is free */
bool USBD_COMPOSITE_CDC_IsTxIdle(USBD_HandleTypeDef *pdev);

#endif /* USB_DEVICE_APP_USBD_COMPOSITE_H_ */
