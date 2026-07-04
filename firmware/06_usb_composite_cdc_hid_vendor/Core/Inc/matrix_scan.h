/*
 * matrix_scan.h
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

#ifndef INC_MATRIX_SCAN_H_
#define INC_MATRIX_SCAN_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/
#define MATRIX_ROW_NUM   4U
#define MATRIX_COL_NUM   4U
#define MATRIX_KEY_NUM   16U

/*
 * Bitmask covering all 16 key positions in the raw state word.
 * Bit N corresponds to keyLoc N (row = N/4, col = N%4).
 */
#define MATRIX_KEY_MASK  0xFFFFU

/* Function prototypes -------------------------------------------------------*/

// Drive all row outputs HIGH (inactive); call once after GPIO init
void MatrixScan_Init(void);

/*
 * Scan all rows and return a 16-bit raw state word.
 * Bit N is set if the key at keyLoc N is currently pressed.
 * No debounce — call MatrixScan_Debounce in a later milestone.
 */
uint16_t MatrixScan_ReadRaw(void);

// Return the number of bits set in rawState (number of keys currently pressed)
uint8_t MatrixScan_CountPressed(uint16_t rawState);

#endif /* INC_MATRIX_SCAN_H_ */
