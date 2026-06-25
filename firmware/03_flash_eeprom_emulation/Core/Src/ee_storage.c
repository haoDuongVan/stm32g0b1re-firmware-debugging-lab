/*
 * ee_storage.c
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "ee_storage.h"
#include "main.h"
#include "uart_log.h"

#include <stdbool.h>
#include <stddef.h>

#include "ee_fault_inject.h"

/* Private defines -----------------------------------------------------------*/
#define EE_FLASH_BANK                    FLASH_BANK_2

#define EE_PAGE_A_INDEX                  0U
#define EE_PAGE_B_INDEX                  1U

#define EE_MARKER_VALUE                  0x0000000000000000ULL
#define EE_INVALID_PAGE_ADDR             0x00000000UL
#define EE_INVALID_PAGE_INDEX            0xFFFFFFFFUL

/* Private variables ---------------------------------------------------------*/
static uint32_t active_page_addr = 0U;
static uint32_t write_offset = EE_HEADER_SIZE;
static uint8_t initialized = 0U;

/* Private function prototypes -----------------------------------------------*/
static uint32_t Ee_GetPageIndex(uint32_t page_addr);
static uint32_t Ee_GetOtherPageAddr(uint32_t page_addr);
static EeStatus_t Ee_ErasePage(uint32_t page_addr);
static EeStatus_t Ee_WriteMarker(uint32_t page_addr, uint32_t marker_offset);
static uint64_t Ee_BuildRecordRaw(uint16_t var_id, uint32_t value);
static uint64_t Ee_BuildCorruptRecordRaw(uint16_t var_id, uint32_t value);
static EeStatus_t Ee_ProgramRecordAt(uint32_t page_addr,
                                     uint32_t *target_offset,
                                     uint16_t var_id,
                                     uint32_t value);
static bool Ee_IsVarCopied(const uint16_t *copied_vars,
                           uint32_t copied_count,
                           uint16_t var_id);
static EeStatus_t Ee_CopyLatestRecords(uint32_t src_page_addr,
                                       uint32_t src_write_offset,
                                       uint32_t dst_page_addr,
                                       uint32_t *dst_write_offset,
                                       uint16_t skip_var_id,
                                       uint32_t *copied_count);
static EeStatus_t Ee_TransferPage(uint16_t var_id, uint32_t value);
static EeStatus_t Ee_RecoverReceivePage(uint32_t receive_page_addr);
static void Ee_PrintPageStates(void);

/* Private functions ---------------------------------------------------------*/

// Convert page address to Flash page index inside Bank 2
static uint32_t Ee_GetPageIndex(uint32_t page_addr)
{
  // Check Page A
  if (page_addr == EE_PAGE_A_ADDR) {
    return EE_PAGE_A_INDEX;
  }

  // Check Page B
  if (page_addr == EE_PAGE_B_ADDR) {
    return EE_PAGE_B_INDEX;
  }

  // Return invalid index
  return EE_INVALID_PAGE_INDEX;
}

// Get the other EEPROM page address
static uint32_t Ee_GetOtherPageAddr(uint32_t page_addr)
{
  // Return Page B when current page is Page A
  if (page_addr == EE_PAGE_A_ADDR) {
    return EE_PAGE_B_ADDR;
  }

  // Return Page A when current page is Page B
  if (page_addr == EE_PAGE_B_ADDR) {
    return EE_PAGE_A_ADDR;
  }

  // Return invalid address
  return EE_INVALID_PAGE_ADDR;
}

// Erase one EEPROM emulation page
static EeStatus_t Ee_ErasePage(uint32_t page_addr)
{
  FLASH_EraseInitTypeDef erase_init;
  uint32_t page_error = 0U;
  uint32_t page_index;
  HAL_StatusTypeDef hal_status;

  // Convert address to page index
  page_index = Ee_GetPageIndex(page_addr);
  if (page_index == EE_INVALID_PAGE_INDEX) {
    return EE_INVALID_PARAM;
  }

  // Unlock Flash
  hal_status = HAL_FLASH_Unlock();
  if (hal_status != HAL_OK) {
    return EE_ERASE_ERROR;
  }

  // Configure page erase
  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks = EE_FLASH_BANK;
  erase_init.Page = page_index;
  erase_init.NbPages = 1U;

  // Erase selected page
  hal_status = HAL_FLASHEx_Erase(&erase_init, &page_error);

  // Lock Flash
  (void)HAL_FLASH_Lock();

  // Check erase result
  if (hal_status != HAL_OK) {
    UartLog_Printf("[EE] erase failed page=0x%08lX page_error=%lu\r\n",
                   (unsigned long)page_addr,
                   (unsigned long)page_error);
    return EE_ERASE_ERROR;
  }

  return EE_OK;
}

