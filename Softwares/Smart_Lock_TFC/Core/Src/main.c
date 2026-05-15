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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "iwdg.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "buffer.h"
#include "Flash.h"
#include "FP_Driver.h"
#include "MFRC522.h"
#include "AP23xxx_G0x.h"
#include "Tsm12.h"
#include "Motor_Driver.h"
#include "Leds.h"
#include "mcu_api.h"
#include <string.h>
#include "common.h"
#include "Battery.h"
#include "Temp_pass.h"
#include "Tuya_App.h"
//#include "pl_uart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
bool system_locked = true;
bool door_opened = false;
uint32_t door_opened_counter = 0;
uint32_t tsm_counter = 0;

extern uint8_t wait_blinking();

Check_Typedef FP_Check = IDLE_CHECK;
Check_Typedef TSM_Check = IDLE_CHECK;
Check_Typedef RFID_Check = IDLE_CHECK;
Check_Typedef APP_Check = IDLE_CHECK;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
uint8_t err_counter = 0;
#define UNAVAILABLE_ID 0xFF
uint32_t cnt_10ms_remote = 0; // tuya counters
uint32_t cnt_10ms_bat = 0; // tuya counters
uint32_t cnt_10ms_wifi = 0; // tuya counters
uint8_t activateTuyaUnlockingRequest = 0;
unsigned char currentWifiWorkingState=0xFF;
uint8_t common_id = 0xFF;
volatile uint8_t system_reset_flag = 0;
volatile uint8_t system_reset_time_flag = 0;
volatile uint32_t system_time_count = 0;
volatile uint8_t Remotly_control_flag = 0;
volatile uint8_t sleep_mode_flag = 0;

volatile uint8_t smart_lock_task_counter = 0;
volatile uint8_t smart_lock_task_start = 0;

extern unsigned char value11[10];
extern uint8_t audio_play_timeout_flag;
volatile uint8_t rtc_wakeup_int_flag = 0;
volatile uint32_t sleep_time_counter = 0;
extern RGB_Typedef RGB_State;

volatile uint32_t lock_counter = 0;
volatile uint32_t to_unlock_counter = 0;
volatile uint32_t standby_time = 0;
volatile uint8_t sleep_rfid_control_flag = 0;
volatile uint8_t rfid_single_read_flag = 0;

volatile uint32_t battery_counter = 0;
static uint8_t battery_cal_start = 0;
volatile uint8_t Tsm_control_flag = 0;
volatile uint8_t FP_control_flag = 0;
volatile uint8_t RF_control_flag = 0;

#ifdef BAT_VOLTAGE_SOFTWARE_ENABLE

volatile uint8_t Batt_Meas_Level_Flag = 0;
volatile uint8_t Batt_Meas_Level_Count = 0;
volatile uint8_t battery_mesurement_count = 0;

extern uint8_t led_delay_flag;

