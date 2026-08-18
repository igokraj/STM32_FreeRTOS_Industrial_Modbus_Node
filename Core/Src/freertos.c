/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "crc.h"
#include "temp_sensor.h"
#include "reset_log.h"
#include "app_shared.h"
#include "usart.h"
#include "iwdg.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for ModbusTask */
osThreadId_t ModbusTaskHandle;
const osThreadAttr_t ModbusTask_attributes = {
  .name = "ModbusTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for SensorTask */
osThreadId_t SensorTaskHandle;
const osThreadAttr_t SensorTask_attributes = {
  .name = "SensorTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for DiagnosticTask */
osThreadId_t DiagnosticTaskHandle;
const osThreadAttr_t DiagnosticTask_attributes = {
  .name = "DiagnosticTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for FrameReadySemaphore */
osSemaphoreId_t FrameReadySemaphoreHandle;
const osSemaphoreAttr_t FrameReadySemaphore_attributes = {
  .name = "FrameReadySemaphore"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartModbusTask(void *argument);
void StartSensorTask(void *argument);
void StartDiagnosticTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of FrameReadySemaphore */
  FrameReadySemaphoreHandle = osSemaphoreNew(1, 1, &FrameReadySemaphore_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of ModbusTask */
  ModbusTaskHandle = osThreadNew(StartModbusTask, NULL, &ModbusTask_attributes);

  /* creation of SensorTask */
  SensorTaskHandle = osThreadNew(StartSensorTask, NULL, &SensorTask_attributes);

  /* creation of DiagnosticTask */
  DiagnosticTaskHandle = osThreadNew(StartDiagnosticTask, NULL, &DiagnosticTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartModbusTask */
/**
  * @brief  Function implementing the ModbusTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartModbusTask */
void StartModbusTask(void *argument)
{
  /* USER CODE BEGIN StartModbusTask */
  /* Infinite loop */
  for(;;)
  { 

      // Wake on a frame, or after 500 ms anyway, so the task can report that it is still alive
      if (osSemaphoreAcquire(FrameReadySemaphoreHandle, 500) == osOK) {

      // Shortest valid 0x03 request is 8 bytes: address + function + reg addr(2) + count(2) + CRC(2)
      if (modbus_rx_len >= 8) {
      // Frame integrity check: compare the CRC the sender attached and the CRC we compute ourselves
      uint16_t received_crc = modbus_rx_buffer[modbus_rx_len - 2] | (modbus_rx_buffer[modbus_rx_len - 1] << 8); // CRC sent by the master (low byte first, then high byte)
      uint16_t calculated_crc = crc16_modbus(modbus_rx_buffer, modbus_rx_len - 2); // CRC we calculate locally with the standard C algorithm (crc.c)


      //  **** VERIFICATION ****
      if (received_crc == calculated_crc) {
        // CRC matches -> frame is valid
        uint8_t slave_address = modbus_rx_buffer[0];
        uint8_t function_code = modbus_rx_buffer[1];

        // Respond only when addressed, for a supported function, and while not in FAULT (fail-safe: stay silent)
        if (slave_address == SLAVE_ADDRESS && function_code == 0x03 && system_state != STATE_FAULT) {

          uint16_t reg_address = (modbus_rx_buffer[2] << 8) | modbus_rx_buffer[3];
          uint16_t reg_count = (modbus_rx_buffer[4] << 8) | modbus_rx_buffer[5];

          // Reject reads past the register map; only single-register reads are supported for now
          if (reg_address < REGISTER_COUNT && reg_count == 1) {

            // Build the response frame

            modbus_tx_buffer[0] = SLAVE_ADDRESS; // Device ID
            modbus_tx_buffer[1] = 0x03; // function code (0x03 -> read)
            modbus_tx_buffer[2] = 2; // byte count
            modbus_tx_buffer[3] = (holding_registers_map[reg_address] >> 8) & 0xFF; // high byte
            modbus_tx_buffer[4] = holding_registers_map[reg_address] & 0xFF; // low byte

            // Compute CRC and attach it to the response
            uint16_t response_crc = crc16_modbus(modbus_tx_buffer, 5);
            modbus_tx_buffer[5] = response_crc & 0xFF;
            modbus_tx_buffer[6] = (response_crc >> 8) & 0xFF;
            
            // Send the response back to the master
            HAL_UART_Transmit(&huart2, modbus_tx_buffer, 7, HAL_MAX_DELAY);
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
          }
        }
      }
      // else: CRC mismatch -> corrupted frame, just ignore it 
      }

      // Restart DMA so the next frame starts writing at buffer position 0 again
      HAL_UART_DMAStop(&huart2);
      HAL_UART_Receive_DMA(&huart2, modbus_rx_buffer, MODBUS_RX_BUFFER_SIZE);

      // Reset frame tracking so the next frame is detected from scratch
      modbus_rx_last_pos = 0;
      modbus_frame_ready = 0;
    }

    // Reported on every wake-up, also after a timeout - that is what proves the task is not stuck
    task_alive_flags |= ALIVE_MODBUS;
  }
  /* USER CODE END StartModbusTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the SensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  /* Infinite loop */
  for(;;)
  {
  uint16_t temp = read_htu21d_temperature();

  // Update the register on a valid reading; a failed read (0xFFFF) puts the node into FAULT state
  if (temp == 0xFFFF) {
    system_state = STATE_FAULT;
    } else {
    holding_registers_map[0] = temp;
    system_state = STATE_NORMAL;
    }

  task_alive_flags |= ALIVE_SENSOR;
  osDelay(1000); // Save the current temp. into register_map every 1 second

  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartDiagnosticTask */
/**
* @brief Function implementing the DiagnosticTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDiagnosticTask */
void StartDiagnosticTask(void *argument)
{
  /* USER CODE BEGIN StartDiagnosticTask */
  /* Infinite loop */
  for(;;)
  {
    task_alive_flags |= ALIVE_DIAG;

    // Feed the watchdog only when every task has reported since the last check.
    // If any task hangs, the refresh stops happening and the MCU resets.
    //
    //   one task missing:            all tasks reported:
    //   flags     = 0b00000101       flags     = 0b00000111
    //   ALIVE_ALL = 0b00000111       ALIVE_ALL = 0b00000111
    //             & ----------                 & ----------
    //   result    = 0b00000101       result    = 0b00000111
    //   -> != ALIVE_ALL, no refresh  -> == ALIVE_ALL, refresh
    if ((task_alive_flags & ALIVE_ALL) == ALIVE_ALL)
    {
      HAL_IWDG_Refresh(&hiwdg);
      task_alive_flags = 0; // clear, so each task must report again
    }

    osDelay(500);
  }
  /* USER CODE END StartDiagnosticTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

