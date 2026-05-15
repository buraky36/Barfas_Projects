/*
 * pl_uart.h
 *
 *  Created on: 2 Şub 2022
 *      Author: ARGE1
 */

#ifndef DEBUG_PRINTF_INC_PL_UART_H_
#define DEBUG_PRINTF_INC_PL_UART_H_

#include "main.h"
#include <stdio.h>
#include <stdint.h>

#define DEBUG_RX_Pin GPIO_PIN_0
#define DEBUG_RX_GPIO_Port GPIOB

#define DEBUG_TX_Pin GPIO_PIN_5
#define DEBUG_TX_GPIO_Port GPIOA

#define dbg_printf(...)  printf(__VA_ARGS__)

void debug_console_init(void);

#endif /* DEBUG_PRINTF_INC_PL_UART_H_ */
