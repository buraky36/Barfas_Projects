/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g0xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32g0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FP_Driver.h"
#include "mcu_api.h"
#include "AP23xxx_G0x.h"
#include "TSM12.h"
#include "Leds.h"
#include "Tuya_App.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
extern uint32_t door_opened_counter;
extern volatile uint8_t system_reset_flag;
extern volatile uint8_t system_reset_button_flag;
extern volatile uint8_t smart_lock_task_start;
extern volatile uint8_t rtc_wakeup_int_flag;
extern uint8_t audio_play_timeout_flag;
extern volatile uint32_t standby_time;

uint32_t audio_play_timeout = 0;

uint32_t system_reset_count = 0;
uint8_t system_reset_control = 0;

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_adc1;
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern RTC_HandleTypeDef hrtc;
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart2;
/* USER CODE BEGIN EV */
extern volatile uint8_t smart_lock_task_counter;

extern uint8_t led_delay_flag;
extern uint32_t led_delay_count;
extern uint8_t led_number;
extern uint32_t led_blink_delay;
extern uint8_t led_Blink_flag;
bool ignoreEccErr = true;
bool eccDoubleBitErrOccurred = false;

extern volatile uint8_t sleep_rfid_control_flag;

extern volatile uint8_t Tuya_notification_flag;
extern volatile uint32_t Tuya_notification_timer;

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
/* Check if ECC double bit error occurred if ignoring ECC errors flag is set */
    if (ignoreEccErr && (READ_BIT(FLASH->ECCR, FLASH_ECCR_ECCD) != 0U))
    {
        /* Reset the detection flag to enable detecting subsequent double bit ECC errors */
        SET_BIT(FLASH->ECCR, FLASH_ECCR_ECCD);
        eccDoubleBitErrOccurred = true;
        return;
    }
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
		SET_BIT(FLASH->ECCR, FLASH_ECCR_ECCC);
		return;
  while (1)
  {
		
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
		NVIC_SystemReset();
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
	if(smart_lock_task_start)//timer 10ms
	{
		smart_lock_task_counter++;
		if(smart_lock_task_counter >= 10)
		{
			smart_lock_task_counter = 0;
			Smart_Lock_Task();
		}
	}
	
  if(led_delay_flag)
	{
		led_delay_count++;
		if(led_delay_count >= LED_DELAY)
		{
			touch_led(led_number, SET);
			led_delay_count = 0;
			led_delay_flag = 0;
		}
	}
	
	if(led_Blink_flag)
	{
		led_blink_delay++;
		if(led_blink_delay >= 50) // 50ms
		{
			led_blink_start();
			led_blink_delay = 0;
		}
	}
	if(audio_play_timeout_flag)
	{
		audio_play_timeout++;
		if(audio_play_timeout > 3000)
		{
			audio_play_timeout = 0;
			audio_play_timeout_flag = 0;
		}
	}
	if(standby_time < 15000)
	{
		standby_time++;
	}
	if(system_reset_control)
	{
		system_reset_count++;
		if(system_reset_count > 200)
			HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
		if(system_reset_count > 1250)
			HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
		if(system_reset_count > 2500)
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
		if(system_reset_count > 3000)
		{
			if(HAL_GPIO_ReadPin(SYSTEM_RESET_INT_GPIO_Port, SYSTEM_RESET_INT_Pin))
			{
				Set_Voice_Add(VOICE_PRASSED_BOTTON); //Sistem Sifirlama
				system_reset_flag = 1;
			}
			system_reset_count = 0;
			system_reset_control = 0;
			led_all_reset();
		}
	}
	if(Tuya_notification_flag)
	{
		Tuya_notification_timer++;
		if(Tuya_notification_timer > 10000)
		{
			Tuya_notification_flag  = 0;
			Tuya_notification_timer = 0;
		}
	}
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32G0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles RTC and TAMP interrupts through EXTI lines 19 and 21.
  */
void RTC_TAMP_IRQHandler(void)
{
  /* USER CODE BEGIN RTC_TAMP_IRQn 0 */
	rtc_wakeup_int_flag = 1;
	sleep_rfid_control_flag = 1;
  /* USER CODE END RTC_TAMP_IRQn 0 */
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
  /* USER CODE BEGIN RTC_TAMP_IRQn 1 */

  /* USER CODE END RTC_TAMP_IRQn 1 */
}

/**
  * @brief This function handles EXTI line 0 and line 1 interrupts.
  */
void EXTI0_1_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI0_1_IRQn 0 */
	rtc_wakeup_int_flag = 0;
  /* USER CODE END EXTI0_1_IRQn 0 */
//  HAL_GPIO_EXTI_IRQHandler(SYSTEM_RESET_INT_Pin);
  /* USER CODE BEGIN EXTI0_1_IRQn 1 */
	
	if(__HAL_GPIO_EXTI_GET_IT(SYSTEM_RESET_INT_Pin) != 0x00u)
  {
		HAL_GPIO_EXTI_IRQHandler(SYSTEM_RESET_INT_Pin);
		led_all_reset();
		system_reset_control = 1;
		Set_Voice_Add(VOICE_PRASSED_BOTTON); //Sistem Sifirlama
	}
	
	
//	_sys1_menu = setting_mode;
//	_sys2_menu = sys_reset_hand;
//	system_reset_button_flag = true;
//	Set_Voice_Add(VOICE_PRASSED_BOTTON); //Sistem Sifirlama
  /* USER CODE END EXTI0_1_IRQn 1 */
}

