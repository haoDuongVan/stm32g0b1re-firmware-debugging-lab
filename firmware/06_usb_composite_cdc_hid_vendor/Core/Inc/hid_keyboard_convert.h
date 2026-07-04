/*
 * hid_keyboard_convert.h
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

#ifndef INC_HID_KEYBOARD_CONVERT_H_
#define INC_HID_KEYBOARD_CONVERT_H_

/* Function prototypes -------------------------------------------------------*/

// Clear the internal report state; call once after transport and queue are ready
void HidKeyboardConvert_Init(void);

/*
 * Drain one event from the queue and send the resulting HID report.
 * Must be called repeatedly from the main loop.
 * Returns immediately if the USB endpoint is busy or the queue is empty.
 */
void HidKeyboardConvert_Run(void);

#endif /* INC_HID_KEYBOARD_CONVERT_H_ */
