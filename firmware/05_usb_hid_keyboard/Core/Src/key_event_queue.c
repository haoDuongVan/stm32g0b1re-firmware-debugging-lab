/*
 * key_event_queue.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "key_event_queue.h"
#include <stddef.h>

/* Private variables ---------------------------------------------------------*/
static KeyEvent_t gQueue[KEY_EVENT_QUEUE_SIZE];
static uint8_t    gHead  = 0U;  /* next write position */
static uint8_t    gTail  = 0U;  /* next read position  */
static uint8_t    gCount = 0U;

/* Function definitions ------------------------------------------------------*/

// Reset queue to empty; call once before use
void KeyEventQueue_Init(void)
{
  gHead  = 0U;
  gTail  = 0U;
  gCount = 0U;
}

// Push one event; returns false if queue is full or event is NULL
bool KeyEventQueue_Push(const KeyEvent_t *event)
{
  if (event == NULL)
  {
    return false;
  }

  if (KeyEventQueue_IsFull())
  {
    return false;
  }

  gQueue[gHead] = *event;

  gHead++;
  if (gHead >= KEY_EVENT_QUEUE_SIZE)
  {
    gHead = 0U;
  }

  gCount++;

  return true;
}

// Pop the oldest event into *event; returns false if queue is empty
// Passing NULL for event discards the front element without copying it
bool KeyEventQueue_Pop(KeyEvent_t *event)
{
  if (KeyEventQueue_IsEmpty())
  {
    return false;
  }

  if (event != NULL)
  {
    *event = gQueue[gTail];
  }

  gTail++;
  if (gTail >= KEY_EVENT_QUEUE_SIZE)
  {
    gTail = 0U;
  }

  gCount--;

  return true;
}

// Copy the oldest event without removing it; returns false if queue is empty or event is NULL
bool KeyEventQueue_Peek(KeyEvent_t *event)
{
  if (event == NULL)
  {
    return false;
  }

  if (KeyEventQueue_IsEmpty())
  {
    return false;
  }

  *event = gQueue[gTail];

  return true;
}

// Return true if the queue contains no events
bool KeyEventQueue_IsEmpty(void)
{
  return (gCount == 0U);
}

// Return true if the queue has no space for another event
bool KeyEventQueue_IsFull(void)
{
  return (gCount >= KEY_EVENT_QUEUE_SIZE);
}

// Return the number of events currently in the queue
uint8_t KeyEventQueue_GetCount(void)
{
  return gCount;
}
