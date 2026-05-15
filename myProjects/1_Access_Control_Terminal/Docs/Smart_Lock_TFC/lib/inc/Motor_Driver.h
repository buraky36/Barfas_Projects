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


#ifndef MOTOR_DRIVER_H_
#define MOTOR_DRIVER_H_
//______________________________ Include Files _________________________________
#include "main.h"
#include <stdint.h>
//==============================================================================


//_____________________________ Generic Objects ________________________________

/**
 * @brief state machine of motor @MOTOR_STATE_DEFINITION
 */
typedef enum
{
    MOTOR_IDLE,
    MOTOR_WAIT,
    MOTOR_CW,
    MOTOR_CCW,
    MOTOR_STOP
}Motor_Typedef;

#define WAIT_TO_CLOSE           3000 // when opened door wait until 3s to close
#define OPEN_CLOSE_OP_TIME      100 // 100 time of opening or closing the door
//==============================================================================


//______________________________ Global Functions ______________________________
void Set_Motor_Active_State(Motor_Typedef State);
Motor_Typedef Get_Motor_Active_State();
void Run_Motor();
//==============================================================================


#endif
