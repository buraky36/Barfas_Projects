/**
 * @file Motor_Driver.h
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This Motor driver
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */

//______________________________ Include Files _________________________________
#include "main.h"
#include <stdint.h>
#include "Motor_Driver.h"
#include "Leds.h"
//==============================================================================


//_____________________________ Generic Objects ________________________________
static Motor_Typedef Motor_Active_State;
//==============================================================================


//______________________________ Global Functions ______________________________
/**
 * @brief Set Motor state
 * @param State motor state base on @MOTOR_STATE_DEFINITION
 */
void Set_Motor_Active_State(Motor_Typedef State)
{
    Motor_Active_State = State;
}

/**
 * @brief Get Motor state
 * @return Motor_Typedef motor state
 */
Motor_Typedef Get_Motor_Active_State()
{
    return Motor_Active_State;
}

/**
 * @brief state machine function of motor  
 */
#include "Tsm12.h"
#include "AP23xxx_G0x.h"
#include "Flash.h"
extern volatile uint8_t Tsm_control_flag;
extern volatile uint8_t RF_control_flag;
extern volatile uint8_t FP_control_flag;
extern uint8_t led_delay_flag;

void Run_Motor()
{
    static uint32_t statrtick = 0;
    switch (Motor_Active_State)
    {
        case MOTOR_IDLE:
            break;
        case MOTOR_CW: // Open the door
                HAL_GPIO_WritePin(Motor_CCW_GPIO_Port, Motor_CCW_Pin, GPIO_PIN_SET);
                HAL_GPIO_WritePin(Motor_CW_GPIO_Port, Motor_CW_Pin, GPIO_PIN_RESET);
								HAL_Delay(100);
                HAL_GPIO_WritePin(Motor_CCW_GPIO_Port, Motor_CCW_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(Motor_CW_GPIO_Port, Motor_CW_Pin, GPIO_PIN_RESET);
                Set_Motor_Active_State(MOTOR_WAIT); statrtick = 0;
            break;
        case MOTOR_CCW: // Close the door
                HAL_GPIO_WritePin(Motor_CW_GPIO_Port, Motor_CW_Pin, GPIO_PIN_SET);
                HAL_GPIO_WritePin(Motor_CCW_GPIO_Port, Motor_CCW_Pin, GPIO_PIN_RESET);
								HAL_Delay(100);
                HAL_GPIO_WritePin(Motor_CCW_GPIO_Port, Motor_CCW_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(Motor_CW_GPIO_Port, Motor_CW_Pin, GPIO_PIN_RESET);
                Set_Motor_Active_State(MOTOR_STOP); statrtick = 0;
				
								Tsm_control_flag = 0;
								if(RF_control_flag)
								{
									Flash_DataBase[0].Flash_DataBase_ST.RFID_State = 1;
									RF_control_flag = 0;
								}
								if(FP_control_flag)
								{
									Flash_DataBase[0].Flash_DataBase_ST.FP_State = 1;
									FP_control_flag = 0;
								}
								
								HAL_GPIO_WritePin(FP_EN_VCC_GPIO_Port,FP_EN_VCC_Pin, GPIO_PIN_RESET);
				
            break;
        case MOTOR_WAIT: // 
            if(statrtick == 0){statrtick = HAL_GetTick();}
            if((HAL_GetTick() - statrtick) > WAIT_TO_CLOSE)
            {
                Set_Motor_Active_State(MOTOR_CCW);
								Set_Voice_Add(VOICE_CLOSE_DOOR); // play door opened voice
                statrtick = 0;
            }
            break;
        case MOTOR_STOP:
            Confi_RGB(RGB_STOP);
						if(tsm_buffer[0] == 0x0E)
						{
							led_delay_flag = 0;
							led_all_reset();
						}
            HAL_GPIO_WritePin(Motor_CCW_GPIO_Port, Motor_CCW_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(Motor_CW_GPIO_Port, Motor_CW_Pin, GPIO_PIN_RESET);
            Set_Motor_Active_State(MOTOR_IDLE);
            statrtick = 0;
            break;
        default:
            break;
    }
}

//==============================================================================