// Write one 8-byte page marker
static EeStatus_t Ee_WriteMarker(uint32_t page_addr, uint32_t marker_offset)
{
  uint32_t marker_addr;
  HAL_StatusTypeDef hal_status;

  // Check marker offset alignment
  if ((marker_offset % EE_MARKER_SLOT_SIZE) != 0U) {
    return EE_INVALID_PARAM;
  }

  // Calculate marker address
  marker_addr = page_addr + marker_offset;

  // Check target marker slot is erased
  if (*(const volatile uint64_t *)marker_addr != EE_ERASED_DOUBLEWORD) {
    return EE_WRITE_ERROR;
  }

  // Unlock Flash
  hal_status = HAL_FLASH_Unlock();
  if (hal_status != HAL_OK) {
    return EE_WRITE_ERROR;
  }

  // Program one double-word marker
  hal_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                 marker_addr,
                                 EE_MARKER_VALUE);

  // Lock Flash
  (void)HAL_FLASH_Lock();

  // Check program result
  if (hal_status != HAL_OK) {
    UartLog_Printf("[EE] marker write failed addr=0x%08lX\r\n",
                   (unsigned long)marker_addr);
    return EE_WRITE_ERROR;
  }

  return EE_OK;
}

// Build one raw 8-byte EEPROM record
static uint64_t Ee_BuildRecordRaw(uint16_t var_id, uint32_t value)
{
  uint16_t crc;
  uint64_t raw;

  // Calculate CRC from var_id and value
  crc = Ee_CalcCrc16(var_id, value);

  // Pack record as var_id + crc + value
  raw = ((uint64_t)var_id) |
        ((uint64_t)crc << 16) |
        ((uint64_t)value << 32);

  return raw;
}

// Build one raw corrupt EEPROM record
static uint64_t Ee_BuildCorruptRecordRaw(uint16_t var_id, uint32_t value)
{
  uint16_t crc;
  uint64_t raw;

  // Calculate CRC and intentionally corrupt it
  crc = (uint16_t)(Ee_CalcCrc16(var_id, value) ^ 0x00FFU);

  // Pack record as var_id + bad crc + value
  raw = ((uint64_t)var_id) |
        ((uint64_t)crc << 16) |
        ((uint64_t)value << 32);

  return raw;
}

// Program one EEPROM record at selected page and offset
static EeStatus_t Ee_ProgramRecordAt(uint32_t page_addr,
                                     uint32_t *target_offset,
                                     uint16_t var_id,
                                     uint32_t value)
{
  uint32_t write_addr;
  uint64_t raw;
  const EeRecord_t *record;
  HAL_StatusTypeDef hal_status;

  // Check parameter
  if (target_offset == NULL) {
    return EE_INVALID_PARAM;
  }

  // Check free space
  if ((*target_offset + EE_RECORD_SIZE) > EE_PAGE_SIZE) {
    return EE_NO_FREE_PAGE;
  }

  // Calculate write address
  write_addr = page_addr + *target_offset;

  // Check target double-word is erased
  if (*(const volatile uint64_t *)write_addr != EE_ERASED_DOUBLEWORD) {
    return EE_WRITE_ERROR;
  }

  // Build raw record
  raw = Ee_BuildRecordRaw(var_id, value);

  // Unlock Flash
  hal_status = HAL_FLASH_Unlock();
  if (hal_status != HAL_OK) {
    return EE_WRITE_ERROR;
  }

  // Program one double-word record
  hal_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                 write_addr,
                                 raw);

  // Lock Flash
  (void)HAL_FLASH_Lock();

  // Check program result
  if (hal_status != HAL_OK) {
    UartLog_Printf("[EE] record write failed addr=0x%08lX\r\n",
                   (unsigned long)write_addr);
    return EE_WRITE_ERROR;
  }

  // Verify written record
  record = (const EeRecord_t *)write_addr;
  if (Ee_IsRecordValid(record) == false) {
    return EE_WRITE_ERROR;
  }

  // Check written value
  if ((record->var_id != var_id) || (record->value != value)) {
    return EE_WRITE_ERROR;
  }

  // Move write offset after successful program
  *target_offset += EE_RECORD_SIZE;

  return EE_OK;
}

