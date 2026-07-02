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

/* Private variables ---------------------------------------------------------*/
static HidKeyboardReport_t gReport;

/* Private function prototypes -----------------------------------------------*/
static bool HidKeyboardConvert_BuildReport(const KeyEvent_t *event,
                                           HidKeyboardReport_t *report);

/* Private functions ---------------------------------------------------------*/

/*
 * Translate one key event into a HID keyboard report.
 * Returns true if the report is ready to send, false if the event should be dropped.
 */
static bool HidKeyboardConvert_BuildReport(const KeyEvent_t *event,
                                           HidKeyboardReport_t *report)
{
  const KeyTableEntry_t *entry;

  if ((event == NULL) || (report == NULL))
  {
    return false;
  }

  switch (event->type) {
  case KEY_EVENT_ON:
  case KEY_EVENT_REPEAT:
    entry = KeyTable_Get(event->keyLoc);

    if (entry == NULL)
    {
      return false;
    }

    if (entry->kind != KEY_KIND_NORMAL)
    {
      /* Macro and special keys handled in a later milestone */
      return false;
    }

    HidKeyboardReport_SetKey(report, entry->modifier, entry->usage);
    return true;

  case KEY_EVENT_OFF:
    /* Single-key boot keyboard: any release sends a null report */
    HidKeyboardReport_Clear(report);
    return true;

  case KEY_EVENT_ERROR:
    /* ErrorRollOver sequence deferred to macro/sequence milestone */
    return false;

  default:
    return false;
  }
}

/* Function definitions ------------------------------------------------------*/

// Clear the internal report state; call once after transport and queue are ready
void HidKeyboardConvert_Init(void)
{
  HidKeyboardReport_Clear(&gReport);
}

/*
 * Check the queue for a pending event, build the corresponding HID report,
 * and forward it to the transport layer.
 * Pop is deferred until SendReport succeeds so no event is silently lost
 * when the endpoint is temporarily busy.
 */
void HidKeyboardConvert_Run(void)
{
  KeyEvent_t event;

  if (!UsbHidTransport_IsIdle())
  {
    return;
  }

  if (!KeyEventQueue_Peek(&event))
  {
    return;
  }

  if (!HidKeyboardConvert_BuildReport(&event, &gReport))
  {
    /* Unsupported event type for this milestone — drop to avoid blocking the queue */
    (void)KeyEventQueue_Pop(NULL);
    return;
  }

  if (UsbHidTransport_SendReport(HidKeyboardReport_GetData(&gReport)))
  {
    /* Pop only after the send request is accepted */
    (void)KeyEventQueue_Pop(NULL);
  }
}
