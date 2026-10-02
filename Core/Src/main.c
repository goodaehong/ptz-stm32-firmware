/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    CMD_PAN_ABSOLUTE,
    CMD_TILT_ABSOLUTE,
    CMD_PAN_RELATIVE,
    CMD_TILT_RELATIVE,
    CMD_CENTER,
    CMD_STOP
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    int value;
} motor_cmd_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for motorTask */
osThreadId_t motorTaskHandle;
const osThreadAttr_t motorTask_attributes = {
  .name = "motorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for statusTask */
osThreadId_t statusTaskHandle;
const osThreadAttr_t statusTask_attributes = {
  .name = "statusTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for buttonTask */
osThreadId_t buttonTaskHandle;
const osThreadAttr_t buttonTask_attributes = {
  .name = "buttonTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for cmdQueue */
osMessageQueueId_t cmdQueueHandle;
const osMessageQueueAttr_t cmdQueue_attributes = {
  .name = "cmdQueue"
};
/* USER CODE BEGIN PV */

int pan_angle = 90;   // current PAN angle (0~180)
int tilt_angle = 90;  // current TILT angle (0~180)

int pan_target = 90;  // target angle for PAN
int tilt_target = 90; // target angle for TILT

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void *argument);
void StartMotorTask(void *argument);
void StartStatusTask(void *argument);
void StartButtonTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void setServoAngle(uint32_t channel, int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    uint32_t ccr = 500 + (angle * 2000) / 180;
    __HAL_TIM_SET_COMPARE(&htim2, channel, ccr);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  // Start PWM output on both channels (PAN=CH1, TILT=CH2)
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

  // Move both servos to center (90 deg) on boot
  setServoAngle(TIM_CHANNEL_1, pan_angle);
  setServoAngle(TIM_CHANNEL_2, tilt_angle);

  // Notify host (RPi) that PTZ firmware is ready
  char msg[] = "PTZ Ready\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* Create queue and threads (manually added due to CubeMX 6.7 bug) */
  cmdQueueHandle = osMessageQueueNew(8, sizeof(motor_cmd_t), &cmdQueue_attributes);

  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  motorTaskHandle = osThreadNew(StartMotorTask, NULL, &motorTask_attributes);
  statusTaskHandle = osThreadNew(StartStatusTask, NULL, &statusTask_attributes);
  buttonTaskHandle = osThreadNew(StartButtonTask, NULL, &buttonTask_attributes);

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

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

/**
  * @brief TIM2 Initialization Function
  */
static void MX_TIM2_Init(void)
{
  /* USER CODE BEGIN TIM2_Init 0 */
  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */
  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) { Error_Handler(); }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) { Error_Handler(); }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) { Error_Handler(); }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) { Error_Handler(); }
  /* USER CODE BEGIN TIM2_Init 2 */
  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);
}

/**
  * @brief USART1 Initialization Function
  */
