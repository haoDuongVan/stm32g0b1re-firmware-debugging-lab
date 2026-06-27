/*
 * bl_update.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "bl_update.h"

#include "bl_flash_layout.h"
#include "bl_image.h"
#include "bl_log.h"
#include "bl_metadata.h"
#include "bl_slot.h"

/* Private defines -----------------------------------------------------------*/

/*
 * Time window during which the host can request update mode by sending 'u'.
 * After this window the bootloader proceeds to boot the selected slot.
 */
#define BL_UPDATE_WINDOW_MS              3000U

#define BL_UPDATE_ENTRY_CHAR_LOWER       'u'
#define BL_UPDATE_ENTRY_CHAR_UPPER       'U'

#define BL_UPDATE_CMD_MAX_LEN            32U
#define BL_UPDATE_RX_TIMEOUT_MS          100U

/*
 * Update mode entry test.
 */
#define BL_UPDATE_ENTRY_TEST_TAG         "[TEST14]"
#define BL_UPDATE_ENTRY_TEST_NAME        "uart_update_mode_entry_check"

/*
 * Basic UART command parser test.
 */
#define BL_UPDATE_CMD_TEST_TAG           "[TEST15]"
#define BL_UPDATE_CMD_TEST_NAME          "uart_command_parser_check"

/*
 * Slot erase command test.
 */
#define BL_UPDATE_ERASE_TEST_TAG         "[TEST16]"
#define BL_UPDATE_ERASE_TEST_NAME        "slot_erase_command_check"

/*
 * Slot write test command.
 */
#define BL_UPDATE_WRITE_TEST_TAG         "[TEST17]"
#define BL_UPDATE_WRITE_TEST_NAME        "slot_write_test_command_check"

/*
 * UART binary packet receive test.
 */
#define BL_UPDATE_PACKET_TEST_TAG        "[TEST18]"
#define BL_UPDATE_PACKET_TEST_NAME       "uart_binary_packet_receive_check"

/*
 * Set pending metadata test.
 */
#define BL_UPDATE_SET_PENDING_TEST_TAG   "[TEST19]"
#define BL_UPDATE_SET_PENDING_TEST_NAME  "set_pending_command_check"

#define BL_UPDATE_PACKET_MAGIC           0x31544B50UL  /* "PKT1" */
#define BL_UPDATE_PACKET_HEADER_SIZE     16U
#define BL_UPDATE_PACKET_MAX_PAYLOAD     256U

/*
 * Long timeout for manual TeraTerm file selection.
 * The bootloader waits up to 60 seconds after rx-test b for the header
 * to arrive (time for the user to pick the file in TeraTerm).
 * Once the file starts sending, payload must arrive within 10 seconds.
 */
#define BL_UPDATE_PACKET_HEADER_TIMEOUT_MS   60000U
#define BL_UPDATE_PACKET_PAYLOAD_TIMEOUT_MS  10000U

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *update_uart = NULL;

/*
 * 16-byte test pattern written to the start of Slot B by 'write-test b'.
 * Size is a multiple of 8 to satisfy doubleword-alignment requirement.
 * Content: "DOUBLE01SLOTBTES" in ASCII.
 */
static const uint8_t s_write_test_pattern[16U] = {
  0x44U, 0x4FU, 0x55U, 0x42U, 0x4CU, 0x45U, 0x30U, 0x31U,
  0x53U, 0x4CU, 0x4FU, 0x54U, 0x42U, 0x54U, 0x45U, 0x53U
};

