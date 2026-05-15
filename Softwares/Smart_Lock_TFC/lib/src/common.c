/**
 * @file common.c
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This file has to be used with fingerprint, TSM(Keypad), RFID & Tuya libraries
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */

//______________________________ Include Files _________________________________
#include "common.h"
#include  <stdio.h>
#include  <string.h>
#include "FP_Driver.h"
#include "Tsm12.h"
#include "Motor_Driver.h"
#include "MFRC522.h"
#include "Leds.h"
#include "Flash.h"
#include "AP23xxx_G0x.h"
#include "Battery.h"
#include "buffer.h"
#include "system.h"
#include "mcu_api.h"
#include "Leds.h"
#include "Temp_pass.h"


//==============================================================================

extern volatile uint8_t system_default_admin;
volatile uint8_t max_admin_user_flag = false;

//_____________________________ Generic Objects ________________________________
Admin_id_typedef admin_id = ADMIN_ID_IDLE;
Smart_Lock Smart_Lock_Val;
//==============================================================================


//______________________________ Global Functions ______________________________
extern volatile uint32_t standby_time;
extern uint8_t led_delay_flag;
extern GPIO_InitTypeDef GPIO_InitStruct;
void system_reset(void)
{
	led_all_reset();
	led_blink_start();

	Set_RGB(RGB_RED, 1);
	Delete_All_User();
	HAL_Delay(500);
	
	Flash_DataBase[0].Flash_DataBase_ST.FP_State = 1;
  Flash_DataBase[0].Flash_DataBase_ST.RFID_State = 1;
  Flash_DataBase[0].Flash_DataBase_ST.VOICE_State = 1;
	Flash_DataBase[0].Flash_DataBase_ST.LANGUAGE_State = 1;
	Flash_DataBase[0].Flash_DataBase_ST.Default_System = 1;
  strncpy((char*)Flash_DataBase[0].Flash_DataBase_ST.VERSION,"1.05", 4);
  Fill_Buffers((uint8_t*)TSM_DataBase, DB_SIZE(TSM_DATA_SIZE, TSM_DataBase_UN));
  Fill_Buffers((uint8_t*)RFID_DataBase, DB_SIZE(RFID_DATA_SIZE, RFID_DataBase_UN));
  TSM_DataBase[0].TSM_DataBase_ST.user_id[0] = 1;
	TSM_DataBase[0].TSM_DataBase_ST.user_id[1] = 2;
	TSM_DataBase[0].TSM_DataBase_ST.user_id[2] = 3;
	TSM_DataBase[0].TSM_DataBase_ST.user_id[3] = 4;
	TSM_DataBase[0].TSM_DataBase_ST.user_id[4] = 11;
	TSM_DataBase[0].TSM_DataBase_ST.access = 2;
	
	Smart_Lock_Val.admin_count = 0;
	Smart_Lock_Val.user_count = 0;
	
	Set_Flash_Write_Active(1);
  Erase_and_Write();
	
	GPIO_InitStruct.Pin = EXTI_FP_Touch_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(EXTI_FP_Touch_GPIO_Port, &GPIO_InitStruct);
	
	wifi_uart_write_frame(WIFI_RESET_CMD, 0);
	HAL_Delay(2000);
	mcu_set_wifi_mode(SMART_CONFIG);
	led_delay_flag = 0;
	led_blink_stop();

	Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);
	err_counter = 0;
	standby_time = 0;
}


/**
 * @brief This function is used to access the system settings when the admin's ID is valid.
 */
void success_admin_login(void)
{
    active_dis_device(0);
    err_counter = 0; // The error counter is reset.
    Confi_RGB(RGB_GREEN); // The RGB green light is illuminated.
    Set_Voice_Add(VOICE_MAIN_MENU); // play main menu voice
    _sys1_menu = setting_mode;  // The system transitions to the next state, "setting_mode."
    admin_id = ADMIN_ID_IDLE; // The admin_id is reset.
    _Reset_And_Clear_Tsm_buffer(); // The TSM buffer is reset.

}

