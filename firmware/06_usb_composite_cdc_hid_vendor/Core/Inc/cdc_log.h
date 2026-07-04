/*
 * cdc_log.h
 *
 *  Created on: Jul 5, 2026
 *      Author: haodu
 */

#ifndef INC_CDC_LOG_H_
#define INC_CDC_LOG_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/

/* Ring buffer capacity in bytes — must be a power of two. */
#define CDC_LOG_BUF_SIZE  512U

/* Function prototypes -------------------------------------------------------*/

// Initialise the ring buffer; call once from HID_Keyboard_Init
void CdcLog_Init(void);

/*
 * Drain pending data to the CDC IN endpoint.
 * Call every iteration of HID_Keyboard_App.
 * Skips silently when the host has not opened the port or when a previous
 * USB transfer is still in progress.
 */
void CdcLog_Run(void);

/*
 * Copy up to len bytes into the ring buffer.
 * Excess bytes are silently dropped when the buffer is full.
 * Must be called from the main loop only — not ISR-safe.
 */
void CdcLog_Write(const char *buf, uint16_t len);

/*
 * Format and write a string to the ring buffer (printf-style).
 * Output longer than 128 bytes is truncated.
 * Must be called from the main loop only — not ISR-safe.
 */
void CdcLog_Printf(const char *fmt, ...);

#endif /* INC_CDC_LOG_H_ */