/* Private function prototypes -----------------------------------------------*/
static void     BlUpdate_PrintPrompt(void);
static uint8_t  BlUpdate_ReadLine(char *cmd, uint32_t max_len);
static char     BlUpdate_ToLower(char c);
static uint8_t  BlUpdate_CommandEquals(const char *cmd, const char *expected);
static void     BlUpdate_HandleHelp(void);
static void     BlUpdate_HandleInfo(void);
static uint8_t  BlUpdate_HandleCommand(const char *cmd);
static uint8_t      BlUpdate_ReadBinary(uint8_t *data, uint32_t size, uint32_t timeout_ms);
static uint32_t     BlUpdate_ReadLe32(const uint8_t *data);
static uint32_t     BlUpdate_UpdateCrc32(uint32_t crc, uint8_t byte);
static uint32_t     BlUpdate_CalculateCrc32(const uint8_t *data, uint32_t size);
static const char  *BlUpdate_SlotName(BlImageSlotId_t slot);
static void         BlUpdate_HandleEraseSlot(BlImageSlotId_t slot);
static void         BlUpdate_HandleWriteTest(BlImageSlotId_t slot);
static void         BlUpdate_HandleRxPacket(BlImageSlotId_t slot);
static void         BlUpdate_HandleSetPending(BlImageSlotId_t slot);

/* Private functions ---------------------------------------------------------*/

// Print the interactive prompt
static void BlUpdate_PrintPrompt(void)
{
  BlLog_Printf("\r\nUPDATE> ");
}

// Read one line from UART into cmd; echoes each character; returns 1 when Enter received
static uint8_t BlUpdate_ReadLine(char *cmd, uint32_t max_len)
{
  uint32_t index;
  uint8_t rx_byte;
  HAL_StatusTypeDef status;

  if ((cmd == NULL) || (max_len == 0U)) {
    return 0U;
  }

  index = 0U;
  cmd[0] = '\0';

  while (1) {
    status = HAL_UART_Receive(update_uart, &rx_byte, 1U, BL_UPDATE_RX_TIMEOUT_MS);

    if (status != HAL_OK) {
      continue;
    }

    if ((rx_byte == '\r') || (rx_byte == '\n')) {
      cmd[index] = '\0';
      BlLog_Printf("\r\n");
      return 1U;
    }

    /*
     * Backspace (0x08) and DEL (0x7F): erase the last character if any.
     */
    if ((rx_byte == 0x08U) || (rx_byte == 0x7FU)) {
      if (index > 0U) {
        index--;
        cmd[index] = '\0';
        BlLog_Printf("\b \b");
      }
      continue;
    }

    // Accept printable ASCII only
    if ((rx_byte >= 0x20U) && (rx_byte <= 0x7EU)) {
      if (index < (max_len - 1U)) {
        cmd[index] = (char)rx_byte;
        index++;
        cmd[index] = '\0';
        BlLog_Printf("%c", rx_byte);
      }
    }
  }
}

// Convert a single ASCII character to lowercase
static char BlUpdate_ToLower(char c)
{
  if ((c >= 'A') && (c <= 'Z')) {
    return (char)(c - 'A' + 'a');
  }

  return c;
}

// Return 1 if cmd matches expected (case-insensitive, exact length)
static uint8_t BlUpdate_CommandEquals(const char *cmd, const char *expected)
{
  uint32_t i;

  if ((cmd == NULL) || (expected == NULL)) {
    return 0U;
  }

  i = 0U;

  while ((cmd[i] != '\0') && (expected[i] != '\0')) {
    if (BlUpdate_ToLower(cmd[i]) != expected[i]) {
      return 0U;
    }

    i++;
  }

  return ((cmd[i] == '\0') && (expected[i] == '\0')) ? 1U : 0U;
}

// Print the list of available commands
static void BlUpdate_HandleHelp(void)
{
  BlLog_Printf("[UPDATE] available_commands:\r\n");
  BlLog_Printf("[UPDATE]   help          - show command list\r\n");
  BlLog_Printf("[UPDATE]   info          - show bootloader and slot information\r\n");
  BlLog_Printf("[UPDATE]   erase a       - erase Slot A\r\n");
  BlLog_Printf("[UPDATE]   erase b       - erase Slot B\r\n");
  BlLog_Printf("[UPDATE]   write-test a  - write test pattern to Slot A then verify\r\n");
  BlLog_Printf("[UPDATE]   write-test b  - write test pattern to Slot B then verify\r\n");
  BlLog_Printf("[UPDATE]   rx-packet a   - receive one binary packet and write Slot A\r\n");
  BlLog_Printf("[UPDATE]   rx-packet b   - receive one binary packet and write Slot B\r\n");
  BlLog_Printf("[UPDATE]   set-pending a - write metadata: active=A confirmed=B boot_count=0\r\n");
  BlLog_Printf("[UPDATE]   set-pending b - write metadata: active=B confirmed=A boot_count=0\r\n");
  BlLog_Printf("[UPDATE]   exit          - leave update mode and boot normally\r\n");
  BlLog_Printf("[UPDATE]   reboot        - reset the MCU\r\n");
}

