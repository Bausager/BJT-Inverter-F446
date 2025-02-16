/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "TimerConfig.h" // Automatic timer configuration for TIM1 and TIM2.
#include "Inverter.h" // SVM algorithm.
#include "adc_meas.h" // ADC measurements + conversations.
#include "measCalc.h" // Calculating various values.
#include "PLL.h" // PLL algorithms
#include "GridEstimation.h" // Grid estimation algorithm
#include "control.h" // For control
#include "misc.h" // For micalanius functons



#include <string.h> // Used for: sprintf(...)
#include <stdio.h> // Used for: input/output
#include <stdbool.h> // Used for: Boolean data type

//#include <stdlib.h> //

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
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
// Switching- , Nominal grid - and Sample frequency
const float f_sw = 20e3 * 2.0f; // 20kHz for every interrupt * 2 for the whole period (20kHz switching)
const float nominal_freq = 50.0f; // Nominal grid frequency
float angular_nominal_freq = nominal_freq *2.0f * 3.1415f; // Nominal angular grid frequency (This variable is used to inject grid changes)
const float sample_freq = 5e3; // Sample frequency

// Flags
bool TIM2_falg = false; // Flag for TIM2
uint32_t TIM2_flag_acumulator = 0; // Helper for TIM2_falg (increases with every sample interrupt and resets when it hits: sample_freq)

// Control variables
float Pref, Qref, angular_freq_ref, Udref;


// Modulation index, PWM and angle variables for SVM
float Mi, angle;
float PWM1, PWM2, PWM3;

// Voltage variables
float Uab, Ubc, Uca;
float UabRMS, UbcRMS, UcaRMS;
float Ua, Ub, Uc;
float UaRMS, UbRMS, UcRMS;
float Ualpha, Ubeta, Ugamma;
float Ud, Uq, Uz;

// Current variables
float Ia, Ib, Ic;
float IaRMS, IbRMS, IcRMS;
float Ialpha, Ibeta, Igamma;
float Id, Iq, Iz;

// Power variables
float P, Q, S, Pf;

// Offset voltage for ADC and DC-Link voltage
float Offset, DCLink;

// Length of arrays
#define GridMeasNSamples 10 // Amount of stored measured samples for grid estimation
#define GridEstiNSamples 4 // Amount of estimating samples for grid estimation

// Grid Estimation
struct GridEstiMeas GridMeasValues[GridMeasNSamples]; // Stored measured samples for grid estimation
struct GridEstiVari GridEstiValues[GridEstiNSamples]; // Stored estimated samples for grid estimation

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
 * Function for simple UART write (mainly used for simple debugging)
 */
