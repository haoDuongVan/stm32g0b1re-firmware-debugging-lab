/*
 * bl_main.c
 *
 *  Created on: Jun 27, 2026
 *      Author: haodu
 */

/* Includes ------------------------------------------------------------------*/
#include "bl_main.h"

#include "bl_flash_layout.h"
#include "bl_image.h"
#include "bl_log.h"
#include "bl_metadata.h"
#include "main.h"

/* Private defines -----------------------------------------------------------*/

// Define LED for heartbeat
#define BL_LED_GPIO_Port                 GPIOA
#define BL_LED_Pin                       GPIO_PIN_5

// Define heartbeat period
#define BL_HEARTBEAT_INTERVAL_MS         1000U   // ms

/*
 * Enable automatic slot selection and jump after bootloader self-checks.
 *
 * Set to 0U when only testing bootloader logs/vector validation.
 * Set to 1U when testing bootloader -> application handoff.
 */
#define BL_AUTO_JUMP_ENABLE              1U

/*
 * Default boot slot used when metadata is empty or invalid.
 */
#define BL_DEFAULT_BOOT_SLOT             BL_IMAGE_SLOT_A

#define BL_JUMP_DELAY_MS                 100U    // ms

/*
 * Wrong-slot validation test.
 *
 * This test is used when a Slot A binary is intentionally written to the
 * Slot B address. In that case:
 *
 *   - MSP is still valid
 *   - Reset_Handler still has Thumb bit
 *   - Reset_Handler address points to Slot A range
 *   - But the image is being validated as Slot B
 *
 * Therefore, Slot B vector validation must fail, and the bootloader should
 * report that the wrong-slot image was correctly rejected.
 */
#define BL_WRONG_SLOT_TEST_TAG           "[TEST6]"
#define BL_WRONG_SLOT_TEST_NAME          "wrong_slot_b_reject_check"

/* Private variables ---------------------------------------------------------*/
static uint8_t boot_check_done = 0U;

/* Private function prototypes -----------------------------------------------*/
static uint8_t BlMain_IsSlotAWrittenToSlotB(const BlImageVectorInfo_t *vector_info);
static void BlMain_PrintWrongSlotBRejectCheck(const BlImageVectorInfo_t *vector_info);
static BlImageSlotId_t BlMain_RunMetadataCheck(void);
#if (BL_AUTO_JUMP_ENABLE != 0U)
static void BlMain_SelectAndJump(BlImageSlotId_t selected_slot);
#endif

/* Private functions ---------------------------------------------------------*/

// Return 1 if Slot B vector check failed because a Slot A binary was written there
static uint8_t BlMain_IsSlotAWrittenToSlotB(const BlImageVectorInfo_t *vector_info)
{
  if (vector_info == NULL) {
    return 0U;
  }

  // This check is only meaningful for Slot B validation
  if (vector_info->slot_base != BL_SLOT_B_BASE_ADDR) {
    return 0U;
  }

  /*
   * A wrong-slot binary is different from an empty Flash area.
   *
   * Empty Flash usually has:
   *   MSP           = 0xFFFFFFFF
   *   Reset_Handler = 0xFFFFFFFF
   *
   * But app_slot_a.bin written to Slot B still has:
   *   valid MSP
   *   valid Thumb bit
   *   Reset_Handler pointing to Slot A range
   */
  if (vector_info->msp_check == 0U) {
    return 0U;
  }

  if (vector_info->reset_thumb_check == 0U) {
    return 0U;
  }

  // reset_range_check must fail (Reset_Handler is not in Slot B)
  if (vector_info->reset_range_check != 0U) {
    return 0U;
  }

  // Reset_Handler must point into Slot A range
  if (vector_info->reset_handler_addr < BL_SLOT_A_BASE_ADDR) {
    return 0U;
  }

  if (vector_info->reset_handler_addr > BL_SLOT_A_END_ADDR) {
    return 0U;
  }

  return 1U;
}

// Print TEST6 PASS if Slot B failed because a Slot A binary was written there
static void BlMain_PrintWrongSlotBRejectCheck(const BlImageVectorInfo_t *vector_info)
{
  if (BlMain_IsSlotAWrittenToSlotB(vector_info) != 0U) {
    BlLog_Printf("[BOOT] reject_reason=reset_handler_points_to_slot_a\r\n");
    BlLog_Printf("%s %s PASS\r\n",
                 BL_WRONG_SLOT_TEST_TAG,
                 BL_WRONG_SLOT_TEST_NAME);
  }
}