static void MX_USART1_UART_Init(void)
{
  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  *         Receives UART commands and pushes to motor queue.
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */

  char rx_buffer[32];
  uint8_t idx = 0;

  for(;;)
  {
    uint8_t rx_byte;

    if (HAL_UART_Receive(&huart1, &rx_byte, 1, 1000) == HAL_OK) {
      HAL_UART_Transmit(&huart1, &rx_byte, 1, 100);

      if (rx_byte == '\r' || rx_byte == '\n') {
        rx_buffer[idx] = '\0';

        if (idx > 0) {
          char response[64];
          int len;
          motor_cmd_t cmd;
          int has_cmd = 1;

          if (strcmp(rx_buffer, "LEFT") == 0) {
            cmd.type = CMD_PAN_RELATIVE;
            cmd.value = -10;
            len = snprintf(response, sizeof(response), "PAN -10\r\n");
          }
          else if (strcmp(rx_buffer, "RIGHT") == 0) {
            cmd.type = CMD_PAN_RELATIVE;
            cmd.value = +10;
            len = snprintf(response, sizeof(response), "PAN +10\r\n");
          }
          else if (strcmp(rx_buffer, "UP") == 0) {
            cmd.type = CMD_TILT_RELATIVE;
            cmd.value = +10;
            len = snprintf(response, sizeof(response), "TILT +10\r\n");
          }
          else if (strcmp(rx_buffer, "DOWN") == 0) {
            cmd.type = CMD_TILT_RELATIVE;
            cmd.value = -10;
            len = snprintf(response, sizeof(response), "TILT -10\r\n");
          }
          else if (strcmp(rx_buffer, "CENTER") == 0) {
            cmd.type = CMD_CENTER;
            cmd.value = 0;
            len = snprintf(response, sizeof(response), "Centered\r\n");
          }
          else if (strcmp(rx_buffer, "STOP") == 0) {
            cmd.type = CMD_STOP;
            cmd.value = 0;
            len = snprintf(response, sizeof(response), "Stop\r\n");
          }
          else if (strcmp(rx_buffer, "STATUS") == 0) {
            has_cmd = 0;
            len = snprintf(response, sizeof(response),
                           "PAN:%d TILT:%d\r\n", pan_angle, tilt_angle);
          }
          else if (strncmp(rx_buffer, "PAN:", 4) == 0) {
            cmd.type = CMD_PAN_ABSOLUTE;
            cmd.value = atoi(rx_buffer + 4);
            len = snprintf(response, sizeof(response), "PAN to %d\r\n", cmd.value);
          }
          else if (strncmp(rx_buffer, "TILT:", 5) == 0) {
            cmd.type = CMD_TILT_ABSOLUTE;
            cmd.value = atoi(rx_buffer + 5);
            len = snprintf(response, sizeof(response), "TILT to %d\r\n", cmd.value);
          }
          else {
            has_cmd = 0;
            len = snprintf(response, sizeof(response),
                           "Unknown command: %s\r\n", rx_buffer);
          }

          if (has_cmd) {
            osMessageQueuePut(cmdQueueHandle, &cmd, 0, 0);
          }

          HAL_UART_Transmit(&huart1, (uint8_t*)response, len, 100);
        }
        idx = 0;
      }
      else if (idx < sizeof(rx_buffer) - 1) {
        rx_buffer[idx++] = rx_byte;
      }
    }
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartMotorTask */
/**
  * @brief Function implementing the motorTask thread.
  *        Consumes queue + smooth move servo.
  */
/* USER CODE END Header_StartMotorTask */
void StartMotorTask(void *argument)
{
  /* USER CODE BEGIN StartMotorTask */

  motor_cmd_t cmd;

  for(;;)
  {
    // Check queue for new command (non-blocking)
    if (osMessageQueueGet(cmdQueueHandle, &cmd, NULL, 0) == osOK) {

      switch (cmd.type) {
        case CMD_PAN_ABSOLUTE:
          pan_target = cmd.value;
          break;
        case CMD_TILT_ABSOLUTE:
          tilt_target = cmd.value;
          break;
        case CMD_PAN_RELATIVE:
          pan_target = pan_angle + cmd.value;
          break;
        case CMD_TILT_RELATIVE:
          tilt_target = tilt_angle + cmd.value;
          break;
        case CMD_CENTER:
          pan_target = 90;
          tilt_target = 90;
          break;
        case CMD_STOP:
          pan_target = pan_angle;
          tilt_target = tilt_angle;
          break;
      }

      // Clamp targets to safe range
      if (pan_target < 0) pan_target = 0;
      if (pan_target > 180) pan_target = 180;
      if (tilt_target < 0) tilt_target = 0;
      if (tilt_target > 180) tilt_target = 180;
    }

    // Smooth move: one degree step per cycle
    if (pan_angle < pan_target) {
      pan_angle++;
      setServoAngle(TIM_CHANNEL_1, pan_angle);
    }
    else if (pan_angle > pan_target) {
      pan_angle--;
      setServoAngle(TIM_CHANNEL_1, pan_angle);
    }

    if (tilt_angle < tilt_target) {
      tilt_angle++;
      setServoAngle(TIM_CHANNEL_2, tilt_angle);
    }
    else if (tilt_angle > tilt_target) {
      tilt_angle--;
      setServoAngle(TIM_CHANNEL_2, tilt_angle);
    }

    osDelay(20);   // 20ms per step => 50 deg/sec
  }
  /* USER CODE END StartMotorTask */
}

/* USER CODE BEGIN Header_StartStatusTask */
/**
  * @brief Function implementing the statusTask thread.
  *        Periodic STATUS report.
  */
/* USER CODE END Header_StartStatusTask */
void StartStatusTask(void *argument)
{
  /* USER CODE BEGIN StartStatusTask */

  char status_msg[64];

  for(;;)
  {
    int len = snprintf(status_msg, sizeof(status_msg),
                       "[STATUS] PAN:%d TILT:%d\r\n", pan_angle, tilt_angle);
    HAL_UART_Transmit(&huart1, (uint8_t*)status_msg, len, 100);

    osDelay(500);
  }
  /* USER CODE END StartStatusTask */
}

/* USER CODE BEGIN Header_StartButtonTask */
/**
  * @brief Function implementing the buttonTask thread.
  *        Polls B1 button -> CENTER command.
  */
/* USER CODE END Header_StartButtonTask */
void StartButtonTask(void *argument)
{
  /* USER CODE BEGIN StartButtonTask */

  GPIO_PinState last_state = GPIO_PIN_SET;

  for(;;)
  {
    GPIO_PinState current = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

    if (last_state == GPIO_PIN_SET && current == GPIO_PIN_RESET) {
      motor_cmd_t cmd;
      cmd.type = CMD_CENTER;
      cmd.value = 0;
      osMessageQueuePut(cmdQueueHandle, &cmd, 0, 0);

      char msg[] = "[BUTTON] Center\r\n";
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
    }

    last_state = current;
    osDelay(50);
  }
  /* USER CODE END StartButtonTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */
  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */