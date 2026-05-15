/*
 * pl_uart.c
 *
 *  Created on:
 *      Author:
 */

#include "pl_uart.h"
#include "stm32g0xx_hal_uart.h"
#include "stm32g0xx_hal.h"

#ifdef __GNUC__
/* With GCC/RAISONANCE, small printf (option LD Linker->Libraries->Small printf
set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

//extern UART_HandleTypeDef huart3;

//PUTCHAR_PROTOTYPE
//{
//	HAL_UART_Transmit(&huart3, (uint8_t*)&ch, 1, 0xFFFF);
//	return ch;
//}

//static void UART3_pin_init(void)
//{
//	 GPIO_InitTypeDef GPIO_InitStruct = {0};

//    __HAL_RCC_USART3_CLK_ENABLE();
//    
//	  __HAL_RCC_GPIOA_CLK_ENABLE();
//    __HAL_RCC_GPIOB_CLK_ENABLE();
//	 
//	 /**USART3 GPIO Configuration
//    PA5     ------> USART3_TX
//    PB0     ------> USART3_RX
//    */

//    GPIO_InitStruct.Pin = DEBUG_TX_Pin;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    GPIO_InitStruct.Alternate = GPIO_AF4_USART3;
//    HAL_GPIO_Init(DEBUG_TX_GPIO_Port, &GPIO_InitStruct);

//    GPIO_InitStruct.Pin = DEBUG_RX_Pin;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    GPIO_InitStruct.Alternate = GPIO_AF4_USART3;
//    HAL_GPIO_Init(DEBUG_RX_GPIO_Port, &GPIO_InitStruct);
//}

//HAL_StatusTypeDef Debug_UART_Init(UART_HandleTypeDef *huart)
//{
//	  /* Check the UART handle allocation */
//	  if (huart == NULL)
//	  {
//	    return HAL_ERROR;
//	  }

//	  if (huart->Init.HwFlowCtl != UART_HWCONTROL_NONE)
//	  {
//	    /* Check the parameters */
//	    assert_param(IS_UART_HWFLOW_INSTANCE(huart->Instance));
//	  }
//	  else
//	  {
//	    /* Check the parameters */
//	    assert_param((IS_UART_INSTANCE(huart->Instance)) || (IS_LPUART_INSTANCE(huart->Instance)));
//	  }

//	  if (huart->gState == HAL_UART_STATE_RESET)
//	  {
//	    huart->Lock = HAL_UNLOCKED;
//	  }

//	  huart->gState = HAL_UART_STATE_BUSY;

//	  /* Disable the Peripheral */
//	  __HAL_UART_DISABLE(huart);

//	  /* Set the UART Communication parameters */
//	  if (UART_SetConfig(huart) == HAL_ERROR)
//	  {
//	    return HAL_ERROR;
//	  }

//	  if (huart->AdvancedInit.AdvFeatureInit != UART_ADVFEATURE_NO_INIT)
//	  {
//	    UART_AdvFeatureConfig(huart);
//	  }

//	  /* In asynchronous mode, the following bits must be kept cleared:
//	  - LINEN and CLKEN bits in the USART_CR2 register,
//	  - SCEN, HDSEL and IREN  bits in the USART_CR3 register.*/
//	  CLEAR_BIT(huart->Instance->CR2, (USART_CR2_LINEN | USART_CR2_CLKEN));
//	  CLEAR_BIT(huart->Instance->CR3, (USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN));

//	  /* Enable the Peripheral */
//	  __HAL_UART_ENABLE(huart);

//	  /* TEACK and/or REACK to check before moving huart->gState and huart->RxState to Ready */
//	  return (UART_CheckIdleState(huart));
//}


//static void USART3_UART_Init(void)
//{

//	huart3.Instance = USART3;
//	huart3.Init.BaudRate = 115200;
//	huart3.Init.WordLength = UART_WORDLENGTH_8B;
//	huart3.Init.StopBits = UART_STOPBITS_1;
//	huart3.Init.Parity = UART_PARITY_NONE;
//	huart3.Init.Mode = UART_MODE_TX_RX;
//	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
//	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
//	huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
//	huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
//  if (Debug_UART_Init(&huart3) != HAL_OK)
//  {
//  }
//}


//void debug_console_init(void)
//{
//	UART3_pin_init();
//	USART3_UART_Init();
//}

