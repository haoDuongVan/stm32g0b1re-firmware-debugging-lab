/*
 * hid_keyboard_report.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "hid_keyboard_report.h"

#include <string.h>

/* Function definitions ------------------------------------------------------*/

// Zero all bytes in the report (all keys released, no modifiers)
void HidKeyboardReport_Clear(HidKeyboardReport_t *report)
{
  if (report == NULL)
  {
    return;
  }

  memset(report->bytes, 0, sizeof(report->bytes));
}

// Set modifier + one key usage in slot 2; clears all other key slots first
void HidKeyboardReport_SetKey(HidKeyboardReport_t *report,
                              uint8_t modifier,
                              uint8_t usage)
{
  if (report == NULL)
  {
    return;
  }

  HidKeyboardReport_Clear(report);

  report->bytes[0] = modifier;
  report->bytes[1] = 0x00U;  /* reserved */
  report->bytes[2] = usage;
}

// Fill key slots 2-7 with ErrorRollOver (too many keys pressed simultaneously)
void HidKeyboardReport_SetErrorRollOver(HidKeyboardReport_t *report)
{
  uint8_t i;

  if (report == NULL)
  {
    return;
  }

  HidKeyboardReport_Clear(report);

  for (i = 2U; i < HID_KEYBOARD_REPORT_SIZE; i++)
  {
    report->bytes[i] = HID_KEYBOARD_ERROR_ROLLOVER;
  }
}

// Return a read-only pointer to the raw report bytes for USB transmission
const uint8_t *HidKeyboardReport_GetData(const HidKeyboardReport_t *report)
{
  if (report == NULL)
  {
    return NULL;
  }

  return report->bytes;
}
