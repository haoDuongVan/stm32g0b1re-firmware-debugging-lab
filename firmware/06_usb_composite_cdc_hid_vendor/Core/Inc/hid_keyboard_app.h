/*
 * hid_keyboard_app.h
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

#ifndef INC_HID_KEYBOARD_APP_H_
#define INC_HID_KEYBOARD_APP_H_

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/
#define USB_HID_KEYBOARD_REPORT_SIZE  8U

/* Exported types ------------------------------------------------------------*/

/*
 * Transport TX state.
 * IDLE  — endpoint is free, SendReport will accept a new report.
 * BUSY  — a transfer is in progress; DataIn callback resets to IDLE.
 */
typedef enum
{
  USB_HID_TX_IDLE = 0,
  USB_HID_TX_BUSY
} UsbHidTxState_t;

/* Function prototypes -------------------------------------------------------*/

/* Transport layer */
void UsbHidTransport_Init(void);
bool UsbHidTransport_IsIdle(void);
bool UsbHidTransport_SendReport(const uint8_t report[USB_HID_KEYBOARD_REPORT_SIZE]);
void UsbHidTransport_TxCpltCallback(void);

/* Application */
void HID_Keyboard_Init(void);
void HID_Keyboard_App(void);

#endif /* INC_HID_KEYBOARD_APP_H_ */
