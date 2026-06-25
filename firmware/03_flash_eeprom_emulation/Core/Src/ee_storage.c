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

/* Private defines -----------------------------------------------------------*/
#define EE_FLASH_BANK                    FLASH_BANK_2

#define EE_PAGE_A_INDEX                  0U
#define EE_PAGE_B_INDEX                  1U

#define EE_MARKER_VALUE                  0x0000000000000000ULL

/* Private variables ---------------------------------------------------------*/
static uint32_t active_page_addr = 0U;
static uint32_t write_offset = EE_HEADER_SIZE;
static uint8_t initialized = 0U;

/* Private function prototypes -----------------------------------------------*/
static uint32_t Ee_GetPageIndex(uint32_t page_addr);
static EeStatus_t Ee_ErasePage(uint32_t page_addr);
static EeStatus_t Ee_WriteMarker(uint32_t page_addr, uint32_t marker_offset);
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

  // Return invalid index fallback
  return 0xFFFFFFFFUL;
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
  if (page_index == 0xFFFFFFFFUL) {
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

// Write one 8-byte marker slot
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

  // Mark EEPROM module as initialized
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