// Check whether a virtual ID was already copied
static bool Ee_IsVarCopied(const uint16_t *copied_vars,
                           uint32_t copied_count,
                           uint16_t var_id)
{
  // Search copied variable list
  for (uint32_t idx = 0U; idx < copied_count; idx++) {
    if (copied_vars[idx] == var_id) {
      return true;
    }
  }

  return false;
}

// Copy latest valid records from old page to new page
static EeStatus_t Ee_CopyLatestRecords(uint32_t src_page_addr,
                                       uint32_t src_write_offset,
                                       uint32_t dst_page_addr,
                                       uint32_t *dst_write_offset,
                                       uint16_t skip_var_id,
                                       uint32_t *copied_count)
{
  uint16_t copied_vars[EE_RECORDS_PER_PAGE];
  uint32_t copied_var_count = 0U;
  uint32_t offset;
  const EeRecord_t *record;
  EeStatus_t status;

  // Check parameter
  if ((dst_write_offset == NULL) || (copied_count == NULL)) {
    return EE_INVALID_PARAM;
  }

  // Start from current write offset
  offset = src_write_offset;

  // Scan old page backward to find latest value for each virtual ID
  while (offset > EE_HEADER_SIZE) {
    // Move to previous record
    offset -= EE_RECORD_SIZE;

    // Get record pointer
    record = (const EeRecord_t *)(src_page_addr + offset);

    // Skip invalid record
    if (Ee_IsRecordValid(record) == false) {
      continue;
    }

    // Skip current write target because the new value will be written later
    if (record->var_id == skip_var_id) {
      continue;
    }

    // Skip duplicated virtual ID
    if (Ee_IsVarCopied(copied_vars, copied_var_count, record->var_id) == true) {
      continue;
    }

    // Check copied list capacity
    if (copied_var_count >= EE_RECORDS_PER_PAGE) {
      return EE_TRANSFER_ERROR;
    }

    // Copy latest valid record to new page
    status = Ee_ProgramRecordAt(dst_page_addr,
                                dst_write_offset,
                                record->var_id,
                                record->value);
    if (status != EE_OK) {
      return status;
    }

    // Save copied virtual ID
    copied_vars[copied_var_count] = record->var_id;
    copied_var_count++;
  }

  // Return copied record count
  *copied_count = copied_var_count;

  return EE_OK;
}

// Transfer latest records to the other page and write new record
static EeStatus_t Ee_TransferPage(uint16_t var_id, uint32_t value)
{
  uint32_t old_page_addr;
  uint32_t new_page_addr;
  uint32_t new_write_offset = EE_HEADER_SIZE;
  uint32_t copied_count = 0U;
  EePageState_t new_page_state;
  EeStatus_t status;

  // Save old and new page addresses
  old_page_addr = active_page_addr;
  new_page_addr = Ee_GetOtherPageAddr(old_page_addr);

  // Check new page address
  if (new_page_addr == EE_INVALID_PAGE_ADDR) {
    return EE_NO_FREE_PAGE;
  }

  // Print transfer start
  UartLog_Printf("[EE] transfer start old=0x%08lX new=0x%08lX\r\n",
                 (unsigned long)old_page_addr,
                 (unsigned long)new_page_addr);

  // Erase new page if it is not empty
  new_page_state = Ee_GetPageState(new_page_addr);
  if (new_page_state != EE_PAGE_ERASED) {
    status = Ee_ErasePage(new_page_addr);
    if (status != EE_OK) {
      return status;
    }
    UartLog_Printf("[EE] erase new page OK\r\n");
  }

  // Set new page to RECEIVE
  status = Ee_WriteMarker(new_page_addr, EE_SLOT_RECEIVE_OFFSET);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] set new page RECEIVE OK\r\n");

  // Inject reset after RECEIVE marker
  EeFault_CheckAfterReceiveMarker();

  // Copy latest valid records except current var_id
  status = Ee_CopyLatestRecords(old_page_addr,
                                write_offset,
                                new_page_addr,
                                &new_write_offset,
                                var_id,
                                &copied_count);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] copied_records=%lu\r\n", (unsigned long)copied_count);

  // Inject reset after copying latest records
  EeFault_CheckAfterCopyLatestValues();

  // Write current new record to new page
  status = Ee_ProgramRecordAt(new_page_addr,
                              &new_write_offset,
                              var_id,
                              value);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] write trigger record OK\r\n");

  // Set new page to ACTIVE
  status = Ee_WriteMarker(new_page_addr, EE_SLOT_ACTIVE_OFFSET);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] set new page ACTIVE OK\r\n");

  // Mark old page as VALID
  status = Ee_WriteMarker(old_page_addr, EE_SLOT_VALID_OFFSET);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] set old page VALID OK\r\n");

  // Mark old page as ERASING
  status = Ee_WriteMarker(old_page_addr, EE_SLOT_ERASING_OFFSET);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] set old page ERASING OK\r\n");

  // Erase old page
  status = Ee_ErasePage(old_page_addr);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] erase old page OK\r\n");

  // Update runtime control variables
  active_page_addr = new_page_addr;
  write_offset = new_write_offset;

  // Print states after transfer
  Ee_PrintPageStates();

  // Print transfer result
  UartLog_Printf("[EE] transfer done active_page=0x%08lX write_offset=%lu\r\n",
                 (unsigned long)active_page_addr,
                 (unsigned long)write_offset);

  return EE_OK;
}

