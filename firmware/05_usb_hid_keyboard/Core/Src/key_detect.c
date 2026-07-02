/*
 * key_detect.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "key_detect.h"

#include "matrix_scan.h"
#include "key_event_queue.h"
#include "key_table.h"
#include <stdbool.h>
#include <stddef.h>

/* Private defines -----------------------------------------------------------*/
#define KEY_DETECT_SCAN_BUFFER_NUM   4U

/*
 * Repeat interval expressed in scan ticks.
 * With a 5 ms scan period, 40 ticks = 200 ms.
 */
#define KEY_DETECT_SCAN_PERIOD_MS    5U
#define KEY_REPEAT_INTERVAL_MS       200U
#define KEY_REPEAT_INTERVAL_TICKS    (KEY_REPEAT_INTERVAL_MS / KEY_DETECT_SCAN_PERIOD_MS)

/* Private variables ---------------------------------------------------------*/
static uint16_t gScanBuffer[KEY_DETECT_SCAN_BUFFER_NUM];
static uint16_t gKeyStatus;
static uint16_t gRepeatTick[MATRIX_KEY_NUM];

/*
 * Set when two or more keys are pressed simultaneously.
 * While active, all normal key detection is suppressed until every
 * physical key has been released (no-diode ghost-key safety policy).
 */
static bool gSimultaneousErrorActive;

/* Private functions ---------------------------------------------------------*/

static void KeyDetect_UpdateScanBuffer(uint16_t rawState)
{
  gScanBuffer[3] = gScanBuffer[2];
  gScanBuffer[2] = gScanBuffer[1];
  gScanBuffer[1] = gScanBuffer[0];
  gScanBuffer[0] = (rawState & MATRIX_KEY_MASK);
}

static bool KeyDetect_PushEvent(KeyEventType_t type, uint8_t keyLoc)
{
  KeyEvent_t event;

  event.type   = type;
  event.keyLoc = keyLoc;

  return KeyEventQueue_Push(&event);
}

static void KeyDetect_PushErrorRollOver(void)
{
  (void)KeyDetect_PushEvent(KEY_EVENT_ERROR, KEY_LOC_ERROR_ROLLOVER);
}

static void KeyDetect_ResetRepeatState(uint8_t keyLoc)
{
  if (keyLoc < MATRIX_KEY_NUM)
  {
    gRepeatTick[keyLoc] = 0U;
  }
}

static void KeyDetect_ResetAllRepeatStates(void)
{
  uint8_t keyLoc;

  for (keyLoc = 0U; keyLoc < MATRIX_KEY_NUM; keyLoc++)
  {
    gRepeatTick[keyLoc] = 0U;
  }
}

static void KeyDetect_ResetScanBuffer(void)
{
  uint8_t i;

  for (i = 0U; i < KEY_DETECT_SCAN_BUFFER_NUM; i++)
  {
    gScanBuffer[i] = 0U;
  }
}

static void KeyDetect_ReleaseAllKeys(void)
{
  uint8_t keyLoc;

  for (keyLoc = 0U; keyLoc < MATRIX_KEY_NUM; keyLoc++)
  {
    uint16_t bit = (uint16_t)(1U << keyLoc);

    if ((gKeyStatus & bit) != 0U)
    {
      gKeyStatus &= (uint16_t)(~bit);
      KeyDetect_ResetRepeatState(keyLoc);
      (void)KeyDetect_PushEvent(KEY_EVENT_OFF, keyLoc);
    }
  }
}

static void KeyDetect_CheckKeyOff(uint16_t stableOffMask)
{
  uint16_t offMask = (stableOffMask & gKeyStatus);
  uint8_t  keyLoc;

  for (keyLoc = 0U; keyLoc < MATRIX_KEY_NUM; keyLoc++)
  {
    uint16_t bit = (uint16_t)(1U << keyLoc);

    if ((offMask & bit) != 0U)
    {
      gKeyStatus &= (uint16_t)(~bit);
      KeyDetect_ResetRepeatState(keyLoc);
      (void)KeyDetect_PushEvent(KEY_EVENT_OFF, keyLoc);
    }
  }
}

/*
 * Generate KEY_EVENT_REPEAT for each key that:
 *   - is already in pressed state (gKeyStatus bit set)
 *   - is still stable ON in the current debounce result
 *   - has repeatEnable = 1 and kind = KEY_KIND_NORMAL in the key table
 *
 * Called between CheckKeyOff and CheckKeyOn so a newly pressed key
 * cannot trigger a repeat in the same scan cycle it was detected.
 */
