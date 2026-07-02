/*
 * key_event_queue.h
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

#ifndef INC_KEY_EVENT_QUEUE_H_
#define INC_KEY_EVENT_QUEUE_H_

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/
#define KEY_EVENT_QUEUE_SIZE   32U

/*
 * Special keyLoc values that do not map to a physical key.
 * Used to signal error conditions through the normal event path.
 */
#define KEY_LOC_ERROR_ROLLOVER 0xFEU  /* too many keys pressed simultaneously */
#define KEY_LOC_ALL_OFF        0xFFU  /* release all keys */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  KEY_EVENT_ON = 0,  /* key pressed */
  KEY_EVENT_OFF,     /* key released */
  KEY_EVENT_REPEAT,  /* key held past repeat threshold */
  KEY_EVENT_ERROR    /* error condition (rollover, etc.) */
} KeyEventType_t;

typedef struct
{
  KeyEventType_t type;
  uint8_t        keyLoc;
} KeyEvent_t;

/* Function prototypes -------------------------------------------------------*/
void    KeyEventQueue_Init(void);
bool    KeyEventQueue_Push(const KeyEvent_t *event);
bool    KeyEventQueue_Pop(KeyEvent_t *event);
bool    KeyEventQueue_Peek(KeyEvent_t *event);
bool    KeyEventQueue_IsEmpty(void);
bool    KeyEventQueue_IsFull(void);
uint8_t KeyEventQueue_GetCount(void);

#endif /* INC_KEY_EVENT_QUEUE_H_ */