// Recover an unfinished RECEIVE page
static EeStatus_t Ee_RecoverReceivePage(uint32_t receive_page_addr)
{
  EeStatus_t status;

  // Print recovery target
  UartLog_Printf("[EE] recovery erase RECEIVE page=0x%08lX\r\n",
                 (unsigned long)receive_page_addr);

  // Erase unfinished RECEIVE page
  status = Ee_ErasePage(receive_page_addr);
  if (status != EE_OK) {
    UartLog_Printf("[EE] recovery erase RECEIVE failed status=%s\r\n",
                   Ee_StatusToString(status));
    return status;
  }

  // Print recovery result
  UartLog_Printf("[EE] recovery erase RECEIVE OK\r\n");

  return EE_OK;
}

// Print current Page A and Page B states
static void Ee_PrintPageStates(void)
{
  EePageState_t page_a_state;
  EePageState_t page_b_state;

  // Read page states
  page_a_state = Ee_GetPageState(EE_PAGE_A_ADDR);
  page_b_state = Ee_GetPageState(EE_PAGE_B_ADDR);

  // Print page states
  UartLog_Printf("[EE] page A state=%s\r\n", Ee_PageStateToString(page_a_state));
  UartLog_Printf("[EE] page B state=%s\r\n", Ee_PageStateToString(page_b_state));
}

/* Function definitions ------------------------------------------------------*/

