#pragma once

#include <stdint.h>

// This function triggers a measurement on the HTU21D sensor and reads the current temperature
uint16_t read_htu21d_temperature(void);