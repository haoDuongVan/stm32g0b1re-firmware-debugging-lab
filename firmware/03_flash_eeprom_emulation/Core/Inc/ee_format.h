/*
 * ee_format.h
 *
 *  Created on: Jun 26, 2026
 *      Author: haodu
 */

#ifndef INC_EE_FORMAT_H_
#define INC_EE_FORMAT_H_

/* Private includes ----------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/
/*
 * EEPROM emulation area in Bank 2.
 *
 * Page A: 0x08040000 - 0x080407FF
 * Page B: 0x08040800 - 0x08040FFF
 */
#define EE_PAGE_A_ADDR                   0x08040000UL
#define EE_PAGE_B_ADDR                   0x08040800UL
#define EE_PAGE_SIZE                     2048U

/*
 * Page layout:
 *
 *   0x000 - 0x007 : RECEIVE marker slot
 *   0x008 - 0x00F : ACTIVE marker slot
 *   0x010 - 0x017 : VALID marker slot
 *   0x018 - 0x01F : ERASING marker slot
 *   0x020 - ...   : 8-byte records
 */
#define EE_HEADER_SIZE                   32U
#define EE_RECORD_SIZE                   8U
#define EE_RECORDS_PER_PAGE              ((EE_PAGE_SIZE - EE_HEADER_SIZE) / EE_RECORD_SIZE)

#define EE_MARKER_SLOT_SIZE              8U
#define EE_SLOT_RECEIVE_OFFSET           0U
#define EE_SLOT_ACTIVE_OFFSET            8U
#define EE_SLOT_VALID_OFFSET             16U
#define EE_SLOT_ERASING_OFFSET           24U

#define EE_ERASED_DOUBLEWORD             0xFFFFFFFFFFFFFFFFULL

/*
 * Reserved virtual IDs.
 */
#define EE_VAR_ID_INVALID                0x0000U
#define EE_VAR_ID_ERASED                 0xFFFFU
#define EE_VAR_ID_TEST_PHASE             0x7F00U

/*
 * Example config virtual IDs used by test modes.
 */
#define CFG_BAUD_RATE                    0x0001U
#define CFG_TIMEOUT_MS                   0x0002U
#define CFG_MODE                         0x0003U

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  EE_PAGE_RECEIVE = 0,
  EE_PAGE_ACTIVE,
  EE_PAGE_VALID,
  EE_PAGE_ERASING,
  EE_PAGE_ERASED,
  EE_PAGE_INVALID
} EePageState_t;

typedef struct __attribute__((packed))
{
  uint16_t var_id;
  uint16_t crc;
  uint32_t value;
} EeRecord_t;

/* Function prototypes -------------------------------------------------------*/
uint16_t Ee_CalcCrc16(uint16_t var_id, uint32_t value);
bool Ee_IsRecordErased(const EeRecord_t *record);
bool Ee_IsRecordValid(const EeRecord_t *record);
EePageState_t Ee_GetPageState(uint32_t page_addr);
uint32_t Ee_FindNextWriteOffset(uint32_t page_addr);
const char *Ee_PageStateToString(EePageState_t state);

#endif /* INC_EE_FORMAT_H_ */
