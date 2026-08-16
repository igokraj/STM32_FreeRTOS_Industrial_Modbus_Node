#include "reset_log.h"
#include "main.h"

#define RESET_LOG_FLASH_ADDR 0x08060000 // Sector 7 start - far from application code

ResetCause_t get_reset_cause(void)
{
  ResetCause_t cause;

  // Check specific causes first - they matter more for diagnostics
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

void log_reset_cause_to_flash(ResetCause_t cause)
{
  HAL_FLASH_Unlock();

  FLASH_EraseInitTypeDef erase_init;
  uint32_t sector_error;

  erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase_init.Sector = FLASH_SECTOR_7;
  erase_init.NbSectors = 1;
  erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3; // matches our 3.3V supply

  HAL_FLASHEx_Erase(&erase_init, &sector_error);
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, RESET_LOG_FLASH_ADDR, (uint8_t)cause);

  HAL_FLASH_Lock();
}

uint8_t read_last_reset_cause_from_flash(void)
{
  return *(uint8_t*)RESET_LOG_FLASH_ADDR;
}
