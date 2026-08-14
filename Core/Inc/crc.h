#pragma once
#include <stdint.h>

// This function is the standard C algorithm for CRC16 Modbus
uint16_t crc16_modbus(const uint8_t *data, uint16_t length);