#endif

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void SystemClock_Config_For_Sleep(void);
void sleep_init(void);
void wakeup_init(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

GPIO_InitTypeDef GPIO_InitStruct = {0};
volatile uint32_t iwdt_value_time =0;

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
//SystemClock_Config_For_Sleep();
  /* USER CODE BEGIN SysInit */

	/* Enable PWR clock */
  __HAL_RCC_PWR_CLK_ENABLE();
	
  Read_When_Run();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_IWDG_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

	ADC->CCR &= ~ADC_CCR_VREFEN; // ADC VREF DISABLE
	if(HAL_ADC_DeInit(&hadc1) != HAL_OK) HAL_ADC_DeInit(&hadc1);
	
	HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
	__HAL_RCC_DMA1_CLK_DISABLE();
	
#ifdef DEBUG_PRINTF_ENABLE

//  debug_console_init();
//  dbg_printf("#    Hasan Huseyin ASLAN #\r\n");
//  dbg_printf("#  SMART LOCK PROJECT V1.06  #\r\n");
//	dbg_printf("      Debugger Started    \r\n");

#endif

	IC_POWER_ON;
	MOTOR_VOICE_POWER_ON;
	TUYA_MODULE_POWER_ON;
	HAL_Delay(20);
	
	led_blink_start();
	Set_Voice_Add(VOICE_ONLOI_SMART_WORLD); 
	Play_Voice();
	
  /* Initialize connected devices */
   MFRC522_Init();
   TSM_Init(0x55); // 0xFF -> 111: 3~5T T=mm  / 0x55 -> 101: 6~8T / 0xBB ve 0x33 -> 011: 08~10T T=mm  // 		TSM_Init(0xBB); // 011: 08~10T T=mm 0xBB
   tuya_init(); /* Enable receive interrupt for Tuya and initialize Tuya device */
	 
   Confi_RGB(RGB_STOP);

//	MX_IWDG_Init();//8 second // prescaler 64  // 4095
//	uint32_t iwdt_count = uwTick;
	Set_FP_Active_State(FP_ACTIVE);
	system_locked = false;
	
//Remote ok // password valid for 7 years  
//	HAL_Delay(2000);
//	value11[0] = 0x01;
//	value11[1] = 0x01;
//	mcu_dp_raw_update(DPID_REMOTE_NO_PD_SETKEY, value11,1);

	led_blink_stop();
	smart_lock_task_start = 1;
	battery_cal_start = 1;
//	Fingerprint - RFID active and passive control  //
	if(Flash_DataBase[0].Flash_DataBase_ST.FP_State != 1)
	{
		/*Configure GPIO pin : PtPin */
		GPIO_InitStruct.Pin = EXTI_FP_Touch_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(EXTI_FP_Touch_GPIO_Port, &GPIO_InitStruct);
	}
	
//	Fingerprint - RFID active and passive control  //	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	__HAL_IWDG_RELOAD_COUNTER(&hiwdg);  //prescaler 128 / counter 2500   10sn
		
	if(sleep_rfid_control_flag == 1)
	{
		if(Flash_DataBase[0].Flash_DataBase_ST.RFID_State == 1){RFID_Task_F();}
		if(sleep_rfid_control_flag == 0)
		{
			rfid_single_read_flag = 1;
			goto jump1;
		}
	}
	else if(sleep_rfid_control_flag == 0)
	{
		if(system_locked == false)
	  {
			if(Flash_DataBase[0].Flash_DataBase_ST.RFID_State == 1){RFID_Task_F();}
			jump1:
			if(rfid_single_read_flag == 1)
			{
				wakeup_init();
			}
			
      TSM_Data_Process();
      if(Flash_DataBase[0].Flash_DataBase_ST.FP_State == 1){FP_Task_F();}
      Erase_and_Write();
      Run_Motor();
	  }else{}
		wait_blinking();
	  Run_RGB();
	  if(Flash_DataBase[0].Flash_DataBase_ST.VOICE_State == 1){Play_Voice();}

	/*****# TUYA APP #*****/
    get_temp_pass();
	  Tuya_loop();
	  wifi_uart_service();
	/*****# TUYA APP #*****/
	}
	
	#ifdef BAT_VOLTAGE_SOFTWARE_ENABLE

		if(battery_mesurement_count <= 5 && (HAL_GPIO_ReadPin(LED1_GPIO_Port,LED1_Pin)))
		{
			battery_mesurement_count++;
			if(Battery_Measurement())
			{
				if(Calc_BAT_Level() == 3)
					Batt_Meas_Level_Count++;
				if(battery_mesurement_count == 5)
				{
					if(Batt_Meas_Level_Count > 3)
					{
						Batt_Meas_Level_Flag = 1;
					}
					else
						Batt_Meas_Level_Flag = 0;
					Batt_Meas_Level_Count = 0;
				}
			}
			else
			{
				if(battery_mesurement_count > 0)
					battery_mesurement_count--;
			}
		}

		if(battery_counter >= 2)
		{
			battery_counter = 0;
			if(Batt_Meas_Level_Flag && !led_delay_flag )
			{
				HAL_GPIO_WritePin(RGB_RED_GPIO_Port, RGB_RED_Pin, GPIO_PIN_SET);
				Set_Voice_Add(VOICE_LOW_BATTERY);
				Play_Voice();
				HAL_Delay(3000);
				HAL_GPIO_WritePin(RGB_RED_GPIO_Port, RGB_RED_Pin, GPIO_PIN_RESET);
			}
		}
#endif
		if(system_reset_flag == 1)
		{
			system_reset();
			to_unlock_counter = 0; 
			system_locked = false;
			system_reset_flag = 0;
			
			_sys2_menu = main_menu; _sys1_menu = standby; RFID_State = RFID_IDLE;	FP_State = FP_IDLE;				
			_Reset_And_Clear_Tsm_buffer();	// Clear and reset some internal variables
			system_reset_time_flag = 1;
			system_time_count = HAL_GetTick();
		}
		if(system_reset_time_flag)
		{
			if(HAL_GetTick() - system_time_count > 20000)
				system_reset_time_flag = 0;
		}

if(sleep_mode_flag || ((RGB_State == RGB_IDLE) && (tsm_buffer[0] == 0x0E) &&(Get_AP23xxx_Active_State() == 2) 
	&& (Get_Motor_Active_State() == MOTOR_IDLE) && (_sys1_menu == standby) &&	(RFID_State == RFID_IDLE) && (!audio_play_timeout_flag) && (!system_reset_time_flag)
	&& (FP_State == FP_IDLE) && (FP_Signal != FP_RECEIVED_SIGNAL) && (LED_ON_OFF == 0) && (standby_time >= 10000)) )
{
	
#ifdef BAT_VOLTAGE_SOFTWARE_ENABLE
	battery_mesurement_count = 0;
#endif
	
	sleep_mode_flag = 0;
	sleep_init();
	
	/* Enter Sleep Mode , wake up is done once User push-button is pressed */
	HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
	
	wakeup_init();
	
	/* Resume Tick interrupt if disabled prior to SLEEP mode entry */
	if(rtc_wakeup_int_flag)
	{
		sleep_time_counter = 200;
		rtc_wakeup_int_flag = 0;
		if (system_locked) 
		{
			to_unlock_counter += sleep_time_counter;
			to_unlock_counter++; // Increment to_unlock_counter
		if ((to_unlock_counter >= 12000)) {to_unlock_counter = 0; system_locked = false;}
		}
	}
}
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void SystemClock_Config_For_Sleep(void) //2 MHz
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV8;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern volatile uint8_t system_reset_button_flag;
extern volatile uint8_t counter_temp_pass;
extern uint8_t led_delay_flag;
void Smart_Lock_Task(void)
{
	cnt_10ms_remote += sleep_time_counter;
	cnt_10ms_bat += sleep_time_counter;
	cnt_10ms_wifi += sleep_time_counter;
	/* 10ms Period Timer */
		/* Check if any input device is not in its idle state */
		if ((tsm_buffer[0] != 14) || (_sys1_menu != standby) ||	(RFID_State != RFID_IDLE) || (FP_State != FP_IDLE)) 
		{
			tsm_counter += sleep_time_counter;
			tsm_counter++;
			if (!(tsm_counter%6000)) //6000 1 minute
			{
				sleep_mode_flag = 1;
				_sys2_menu = main_menu; _sys1_menu = standby; RFID_State = RFID_IDLE;	FP_State = FP_IDLE;				
				_Reset_And_Clear_Tsm_buffer();	// Clear and reset some internal variables
				Set_RGB(RGB_IDLE, 1);			// Set RGB LEDs to an idle state
				Set_FP_Active_State(FP_ACTIVE);	// Activate fingerprint sensor
				Set_MFRC522_Active_State(RFID_ACTIVE);	// Activate MFRC522
				led_delay_flag = 0;
				led_all_reset();
				system_reset_button_flag = false;
			}
		}
		if ((tsm_buffer[0] != 14) && (_sys1_menu == standby) &&	(RFID_State == RFID_IDLE) && (FP_State == FP_IDLE)) 
		{
			if (!(tsm_counter%1000)) //1000 10 seconds
			{
				sleep_mode_flag = 1;
				_sys2_menu = main_menu; _sys1_menu = standby; RFID_State = RFID_IDLE;	FP_State = FP_IDLE;				
				_Reset_And_Clear_Tsm_buffer();	// Clear and reset some internal variables
				Set_RGB(RGB_IDLE, 1);			// Set RGB LEDs to an idle state
				Set_FP_Active_State(FP_ACTIVE);	// Activate fingerprint sensor
				Set_MFRC522_Active_State(RFID_ACTIVE);	// Activate MFRC522
				led_delay_flag = 0;
				led_all_reset();
				system_reset_button_flag = false;
			}
			
		}
		/* Check if err_counter has reached ERROR_COUNTER */
		if(err_counter >= ERROR_COUNTER) 
		{
			Confi_RGB(RGB_STOP);  
			Set_Voice_Add(VOICE_SYSTEM_LOCKED);
			led_delay_flag = 0;
			led_all_reset();
			err_counter = 0; system_locked = true;
		}else if (err_counter >= 1)
		{
			lock_counter += sleep_time_counter;
			lock_counter++; // Increment lock_counter
			if(lock_counter >= 12000) {lock_counter = 0; err_counter = 0;}
		}
		/* Check if the system is locked */
		if (system_locked) 
		{
//			to_unlock_counter += sleep_time_counter;
			to_unlock_counter++; // Increment to_unlock_counter
			if ((to_unlock_counter >= 12000)) 
			{
				to_unlock_counter = 0; 
				system_locked = false;
				
			}
		}
		/* Check if the door is opened */
		if (door_opened)
		{
			// Check if door_opened_counter is greater than 1000 or less than 1000 (alternate condition)
      door_opened_counter++;
			if (!(door_opened_counter%1000)) 
			{
				Set_Voice_Add(VOICE_SYSTEM_LOCKED);
			}
		}
		/* One second timer */
		if(cnt_10ms_remote >= 200)
		{
			cnt_10ms_remote = 0;
      uint8_t check_wifi_state = mcu_get_wifi_work_state();
      if((check_wifi_state == SMART_CONFIG_STATE) || (check_wifi_state == LOW_POWER_STATE))
			{
				check_communcation_counter++;
			}
			if(unlockRequestDowncounter > 0){unlockRequestDowncounter--; unlock_remotly_counter = 1;}
      if(cnt_10ms_wifi >= 200)//5 seconds
			{
				cnt_10ms_wifi = 0;
        if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_15)) // When the tuya app password is requested, the touch button must be pressed and connected to the cloud.
        {
          if(check_wifi_state == WIFI_CONN_CLOUD)
          {
						counter_temp_pass = 1;
					}
        }
      }
		} 
		cnt_10ms_remote++; // Increment 10ms counter
		cnt_10ms_bat++; // Increment 10ms counter
		cnt_10ms_wifi++; // Increment 10ms counter
		sleep_time_counter = 0;
