#pragma once

#include <stdint.h>
#include <stdbool.h>

// THIS FILE IS USED TO SHARE DECLARATIONS, MACROS AND SYSTEM STATE TYPE BETWEEN main.c and freertos.c

// **** MODBUS ****
#define MODBUS_RX_BUFFER_SIZE 256
#define SLAVE_ADDRESS 1   // device ID
#define REGISTER_COUNT 4  // Number of registers

// Only declarations (extern) belong here - the variables themselves are defined in main.c.
extern uint8_t modbus_rx_buffer[MODBUS_RX_BUFFER_SIZE]; // Buffer for the circular DMA
extern uint8_t modbus_tx_buffer[MODBUS_RX_BUFFER_SIZE]; // Buffer for sending data
extern volatile uint16_t modbus_rx_len;                 // Length of the frame, set in the TIM1 callback
extern volatile bool modbus_frame_ready;                // Frame status, set in the TIM1 callback
extern uint16_t modbus_rx_last_pos;                     // DMA write position at the previous TIM1 tick
extern uint16_t holding_registers_map[REGISTER_COUNT];  // Register map

// **** SYSTEM STATUS ****
typedef enum {
  STATE_INIT,   // System start status
  STATE_NORMAL, // System works normally
  STATE_FAULT,  // System failed
} System_State_t;

// Only declaration (extern) belong here - the variable is defined in main.c.
extern volatile System_State_t system_state;


// **** DIAGNOSTIC TASK **** 

// Each task sets its own bit; DiagnosticTask refreshes the watchdog only when all of them reported.
#define ALIVE_MODBUS (1 << 0) // 0b00000001
#define ALIVE_SENSOR (1 << 1) // 0b00000011
#define ALIVE_DIAG   (1 << 2) // 0b00000111
#define ALIVE_ALL    (ALIVE_MODBUS | ALIVE_SENSOR | ALIVE_DIAG)

// Only declaration (extern) belong here - the variable is defined in main.c.
extern volatile uint8_t task_alive_flags;