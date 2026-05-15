/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
//#define DEBUG_PRINTF_ENABLE  //DEBUG PRINTF AKTIF
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define ENABLE_TIMER_PERIP_CODES
#define TRACE_DEBUG

#ifdef TRACE_DEBUG
void printd(char *format, ...);
#else
//#define printd(...)
#endif

typedef enum {
	DOOR_LOCKED,
	DOOR_UNLOCKED
}doorLockState_t;

typedef enum {
	TUYA_IDLE,
	TUYA_ADD_PASSWORD_STARTED,
	TUYA_ADD_FINGERPRINT_STARTED,
	TUYA_ADD_CARD_STARTED
}tuyaState_t;

/**
 * @brief input type @INPUT_TYPE_DEFINITION
 */
typedef enum
{
  IDLE_INPUT,
  FP_INPUT,
  TSM_INPUT,
  RFID_INPUT,
  APP_INPUT
}Input_Typedef;

/**
 * @brief id validition type @VALIDITY_TYPE_DEFINITION
 */
typedef enum
{
  IDLE_CHECK,
  SUCCESS_CHECK,
  FAIL_CHECK
}Check_Typedef;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define EXTI_FP_Touch_Pin GPIO_PIN_13
#define EXTI_FP_Touch_GPIO_Port GPIOC
#define EXTI_FP_Touch_EXTI_IRQn EXTI4_15_IRQn
#define LED7_Pin GPIO_PIN_14
#define LED7_GPIO_Port GPIOC
#define LED10_Pin GPIO_PIN_15
#define LED10_GPIO_Port GPIOC
#define MOTOR_ENABLE_Pin GPIO_PIN_0
#define MOTOR_ENABLE_GPIO_Port GPIOF
#define SYSTEM_RESET_INT_Pin GPIO_PIN_1
#define SYSTEM_RESET_INT_GPIO_Port GPIOF
#define SYSTEM_RESET_INT_EXTI_IRQn EXTI0_1_IRQn
#define IC_ENABLE_Pin GPIO_PIN_0
#define IC_ENABLE_GPIO_Port GPIOA
#define SPI_RFID_SCK_Pin GPIO_PIN_1
#define SPI_RFID_SCK_GPIO_Port GPIOA
#define WBR1_TX_Pin GPIO_PIN_2
#define WBR1_TX_GPIO_Port GPIOA
#define WBR1_RX_Pin GPIO_PIN_3
#define WBR1_RX_GPIO_Port GPIOA
#define WBR1_EN_Pin GPIO_PIN_4
#define WBR1_EN_GPIO_Port GPIOA
#define LED5_Pin GPIO_PIN_5
#define LED5_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_6
#define LED1_GPIO_Port GPIOA
#define BAT_VOLTAGE_Pin GPIO_PIN_7
#define BAT_VOLTAGE_GPIO_Port GPIOA
#define LED4_Pin GPIO_PIN_0
#define LED4_GPIO_Port GPIOB
#define Out_RFID_NSS_Pin GPIO_PIN_1
#define Out_RFID_NSS_GPIO_Port GPIOB
#define RGB_GREEN_Pin GPIO_PIN_2
#define RGB_GREEN_GPIO_Port GPIOB
#define Out_AP23_SBT_Pin GPIO_PIN_12
#define Out_AP23_SBT_GPIO_Port GPIOB
#define Motor_CCW_Pin GPIO_PIN_13
#define Motor_CCW_GPIO_Port GPIOB
#define Motor_CW_Pin GPIO_PIN_14
#define Motor_CW_GPIO_Port GPIOB
#define LED6_Pin GPIO_PIN_15
#define LED6_GPIO_Port GPIOB
#define FP_EN_VCC_Pin GPIO_PIN_8
#define FP_EN_VCC_GPIO_Port GPIOA
#define USART_FP_Tx_Pin GPIO_PIN_9
#define USART_FP_Tx_GPIO_Port GPIOA
#define LED9_Pin GPIO_PIN_6
#define LED9_GPIO_Port GPIOC
#define LED12_Pin GPIO_PIN_7
#define LED12_GPIO_Port GPIOC
#define USART_FP_Rx_Pin GPIO_PIN_10
#define USART_FP_Rx_GPIO_Port GPIOA
#define SPI_RFID_MISO_Pin GPIO_PIN_11
#define SPI_RFID_MISO_GPIO_Port GPIOA
#define SPI_RFID_MOSI_Pin GPIO_PIN_12
#define SPI_RFID_MOSI_GPIO_Port GPIOA
#define LED11_Pin GPIO_PIN_15
#define LED11_GPIO_Port GPIOA
#define TSM_RESET_Pin GPIO_PIN_0
#define TSM_RESET_GPIO_Port GPIOD
#define LED3_Pin GPIO_PIN_1
#define LED3_GPIO_Port GPIOD
#define RGB_BLUE_Pin GPIO_PIN_2
#define RGB_BLUE_GPIO_Port GPIOD
#define RGB_RED_Pin GPIO_PIN_3
#define RGB_RED_GPIO_Port GPIOD
#define LED2_Pin GPIO_PIN_3
#define LED2_GPIO_Port GPIOB
#define LED8_Pin GPIO_PIN_4
#define LED8_GPIO_Port GPIOB
#define TOUCH_INT_Pin GPIO_PIN_7
#define TOUCH_INT_GPIO_Port GPIOB
#define TOUCH_INT_EXTI_IRQn EXTI4_15_IRQn
#define I2C_Tsm_SCL_Pin GPIO_PIN_8
#define I2C_Tsm_SCL_GPIO_Port GPIOB
#define I2C1_Tsm_SDA_Pin GPIO_PIN_9
#define I2C1_Tsm_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