void writeValueToUART(double value){
	char outputBuffer[256];
	uint8_t len = snprintf(outputBuffer, sizeof(outputBuffer), "Value: %f \r\n", value);
	HAL_UART_Transmit(&huart2, (uint8_t *)outputBuffer, len, 100);
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
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

/*
 * Starting PWM output channels
 */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

  /*
   * Configure timer interrupts for switching and sampling
   */
  TIM_freq(1, f_sw);
  TIM_freq(2, sample_freq);

  /*
   * Initialise SVM with timer information and interrupt frequency
   */
  svm_block_init(TIM1->ARR, f_sw);

  /*
   *  Set filter coefficients for measurements
   */
  Voltage_Filter_Length(0); // Set voltage measuring filter length
  Current_Filter_Length(0); // Set current measuring filter length
  Power_Filter_Length((uint32_t)((sample_freq/nominal_freq)*10.0f)); // Set power calculation filter length to 10 periods of nominal_freq
  RMS_Filter_Length((uint32_t)((sample_freq/nominal_freq)*10.0f)); // Set RMS filter length to 10 periods of nominal_freq

  /*
   * Initialise Grid estimation variables
   */
  InitiliseGridStruct(GridEstiNSamples, GridEstiValues);


  /*
   * Initialise Droop
   */
  Droop_Config(25.0f, 25.0f, f_sw);
  Udref = 1.9f; // 1.9V*sqrt(3) = 3.3Vpp Line-line voltage
  Pref = 0.05f; // Active Power reference for Droop control
  Qref = 0.0f; // Reactive Power reference for Droop control
  angular_freq_ref = angular_nominal_freq; // angular grid frequency reference for Droop control


  /*
   * Initialisation for Grid Following
   */
  LCL_Angle_Compensation(nominal_freq);
  dqPLL_Config(nominal_freq, sample_freq);
  AlphaBetaPLL_Config(nominal_freq, sample_freq);

  /*
   * Initialisation for Grid Forming
   */





  /*
   * Initial measurement for ADC Offset (is a must!) and DC Link voltage
   */
  for (int i = 0; i < (uint32_t)10e3; ++i) {
	  DCLink = Voltage_DCLink();
	  Offset = Voltage_Offset();
  }

  /*
   *  Start TIM1 and TIM2 Interrupts!
   */
  HAL_TIM_Base_Start_IT(&htim1);
  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  /*
	   * Runs every 1/sample_freq
	   */
	  if (TIM2_falg) {
		  TIM2_falg = false;

		  // Updates DC link voltage slow
		  Voltage_Offset();

		  /*
		   * Measure grid voltages
		   */
		  Uab = meas_Uab(Uab);
		  Ubc = meas_Ubc(Ubc);
		  Uca = meas_Uca(Uca);
		  // Re-calculate phase-phase voltages to phase-neutral voltages
		  calc_Uxx_to_Uxn(Uab, Ubc, Uca, &Ua, &Ub, &Uc);

		  /*
		   * Measure grid currents
		   */
		  Ia = meas_Ia(Ia);
		  Ib = meas_Ib(Ib);
		  Ic = meas_Ic(Ic);
		  // Calculating the Power
		  calc_Instantaneous_Power(Ua, Ub, Uc, Ia, Ib, Ic, &P, &Q);


		  // Calculate the Voltage RMS values
		  calc_RMS(Ua, Ub, Uc, &UaRMS, &UbRMS, &UcRMS);
		  calc_RMS(Uab, Ubc, Uca, &UabRMS, &UbcRMS, &UcaRMS);
		  // Calculate the Current RMS values
		  calc_RMS(Ia, Ib, Ic, &IaRMS, &IbRMS, &IcRMS);
		  // Calculating Power Factor
		  Pf = calc_Power_Factor(P, Q);

/*
		  if(TIM2_flag_acumulator < GridMeasNSamples){
			  GridMeasValues[TIM2_flag_acumulator].Pn = sqrtf(P*P);
			  GridMeasValues[TIM2_flag_acumulator].Qn = sqrtf(Q*Q);
			  GridMeasValues[TIM2_flag_acumulator].Vn = (UaRMS + UbRMS + UcRMS)/3.0f;
		  }
*/

		  /*
		   * Runs every second
		   */
		  if (TIM2_flag_acumulator >= sample_freq) {
			  TIM2_flag_acumulator = 0;

			  //transf_abc_to_dq(Ua, Ub, Uc, angle1, &Ud, &Uq);
			  //GeneticandRandomSearch(GridMeasNSamples, GridEstiNSamples, GridMeasValues, GridEstiValues);

			  char outputBuffer[256];
			  //uint8_t len = snprintf(outputBuffer, sizeof(outputBuffer), "UaRMS: %.2f. UbRMS: %.2f. UcRMS: %.2f. IaRMS: %.2f. IbRMS: %.2f. IcRMS: %.2f. P: %.2f. Q: %.2f. X: %.2f. R: %.2f. Eg: %.2f. Error: %.2f. SVM angle: %.2f. PLL angle: %.2f. Angle Error: %.2f.\r\n", UaRMS, UbRMS, UcRMS, IaRMS, IbRMS, IcRMS, P, Q, GridEstiValues[0].X, GridEstiValues[0].R, GridEstiValues[0].Eg, GridEstiValues[0].Error, angle, theta, residiual_angle);
			  uint8_t len = snprintf(outputBuffer, sizeof(outputBuffer), "UaRMS: %.2f. UbRMS: %.2f. UcRMS: %.2f. IaRMS: %.3f. IbRMS: %.3f. IcRMS: %.3f. P: %.4f. Q: %.4f. PF: %.2f. SVM angle: %.2f. Ud: %0.2f. Uq: %0.2f.\r\n", UaRMS, UbRMS, UcRMS, IaRMS, IbRMS, IcRMS, P, Q, Pf, angle, Ud, Uq);
			  HAL_UART_Transmit(&huart2, (uint8_t *)outputBuffer, len, 100);

		  }
		  TIM2_flag_acumulator++;

	  }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
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
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_12;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.ScanConvMode = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DMAContinuousRequests = DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TOGGLE;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_OC_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_ENABLE_OCxPRELOAD(&htim1, TIM_CHANNEL_1);
  if (HAL_TIM_OC_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_ENABLE_OCxPRELOAD(&htim1, TIM_CHANNEL_2);
  if (HAL_TIM_OC_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_ENABLE_OCxPRELOAD(&htim1, TIM_CHANNEL_3);
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 5;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim){

	if(htim->Instance==TIM1){


		/*
		 * Grid Forming
		 */
		Droop_Forming_Resistive(P, Q, Pref, Qref, angular_nominal_freq, Udref, &angle, &Ud);
		Mi = Ud_to_Mi(DCLink, Ud);

		/*
		 * Grid Following
		 */
		//angle = dqPLL(Ua, Ub, Uc, &Ud);
		//angle = AlphaBetaPLL(Ua, Ub, Uc);


	    svm_block(Mi, angle, &PWM1, &PWM2, &PWM3);
	    TIM1->CCR1 = PWM1;
	    TIM1->CCR2 = PWM2;
	    TIM1->CCR3 = PWM3;

	}

	if(htim->Instance==TIM2){
		/*
		 * TIM2_falg to use in main-loop so it doesn't clog up the SVM.
		 */
		//char outputBuffer[256];
		//uint8_t len = snprintf(outputBuffer, sizeof(outputBuffer), "P: %.4f. Q: %.4f. Ud: %0.2f. Uq: %0.2f. Mi: %0.2f.\r\n", P, Q, Ud, Uq, Mi);
		//HAL_UART_Transmit(&huart2, (uint8_t *)outputBuffer, len, 100);
		TIM2_falg = true;
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

	char ErrorBuffer[256];
	uint8_t ErrorLen = snprintf(ErrorBuffer, sizeof(ErrorBuffer), "\r\n Error! Stuck in Error_Handler! \r\n");
	HAL_UART_Transmit(&huart2, (uint8_t *)ErrorBuffer, ErrorLen, 100);

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
