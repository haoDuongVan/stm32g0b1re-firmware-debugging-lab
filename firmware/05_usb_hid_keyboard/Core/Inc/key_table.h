/*
 * key_table.h
 *
 *  Created on: Jul 2, 2026
 *      Author: haodu
 */

#ifndef INC_KEY_TABLE_H_
#define INC_KEY_TABLE_H_

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/* Exported defines ----------------------------------------------------------*/
#define KEY_TABLE_SIZE  16U

/* HID modifier byte bitmasks (report byte 0) */
#define HID_MOD_NONE        0x00U
#define HID_MOD_LEFT_CTRL   0x01U
#define HID_MOD_LEFT_SHIFT  0x02U
#define HID_MOD_LEFT_ALT    0x04U
#define HID_MOD_LEFT_GUI    0x08U

/* HID keyboard usage IDs (report bytes 2-7) */
#define HID_USAGE_A          0x04U
#define HID_USAGE_B          0x05U
#define HID_USAGE_C          0x06U
#define HID_USAGE_D          0x07U
#define HID_USAGE_S          0x16U
#define HID_USAGE_V          0x19U

#define HID_USAGE_1          0x1EU
#define HID_USAGE_2          0x1FU
#define HID_USAGE_3          0x20U
#define HID_USAGE_4          0x21U

#define HID_USAGE_ENTER      0x28U
#define HID_USAGE_ESCAPE     0x29U
#define HID_USAGE_BACKSPACE  0x2AU
#define HID_USAGE_TAB        0x2BU
#define HID_USAGE_SPACE      0x2CU

/* Exported types ------------------------------------------------------------*/

// Determines how the key event is processed by the application
typedef enum
{
  KEY_KIND_NORMAL = 0,  /* single keystroke: modifier + usage */
  KEY_KIND_MACRO,       /* multi-step sequence identified by macroId */
  KEY_KIND_SPECIAL      /* reserved for layer switch, media keys, etc. */
} KeyKind_t;

// Identifies the macro sequence to execute for KEY_KIND_MACRO entries
typedef enum
{
  MACRO_NONE    = 0,
  MACRO_CTRL_C,
  MACRO_CTRL_V,
  MACRO_CTRL_S,
  MACRO_ALT_TAB
} MacroId_t;

/*
 * One entry in the key table.
 * keyLoc (physical position 0-15) maps directly to the table index.
 */
typedef struct
{
  KeyKind_t kind;
  uint8_t   modifier;
  uint8_t   usage;
  MacroId_t macroId;
  uint8_t   repeatEnable;  /* 1 = key repeat allowed, 0 = fire-once only */
} KeyTableEntry_t;

/* Function prototypes -------------------------------------------------------*/

// Return true if keyLoc is within the valid table range
bool KeyTable_IsValidKeyLoc(uint8_t keyLoc);

// Return a pointer to the table entry for keyLoc, or NULL if out of range
const KeyTableEntry_t *KeyTable_Get(uint8_t keyLoc);

#endif /* INC_KEY_TABLE_H_ */
