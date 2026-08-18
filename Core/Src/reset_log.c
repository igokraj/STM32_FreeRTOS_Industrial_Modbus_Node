#include "reset_log.h"
#include "main.h"


// 


#define RESET_LOG_BASE 0x08060000 // Sector 7 start - far from application code
#define RESET_LOG_SIZE 1024       // Entries stored before the sector has to be erased again

ResetCause_t get_reset_cause(void)
{
  ResetCause_t cause;

  // Check specific causes of reset first 
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
    cause = RESET_CAUSE_WATCHDOG_IWDG;
  } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
    cause = RESET_CAUSE_WATCHDOG_WWDG;
  } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
    cause = RESET_CAUSE_SOFTWARE;
  } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST)) {
    cause = RESET_CAUSE_BROWNOUT;
  } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)) {
    cause = RESET_CAUSE_POWER_ON;
  } else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) {
    cause = RESET_CAUSE_PIN;
  } else {
    cause = RESET_CAUSE_UNKNOWN;
  }

  __HAL_RCC_CLEAR_RESET_FLAGS(); // ready to correctly detect the next reset

  return cause;
}

// Returns the index of the next free slot.
// Flash erases to 0xFF, so the first 0xFF byte marks where the next entry goes.
// The same number doubles as the entry count - see get_reset_count().
static uint32_t find_next_free_slot(void)
{
  for (uint32_t i = 0; i < RESET_LOG_SIZE; i++) {
    if (*(uint8_t *)(RESET_LOG_BASE + i) == 0xFF) {
      return i;
    }
  }
  return RESET_LOG_SIZE; // log is full
}

// Saves the reset cause into the slot pointed to by find_next_free_slot().
// The sector is erased only once the log fills up, instead of on every boot -
// an erase freezes the CPU for 1-2 s and wears the Flash out.
void log_reset_cause_to_flash(ResetCause_t cause)
{
  uint32_t slot = find_next_free_slot();

  HAL_FLASH_Unlock();

  if (slot >= RESET_LOG_SIZE) {
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error;

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = FLASH_SECTOR_7;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3; // matches our 3.3V supply

    HAL_FLASHEx_Erase(&erase_init, &sector_error);
    slot = 0;
  }

  HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, RESET_LOG_BASE + slot, (uint8_t)cause);

  HAL_FLASH_Lock();
}

// Read the most recently appended entry
uint8_t read_last_reset_cause_from_flash(void)
{
  uint32_t slot = find_next_free_slot();

  if (slot == 0) {
    return RESET_CAUSE_UNKNOWN; // nothing logged yet
  }
  return *(uint8_t *)(RESET_LOG_BASE + slot - 1);
}

// Number of resets recorded since the last sector erase
uint16_t get_reset_count(void)
{
  return (uint16_t)find_next_free_slot();
}