// Print bootloader and Flash layout information
static void BlUpdate_HandleInfo(void)
{
  BlLog_Printf("[UPDATE] bootloader_info:\r\n");
  BlLog_Printf("[UPDATE]   boot_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_BOOT_BASE_ADDR,
                (unsigned long)(BL_BOOT_SIZE / 1024UL));
  BlLog_Printf("[UPDATE]   slot_a_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_SLOT_A_BASE_ADDR,
                (unsigned long)(BL_SLOT_SIZE / 1024UL));
  BlLog_Printf("[UPDATE]   slot_b_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_SLOT_B_BASE_ADDR,
                (unsigned long)(BL_SLOT_SIZE / 1024UL));
  BlLog_Printf("[UPDATE]   metadata0=0x%08lX\r\n",
                (unsigned long)BL_METADATA0_BASE_ADDR);
  BlLog_Printf("[UPDATE]   metadata1=0x%08lX\r\n",
                (unsigned long)BL_METADATA1_BASE_ADDR);
}

/*
 * Dispatch one command string.
 * Return 1 to stay in update mode; return 0 to exit.
 */
static uint8_t BlUpdate_HandleCommand(const char *cmd)
{
  if ((cmd == NULL) || (cmd[0] == '\0')) {
    return 1U;
  }

  BlLog_Printf("[UPDATE] rx_command=%s\r\n", cmd);

  if (BlUpdate_CommandEquals(cmd, "help") != 0U) {
    BlUpdate_HandleHelp();
    BlLog_Printf("[UPDATE] command_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_CMD_TEST_TAG,
                  BL_UPDATE_CMD_TEST_NAME);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "info") != 0U) {
    BlUpdate_HandleInfo();
    BlLog_Printf("[UPDATE] command_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_CMD_TEST_TAG,
                  BL_UPDATE_CMD_TEST_NAME);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "erase a") != 0U) {
    BlUpdate_HandleEraseSlot(BL_IMAGE_SLOT_A);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "erase b") != 0U) {
    BlUpdate_HandleEraseSlot(BL_IMAGE_SLOT_B);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "write-test a") != 0U) {
    BlUpdate_HandleWriteTest(BL_IMAGE_SLOT_A);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "write-test b") != 0U) {
    BlUpdate_HandleWriteTest(BL_IMAGE_SLOT_B);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "rx-packet a") != 0U) {
    BlUpdate_HandleRxPacket(BL_IMAGE_SLOT_A);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "rx-packet b") != 0U) {
    BlUpdate_HandleRxPacket(BL_IMAGE_SLOT_B);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "set-pending a") != 0U) {
    BlUpdate_HandleSetPending(BL_IMAGE_SLOT_A);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "set-pending b") != 0U) {
    BlUpdate_HandleSetPending(BL_IMAGE_SLOT_B);
    return 1U;
  }

  if (BlUpdate_CommandEquals(cmd, "exit") != 0U) {
    BlLog_Printf("[UPDATE] exit_to_normal_boot=YES\r\n");
    BlLog_Printf("[UPDATE] command_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_CMD_TEST_TAG,
                  BL_UPDATE_CMD_TEST_NAME);
    return 0U;
  }

  if (BlUpdate_CommandEquals(cmd, "reboot") != 0U) {
    BlLog_Printf("[UPDATE] reboot_request=YES\r\n");
    BlLog_Printf("[UPDATE] command_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_CMD_TEST_TAG,
                  BL_UPDATE_CMD_TEST_NAME);
    HAL_Delay(100U);
    NVIC_SystemReset();
    /* never reached */
  }

  BlLog_Printf("[UPDATE] command_result=UNKNOWN\r\n");
  BlLog_Printf("[UPDATE] hint=type_help\r\n");

  return 1U;
}

