/*
 * ee_fault_inject.c
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "ee_fault_inject.h"
#include "main.h"

/* Private variables ---------------------------------------------------------*/
static uint32_t fault_mode = EE_FAULT_NONE;

/* Function definitions ------------------------------------------------------*/

// Set fault injection mode
void EeFault_SetMode(uint32_t mode)
{
  // Store selected fault mode
  fault_mode = mode;
}

// Get current fault injection mode
uint32_t EeFault_GetMode(void)
{
  // Return selected fault mode
  return fault_mode;
}

// Reset MCU after RECEIVE marker is written
void EeFault_CheckAfterReceiveMarker(void)
{
  // Check fault mode
  if (fault_mode == EE_FAULT_AFTER_RECEIVE) {
    // Trigger software reset
    NVIC_SystemReset();
  }
}

// Reset MCU after latest records are copied
void EeFault_CheckAfterCopyLatestValues(void)
{
  // Check fault mode
  if (fault_mode == EE_FAULT_AFTER_COPY) {
    // Trigger software reset
    NVIC_SystemReset();
  }
}

// Reset MCU after Flash program succeeds
void EeFault_CheckAfterProgramOk(void)
{
  // Check fault mode
  if (fault_mode == EE_FAULT_AFTER_PROGRAM) {
    // Trigger software reset
    NVIC_SystemReset();
  }
}
