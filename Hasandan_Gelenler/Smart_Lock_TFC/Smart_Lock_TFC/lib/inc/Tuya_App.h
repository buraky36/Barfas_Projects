/**
 * @file Tuya_App.h
 * @author Adem marangoz (adem.marangoz95@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-10-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */


//______________________________ Include Files _________________________________
#include "main.h"
//==============================================================================


//_____________________________ Generic Objects ________________________________
typedef enum
{
    REMOTELY_UNLOCK_IDEL,
    REMOTELY_UNLOCK_WAIT,
    REMOTELY_UNLOCK_OPEN,
    REMOTELY_UNLOCK_CLOSE,
    REMOTELY_UNLOCK_WAIT_AFTER_OPEN
}Remotely_Unlock_state;

volatile extern Remotely_Unlock_state remotely_unlock_st;
extern volatile uint32_t check_communcation_counter;
extern uint8_t unlockRequestDowncounter;
extern uint8_t unlock_remotly_counter;
#define UNLOCK_REQUEST_TIME_SEC 30
//==============================================================================


//______________________________ Global Functions ______________________________
void tuya_init(void);
uint8_t Tuya_loop();
void update_battery_state(uint8_t flag);
void Set_RTC_By_Tuya(void);
//==============================================================================