//	}

}
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern I2C_HandleTypeDef hi2c1;

void sleep_init(void)
{
	//  /* Disable Prefetch Buffer */
// __HAL_FLASH_PREFETCH_BUFFER_DISABLE();

	if(sleep_rfid_control_flag == 0)
	{
		led_all_reset();
	
		GPIO_InitStruct.Pin = SPI_RFID_SCK_Pin| SPI_RFID_MISO_Pin| SPI_RFID_MOSI_Pin|LED5_Pin|USART_FP_Tx_Pin|USART_FP_Rx_Pin|//FP_EN_VCC_Pin|
												LED1_Pin|LED11_Pin|WBR1_TX_Pin|WBR1_RX_Pin|/*WBR1_EN_Pin|*/BAT_VOLTAGE_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
		GPIO_InitStruct.Pin = Out_RFID_NSS_Pin|Out_AP23_SBT_Pin|Motor_CCW_Pin|Motor_CW_Pin|GPIO_PIN_10|GPIO_PIN_11|
												LED2_Pin|LED4_Pin|LED6_Pin|LED7_Pin|RGB_GREEN_Pin|I2C_Tsm_SCL_Pin|I2C1_Tsm_SDA_Pin|TOUCH_EN_PIN|GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
		GPIO_InitStruct.Pin = LED8_Pin|LED9_Pin|LED10_Pin|LED12_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	
		GPIO_InitStruct.Pin = LED3_Pin|RGB_BLUE_Pin|RGB_RED_Pin|TSM_RESET_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

		/***********************    ANALOG PIN ********************************/

		__HAL_RCC_SPI2_CLK_DISABLE();
	
		__HAL_RCC_SPI1_CLK_DISABLE();
		HAL_NVIC_DisableIRQ(SPI1_IRQn);
			
		__HAL_RCC_I2C1_CLK_DISABLE();
		__HAL_RCC_USART1_CLK_DISABLE();
		__HAL_RCC_USART2_CLK_DISABLE();	
			
		IC_POWER_OFF;
		MOTOR_VOICE_POWER_OFF;
		TUYA_MODULE_POWER_OFF;
		HAL_Delay(5);		
	}
	else
	{
		sleep_rfid_control_flag = 0;
		
		GPIO_InitStruct.Pin = SPI_RFID_SCK_Pin| SPI_RFID_MISO_Pin| SPI_RFID_MOSI_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
		
		GPIO_InitStruct.Pin = Out_RFID_NSS_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
		
		__HAL_RCC_SPI1_CLK_DISABLE();
		HAL_NVIC_DisableIRQ(SPI1_IRQn);
		
		IC_POWER_OFF;
		HAL_Delay(5);		
	}
	
//	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF);
//	__HAL_RCC_PWR_CLK_DISABLE();
	
		SystemClock_Config_For_Sleep();
	
/************************ ## RTC SLEEP ##**********************/
/* Disable the write protection for RTC registers */
  __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
  /* Disable the Wake-Up timer */
  __HAL_RTC_WAKEUPTIMER_ENABLE(&hrtc);
	/* Clear flag Wake-Up */
	__HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
	uint32_t tickstart;
  if (READ_BIT(RTC->ICSR, RTC_ICSR_INITF) == 0U)
  {
    tickstart = HAL_GetTick();
    while(__HAL_RTC_WAKEUPTIMER_GET_FLAG(&hrtc, RTC_FLAG_WUTWF) == 0U)
    {
			if((HAL_GetTick() - tickstart ) > 1000)
      {
        goto jump;
      }
		}	
  }
	jump:
	/* Enable the write protection for RTC registers */
	__HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
/************************ ## RTC SLEEP ##**********************/
	
	HAL_SuspendTick();

}
void wakeup_init(void)
{
	
	SystemClock_Config();

	HAL_ResumeTick();

/************************ ## RTC WAKE UP ##**********************/	
	  /* Disable the write protection for RTC registers */
  __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
  /* Disable the Wake-Up timer */
  __HAL_RTC_WAKEUPTIMER_DISABLE(&hrtc);
  /* Clear flag Wake-Up */
  __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
	/* Enable the write protection for RTC registers */
  __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
/************************ ## RTC WAKE UP ##**********************/	
	rfid_single_read_flag = 0;
	if(sleep_rfid_control_flag == 0)
	{
		IC_POWER_ON;
		MOTOR_VOICE_POWER_ON;
		TUYA_MODULE_POWER_ON;
		HAL_Delay(5);
		
		GPIO_InitStruct.Pin = LED5_Pin|LED1_Pin|LED11_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
		GPIO_InitStruct.Pin = Out_RFID_NSS_Pin|Out_AP23_SBT_Pin|Motor_CCW_Pin|Motor_CW_Pin|//FP_EN_VCC_Pin|
												LED2_Pin|LED4_Pin|LED6_Pin|LED7_Pin|RGB_GREEN_Pin|TOUCH_EN_PIN|GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
		GPIO_InitStruct.Pin = LED8_Pin|LED9_Pin|LED10_Pin|LED12_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	
		GPIO_InitStruct.Pin = LED3_Pin|RGB_BLUE_Pin|RGB_RED_Pin|TSM_RESET_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
			GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	
		GPIO_InitStruct.Pin = I2C_Tsm_SCL_Pin|I2C1_Tsm_SDA_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF6_I2C1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
		__HAL_RCC_I2C1_CLK_ENABLE();
	
		__HAL_RCC_USART1_CLK_ENABLE();
		GPIO_InitStruct.Pin = USART_FP_Tx_Pin|USART_FP_Rx_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF1_USART1;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		__HAL_RCC_USART2_CLK_ENABLE();
		GPIO_InitStruct.Pin = WBR1_TX_Pin|WBR1_RX_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF1_USART2;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		__HAL_RCC_SPI2_CLK_ENABLE();
		GPIO_InitStruct.Pin = GPIO_PIN_10;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = GPIO_PIN_11;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF0_SPI2;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		__HAL_RCC_SPI1_CLK_ENABLE();
		GPIO_InitStruct.Pin = SPI_RFID_SCK_Pin|SPI_RFID_MISO_Pin|SPI_RFID_MOSI_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF0_SPI1;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
		HAL_NVIC_SetPriority(SPI1_IRQn, 1, 0);
		HAL_NVIC_EnableIRQ(SPI1_IRQn);

		MFRC522_Init();
	
		wifi_protocol_init();
		
//		wifi_uart_write_frame(SCHEDULE_TEMP_PASS_CMD, 0);
//		HAL_Delay(100); // Wait for a second
//		wifi_uart_write_frame(GET_GL_TIME_CMD,0);// Green TIME for Cloud password  
//		HAL_Delay(100); // Wait for a second
		
		battery_counter++;

	}
	else
	{
		IC_POWER_ON;
		
		GPIO_InitStruct.Pin = Out_RFID_NSS_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
		
		__HAL_RCC_SPI1_CLK_ENABLE();
		GPIO_InitStruct.Pin = SPI_RFID_SCK_Pin|SPI_RFID_MISO_Pin|SPI_RFID_MOSI_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF0_SPI1;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
		HAL_NVIC_SetPriority(SPI1_IRQn, 1, 0);
		HAL_NVIC_EnableIRQ(SPI1_IRQn);

		MFRC522_Init();
		
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
