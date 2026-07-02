/*
 * key_table.c
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "key_table.h"

/* Private variables ---------------------------------------------------------*/

/*
 * Physical key location → HID entry map.
 * Index = keyLoc (0-15), matching the 4×4 matrix scan order:
 *   row 0: keyLoc 0-3
 *   row 1: keyLoc 4-7
 *   row 2: keyLoc 8-11
 *   row 3: keyLoc 12-15
 */
static const KeyTableEntry_t gKeyTable[KEY_TABLE_SIZE] =
{
  /* row 0: keyLoc 0-3 → digits 1 2 3 4 */
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_1,         MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_2,         MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_3,         MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_4,         MACRO_NONE,    1U },

  /* row 1: keyLoc 4-7 → letters a b c d */
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_A,         MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_B,         MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_C,         MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_D,         MACRO_NONE,    1U },

  /* row 2: keyLoc 8-11 → Enter Space Backspace Tab */
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_ENTER,     MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_SPACE,     MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_BACKSPACE, MACRO_NONE,    1U },
  { KEY_KIND_NORMAL, HID_MOD_NONE, HID_USAGE_TAB,       MACRO_NONE,    1U },

  /* row 3: keyLoc 12-15 → Ctrl+C Ctrl+V Ctrl+S Alt+Tab (fire-once macros) */
  { KEY_KIND_MACRO,  HID_MOD_NONE, 0x00U,               MACRO_CTRL_C,  0U },
  { KEY_KIND_MACRO,  HID_MOD_NONE, 0x00U,               MACRO_CTRL_V,  0U },
  { KEY_KIND_MACRO,  HID_MOD_NONE, 0x00U,               MACRO_CTRL_S,  0U },
  { KEY_KIND_MACRO,  HID_MOD_NONE, 0x00U,               MACRO_ALT_TAB, 0U },
};

/* Function definitions ------------------------------------------------------*/

// Return true if keyLoc is within the valid table range
bool KeyTable_IsValidKeyLoc(uint8_t keyLoc)
{
  return (keyLoc < KEY_TABLE_SIZE);
}

// Return a pointer to the table entry for keyLoc, or NULL if out of range
const KeyTableEntry_t *KeyTable_Get(uint8_t keyLoc)
{
  if (!KeyTable_IsValidKeyLoc(keyLoc))
  {
    return NULL;
  }

  return &gKeyTable[keyLoc];
}
