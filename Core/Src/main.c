/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdbool.h"
#include "crc.h"
#include "temp_sensor.h"
#include "reset_log.h"
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

/* USER CODE BEGIN PV */
// **** MODBUS ****
#define MODBUS_RX_BUFFER_SIZE 256
uint8_t modbus_rx_buffer[MODBUS_RX_BUFFER_SIZE]; // Buffer for the circular DMA
volatile uint16_t modbus_rx_len = 0; // Length of the frame, handled in callback (volatile)
volatile bool modbus_frame_ready = 0; // Status of the frame, handled in callback (volatile)
static uint16_t modbus_rx_last_pos = 0; // DMA write position at the previous TIM1 tick

#define SLAVE_ADDRESS 1 // devicde ID
#define REGISTER_COUNT 4 // Number of registers
uint16_t holding_registers_map[REGISTER_COUNT] = {0, 0 ,0 ,0}; // Register map
uint8_t modbus_tx_buffer[MODBUS_RX_BUFFER_SIZE]; // buffer for sending data

// **** SYSTEM STATUS **** 

typedef enum {
  STATE_INIT, // System start status 
  STATE_NORMAL, // System works normally 
  STATE_FAULT, // System failed 
} System_State_t;

volatile System_State_t system_state = STATE_INIT;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  ResetCause_t reset_cause = get_reset_cause();
  log_reset_cause_to_flash(reset_cause);
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  __HAL_DBGMCU_FREEZE_IWDG(); // Freeze IWDG when the debugger halts the core, otherwise debugging resets the MCU

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim1);
  // TIM1 period = 1.75 ms = fixed Modbus RTU t3.5 (inter-frame silence) value for baud rates > 19200 bps

  HAL_UART_Receive_DMA(&huart2, modbus_rx_buffer, MODBUS_RX_BUFFER_SIZE);
  // Start continuous circular DMA reception into modbus_rx_buffer (runs in the background, never stops)

  uint32_t last_temp_read_tick = 0;

  // Expose the cause of the reset that just happened as a Modbus register
  holding_registers_map[2] = read_last_reset_cause_from_flash();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  while (1)
  {
    // Save the current temp. into register_map every 1 second
    if (HAL_GetTick() - last_temp_read_tick >= 1000) {
  uint16_t temp = read_htu21d_temperature();

  if (temp == 0xFFFF) {
    system_state = STATE_FAULT;
    } else {
    holding_registers_map[0] = temp;
    system_state = STATE_NORMAL;
    }

  last_temp_read_tick = HAL_GetTick();
}
    if (modbus_frame_ready) {

      if (modbus_rx_len >= 8) {
      // Frame integrity check: compare the CRC the sender attached and the CRC we compute ourselves
      uint16_t received_crc = modbus_rx_buffer[modbus_rx_len - 2] | (modbus_rx_buffer[modbus_rx_len - 1] << 8); // CRC sent by the master (low byte first, then high byte)
      uint16_t calculated_crc = crc16_modbus(modbus_rx_buffer, modbus_rx_len - 2); // CRC we calculate locally with the standard C algorithm (crc.c)


      //  **** VERIFICATION ****
      if (received_crc == calculated_crc) {
        // CRC matches -> frame is valid
        uint8_t slave_address = modbus_rx_buffer[0];
        uint8_t function_code = modbus_rx_buffer[1];

        if (slave_address == SLAVE_ADDRESS && function_code == 0x03 && system_state != STATE_FAULT) {

          uint16_t reg_address = (modbus_rx_buffer[2] << 8) | modbus_rx_buffer[3];
          uint16_t reg_count = (modbus_rx_buffer[4] << 8) | modbus_rx_buffer[5];

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
            
            // for testing:
            HAL_UART_Transmit(&huart2, modbus_tx_buffer, 7, HAL_MAX_DELAY);
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
          }
        }
      }
      // else: CRC mismatch -> corrupted frame, just ignore it 
      }

      HAL_UART_DMAStop(&huart2);
      HAL_UART_Receive_DMA(&huart2, modbus_rx_buffer, MODBUS_RX_BUFFER_SIZE);

      modbus_rx_last_pos = 0;
      modbus_frame_ready = 0;
    }

          HAL_IWDG_Refresh(&hiwdg); // WatchDog Update - runs every loop iteration, not only when a frame arrives
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

// Fires every 1.75 ms (TIM1 overflow). Detects Modbus frame end by checking whether the DMA write position has stayed the same since the last tick.
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
 if (htim->Instance == TIM1) { 
  
    // current DMA write offset in the buffer (counts up as bytes arrive)
    uint16_t pos = MODBUS_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart2.hdmarx);

    // check if there are any new bytes since the last 1.75 ms tick
    if (pos != modbus_rx_last_pos) {
      modbus_rx_last_pos = pos;
    }
    // no new bytes for a full 1.75 ms tick -> silence detected, frame is complete
    else if (pos != 0 && !modbus_frame_ready) {
      modbus_rx_len = pos;
      modbus_frame_ready = true;
    }
    }
}

// Read the state of the cylinder
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == Cylinder_Limit_Switch_Pin_Pin) {
    holding_registers_map[1] = (HAL_GPIO_ReadPin(Cylinder_Limit_Switch_Pin_GPIO_Port, Cylinder_Limit_Switch_Pin_Pin) == GPIO_PIN_RESET);
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
