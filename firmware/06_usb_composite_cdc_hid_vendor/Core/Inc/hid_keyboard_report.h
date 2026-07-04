/*
 * hid_keyboard_report.h
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

#ifndef INC_HID_KEYBOARD_REPORT_H_
#define INC_HID_KEYBOARD_REPORT_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/
#define HID_KEYBOARD_REPORT_SIZE      8U

/*
 * HID Boot Keyboard error rollover code (usage 0x01).
 * Sent in key slots 2-7 when more keys are pressed than the report can hold.
 */
#define HID_KEYBOARD_ERROR_ROLLOVER   0x01U

/* Exported types ------------------------------------------------------------*/

/*
 * One HID keyboard input report (8 bytes):
 *   bytes[0] — modifier bitmask (Ctrl / Shift / Alt / GUI)
 *   bytes[1] — reserved, always 0x00
 *   bytes[2..7] — up to 6 simultaneous key usages (0x00 = no key)
 */
typedef struct
{
  uint8_t bytes[HID_KEYBOARD_REPORT_SIZE];
} HidKeyboardReport_t;

/* Function prototypes -------------------------------------------------------*/

// Zero all bytes in the report (all keys released, no modifiers)
void HidKeyboardReport_Clear(HidKeyboardReport_t *report);

// Set modifier + one key usage; clears all other key slots
void HidKeyboardReport_SetKey(HidKeyboardReport_t *report,
                              uint8_t modifier,
                              uint8_t usage);

// Fill key slots 2-7 with ErrorRollOver (too many keys pressed simultaneously)
void HidKeyboardReport_SetErrorRollOver(HidKeyboardReport_t *report);

// Return a read-only pointer to the raw report bytes for USB transmission
const uint8_t *HidKeyboardReport_GetData(const HidKeyboardReport_t *report);

#endif /* INC_HID_KEYBOARD_REPORT_H_ */