// Initialize EEPROM emulation from current Flash state
EeStatus_t Ee_Init(void)
{
  EePageState_t page_a_state;
  EePageState_t page_b_state;
  EeStatus_t status;

  // Clear runtime control variables
  active_page_addr = 0U;
  write_offset = EE_HEADER_SIZE;
  initialized = 0U;

  // Print init start
  UartLog_Printf("[EE] init start\r\n");

  // Read page states
  page_a_state = Ee_GetPageState(EE_PAGE_A_ADDR);
  page_b_state = Ee_GetPageState(EE_PAGE_B_ADDR);

  // Print detected states
  UartLog_Printf("[EE] page A state=%s\r\n", Ee_PageStateToString(page_a_state));
  UartLog_Printf("[EE] page B state=%s\r\n", Ee_PageStateToString(page_b_state));

  // Recover Page B RECEIVE when Page A is still ACTIVE
  if ((page_a_state == EE_PAGE_ACTIVE) &&
      (page_b_state == EE_PAGE_RECEIVE)) {
    status = Ee_RecoverReceivePage(EE_PAGE_B_ADDR);
    if (status != EE_OK) {
      return status;
    }

    // Update Page B state after recovery
    page_b_state = Ee_GetPageState(EE_PAGE_B_ADDR);
    UartLog_Printf("[EE] page B state after recovery=%s\r\n",
                   Ee_PageStateToString(page_b_state));
  }

  // Recover Page A RECEIVE when Page B is still ACTIVE
  if ((page_b_state == EE_PAGE_ACTIVE) &&
      (page_a_state == EE_PAGE_RECEIVE)) {
    status = Ee_RecoverReceivePage(EE_PAGE_A_ADDR);
    if (status != EE_OK) {
      return status;
    }

    // Update Page A state after recovery
    page_a_state = Ee_GetPageState(EE_PAGE_A_ADDR);
    UartLog_Printf("[EE] page A state after recovery=%s\r\n",
                   Ee_PageStateToString(page_a_state));
  }

  // Format EEPROM area if both pages are empty
  if ((page_a_state == EE_PAGE_ERASED) &&
      (page_b_state == EE_PAGE_ERASED)) {
    return Ee_Format();
  }

  // Select Page A if it is ACTIVE
  if (page_a_state == EE_PAGE_ACTIVE) {
    active_page_addr = EE_PAGE_A_ADDR;
  }

  // Select Page B if it is ACTIVE
  if (page_b_state == EE_PAGE_ACTIVE) {
    active_page_addr = EE_PAGE_B_ADDR;
  }

  // Check active page
  if (active_page_addr == 0U) {
    UartLog_Printf("[EE] no active page\r\n");
    return EE_NO_ACTIVE_PAGE;
  }

  // Rebuild write offset from Flash
  write_offset = Ee_FindNextWriteOffset(active_page_addr);

  // Mark module as initialized
  initialized = 1U;

  // Print selected active page
  UartLog_Printf("[EE] active_page=0x%08lX\r\n", (unsigned long)active_page_addr);
  UartLog_Printf("[EE] write_offset=%lu\r\n", (unsigned long)write_offset);
  UartLog_Printf("[EE] init done\r\n");

  return EE_OK;
}

// Format EEPROM emulation area
EeStatus_t Ee_Format(void)
{
  EeStatus_t status;

  // Print format start
  UartLog_Printf("[EE] format start\r\n");

  // Erase Page A
  status = Ee_ErasePage(EE_PAGE_A_ADDR);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] erase page A OK\r\n");

  // Erase Page B
  status = Ee_ErasePage(EE_PAGE_B_ADDR);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] erase page B OK\r\n");

  // Set Page A to ACTIVE
  status = Ee_WriteMarker(EE_PAGE_A_ADDR, EE_SLOT_ACTIVE_OFFSET);
  if (status != EE_OK) {
    return status;
  }
  UartLog_Printf("[EE] set page A ACTIVE OK\r\n");

  // Update runtime control variables
  active_page_addr = EE_PAGE_A_ADDR;
  write_offset = EE_HEADER_SIZE;
  initialized = 1U;

  // Print states after format
  Ee_PrintPageStates();

  // Print format result
  UartLog_Printf("[EE] active_page=0x%08lX\r\n", (unsigned long)active_page_addr);
  UartLog_Printf("[EE] write_offset=%lu\r\n", (unsigned long)write_offset);
  UartLog_Printf("[EE] format done\r\n");

  return EE_OK;
}

// Read latest valid value by virtual ID
EeStatus_t Ee_Read(uint16_t var_id, uint32_t *value)
{
  uint32_t offset;
  const EeRecord_t *record;

  // Check initialization state
  if (initialized == 0U) {
    return EE_NOT_INIT;
  }

  // Check parameter
  if (value == NULL) {
    return EE_INVALID_PARAM;
  }

  // Check virtual ID
  if ((var_id == EE_VAR_ID_INVALID) ||
      (var_id == EE_VAR_ID_ERASED)) {
    return EE_INVALID_PARAM;
  }

  // Check active page
  if (active_page_addr == 0U) {
    return EE_NO_ACTIVE_PAGE;
  }

  // Start from current write offset
  offset = write_offset;

  // Scan records backward
  while (offset > EE_HEADER_SIZE) {
    // Move to previous record
    offset -= EE_RECORD_SIZE;

    // Get record pointer
    record = (const EeRecord_t *)(active_page_addr + offset);

    // Skip different virtual ID
    if (record->var_id != var_id) {
      continue;
    }

    // Skip invalid or corrupt record
    if (Ee_IsRecordValid(record) == false) {
      continue;
    }

    // Return latest valid value
    *value = record->value;

    return EE_OK;
  }

  return EE_NOT_FOUND;
}

