/*
 * app_metadata.h
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

#ifndef INC_APP_METADATA_H_
#define INC_APP_METADATA_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "bl_metadata_format.h"

/* Function prototypes -------------------------------------------------------*/
void     AppMetadata_Read(BlMetadata_t *meta);
uint8_t  AppMetadata_IsHeaderValid(const BlMetadata_t *meta);
uint32_t AppMetadata_CalculateCrc(const BlMetadata_t *meta);
uint8_t  AppMetadata_IsCrcValid(const BlMetadata_t *meta);
uint8_t  AppMetadata_IsValid(const BlMetadata_t *meta);
uint8_t  AppMetadata_Write(const BlMetadata_t *meta);

#ifdef __cplusplus
}
#endif

#endif /* INC_APP_METADATA_H_ */