// Verify that all Flash layout addresses compile to the expected values; returns 1 if OK
static int BlMain_CheckFlashLayout(void)
{
  if (BL_BOOT_BASE_ADDR != 0x08000000UL) {
    return 0;
  }

  if (BL_BOOT_END_ADDR != 0x0800FFFFUL) {
    return 0;
  }

  if (BL_SLOT_A_BASE_ADDR != 0x08010000UL) {
    return 0;
  }

  if (BL_SLOT_A_END_ADDR != 0x0803FFFFUL) {
    return 0;
  }

  if (BL_SLOT_B_BASE_ADDR != 0x08040000UL) {
    return 0;
  }

  if (BL_SLOT_B_END_ADDR != 0x0806FFFFUL) {
    return 0;
  }

  if (BL_METADATA0_BASE_ADDR != 0x0807F000UL) {
    return 0;
  }

  if (BL_METADATA1_BASE_ADDR != 0x0807F800UL) {
    return 0;
  }

  if (BL_METADATA1_END_ADDR != 0x0807FFFFUL) {
    return 0;
  }

  return 1;
}

// Print project and board information
static void BlMain_PrintBootLog(void)
{
  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] STM32 Dual-Slot UART Bootloader V1\r\n");
  BlLog_Printf("[BOOT] Project: 04_uart_bootloader\r\n");
  BlLog_Printf("[BOOT] Board: NUCLEO-G0B1RE\r\n");
  BlLog_Printf("[BOOT] System clock: %lu Hz\r\n",
                (unsigned long)HAL_RCC_GetSysClockFreq());
}

// Print Flash layout addresses and sizes
static void BlMain_PrintFlashLayout(void)
{
  BlLog_Printf("[BOOT] boot_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_BOOT_BASE_ADDR,
                (unsigned long)(BL_BOOT_SIZE / 1024UL));

  BlLog_Printf("[BOOT] slot_a_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_SLOT_A_BASE_ADDR,
                (unsigned long)(BL_SLOT_SIZE / 1024UL));

  BlLog_Printf("[BOOT] slot_b_base=0x%08lX size=%luKB\r\n",
                (unsigned long)BL_SLOT_B_BASE_ADDR,
                (unsigned long)(BL_SLOT_SIZE / 1024UL));

  BlLog_Printf("[BOOT] metadata0=0x%08lX\r\n",
                (unsigned long)BL_METADATA0_BASE_ADDR);

  BlLog_Printf("[BOOT] metadata1=0x%08lX\r\n",
                (unsigned long)BL_METADATA1_BASE_ADDR);
}

// Check VTOR and Flash layout once after boot; runs only on first call
static void BlMain_RunBootCheck(void)
{
  uint32_t expected_vtor;
  uint32_t actual_vtor;

  // Run boot check only once after boot
  if (boot_check_done != 0U) {
    return;
  }

  boot_check_done = 1U;

  expected_vtor = BL_BOOT_BASE_ADDR;
  actual_vtor = SCB->VTOR;

  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] expected_vtor=0x%08lX\r\n",
                (unsigned long)expected_vtor);
  BlLog_Printf("[BOOT] actual_vtor=0x%08lX\r\n",
                (unsigned long)actual_vtor);

  if (actual_vtor == expected_vtor) {
    BlLog_Printf("[BOOT] vtor_check=OK\r\n");
    BlLog_Printf("[TEST0] bootloader_boot_check PASS\r\n");
  } else {
    BlLog_Printf("[BOOT] vtor_check=NG\r\n");
    BlLog_Printf("[TEST0] bootloader_boot_check FAIL\r\n");
  }

  if (BlMain_CheckFlashLayout() != 0) {
    BlLog_Printf("[TEST1] flash_layout_check PASS\r\n");
  } else {
    BlLog_Printf("[TEST1] flash_layout_check FAIL\r\n");
  }
}