// Write a new append-only record
EeStatus_t Ee_Write(uint16_t var_id, uint32_t value)
{
  EeStatus_t status;

  // Check initialization state
  if (initialized == 0U) {
    return EE_NOT_INIT;
  }

  // Check virtual ID
  if ((var_id == EE_VAR_ID_INVALID) ||
      (var_id == EE_VAR_ID_ERASED)) {
    return EE_INVALID_PARAM;
  }

  // Check active page
  if (active_page_addr == 0U) {
    return EE_NO_ACTIVE_PAGE;
  }

  // Transfer page when active page is full
  if ((write_offset + EE_RECORD_SIZE) > EE_PAGE_SIZE) {
    status = Ee_TransferPage(var_id, value);
    return status;
  }

  // Program record at current active page
  status = Ee_ProgramRecordAt(active_page_addr,
                              &write_offset,
                              var_id,
                              value);
  if (status != EE_OK) {
    return status;
  }

  return EE_OK;
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
  uint32_t offset = EE_HEADER_SIZE;
  uint32_t count = 0U;
  const EeRecord_t *record;

  // Scan records forward
  while (offset < EE_PAGE_SIZE) {
    // Get record pointer
    record = (const EeRecord_t *)(page_addr + offset);

    // Stop at first erased record
    if (Ee_IsRecordErased(record)) {
      break;
    }

    // Count valid record
    if (Ee_IsRecordValid(record)) {
      count++;
    }

    // Move to next record
    offset += EE_RECORD_SIZE;
  }

  return count;
}

// Count valid records for one virtual ID
uint32_t Ee_CountRecordsForVar(uint16_t var_id)
{
  uint32_t offset = EE_HEADER_SIZE;
  uint32_t count = 0U;
  const EeRecord_t *record;

  // Check active page
  if (active_page_addr == 0U) {
    return 0U;
  }

  // Scan records forward
  while (offset < write_offset) {
    // Get record pointer
    record = (const EeRecord_t *)(active_page_addr + offset);

    // Count matching valid record
    if ((record->var_id == var_id) &&
        (Ee_IsRecordValid(record) == true)) {
      count++;
    }

    // Move to next record
    offset += EE_RECORD_SIZE;
  }

  return count;
}

// Write one intentionally corrupt record for test
EeStatus_t Ee_TestWriteCorruptRecord(uint16_t var_id, uint32_t value)
{
  uint32_t write_addr;
  uint64_t raw;
  const EeRecord_t *record;
  HAL_StatusTypeDef hal_status;

  // Check initialization state
  if (initialized == 0U) {
    return EE_NOT_INIT;
  }

  // Check virtual ID
  if ((var_id == EE_VAR_ID_INVALID) ||
      (var_id == EE_VAR_ID_ERASED)) {
    return EE_INVALID_PARAM;
  }

  // Check active page
  if (active_page_addr == 0U) {
    return EE_NO_ACTIVE_PAGE;
  }

  // Check free space
  if ((write_offset + EE_RECORD_SIZE) > EE_PAGE_SIZE) {
    return EE_NO_FREE_PAGE;
  }

  // Calculate write address
  write_addr = active_page_addr + write_offset;

  // Check target double-word is erased
  if (*(const volatile uint64_t *)write_addr != EE_ERASED_DOUBLEWORD) {
    return EE_WRITE_ERROR;
  }

  // Build corrupt raw record
  raw = Ee_BuildCorruptRecordRaw(var_id, value);

  // Unlock Flash
  hal_status = HAL_FLASH_Unlock();
  if (hal_status != HAL_OK) {
    return EE_WRITE_ERROR;
  }

  // Program corrupt record
  hal_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                 write_addr,
                                 raw);

  // Lock Flash
  (void)HAL_FLASH_Lock();

  // Check program result
  if (hal_status != HAL_OK) {
    UartLog_Printf("[EE] corrupt record write failed addr=0x%08lX\r\n",
                   (unsigned long)write_addr);
    return EE_WRITE_ERROR;
  }

  // Get written record pointer
  record = (const EeRecord_t *)write_addr;

  // Check that record is really corrupt
  if (Ee_IsRecordValid(record) == true) {
    return EE_WRITE_ERROR;
  }

  // Check written var_id and value
  if ((record->var_id != var_id) || (record->value != value)) {
    return EE_WRITE_ERROR;
  }

  // Move write offset after successful corrupt program
  write_offset += EE_RECORD_SIZE;

  return EE_OK;
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
