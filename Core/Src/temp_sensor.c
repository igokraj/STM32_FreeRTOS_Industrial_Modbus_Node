
#include "i2c.h"
#include "cmsis_os.h" // for osDelay()

#define HTU21D_ADDR (0x40 << 1) // I2C address of the HTU21D sensor
#define Timeout_delay 100 // delay for the master functions

// HTU21D commands (no-hold master)
#define TEMP_CMD               0xF3         // trigger temperature measurement

// This function triggers a measurement on the HTU21D and reads the current temperature
uint16_t read_htu21d_temperature(void) {

uint8_t cmd = TEMP_CMD;
uint8_t data[3]; // buffer for the data 

// Master sends request
if (HAL_I2C_Master_Transmit(&hi2c1, HTU21D_ADDR, &cmd, 1, Timeout_delay) != HAL_OK) {
return 0xFFFF; // I2C failed - return error value 
}

osDelay(50);

// Master receives the data
if (HAL_I2C_Master_Receive(&hi2c1, HTU21D_ADDR, data, 3, Timeout_delay) != HAL_OK) {
    return 0xFFFF; // I2C failed - return error value
}

// data conversion
uint16_t raw_temp = ((data[0] << 8) | data[1]) & 0xFFFC; // mask status bits
float temperature = -46.85f + (175.72f * raw_temp / 65536.0f);

return (uint16_t)(temperature * 10); // return 234 instead of 23.4 to avoid using a decimal point

}


