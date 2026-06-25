/*
 * ee_storage.c
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "ee_storage.h"

/* Private defines -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static uint32_t active_page_addr = 0U;
static uint32_t write_offset = EE_HEADER_SIZE;
static uint8_t initialized = 0U;

/* Function definitions ------------------------------------------------------*/

// Initialize EEPROM emulation control variables
EeStatus_t Ee_Init(void)
{
  /*
   * Temporary skeleton.
   *
   * Real implementation will:
   *   - read page marker slots
   *   - select ACTIVE page
   *   - recover incomplete RECEIVE page
   *   - scan Flash to rebuild write_offset
   */

  // Clear active page address
  active_page_addr = 0U;

  // Reset write offset to the first record position
  write_offset = EE_HEADER_SIZE;

  // Mark EEPROM module as initialized
  initialized = 1U;

  return EE_OK;
}

// Read latest valid value by virtual ID
EeStatus_t Ee_Read(uint16_t var_id, uint32_t *value)
{
  // Mark parameters as unused in skeleton
  (void)var_id;
  (void)value;

  // Check initialization state
  if (initialized == 0U) {
    return EE_NOT_INIT;
  }

  /*
   * Temporary skeleton.
   *
   * Real implementation will:
   *   - scan active page backward
   *   - compare var_id
   *   - verify CRC
   *   - skip corrupt record
   *   - return latest valid value
   */

  return EE_NOT_FOUND;
}

// Write a new append-only record
EeStatus_t Ee_Write(uint16_t var_id, uint32_t value)
{
  // Mark parameters as unused in skeleton
  (void)var_id;
  (void)value;

  // Check initialization state
  if (initialized == 0U) {
    return EE_NOT_INIT;
  }

  /*
   * Temporary skeleton.
   *
   * Real implementation will:
   *   - validate virtual ID
   *   - check free space
   *   - trigger page transfer if page is full
   *   - build record with CRC
   *   - program one double-word
   *   - update write_offset after successful program
   */

  return EE_WRITE_ERROR;
}

// Get current active page address
uint32_t Ee_GetActivePageAddr(void)
{
  // Return active page address
  return active_page_addr;
}

// Get current write offset
uint32_t Ee_GetWriteOffset(void)
{
  // Return next write offset
  return write_offset;
}

// Count valid records in one page
uint32_t Ee_CountValidRecords(uint32_t page_addr)
{
  // Mark parameter as unused in skeleton
  (void)page_addr;

  /*
   * Temporary skeleton.
   *
   * Real implementation will:
   *   - scan all records in the selected page
   *   - count records with valid CRC
   */

  return 0U;
}

// Count valid records for one virtual ID
uint32_t Ee_CountRecordsForVar(uint16_t var_id)
{
  // Mark parameter as unused in skeleton
  (void)var_id;

  /*
   * Temporary skeleton.
   *
   * Real implementation will:
   *   - scan active page
   *   - count valid records matching var_id
   */

  return 0U;
}

// Convert EEPROM status to readable string
const char *Ee_StatusToString(EeStatus_t status)
{
  // Return readable status name
  switch (status)
  {
    case EE_OK:
      return "EE_OK";

    case EE_NOT_INIT:
      return "EE_NOT_INIT";

    case EE_NOT_FOUND:
      return "EE_NOT_FOUND";

    case EE_INVALID_PARAM:
      return "EE_INVALID_PARAM";

    case EE_WRITE_ERROR:
      return "EE_WRITE_ERROR";

    case EE_ERASE_ERROR:
      return "EE_ERASE_ERROR";

    case EE_NO_ACTIVE_PAGE:
      return "EE_NO_ACTIVE_PAGE";

    case EE_NO_FREE_PAGE:
      return "EE_NO_FREE_PAGE";

    case EE_TRANSFER_ERROR:
      return "EE_TRANSFER_ERROR";

    case EE_CRC_MISMATCH:
      return "EE_CRC_MISMATCH";

    default:
      return "EE_UNKNOWN";
  }
}