#define BAT_VOLTAGE_SOFTWARE_ENABLE  	1

#define KEYPAD_LEDS_PORT GPIOB
#define KEYPAD_LEDS_PIN GPIO_PIN_15 
#define TOUCH_PORT_INT  GPIOB  
#define TOUCH_Pin_INT  GPIO_PIN_7  

#define TIM_1K_100HZ_DIV	10
#define WIFI_REINIT_TRIAL_NUM		15	// Number of trials before reinitializing wifi
#define WIFI_MODE_RESET_TRIAL_NUM	6	// Number of trials before resetting wifi mode
// #define USE_TUYA
#define AUTO_LOCK_SECONDS 3
#define TUYA_WIFI_ENABLE
#define UNLOCK_REQUEST_TIME_SEC 30

#define IC_POWER_ON 											HAL_GPIO_WritePin(IC_ENABLE_GPIO_Port, IC_ENABLE_Pin, GPIO_PIN_SET);
#define IC_POWER_OFF 											HAL_GPIO_WritePin(IC_ENABLE_GPIO_Port, IC_ENABLE_Pin, GPIO_PIN_RESET);

#define MOTOR_VOICE_POWER_ON 							HAL_GPIO_WritePin(MOTOR_ENABLE_GPIO_Port, MOTOR_ENABLE_Pin, GPIO_PIN_SET);
#define MOTOR_VOICE_POWER_OFF 						HAL_GPIO_WritePin(MOTOR_ENABLE_GPIO_Port, MOTOR_ENABLE_Pin, GPIO_PIN_RESET);

#define TUYA_MODULE_POWER_ON 							HAL_GPIO_WritePin(WBR1_EN_GPIO_Port, WBR1_EN_Pin, GPIO_PIN_SET);
#define TUYA_MODULE_POWER_OFF 						HAL_GPIO_WritePin(WBR1_EN_GPIO_Port, WBR1_EN_Pin, GPIO_PIN_RESET);

extern unsigned char currentWifiWorkingState;
#define ERROR_COUNTER               5
extern uint8_t err_counter;
extern bool system_locked;
extern bool door_opened;
extern uint32_t tsm_counter;
extern Check_Typedef FP_Check;
extern Check_Typedef TSM_Check;
extern Check_Typedef RFID_Check;
extern Check_Typedef APP_Check;
extern uint8_t common_id;
extern uint8_t activateTuyaUnlockingRequest;
void Check_Input(Input_Typedef _input, Check_Typedef _check, uint8_t id);
void Smart_Lock_IO();
extern unsigned char tuya_RxByte; 
extern volatile uint8_t Tsm_control_flag;

void Smart_Lock_Task(void);
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