/**
  * @brief This function handles EXTI line 4 to 15 interrupts.
  */
void EXTI4_15_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI4_15_IRQn 0 */
	rtc_wakeup_int_flag = 0;
	sleep_rfid_control_flag = 0;
  /* USER CODE END EXTI4_15_IRQn 0 */
//  HAL_GPIO_EXTI_IRQHandler(TOUCH_INT_Pin);
//  HAL_GPIO_EXTI_IRQHandler(EXTI_FP_Touch_Pin);
  /* USER CODE BEGIN EXTI4_15_IRQn 1 */
	standby_time = 0;
	/********# Touch Button Interrupt #****************/
	if(__HAL_GPIO_EXTI_GET_IT(TOUCH_INT_Pin) != 0x00u)
  {
		HAL_GPIO_EXTI_IRQHandler(TOUCH_INT_Pin);
	}
	
	/********# Finger Print  Interrupt ****************/
	if(__HAL_GPIO_EXTI_GET_IT(EXTI_FP_Touch_Pin) != 0x00u)
  {
		
		HAL_GPIO_EXTI_IRQHandler(EXTI_FP_Touch_Pin);
		if(system_locked == false)
    {

			tsm_counter = 0;
      HAL_GPIO_WritePin(FP_EN_VCC_GPIO_Port,FP_EN_VCC_Pin, GPIO_PIN_SET);
      FP_Signal = FP_RECEIVED_SIGNAL;
      if(FP_State == FP_IDLE){FP_State = FP_COMPARE_USER;}
    }
	}
  /* USER CODE END EXTI4_15_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel 1 interrupt.
  */
void DMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */

  /* USER CODE END DMA1_Channel1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_adc1);
  /* USER CODE BEGIN DMA1_Channel1_IRQn 1 */

  /* USER CODE END DMA1_Channel1_IRQn 1 */
}

/**
  * @brief This function handles ADC1 interrupt.
  */
void ADC1_IRQHandler(void)
{
  /* USER CODE BEGIN ADC1_IRQn 0 */

  /* USER CODE END ADC1_IRQn 0 */
  HAL_ADC_IRQHandler(&hadc1);
  /* USER CODE BEGIN ADC1_IRQn 1 */

  /* USER CODE END ADC1_IRQn 1 */
}

/**
  * @brief This function handles TIM3 global interrupt.
  */
void TIM3_IRQHandler(void)
{
  /* USER CODE BEGIN TIM3_IRQn 0 */

  /* USER CODE END TIM3_IRQn 0 */
  HAL_TIM_IRQHandler(&htim3);
  /* USER CODE BEGIN TIM3_IRQn 1 */

  /* USER CODE END TIM3_IRQn 1 */
}

/**
  * @brief This function handles I2C1 event global interrupt / I2C1 wake-up interrupt through EXTI line 23.
  */
void I2C1_IRQHandler(void)
{
  /* USER CODE BEGIN I2C1_IRQn 0 */
//	
///********# Touch Button TSM12 Communication Interrupt #****************/
//  
  /* USER CODE END I2C1_IRQn 0 */
  if (hi2c1.Instance->ISR & (I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR)) {
    HAL_I2C_ER_IRQHandler(&hi2c1);
  } else {
    HAL_I2C_EV_IRQHandler(&hi2c1);
  }
  /* USER CODE BEGIN I2C1_IRQn 1 */

  /* USER CODE END I2C1_IRQn 1 */
}

/**
  * @brief This function handles SPI1 global interrupt.
  */
void SPI1_IRQHandler(void)
{
  /* USER CODE BEGIN SPI1_IRQn 0 */

/********# RFID MFRC52201HN1 Module Communication Interrupt #****************/	
	
  /* USER CODE END SPI1_IRQn 0 */
  HAL_SPI_IRQHandler(&hspi1);
  /* USER CODE BEGIN SPI1_IRQn 1 */

  /* USER CODE END SPI1_IRQn 1 */
}

/**
  * @brief This function handles USART2 global interrupt / USART2 wake-up interrupt through EXTI line 26.
  */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */
	
/********# WBR1 TUYA Module Communication Interrupt #****************/
	
  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */
  uart_receive_input(tuya_RxByte);	//(unsigned char)(USART2->RDR)
  HAL_UART_Receive_IT(&huart2, &tuya_RxByte, 1);
  /* USER CODE END USART2_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