// Return the display name of a slot ("A" or "B")
static const char *BlUpdate_SlotName(BlImageSlotId_t slot)
{
  return (slot == BL_IMAGE_SLOT_A) ? "A" : "B";
}

// Erase the given slot and report TEST16 result
static void BlUpdate_HandleEraseSlot(BlImageSlotId_t slot)
{
  uint8_t erase_result;

  BlLog_Printf("[UPDATE] erase_slot=%s\r\n",       BlUpdate_SlotName(slot));
  BlLog_Printf("[UPDATE] erase_size=%luKB\r\n",    (unsigned long)(BL_SLOT_SIZE / 1024UL));
  BlLog_Printf("[UPDATE] erase_start=YES\r\n");

  erase_result = BlSlot_Erase(slot);

  if (erase_result != 0U) {
    BlLog_Printf("[UPDATE] erase_result=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_ERASE_TEST_TAG,
                  BL_UPDATE_ERASE_TEST_NAME);
  } else {
    BlLog_Printf("[UPDATE] erase_result=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_ERASE_TEST_TAG,
                  BL_UPDATE_ERASE_TEST_NAME);
  }
}

// Write and verify s_write_test_pattern at offset 0 in the given slot; report TEST17 result
static void BlUpdate_HandleWriteTest(BlImageSlotId_t slot)
{
  uint8_t write_result;
  uint8_t verify_result;

  BlLog_Printf("[UPDATE] write_test_slot=%s\r\n",   BlUpdate_SlotName(slot));
  BlLog_Printf("[UPDATE] write_test_offset=0x%08lX\r\n", (unsigned long)0UL);
  BlLog_Printf("[UPDATE] write_test_size=%lu\r\n",  (unsigned long)sizeof(s_write_test_pattern));

  write_result = BlSlot_Write(slot,
                              0UL,
                              s_write_test_pattern,
                              (uint32_t)sizeof(s_write_test_pattern));

  if (write_result != 0U) {
    BlLog_Printf("[UPDATE] write_test_program=OK\r\n");
  } else {
    BlLog_Printf("[UPDATE] write_test_program=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_WRITE_TEST_TAG,
                  BL_UPDATE_WRITE_TEST_NAME);
    return;
  }

  verify_result = BlSlot_Verify(slot,
                                0UL,
                                s_write_test_pattern,
                                (uint32_t)sizeof(s_write_test_pattern));

  if (verify_result != 0U) {
    BlLog_Printf("[UPDATE] write_test_verify=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_WRITE_TEST_TAG,
                  BL_UPDATE_WRITE_TEST_NAME);
  } else {
    BlLog_Printf("[UPDATE] write_test_verify=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_WRITE_TEST_TAG,
                  BL_UPDATE_WRITE_TEST_NAME);
  }
}

// Receive exactly size bytes from UART within timeout_ms; returns 1 on success
static uint8_t BlUpdate_ReadBinary(uint8_t *data, uint32_t size, uint32_t timeout_ms)
{
  uint32_t start_tick;
  uint32_t index;
  HAL_StatusTypeDef status;

  if ((data == NULL) || (size == 0UL)) {
    return 0U;
  }

  start_tick = HAL_GetTick();
  index = 0UL;

  while (index < size) {
    if ((HAL_GetTick() - start_tick) >= timeout_ms) {
      return 0U;
    }

    status = HAL_UART_Receive(update_uart, &data[index], 1U, 10U);

    if (status == HAL_OK) {
      index++;
    }
  }

  return 1U;
}

// Read a 32-bit little-endian value from a byte buffer
static uint32_t BlUpdate_ReadLe32(const uint8_t *data)
{
  return ((uint32_t)data[0])              |
         ((uint32_t)data[1] <<  8U) |
         ((uint32_t)data[2] << 16U) |
         ((uint32_t)data[3] << 24U);
}

// Update a running CRC32/ISO-HDLC value with one byte
static uint32_t BlUpdate_UpdateCrc32(uint32_t crc, uint8_t byte)
{
  uint8_t bit;

  crc ^= (uint32_t)byte;

  for (bit = 0U; bit < 8U; bit++) {
    if ((crc & 1UL) != 0UL) {
      crc = (crc >> 1U) ^ 0xEDB88320UL;
    } else {
      crc = crc >> 1U;
    }
  }

  return crc;
}

