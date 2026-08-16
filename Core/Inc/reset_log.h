#pragma once

#include <stdint.h>

typedef enum {
  RESET_CAUSE_UNKNOWN = 0,
  RESET_CAUSE_POWER_ON,
  RESET_CAUSE_PIN,
  RESET_CAUSE_WATCHDOG_IWDG,
  RESET_CAUSE_WATCHDOG_WWDG,
  RESET_CAUSE_SOFTWARE,
  RESET_CAUSE_BROWNOUT,
} ResetCause_t;

ResetCause_t get_reset_cause(void);
void log_reset_cause_to_flash(ResetCause_t cause);
uint8_t read_last_reset_cause_from_flash(void);