// Read metadata from Flash, print TEST7 result, and return the slot to boot
static BlImageSlotId_t BlMain_RunMetadataCheck(void)
{
  BlMetadata_t meta;

  BlMetadata_Read(&meta);

  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] metadata_page=0x%08lX\r\n",
               (unsigned long)BL_METADATA0_BASE_ADDR);

  if (BlMetadata_IsValid(&meta) == 0U) {
    /*
     * Empty Flash after mass erase is expected during early milestones.
     * Use the compile-time default slot instead of stopping.
     */
    BlLog_Printf("[BOOT] metadata_magic=NG\r\n");
    BlLog_Printf("[BOOT] use_default_slot=%s\r\n",
                 (BL_DEFAULT_BOOT_SLOT == BL_IMAGE_SLOT_B) ? "B" : "A");
    BlLog_Printf("[TEST7] metadata_read_check PASS\r\n");
    return BL_DEFAULT_BOOT_SLOT;
  }

  BlLog_Printf("[BOOT] metadata_magic=OK\r\n");
  BlLog_Printf("[BOOT] active_slot=%c\r\n",
               (meta.active_slot == BL_METADATA_SLOT_B) ? 'B' : 'A');
  BlLog_Printf("[BOOT] confirmed_slot=%c\r\n",
               (meta.confirmed_slot == BL_METADATA_SLOT_B) ? 'B' : 'A');
  BlLog_Printf("[TEST7] metadata_read_check PASS\r\n");

  return (meta.active_slot == BL_METADATA_SLOT_B) ? BL_IMAGE_SLOT_B : BL_IMAGE_SLOT_A;
}

#if (BL_AUTO_JUMP_ENABLE != 0U)
// Validate the selected slot and jump; try the other slot as fallback if selected is invalid
static void BlMain_SelectAndJump(BlImageSlotId_t selected_slot)
{
  BlImageSlotId_t fallback_slot;
  BlImageVectorInfo_t vector_info;

  fallback_slot = (selected_slot == BL_IMAGE_SLOT_A) ? BL_IMAGE_SLOT_B : BL_IMAGE_SLOT_A;

  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] selected_slot=%s\r\n",
               (selected_slot == BL_IMAGE_SLOT_A) ? "A" : "B");

  // Validate selected slot
  BlImage_ValidateSlot(selected_slot, &vector_info);

  if (vector_info.vector_check != 0U) {
    BlLog_Printf("[BOOT] selected_slot_vector_check=OK\r\n");
    BlLog_Printf("[BOOT] jump_slot=%s\r\n", vector_info.slot_name);
    BlLog_Printf("[BOOT] jump_msp=0x%08lX\r\n",
                 (unsigned long)vector_info.initial_msp);
    BlLog_Printf("[BOOT] jump_reset_handler=0x%08lX\r\n",
                 (unsigned long)vector_info.reset_handler_raw);
    /*
     * Give UART enough time to finish the last visible log before handoff.
     * The logger itself uses blocking transmit, but this small delay makes
     * the boot-to-app transition easier to read on TeraTerm.
     */
    BlLog_Printf("[BOOT] jump_result=START\r\n");
    HAL_Delay(BL_JUMP_DELAY_MS);
    (void)BlImage_JumpToImage(&vector_info);
    BlLog_Printf("[BOOT] jump_result=FAIL\r\n");
    return;
  }

  // Selected slot invalid, try fallback
  BlLog_Printf("[BOOT] selected_slot_vector_check=NG\r\n");
  BlLog_Printf("[BOOT] fallback_slot=%s\r\n",
               (fallback_slot == BL_IMAGE_SLOT_A) ? "A" : "B");

  BlImage_ValidateSlot(fallback_slot, &vector_info);

  if (vector_info.vector_check != 0U) {
    BlLog_Printf("[BOOT] fallback_slot_vector_check=OK\r\n");
    BlLog_Printf("[BOOT] jump_slot=%s\r\n", vector_info.slot_name);
    BlLog_Printf("[BOOT] jump_msp=0x%08lX\r\n",
                 (unsigned long)vector_info.initial_msp);
    BlLog_Printf("[BOOT] jump_reset_handler=0x%08lX\r\n",
                 (unsigned long)vector_info.reset_handler_raw);
    BlLog_Printf("[BOOT] jump_result=START\r\n");
    HAL_Delay(BL_JUMP_DELAY_MS);
    (void)BlImage_JumpToImage(&vector_info);
    BlLog_Printf("[BOOT] jump_result=FAIL\r\n");
    return;
  }

  // Both slots invalid
  BlLog_Printf("[BOOT] fallback_slot_vector_check=NG\r\n");
  BlLog_Printf("[BOOT] no_valid_slot_found\r\n");
}
#endif

