#pragma once

#include <stdint.h>

// Stored in Flash as a single byte, where 0xFF marks an empty slot, so no cause may ever be given the value 255.
typedef enum {
  RESET_CAUSE_UNKNOWN = 0,
  RESET_CAUSE_POWER_ON,
  RESET_CAUSE_PIN,
  RESET_CAUSE_WATCHDOG_IWDG,
  RESET_CAUSE_WATCHDOG_WWDG,
  RESET_CAUSE_SOFTWARE,
  RESET_CAUSE_BROWNOUT,
} ResetCause_t;

ResetCause_t get_reset_cause(void);  // Reads the RCC reset flags and clears them
void log_reset_cause_to_flash(ResetCause_t cause); // Appends one entry to the Flash log
uint8_t read_last_reset_cause_from_flash(void);  // Cause of the reset that just happened
uint16_t get_reset_count(void);  // Entries logged since the last sector erase
