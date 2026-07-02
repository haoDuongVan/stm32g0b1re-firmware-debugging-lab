/*
 * hid_keyboard_convert.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "hid_keyboard_convert.h"

#include "hid_keyboard_app.h"
#include "hid_keyboard_report.h"
#include "key_event_queue.h"
#include "key_table.h"

#include <stddef.h>
#include <stdbool.h>

/* Private variables ---------------------------------------------------------*/
static HidKeyboardReport_t gReport;
static bool                gNeedNullReport;

/* Private function prototypes -----------------------------------------------*/
static bool HidKeyboardConvert_BuildKeyReport(const KeyEvent_t *event,
                                              HidKeyboardReport_t *report);

/* Private functions ---------------------------------------------------------*/

/*
 * Translate a key ON/REPEAT event into a HID keyboard key-down report.
 * OFF is handled separately because this firmware uses tap-style output:
 *
 *   key down → null report
 *
 * This avoids OS typematic repeat when a physical key is held.
 */
static bool HidKeyboardConvert_BuildKeyReport(const KeyEvent_t *event,
                                              HidKeyboardReport_t *report)
{
  const KeyTableEntry_t *entry;

  if ((event == NULL) || (report == NULL))
  {
    return false;
  }

  if ((event->type != KEY_EVENT_ON) &&
      (event->type != KEY_EVENT_REPEAT))
  {
    return false;
  }

  entry = KeyTable_Get(event->keyLoc);

  if (entry == NULL)
  {
    return false;
  }

  if (entry->kind != KEY_KIND_NORMAL)
  {
    /* Macro and special keys will be handled in a later milestone */
    return false;
  }

  HidKeyboardReport_SetKey(report, entry->modifier, entry->usage);

  return true;
}

/* Function definitions ------------------------------------------------------*/

// Clear the internal report state and null-report flag; call once before use
void HidKeyboardConvert_Init(void)
{
  HidKeyboardReport_Clear(&gReport);
  gNeedNullReport = false;
}

/*
 * Tap-style keyboard output:
 * after sending a key-down report, send a null report as soon as the
 * endpoint becomes idle. This prevents the host OS from treating the key
 * as physically held and triggering typematic repeat.
 *
 * KEY_EVENT_OFF is dropped here; the key-status update lives in key_detect.c.
 */
void HidKeyboardConvert_Run(void)
{
  KeyEvent_t event;

  if (!UsbHidTransport_IsIdle())
  {
    return;
  }

  if (gNeedNullReport)
  {
    HidKeyboardReport_Clear(&gReport);

    if (UsbHidTransport_SendReport(HidKeyboardReport_GetData(&gReport)))
    {
      gNeedNullReport = false;
    }

    return;
  }

  if (!KeyEventQueue_Peek(&event))
  {
    return;
  }

  switch (event.type)
  {
  case KEY_EVENT_ON:
  case KEY_EVENT_REPEAT:
    if (!HidKeyboardConvert_BuildKeyReport(&event, &gReport))
    {
      /* Unsupported event for current milestone — drop to unblock queue */
      (void)KeyEventQueue_Pop(NULL);
      return;
    }

    if (UsbHidTransport_SendReport(HidKeyboardReport_GetData(&gReport)))
    {
      /* Pop only after send is accepted, then schedule null report */
      (void)KeyEventQueue_Pop(NULL);
      gNeedNullReport = true;
    }
    break;

  case KEY_EVENT_OFF:
    /*
     * Release report is generated automatically after KEY_EVENT_ON/REPEAT.
     * Physical OFF only updates key state in key_detect.c — drop here.
     */
    (void)KeyEventQueue_Pop(NULL);
    break;

  case KEY_EVENT_ERROR:
    if (event.keyLoc != KEY_LOC_ERROR_ROLLOVER)
    {
      /* Unknown error variant — drop to unblock queue */
      (void)KeyEventQueue_Pop(NULL);
      break;
    }

    HidKeyboardReport_SetErrorRollOver(&gReport);

    if (UsbHidTransport_SendReport(HidKeyboardReport_GetData(&gReport)))
    {
      /* Pop only after ErrorRollOver is accepted, then release via null report */
      (void)KeyEventQueue_Pop(NULL);
      gNeedNullReport = true;
    }
    break;

  default:
    (void)KeyEventQueue_Pop(NULL);
    break;
  }
}
