/*
 * ring_buffer.h
 *
 *  Created on: Jun 23, 2026
 *      Author: haodu
 */

#ifndef INC_RING_BUFFER_H_
#define INC_RING_BUFFER_H_

/* Private includes ----------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
typedef struct
{
  uint8_t *buffer;
  uint16_t capacity;
  volatile uint16_t head;
  volatile uint16_t tail;
} RingBuffer_t;

/* Function prototypes -------------------------------------------------------*/
void     RingBuffer_Init (RingBuffer_t *rb, uint8_t *buffer, uint16_t capacity);
bool     RingBuffer_Write (RingBuffer_t *rb, const uint8_t *data, uint16_t len);
bool     RingBuffer_AdvanceTail (RingBuffer_t *rb, uint16_t len);
uint16_t RingBuffer_GetUsed (const RingBuffer_t *rb);
uint16_t RingBuffer_GetFree (const RingBuffer_t *rb);
uint16_t RingBuffer_PeekContiguous (const RingBuffer_t *rb, uint8_t **data);

#endif /* INC_RING_BUFFER_H_ */
