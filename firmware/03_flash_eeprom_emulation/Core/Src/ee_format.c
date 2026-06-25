/*
 * ee_format.c
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "ee_format.h"

#include <stddef.h>

/* Private defines -----------------------------------------------------------*/
#define CRC16_CCITT_INIT_VALUE          0xFFFFU
#define CRC16_CCITT_POLY               0x1021U

/* Private variables ---------------------------------------------------------*/

/* Function definitions ------------------------------------------------------*/

// Calculate CRC16-CCITT for one EEPROM record
uint16_t Ee_CalcCrc16(uint16_t var_id, uint32_t value)
{
  uint16_t crc = CRC16_CCITT_INIT_VALUE;
  uint8_t data[6];

  // Pack var_id and value into byte buffer
  data[0] = (uint8_t)(var_id & 0xFFU);
  data[1] = (uint8_t)((var_id >> 8) & 0xFFU);
  data[2] = (uint8_t)(value & 0xFFU);
  data[3] = (uint8_t)((value >> 8) & 0xFFU);
  data[4] = (uint8_t)((value >> 16) & 0xFFU);
  data[5] = (uint8_t)((value >> 24) & 0xFFU);

  // Feed each byte into CRC calculator
  for (uint32_t idx = 0U; idx < sizeof(data); idx++) {
    crc ^= (uint16_t)data[idx] << 8;

    // Process 8 bits
    for (uint32_t bit = 0U; bit < 8U; bit++) {
      if ((crc & 0x8000U) != 0U) {
        crc = (uint16_t)((crc << 1) ^ CRC16_CCITT_POLY);
      } else {
        crc = (uint16_t)(crc << 1);
      }
    }
  }

  return crc;
}

// Check whether a record is still erased
bool Ee_IsRecordErased(const EeRecord_t *record)
{
  // Check parameter
  if (record == NULL) {
    return false;
  }

  // Check erased pattern
  return ((record->var_id == 0xFFFFU) &&
          (record->crc == 0xFFFFU) &&
          (record->value == 0xFFFFFFFFUL));
}

// Check whether a record has valid var_id and CRC
bool Ee_IsRecordValid(const EeRecord_t *record)
{
  uint16_t crc;

  // Check parameter
  if (record == NULL) {
    return false;
  }

  // Reject erased record
  if (Ee_IsRecordErased(record)) {
    return false;
  }

  // Reject reserved virtual IDs
  if ((record->var_id == EE_VAR_ID_INVALID) ||
      (record->var_id == EE_VAR_ID_ERASED)) {
    return false;
  }

  // Calculate expected CRC
  crc = Ee_CalcCrc16(record->var_id, record->value);

  // Compare stored CRC and calculated CRC
  return (record->crc == crc);
}

// Get page state from 32-byte marker header
EePageState_t Ee_GetPageState(uint32_t page_addr)
{
  uint64_t receive_slot;
  uint64_t active_slot;
  uint64_t valid_slot;
  uint64_t erasing_slot;

  // Read 4 marker slots
  receive_slot = *(const volatile uint64_t *)(page_addr + EE_SLOT_RECEIVE_OFFSET);
  active_slot  = *(const volatile uint64_t *)(page_addr + EE_SLOT_ACTIVE_OFFSET);
  valid_slot   = *(const volatile uint64_t *)(page_addr + EE_SLOT_VALID_OFFSET);
  erasing_slot = *(const volatile uint64_t *)(page_addr + EE_SLOT_ERASING_OFFSET);

  // Check newest marker first
  if (erasing_slot != EE_ERASED_DOUBLEWORD) {
    return EE_PAGE_ERASING;
  }

  // Check VALID marker
  if (valid_slot != EE_ERASED_DOUBLEWORD) {
    return EE_PAGE_VALID;
  }

  // Check ACTIVE marker
  if (active_slot != EE_ERASED_DOUBLEWORD) {
    return EE_PAGE_ACTIVE;
  }

  // Check RECEIVE marker
  if (receive_slot != EE_ERASED_DOUBLEWORD) {
    return EE_PAGE_RECEIVE;
  }

  // All slots are erased
  return EE_PAGE_ERASED;
}

// Find next writable offset in a page
uint32_t Ee_FindNextWriteOffset(uint32_t page_addr)
{
  uint32_t offset = EE_HEADER_SIZE;

  // Scan records from the first record offset
  while (offset < EE_PAGE_SIZE) {
    uint64_t raw = *(const volatile uint64_t *)(page_addr + offset);

    // Stop at the first erased double-word
    if (raw == EE_ERASED_DOUBLEWORD) {
      break;
    }

    // Move to next record
    offset += EE_RECORD_SIZE;
  }

  return offset;
}

// Convert page state to string for debug log
const char *Ee_PageStateToString(EePageState_t state)
{
  // Return readable state name
  switch (state)
  {
    case EE_PAGE_RECEIVE:
      return "RECEIVE";

    case EE_PAGE_ACTIVE:
      return "ACTIVE";

    case EE_PAGE_VALID:
      return "VALID";

    case EE_PAGE_ERASING:
      return "ERASING";

    case EE_PAGE_ERASED:
      return "ERASED";

    case EE_PAGE_INVALID:
    default:
      return "INVALID";
  }
}
