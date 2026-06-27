/*
 * bl_metadata.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_BL_METADATA_H_
#define INC_BL_METADATA_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "bl_metadata_format.h"

/* Function prototypes -------------------------------------------------------*/
void     BlMetadata_Read(BlMetadata_t *meta);
uint8_t  BlMetadata_IsHeaderValid(const BlMetadata_t *meta);
uint32_t BlMetadata_CalculateCrc(const BlMetadata_t *meta);
uint8_t  BlMetadata_IsCrcValid(const BlMetadata_t *meta);
uint8_t  BlMetadata_IsValid(const BlMetadata_t *meta);
uint8_t  BlMetadata_Write(const BlMetadata_t *meta);

#ifdef __cplusplus
}
#endif

#endif /* INC_BL_METADATA_H_ */
