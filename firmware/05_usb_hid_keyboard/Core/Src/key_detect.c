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
#include <stdbool.h>

/* Private defines -----------------------------------------------------------*/
#define KEY_DETECT_SCAN_BUFFER_NUM  4U

/* Private variables ---------------------------------------------------------*/
static uint16_t gScanBuffer[KEY_DETECT_SCAN_BUFFER_NUM];
static uint16_t gKeyStatus;

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

static void KeyDetect_PushEvent(KeyEventType_t type, uint8_t keyLoc)
{
  KeyEvent_t event;

  event.type   = type;
  event.keyLoc = keyLoc;

  (void)KeyEventQueue_Push(&event);
}

static void KeyDetect_PushErrorRollOver(void)
{
  KeyDetect_PushEvent(KEY_EVENT_ERROR, KEY_LOC_ERROR_ROLLOVER);
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
      KeyDetect_PushEvent(KEY_EVENT_OFF, keyLoc);
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
      KeyDetect_PushEvent(KEY_EVENT_OFF, keyLoc);
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
      KeyDetect_PushEvent(KEY_EVENT_ON, keyLoc);
    }
  }
}

/* Function definitions ------------------------------------------------------*/

// Clear scan buffers, key status, and error flag; call once after MatrixScan_Init
void KeyDetect_Init(void)
{
  uint8_t i;

  for (i = 0U; i < KEY_DETECT_SCAN_BUFFER_NUM; i++)
  {
    gScanBuffer[i] = 0U;
  }

  gKeyStatus              = 0U;
  gSimultaneousErrorActive = false;
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
   * No-diode matrix safety policy:
   * If two or more keys are pressed at the same scan, do not try to guess.
   * Send ErrorRollOver once, then wait until all keys are released.
   */
  if (pressedCount >= 2U)
  {
    if (!gSimultaneousErrorActive)
    {
      KeyDetect_PushErrorRollOver();
      gSimultaneousErrorActive = true;
    }

    return;
  }

  /*
   * After a simultaneous error, wait for every key to be released before
   * resuming normal detection. Do not fire on a single remaining key when
   * the user lifts only one finger from a multi-key hold.
   */
  if (gSimultaneousErrorActive)
  {
    if (pressedCount != 0U)
    {
      return;
    }

    KeyDetect_ReleaseAllKeys();
    KeyDetect_ResetScanBuffer();
    gSimultaneousErrorActive = false;

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
   * 1. Key off first  — free any held slot before a new key occupies it.
   * 2. Key on last.
   * (Repeat added in a later milestone.)
   */
  KeyDetect_CheckKeyOff(stableOffMask);
  KeyDetect_CheckKeyOn(stableOnMask);
}

// Return the current debounced key status bitmask (bit N = keyLoc N is held)
uint16_t KeyDetect_GetKeyStatus(void)
{
  return gKeyStatus;
}