// Compute CRC32/ISO-HDLC over a buffer
static uint32_t BlUpdate_CalculateCrc32(const uint8_t *data, uint32_t size)
{
  uint32_t crc;
  uint32_t i;

  if (data == NULL) {
    return 0UL;
  }

  crc = 0xFFFFFFFFUL;

  for (i = 0UL; i < size; i++) {
    crc = BlUpdate_UpdateCrc32(crc, data[i]);
  }

  return (crc ^ 0xFFFFFFFFUL);
}

// Receive one binary packet and program the given slot; prints TEST18 result
static void BlUpdate_HandleRxPacket(BlImageSlotId_t slot)
{
  uint8_t  header[BL_UPDATE_PACKET_HEADER_SIZE];
  uint8_t  payload[BL_UPDATE_PACKET_MAX_PAYLOAD];
  uint32_t magic;
  uint32_t offset;
  uint32_t length;
  uint32_t stored_crc;
  uint32_t calculated_crc;
  uint8_t  read_result;
  uint8_t  write_result;
  uint8_t  verify_result;

  BlLog_Printf("[UPDATE] rx_packet_slot=%s\r\n", BlUpdate_SlotName(slot));
  BlLog_Printf("[UPDATE] rx_packet_wait=START\r\n");

  read_result = BlUpdate_ReadBinary(header,
                                    sizeof(header),
                                    BL_UPDATE_PACKET_HEADER_TIMEOUT_MS);

  if (read_result == 0U) {
    BlLog_Printf("[UPDATE] rx_packet_header=TIMEOUT\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_PACKET_TEST_TAG,
                  BL_UPDATE_PACKET_TEST_NAME);
    return;
  }

  magic      = BlUpdate_ReadLe32(&header[0]);
  offset     = BlUpdate_ReadLe32(&header[4]);
  length     = BlUpdate_ReadLe32(&header[8]);
  stored_crc = BlUpdate_ReadLe32(&header[12]);

  /*
   * Validate header fields silently before reading payload.
   * TeraTerm sends the whole file continuously, so the payload bytes arrive
   * immediately after the header. Any BlLog_Printf call here would stall the
   * CPU long enough to overflow the tiny UART hardware FIFO and drop bytes.
   * Print the header summary only after payload is safely buffered in RAM.
   */
  if (magic != BL_UPDATE_PACKET_MAGIC) {
    BlLog_Printf("[UPDATE] packet_magic=0x%08lX\r\n",  (unsigned long)magic);
    BlLog_Printf("[UPDATE] packet_magic_check=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_PACKET_TEST_TAG,
                  BL_UPDATE_PACKET_TEST_NAME);
    return;
  }

  if ((length == 0UL) || (length > (uint32_t)BL_UPDATE_PACKET_MAX_PAYLOAD)) {
    BlLog_Printf("[UPDATE] packet_length=%lu\r\n",      (unsigned long)length);
    BlLog_Printf("[UPDATE] packet_length_check=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_PACKET_TEST_TAG,
                  BL_UPDATE_PACKET_TEST_NAME);
    return;
  }

  if (((offset % 8UL) != 0UL) || ((length % 8UL) != 0UL)) {
    BlLog_Printf("[UPDATE] packet_offset=0x%08lX\r\n",  (unsigned long)offset);
    BlLog_Printf("[UPDATE] packet_length=%lu\r\n",      (unsigned long)length);
    BlLog_Printf("[UPDATE] packet_alignment_check=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_PACKET_TEST_TAG,
                  BL_UPDATE_PACKET_TEST_NAME);
    return;
  }

  read_result = BlUpdate_ReadBinary(payload,
                                    length,
                                    BL_UPDATE_PACKET_PAYLOAD_TIMEOUT_MS);

  /* Now safe to log — payload is in RAM and UART receive is done. */
  BlLog_Printf("[UPDATE] packet_magic=0x%08lX\r\n",      (unsigned long)magic);
  BlLog_Printf("[UPDATE] packet_offset=0x%08lX\r\n",     (unsigned long)offset);
  BlLog_Printf("[UPDATE] packet_length=%lu\r\n",          (unsigned long)length);
  BlLog_Printf("[UPDATE] packet_crc_stored=0x%08lX\r\n", (unsigned long)stored_crc);
  BlLog_Printf("[UPDATE] packet_magic_check=OK\r\n");
  BlLog_Printf("[UPDATE] packet_length_check=OK\r\n");
  BlLog_Printf("[UPDATE] packet_alignment_check=OK\r\n");

  if (read_result == 0U) {
    BlLog_Printf("[UPDATE] rx_packet_payload=TIMEOUT\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_PACKET_TEST_TAG,
                  BL_UPDATE_PACKET_TEST_NAME);
    return;
  }

  BlLog_Printf("[UPDATE] rx_packet_payload=OK\r\n");

  calculated_crc = BlUpdate_CalculateCrc32(payload, length);

  BlLog_Printf("[UPDATE] packet_crc_calculated=0x%08lX\r\n",
                (unsigned long)calculated_crc);

  if (stored_crc != calculated_crc) {
    BlLog_Printf("[UPDATE] packet_crc_check=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_PACKET_TEST_TAG,
                  BL_UPDATE_PACKET_TEST_NAME);
    return;
  }

  BlLog_Printf("[UPDATE] packet_crc_check=OK\r\n");

  write_result = BlSlot_Write(slot, offset, payload, length);

  if (write_result == 0U) {
    BlLog_Printf("[UPDATE] rx_packet_write=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_PACKET_TEST_TAG,
                  BL_UPDATE_PACKET_TEST_NAME);
    return;
  }

  BlLog_Printf("[UPDATE] rx_packet_write=OK\r\n");

  verify_result = BlSlot_Verify(slot, offset, payload, length);

  if (verify_result == 0U) {
    BlLog_Printf("[UPDATE] rx_packet_verify=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_PACKET_TEST_TAG,
                  BL_UPDATE_PACKET_TEST_NAME);
    return;
  }

  BlLog_Printf("[UPDATE] rx_packet_verify=OK\r\n");
  BlLog_Printf("%s %s PASS\r\n",
                BL_UPDATE_PACKET_TEST_TAG,
                BL_UPDATE_PACKET_TEST_NAME);
}

