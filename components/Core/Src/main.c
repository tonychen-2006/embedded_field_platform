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
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>

#include "gps.h"
#include "gps_logger.h"
#include "rtc_ds3231.h"
#include "sd_card.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TEST_PASS_BLINK_MS             1000U
#define TEST_WAIT_BLINK_MS             250U
#define TEST_FAIL_BLINK_MS             125U
#define RTC_TEST_READ_INTERVAL_MS      1000U
#define GPX_EN_DEBOUNCE_MS            50U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */
static volatile RTC_Time rtc_test_last_time;
static volatile HAL_StatusTypeDef rtc_test_last_status;
static volatile uint8_t rtc_test_passed;
static volatile GPS gps_test_last_data;
static volatile HAL_StatusTypeDef gps_test_start_status;
static volatile uint8_t gps_test_started;
static volatile uint8_t gps_test_has_bytes;
static volatile uint8_t gps_test_has_parsed_sentence;
static volatile uint8_t gps_test_has_fix;
static volatile SDCard_Status sd_card_test_status;
static volatile uint8_t sd_card_test_ready;
static volatile uint32_t sd_card_test_write_errors;
static volatile uint32_t gps_logger_test_write_errors;
static volatile uint8_t gps_logger_test_gpx_enabled;
static volatile uint8_t gps_logger_test_gpx_closed;

static uint32_t rtc_test_last_read_ms;
static uint32_t test_led_last_toggle_ms;
static uint32_t gpx_en_last_change_ms;
static uint8_t gpx_en_raw_state;
static uint8_t gpx_en_stable_state;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
static bool RTC_TestRunOnce(void);
static void RTC_TestPoll(uint32_t now_ms);
static bool RTC_TestTimeMatches(const RTC_Time *expected, const RTC_Time *actual);
static void GPS_TestCapture(const GPS *snapshot);
static bool GPX_EnablePoll(uint32_t now_ms);
static void Test_UpdateLed(uint32_t now_ms);

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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
  rtc_test_passed = RTC_TestRunOnce() ? 1U : 0U;
  GPS_Init();
  gps_test_start_status = GPS_StartReceive(&huart1);
  gps_test_started = gps_test_start_status == HAL_OK ? 1U : 0U;
  sd_card_test_status = SDCard_Init(&hspi1);
  sd_card_test_ready = sd_card_test_status == SD_CARD_OK ? 1U : 0U;
  if (sd_card_test_ready != 0U)
  {
    GPSLogger_Init();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now_ms = HAL_GetTick();
    bool gpx_enabled = GPX_EnablePoll(now_ms);

    RTC_TestPoll(now_ms);
    GPS_Process();
    GPS_TestCapture(GPS_GetDataPtr());
    if (sd_card_test_ready != 0U)
    {
      (void)GPSLogger_SetGpxEnabled(gpx_enabled, now_ms);
    }
    if (GPS_HasNewData())
    {
      GPS snapshot = GPS_GetData();

      GPS_TestCapture(&snapshot);
      if (sd_card_test_ready != 0U)
      {
        GPSLogger_Process(&snapshot, now_ms);
      }
    }
    if (sd_card_test_ready != 0U)
    {
      GPSLogger_Tick(now_ms);
    }

    sd_card_test_status = SDCard_GetLastStatus();
    sd_card_test_ready = SDCard_IsReady() ? 1U : 0U;
    sd_card_test_write_errors = SDCard_GetWriteErrors();
    gps_logger_test_write_errors = GPSLogger_GetWriteErrors();
    gps_logger_test_gpx_enabled = GPSLogger_IsGpxEnabled() ? 1U : 0U;
    gps_logger_test_gpx_closed = GPSLogger_IsGpxClosed() ? 1U : 0U;
    Test_UpdateLed(now_ms);
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_CS_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : GPX_EN_Pin */
  GPIO_InitStruct.Pin = GPX_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPX_EN_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static bool RTC_TestRunOnce(void)
{
  const RTC_Time test_time = {
      .seconds = 0U,
      .minutes = 34U,
      .hours = 12U,
      .day = 2U,
      .date = 8U,
      .month = 6U,
      .year = 26U,
  };
  RTC_Time read_time = {0};

  rtc_test_last_status = DS3231_SetTime(&hi2c1, &test_time);
  if (rtc_test_last_status != HAL_OK)
  {
    return false;
  }

  HAL_Delay(1500U);

  rtc_test_last_status = DS3231_ReadTime(&hi2c1, &read_time);
  rtc_test_last_time = read_time;
  if (rtc_test_last_status != HAL_OK)
  {
    return false;
  }

  return RTC_TestTimeMatches(&test_time, &read_time);
}

static void RTC_TestPoll(uint32_t now_ms)
{
  RTC_Time read_time = {0};

  if ((now_ms - rtc_test_last_read_ms) < RTC_TEST_READ_INTERVAL_MS)
  {
    return;
  }

  rtc_test_last_read_ms = now_ms;
  rtc_test_last_status = DS3231_ReadTime(&hi2c1, &read_time);
  rtc_test_last_time = read_time;
  if (rtc_test_last_status != HAL_OK)
  {
    rtc_test_passed = 0U;
  }
}

static bool RTC_TestTimeMatches(const RTC_Time *expected, const RTC_Time *actual)
{
  if (expected == NULL || actual == NULL)
  {
    return false;
  }

  return actual->year == expected->year &&
         actual->month == expected->month &&
         actual->date == expected->date &&
         actual->day == expected->day &&
         actual->hours == expected->hours &&
         actual->minutes == expected->minutes &&
         actual->seconds >= 1U &&
         actual->seconds <= 3U;
}

static void GPS_TestCapture(const GPS *snapshot)
{
  if (snapshot == NULL)
  {
    return;
  }

  gps_test_last_data = *snapshot;
  gps_test_has_bytes = snapshot->bytesReceived > 0U ? 1U : 0U;
  gps_test_has_parsed_sentence = snapshot->sentencesParsed > 0U ? 1U : 0U;
  gps_test_has_fix = snapshot->fix != 0U ? 1U : 0U;
}

static bool GPX_EnablePoll(uint32_t now_ms)
{
  uint8_t raw_state = HAL_GPIO_ReadPin(GPX_EN_GPIO_Port, GPX_EN_Pin) == GPIO_PIN_SET ? 1U : 0U;

  if (raw_state != gpx_en_raw_state)
  {
    gpx_en_raw_state = raw_state;
    gpx_en_last_change_ms = now_ms;
  }

  if (raw_state != gpx_en_stable_state &&
      (now_ms - gpx_en_last_change_ms) >= GPX_EN_DEBOUNCE_MS)
  {
    gpx_en_stable_state = raw_state;
  }

  return gpx_en_stable_state != 0U;
}

static void Test_UpdateLed(uint32_t now_ms)
{
  uint32_t interval_ms;

  if (rtc_test_passed == 0U || gps_test_started == 0U || sd_card_test_ready == 0U)
  {
    interval_ms = TEST_FAIL_BLINK_MS;
  }
  else if (gps_test_has_parsed_sentence == 0U)
  {
    interval_ms = TEST_WAIT_BLINK_MS;
  }
  else
  {
    interval_ms = TEST_PASS_BLINK_MS;
  }

  if ((now_ms - test_led_last_toggle_ms) >= interval_ms)
  {
    test_led_last_toggle_ms = now_ms;
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
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

#ifdef  USE_FULL_ASSERT
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