/* Function definitions ------------------------------------------------------*/

// Set VTOR to the bootloader base address before the scheduler or any interrupt fires
void BlMain_ApplyVectorTable(void)
{
  /*
   * The bootloader always starts from the beginning of Flash.
   *
   * Set VTOR explicitly so the interrupt vector table is known even if
   * generated SystemInit() does not relocate it.
   */
  SCB->VTOR = BL_BOOT_BASE_ADDR;
  __DSB();
  __ISB();
}

// Print application slot vector table validation result.
static void BlMain_PrintSlotVectorCheck(BlImageSlotId_t slot_id,
                                        const char *test_tag,
                                        const char *test_name)
{
  BlImageVectorInfo_t vector_info;

  BlImage_ValidateSlot(slot_id, &vector_info);

  BlLog_Printf("\r\n");
  BlLog_Printf("[BOOT] validate_slot=%s\r\n", vector_info.slot_name);
  BlLog_Printf("[BOOT] slot_base=0x%08lX\r\n",
                (unsigned long)vector_info.slot_base);
  BlLog_Printf("[BOOT] slot_end=0x%08lX\r\n",
                (unsigned long)vector_info.slot_end);

  BlLog_Printf("[BOOT] initial_msp=0x%08lX\r\n",
                (unsigned long)vector_info.initial_msp);
  BlLog_Printf("[BOOT] reset_handler_raw=0x%08lX\r\n",
                (unsigned long)vector_info.reset_handler_raw);
  BlLog_Printf("[BOOT] reset_handler_addr=0x%08lX\r\n",
                (unsigned long)vector_info.reset_handler_addr);

  BlLog_Printf("[BOOT] msp_check=%s\r\n",
                (vector_info.msp_check != 0U) ? "OK" : "NG");
  BlLog_Printf("[BOOT] reset_thumb_check=%s\r\n",
                (vector_info.reset_thumb_check != 0U) ? "OK" : "NG");
  BlLog_Printf("[BOOT] reset_range_check=%s\r\n",
                (vector_info.reset_range_check != 0U) ? "OK" : "NG");

  if (vector_info.vector_check != 0U) {
    BlLog_Printf("[BOOT] vector_check=OK\r\n");
    BlLog_Printf("%s %s PASS\r\n", test_tag, test_name);
  } else {
    BlLog_Printf("[BOOT] vector_check=NG\r\n");
    BlLog_Printf("%s %s FAIL\r\n", test_tag, test_name);

    /*
     * If Slot B fails because a Slot A binary was written there, print a
     * dedicated PASS result for the wrong-slot rejection test.
     */
    if (slot_id == BL_IMAGE_SLOT_B) {
      BlMain_PrintWrongSlotBRejectCheck(&vector_info);
    }
  }
}

// Validate Slot A and Slot B vector tables.
static void BlMain_RunSlotVectorChecks(void)
{
  BlMain_PrintSlotVectorCheck(BL_IMAGE_SLOT_A,
                              "[TEST4]",
                              "slot_a_vector_check");

  BlMain_PrintSlotVectorCheck(BL_IMAGE_SLOT_B,
                              "[TEST5]",
                              "slot_b_vector_check");
}

// Initialize bootloader, run self-checks, read metadata, and jump to the active slot
void BlMain_Init(UART_HandleTypeDef *debug_uart)
{
  BlLog_Init(debug_uart);

  BlMain_PrintBootLog();
  BlMain_PrintFlashLayout();
  BlMain_RunBootCheck();
  BlMain_RunSlotVectorChecks();

#if (BL_AUTO_JUMP_ENABLE != 0U)
  BlMain_SelectAndJump(BlMain_RunMetadataCheck());
#else
  (void)BlMain_RunMetadataCheck();
#endif
}

// Toggle the LED at a fixed interval as a heartbeat indicator
void BlMain_Run(void)
{
  // Persists across calls to track when the LED was toggled last time
  static uint32_t last_tick = 0U;
  uint32_t now = HAL_GetTick();

  // uint32_t subtraction is wrap-safe, so this works correctly after HAL_GetTick() overflows
  if ((now - last_tick) >= BL_HEARTBEAT_INTERVAL_MS) {
    last_tick = now;

    // Toggle LED as a heartbeat indicator
    HAL_GPIO_TogglePin(BL_LED_GPIO_Port, BL_LED_Pin);
  }
}