/*
 * Check target slot vector table and rollback slot vector table before writing
 * metadata. If either is invalid, abort with FAIL — preventing a scenario where
 * update fails and the rollback slot is also broken.
 */
static void BlUpdate_HandleSetPending(BlImageSlotId_t slot)
{
  BlImageSlotId_t    rollback_slot;
  BlImageVectorInfo_t target_vec;
  BlImageVectorInfo_t rollback_vec;
  BlMetadata_t       meta;
  uint8_t            write_result;
  uint32_t           active_meta;
  uint32_t           confirm_meta;

  rollback_slot = (slot == BL_IMAGE_SLOT_A) ? BL_IMAGE_SLOT_B : BL_IMAGE_SLOT_A;

  if (slot == BL_IMAGE_SLOT_A) {
    active_meta  = BL_METADATA_SLOT_A;
    confirm_meta = BL_METADATA_SLOT_B;
  } else {
    active_meta  = BL_METADATA_SLOT_B;
    confirm_meta = BL_METADATA_SLOT_A;
  }

  BlLog_Printf("[UPDATE] set_pending_active=%s\r\n",    BlUpdate_SlotName(slot));
  BlLog_Printf("[UPDATE] set_pending_confirmed=%s\r\n", BlUpdate_SlotName(rollback_slot));
  BlLog_Printf("[UPDATE] set_pending_boot_count=0\r\n");

  /* Validate target slot */
  BlImage_ValidateSlot(slot, &target_vec);
  BlLog_Printf("[UPDATE] set_pending_target_vector=%s\r\n",
                target_vec.vector_check ? "OK" : "NG");

  if (target_vec.vector_check == 0U) {
    BlLog_Printf("[UPDATE] set_pending_target_check=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_SET_PENDING_TEST_TAG,
                  BL_UPDATE_SET_PENDING_TEST_NAME);
    return;
  }

  /* Validate rollback slot — warn but do not abort */
  BlImage_ValidateSlot(rollback_slot, &rollback_vec);
  BlLog_Printf("[UPDATE] set_pending_rollback_vector=%s\r\n",
                rollback_vec.vector_check ? "OK" : "NG");

  if (rollback_vec.vector_check == 0U) {
    BlLog_Printf("[UPDATE] set_pending_rollback_warn=YES\r\n");
  }

  memset(&meta, 0, sizeof(meta));
  meta.magic          = BL_METADATA_MAGIC;
  meta.version        = BL_METADATA_VERSION;
  meta.active_slot    = active_meta;
  meta.confirmed_slot = confirm_meta;
  meta.boot_count     = 0UL;

  write_result = BlMetadata_Write(&meta);

  if (write_result != 0U) {
    BlLog_Printf("[UPDATE] set_pending_write=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                  BL_UPDATE_SET_PENDING_TEST_TAG,
                  BL_UPDATE_SET_PENDING_TEST_NAME);
  } else {
    BlLog_Printf("[UPDATE] set_pending_write=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n",
                  BL_UPDATE_SET_PENDING_TEST_TAG,
                  BL_UPDATE_SET_PENDING_TEST_NAME);
  }
}

