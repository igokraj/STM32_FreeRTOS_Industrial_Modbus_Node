#include <stdint.h>

uint16_t crc16_modbus(const uint8_t *data, uint16_t length) 
{
    uint16_t crc = 0xFFFF; // 1. Start: initial value

    for (uint16_t pos = 0; pos < length; pos++) {
        crc ^= data[pos];    // 2. XOR the byte into the low part of the register

        for (uint8_t i = 0; i < 8; i++) { // Loop 8 times
            if (crc & 0x0001) {           // Check the least significant bit
                crc = (crc >> 1) ^ 0xA001; // Shift right and XOR with the constant
            } else {
                crc >>= 1;                 // Just shift right
            }
        }
    }

    return crc; // 3. Return the result
}