/*
 * bl_metadata.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_BL_METADATA_H_
#define INC_BL_METADATA_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/

#define BL_METADATA_MAGIC                0x424C4D44UL   /* "BLMD" */
#define BL_METADATA_VERSION              1UL

#define BL_METADATA_SLOT_A               0x00000041UL   /* 'A' */
#define BL_METADATA_SLOT_B               0x00000042UL   /* 'B' */

/* Exported types ------------------------------------------------------------*/

/*
 * Metadata record stored at the beginning of the metadata page.
 *
 * Layout (9 words = 36 bytes):
 *
 *   magic          : must be BL_METADATA_MAGIC to be considered valid
 *   version        : must be BL_METADATA_VERSION
 *   active_slot    : BL_METADATA_SLOT_A or BL_METADATA_SLOT_B
 *   confirmed_slot : last slot that booted successfully
 *   boot_count     : number of boot attempts since last confirm
 *   reserved[3]    : reserved for future use
 *   crc32          : CRC32 of the record (not checked in this milestone)
 */
typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t active_slot;
  uint32_t confirmed_slot;
  uint32_t boot_count;
  uint32_t reserved[3];
  uint32_t crc32;          /* CRC32 of the record; not checked in this milestone */
} BlMetadata_t;

/* Function prototypes -------------------------------------------------------*/
void     BlMetadata_Read(BlMetadata_t *meta);
uint8_t  BlMetadata_IsHeaderValid(const BlMetadata_t *meta);
uint32_t BlMetadata_CalculateCrc(const BlMetadata_t *meta);
uint8_t  BlMetadata_IsCrcValid(const BlMetadata_t *meta);
uint8_t  BlMetadata_IsValid(const BlMetadata_t *meta);

#endif /* INC_BL_METADATA_H_ */