static void KeyDetect_CheckRepeat(uint16_t stableOnMask)
{
  const KeyTableEntry_t *entry;
  uint8_t                keyLoc;

  for (keyLoc = 0U; keyLoc < MATRIX_KEY_NUM; keyLoc++)
  {
    uint16_t bit = (uint16_t)(1U << keyLoc);

    if (((gKeyStatus & bit) == 0U) || ((stableOnMask & bit) == 0U))
    {
      continue;
    }

    entry = KeyTable_Get(keyLoc);

    if ((entry == NULL) || (entry->repeatEnable == 0U) || (entry->kind != KEY_KIND_NORMAL))
    {
      KeyDetect_ResetRepeatState(keyLoc);
      continue;
    }

    if (gRepeatTick[keyLoc] < KEY_REPEAT_INTERVAL_TICKS)
    {
      gRepeatTick[keyLoc]++;
    }

    if (gRepeatTick[keyLoc] >= KEY_REPEAT_INTERVAL_TICKS)
    {
      if (KeyDetect_PushEvent(KEY_EVENT_REPEAT, keyLoc))
      {
        gRepeatTick[keyLoc] = 0U;
      }
    }
  }
}

static void KeyDetect_CheckKeyOn(uint16_t stableOnMask)
{
  uint16_t onMask = (stableOnMask & (uint16_t)(~gKeyStatus));
  uint8_t  keyLoc;

  for (keyLoc = 0U; keyLoc < MATRIX_KEY_NUM; keyLoc++)
  {
    uint16_t bit = (uint16_t)(1U << keyLoc);

    if ((onMask & bit) != 0U)
    {
      gKeyStatus |= bit;
      KeyDetect_ResetRepeatState(keyLoc);
      (void)KeyDetect_PushEvent(KEY_EVENT_ON, keyLoc);
    }
  }
}

/* Function definitions ------------------------------------------------------*/

// Clear all state; call once after MatrixScan_Init
void KeyDetect_Init(void)
{
  uint8_t i;

  for (i = 0U; i < KEY_DETECT_SCAN_BUFFER_NUM; i++)
  {
    gScanBuffer[i] = 0U;
  }

  gKeyStatus               = 0U;
  gSimultaneousErrorActive = false;

  KeyDetect_ResetAllRepeatStates();
}

void KeyDetect_Run(void)
{
  uint16_t rawState;
  uint16_t stableOnMask;
  uint16_t stableOffMask;
  uint8_t  pressedCount;

  rawState     = MatrixScan_ReadRaw();
  pressedCount = MatrixScan_CountPressed(rawState);

  /*
   * Strict simultaneous-error latch (no-diode safety policy):
   *
   * Once a simultaneous error is active, ALL key input is ignored until
   * every physical key is released. A single key remaining after releasing
   * one finger does NOT re-trigger normal detection — the user must lift
   * everything before the firmware accepts new input.
   */
  if (gSimultaneousErrorActive)
  {
    if (pressedCount == 0U)
    {
      KeyDetect_ResetScanBuffer();
      KeyDetect_ResetAllRepeatStates();
      gKeyStatus               = 0U;
      gSimultaneousErrorActive = false;
    }

    return;
  }

  /*
   * Enter simultaneous error state.
   * Cancel all currently held keys immediately so no phantom key lingers
   * while waiting for full release.
   */
  if (pressedCount >= 2U)
  {
    KeyDetect_PushErrorRollOver();
    KeyDetect_ReleaseAllKeys();
    KeyDetect_ResetScanBuffer();
    KeyDetect_ResetAllRepeatStates();
    gKeyStatus               = 0U;
    gSimultaneousErrorActive = true;

    return;
  }

  KeyDetect_UpdateScanBuffer(rawState);

  /*
   * Debounce policy:
   * - ON  is accepted when the key appears in the 2 newest scan buffers.
   * - OFF is accepted when the key is absent from the 2 newest scan buffers.
   */
  stableOnMask  = (uint16_t)( gScanBuffer[0] &  gScanBuffer[1]);
  stableOffMask = (uint16_t)(~(gScanBuffer[0] | gScanBuffer[1]) & MATRIX_KEY_MASK);

  /*
   * Detection order matters:
   * 1. Key off first  — free held state before new keys can fire.
   * 2. Repeat for already-held keys — must come before key-on so a new
   *    key cannot repeat in the same cycle it is first detected.
   * 3. Key on last.
   */
  KeyDetect_CheckKeyOff(stableOffMask);
  KeyDetect_CheckRepeat(stableOnMask);
  KeyDetect_CheckKeyOn(stableOnMask);
}

// Return the current debounced key status bitmask (bit N = keyLoc N is held)
uint16_t KeyDetect_GetKeyStatus(void)
{
  return gKeyStatus;
}
