/*
 * ring_buffer.c
 *
 *  Created on: Jun 23, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "ring_buffer.h"
#include <stddef.h>
#include <string.h>

/* Private functions ---------------------------------------------------------*/
// Returns the minimum of two unsigned 16-bit values
static uint16_t RingBuffer_MinU16(uint16_t a, uint16_t b)
{
  return (a < b) ? a : b;
}

// Initializes a ring buffer
void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buffer, uint16_t capacity)
{
  // Ring buffer must have at least two bytes and pointers must not be NULL
  if ((rb == NULL) || (buffer == NULL) || (capacity < 2U)) {
    return;
  }

  // Initialize the ring buffer
  rb->buffer = buffer;
  rb->capacity = capacity;
  rb->head = 0U;
  rb->tail = 0U;
}

// Returns the number of bytes currently used in the ring buffer
uint16_t RingBuffer_GetUsed(const RingBuffer_t *rb)
{
  uint16_t head;
  uint16_t tail;
  uint16_t used;

  // Return 0 if the ring buffer is not valid
  if ((rb == NULL) || (rb->buffer == NULL) || (rb->capacity < 2)) {
    return 0;
  }

  // Calculate the number of bytes used
  head = rb->head;
  tail = rb->tail;

  // Linear: head - tail; wrapped: capacity - tail + head
  head >= tail ? (used = head - tail) : (used = rb->capacity - tail + head);

  return used;
}

// Returns the number of free bytes (one slot is always unused to distinguish full vs empty)
uint16_t RingBuffer_GetFree(const RingBuffer_t *rb)
{
  // Return 0 if the ring buffer is not valid
  if ((rb == NULL) || (rb->buffer == NULL) || (rb->capacity < 2)) {
    return 0;
  }

  /*
   * One byte is intentionally left unused.
   * This makes head == tail mean "empty".
   */
  return (uint16_t)(rb->capacity - RingBuffer_GetUsed(rb) - 1U);
}

// Adds data to the ring buffer
bool RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, uint16_t len)
{
  uint16_t first_len;
  uint16_t second_len;
  uint16_t space_to_end;

  // Return false if the ring buffer is not valid or no data is provided
  if ((rb == NULL) || (rb->buffer == NULL) || (data == NULL) || (len == 0U)) {
    return false;
  }

  // Return false if there is not enough free space to write 'len' bytes
  if (len > RingBuffer_GetFree(rb)) {
    return false;
  }

  // Split the write into at most two memcpy calls to handle wrap-around
  space_to_end = (uint16_t)(rb->capacity - rb->head);  // bytes from head to end of array
  first_len    = RingBuffer_MinU16(len, space_to_end); // bytes written before wrap
  second_len   = (uint16_t)(len - first_len);          // bytes written after wrap (0 if no wrap)

  memcpy(&rb->buffer[rb->head], data, first_len);

  if (second_len > 0) {
    // Write the remaining bytes starting from index 0 after wrap-around
    memcpy(&rb->buffer[0], &data[first_len], second_len);
  }

  // Update the ring buffer write pointer
  rb->head = (uint16_t)((rb->head + len) % rb->capacity);

  return true;
}

// Returns the number of contiguous bytes in the ring buffer
uint16_t RingBuffer_PeekContiguous(const RingBuffer_t *rb, uint8_t **data)
{
  uint16_t head;
  uint16_t tail;
  uint16_t contiguous;

  // Set the data pointer to NULL to avoid garbage
  if (data != 0) {
    *data = 0;
  }

  // Return 0 if the ring buffer is not valid
  if ((rb == 0) || (rb->buffer == 0) || (data == 0)) {
    return 0U;
  }

  // Get head and tail of the ring buffer
  head = rb->head;
  tail = rb->tail;

  // Return 0 if the ring buffer is empty
  if (head == tail) {
    return 0;
  }

  // Set the data pointer from the tail
  *data = &rb->buffer[tail];

  /*
   * Only the bytes from tail to the end of the backing array are contiguous.
   * When head >= tail all data is already in one linear chunk.
   * When head < tail (wrap-around) only capacity-tail bytes are contiguous;
   * the remaining bytes from 0 to head-1 will be sent in the next transfer.
   */
  head >= tail ? (contiguous = head - tail) : (contiguous = rb->capacity - tail);

  return contiguous;
}

// Advances the ring buffer tail
bool RingBuffer_AdvanceTail(RingBuffer_t *rb, uint16_t len)
{
  // Return false if the ring buffer is not valid or no data is provided
  if ((rb == NULL) || (rb->buffer == NULL) || (len == 0U)) {
    return false;
  }

  // Return false if there is not enough data to consume
  if (len > RingBuffer_GetUsed(rb)) {
    return false;
  }

  // Update the ring buffer read pointer
  rb->tail = (uint16_t)((rb->tail + len) % rb->capacity);

  return true;
}


