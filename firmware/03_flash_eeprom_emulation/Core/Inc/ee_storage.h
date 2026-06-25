/*
 * ee_storage.h
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

#ifndef INC_EE_STORAGE_H_
#define INC_EE_STORAGE_H_

/* Private includes ----------------------------------------------------------*/
#include "ee_format.h"

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  EE_OK = 0,
  EE_NOT_INIT,
  EE_NOT_FOUND,
  EE_INVALID_PARAM,
  EE_WRITE_ERROR,
  EE_ERASE_ERROR,
  EE_NO_ACTIVE_PAGE,
  EE_NO_FREE_PAGE,
  EE_TRANSFER_ERROR,
  EE_CRC_MISMATCH
} EeStatus_t;

/* Function prototypes -------------------------------------------------------*/
EeStatus_t Ee_Init(void);
EeStatus_t Ee_Format(void);
EeStatus_t Ee_Read(uint16_t var_id, uint32_t *value);
EeStatus_t Ee_Write(uint16_t var_id, uint32_t value);

uint32_t Ee_GetActivePageAddr(void);
uint32_t Ee_GetWriteOffset(void);
uint32_t Ee_CountValidRecords(uint32_t page_addr);
uint32_t Ee_CountRecordsForVar(uint16_t var_id);

const char *Ee_StatusToString(EeStatus_t status);

#endif /* INC_EE_STORAGE_H_ */
