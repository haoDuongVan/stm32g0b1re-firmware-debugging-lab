/*
 * key_detect.h
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

#ifndef INC_KEY_DETECT_H_
#define INC_KEY_DETECT_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Function prototypes -------------------------------------------------------*/

// Clear scan buffers and key status; call once after MatrixScan_Init
void KeyDetect_Init(void);

/*
 * Read one raw scan, debounce, and push KEY_EVENT_ON / KEY_EVENT_OFF
 * events into the queue for any state change detected.
 * Call at a fixed rate (e.g. every 5 ms).
 */
void KeyDetect_Run(void);

// Return the current debounced key status bitmask (bit N = keyLoc N is held)
uint16_t KeyDetect_GetKeyStatus(void);

#endif /* INC_KEY_DETECT_H_ */
