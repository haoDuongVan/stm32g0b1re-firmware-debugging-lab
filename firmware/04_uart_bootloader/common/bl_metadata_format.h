/*
 * bl_metadata_format.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 *
 * Shared metadata struct and constants for both the bootloader and the application.
 * Include this header in both projects to guarantee struct layout compatibility.
 */

#ifndef BL_METADATA_FORMAT_H_
#define BL_METADATA_FORMAT_H_

#ifdef __cplusplus
extern "C" {
#endif

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
 *   crc32          : CRC32 over all fields above; recalculated on every write
 */
typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t active_slot;
  uint32_t confirmed_slot;
  uint32_t boot_count;
  uint32_t reserved[3];
  uint32_t crc32;
} BlMetadata_t;

#ifdef __cplusplus
}
#endif

#endif /* BL_METADATA_FORMAT_H_ */