/**
 * @brief This function is used to access the system settings when the admin's ID is invalid.
 */
void fail_ademin_login(void)
{
    Confi_RGB(RGB_RED); // The RGB green light is illuminated.
    if(++err_counter != ERROR_COUNTER){Set_Voice_Add(VOICE_WORNG_ENTRY); } // increment error counter play main menu voice
    admin_id = ADMIN_ID_IDLE; // The admin_id is reset.
    _Reset_And_Clear_Tsm_buffer(); // The TSM buffer is reset.
}


/**
 * @brief This function is used to register a new user and save them in the database.
 * 
 * @param db_add DB address
 * @param db_size DB size
 * @param st_size structer of DB's size
 * @param des uint8_t pointer to DB   
 * @param id_size id size of DB
 * @param id_type id type
 */
void registrtion_id(uint64_t *db_add, uint16_t db_size, uint8_t st_size, uint8_t *des, uint8_t id_size, ID_Type id_type)
{
    uint32_t index;
		uint16_t admin_control_counter = 0;
		uint8_t *register_start_address = (uint8_t*)db_add;
		register_start_address +=5;
		max_admin_user_flag = false;
		
		if((Flash_DataBase[0].Flash_DataBase_ST.Default_System == 1) && (system_default_admin == 1))
		{
			system_default_admin = 0;
			index = 0;	
			Flash_DataBase[0].Flash_DataBase_ST.Default_System = 0;
		}
		else
		{
			if(id_type == ADMIN_ID)
			{
				for(uint32_t j = 0; j < db_size; j++ ) // admin 1 den fazlami bak
				{
					if( *register_start_address == 2)
						admin_control_counter++;
					register_start_address += 8; 					
				}
				if(admin_control_counter >= MAX_ADMIN)
				{
					max_admin_user_flag = true;
					return;
				}
					
			}
			else
			{
				for(uint32_t j = 0; j < db_size; j++ ) // admin 1 den fazlami bak
				{
					if( *register_start_address == 1)
						admin_control_counter++;	
					register_start_address += 8; 
				}
				if(admin_control_counter >= MAX_USER)
				{
					max_admin_user_flag = true;
					return;
				}
					
			}
			
			index = Find_Empty_Address(db_add, db_size,  st_size);
			
		}
		
    if(index != (uint32_t)0xFFFFFFFF) // if find empty index in RFID DB
    {
        Write_To_DB((uint8_t*)(db_add + index), des, id_size, id_type);
        Set_Flash_Write_Active(1);
        Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);
    }else
    {
        // TODO MEMORY IS FULL
        // Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);
    }
    tsm_counter = 0;
}


/**
 * @brief This function is used to delete a user from the database.
 * @param src uint8_t pointer to DB  
 * @param des uint8_t pointer to received id  
 * @param db_size DB size
 * @param st_size structer of DB's size
 * @param id_size id size of DB
 * @param id_type id type
 */
void delete_id(uint8_t *src, uint8_t *des, uint8_t db_size, uint8_t st_size, uint8_t id_size, ID_Type id_type)
{
		uint8_t i = 0;
    i = Delete_Id(src, des, db_size , st_size, id_size, id_type);
		if(i == 33)
			Set_Voice_Add(VOICE_WORNG_ENTRY);
    else 
			Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);
    tsm_counter = 0;
}


/**
 * @brief This function is used to temporarily disable and enable both FP and RFID when entering the system settings.
 * 
 * @param _active 1 : RFID and FP active
 *                0 : RFID and FP inactive  
 */
void active_dis_device(uint8_t _active)
{
    if(_active)
    {
        Set_MFRC522_Active_State(RFID_ACTIVE);
        Set_FP_Active_State(FP_ACTIVE);
    }else
    {
        Set_MFRC522_Active_State(RFID_DISACTIVE);
        Set_FP_Active_State(FP_DISACTIVE);

    }
     _Reset_And_Clear_Temp_buffer();
}

//==============================================================================