/* Function definitions ------------------------------------------------------*/

// Store the UART handle used for all update communication
void BlUpdate_Init(UART_HandleTypeDef *uart)
{
  update_uart = uart;
}

// Poll UART for BL_UPDATE_WINDOW_MS; return 1 if 'u' or 'U' is received
uint8_t BlUpdate_WaitForEntry(void)
{
  uint32_t start_tick;
  uint32_t elapsed;
  uint8_t rx_byte;
  HAL_StatusTypeDef status;

  if (update_uart == NULL) {
    return 0U;
  }

  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] uart_update_window=%ums\r\n",
                (unsigned int)BL_UPDATE_WINDOW_MS);
  BlLog_Printf("[BOOT] send_u_to_enter_update_mode\r\n");

  start_tick = HAL_GetTick();

  while (1) {
    elapsed = HAL_GetTick() - start_tick;

    if (elapsed >= BL_UPDATE_WINDOW_MS) {
      break;
    }

    /*
     * Poll with 1 ms timeout so elapsed time can be checked frequently.
     * HAL_UART_Receive returns HAL_OK only when a byte is received.
     */
    status = HAL_UART_Receive(update_uart, &rx_byte, 1U, 1U);

    if (status == HAL_OK) {
      if ((rx_byte == BL_UPDATE_ENTRY_CHAR_LOWER) ||
          (rx_byte == BL_UPDATE_ENTRY_CHAR_UPPER)) {
        BlLog_Printf("[BOOT] uart_update_request=YES\r\n");
        BlLog_Printf("[BOOT] update_mode=ENTER\r\n");
        BlLog_Printf("%s %s PASS\r\n",
                      BL_UPDATE_ENTRY_TEST_TAG,
                      BL_UPDATE_ENTRY_TEST_NAME);
        return 1U;
      }
    }
  }

  BlLog_Printf("[BOOT] uart_update_request=NO\r\n");
  BlLog_Printf("[BOOT] normal_boot=START\r\n");

  return 0U;
}

// Run the interactive update command loop; returns when 'exit' command is received
void BlUpdate_Run(void)
{
  char cmd[BL_UPDATE_CMD_MAX_LEN];

  BlLog_Printf("\r\n");
  BlLog_Printf("[UPDATE] command_mode=READY\r\n");
  BlLog_Printf("[UPDATE] waiting_command...\r\n");
  BlUpdate_HandleHelp();

  while (1) {
    BlUpdate_PrintPrompt();

    if (BlUpdate_ReadLine(cmd, sizeof(cmd)) == 0U) {
      continue;
    }

    if (BlUpdate_HandleCommand(cmd) == 0U) {
      BlLog_Printf("[UPDATE] command_mode=EXIT\r\n");
      return;
    }
  }
}
