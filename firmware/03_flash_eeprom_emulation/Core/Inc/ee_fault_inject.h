/*
 * ee_fault_inject.h
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

#ifndef INC_EE_FAULT_INJECT_H_
#define INC_EE_FAULT_INJECT_H_

/* Private includes ----------------------------------------------------------*/
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/
#define EE_FAULT_NONE                  0U
#define EE_FAULT_AFTER_RECEIVE         1U
#define EE_FAULT_AFTER_COPY            2U
#define EE_FAULT_AFTER_PROGRAM         3U

#define TEST_PHASE_IDLE                0U
#define TEST_PHASE_AFTER_RESET         1U
#define TEST_PHASE_DONE                2U

/* Function prototypes -------------------------------------------------------*/
void EeFault_SetMode(uint32_t mode);
uint32_t EeFault_GetMode(void);

void EeFault_CheckAfterReceiveMarker(void);
void EeFault_CheckAfterCopyLatestValues(void);
void EeFault_CheckAfterProgramOk(void);

#endif /* INC_EE_FAULT_INJECT_H_ */
