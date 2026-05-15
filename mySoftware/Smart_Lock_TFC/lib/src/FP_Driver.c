/**
 * @file Fingerprint.c
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This Fingerprint driver uses UART for communication
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */

//______________________________ Include Files _________________________________
#include  <stdio.h>
#include  <string.h>
#include "FP_Driver.h"
#include "Flash.h"
#include "buffer.h"
#include "Tsm12.h"
#include "Motor_Driver.h"
#include "AP23xxx_G0x.h"
#include "usart.h"
#include "Leds.h"
#include "common.h"
//==============================================================================



//-------------------------------- FP_INIT_CONF --------------------------------
extern UART_HandleTypeDef huart1;
/* Through this structure, the VCC port, VCC pin, and also the function that will 
be invoked for the transmit and receive operation can be assigned.*/
Fingerprint_Driver Finger1 =    {
                                    .GPIOx.VCC_Port = FP_VCC_PORT,
                                    .GPIOx.VCC_Pin = FP_VCC_PIN,
                                    .GPIOx.Signal_Port = FP_INT_SIGNAL_PORT,
                                    .GPIOx.Signal_Pin = FP_INT_SIGNAL_PIN,
                                    .GPIOx.Uartx = &FP_USART_INST,
                                    #if (FP_TX_TYPE == FINGER_TRANSMIT_POLLING)
                                        .Tx_Polling = HAL_UART_Transmit,
                                    #elif (FP_TX_TYPE == FINGER_TRANSMIT_IT)
                                        .Tx_IT = HAL_UART_Transmit_IT,
                                    #elif (FP_TX_TYPE == FINGER_TRANSMIT_DMA)
                                        .Tx_DMA = HAL_UART_Transmit_DMA,
                                    #endif
                                    #if (FP_RX_TYPE == FINGER_RECEIVE_POLLING)
                                        .Rx_Polling = HAL_UART_Receive
                                    #elif (FP_RX_TYPE == FINGER_RECEIVE_IT)
                                        .Rx_IT = HAL_UART_Receive_IT
                                    #elif (FP_RX_TYPE == FINGER_RECEIVE_DMA)
                                        .Rx_DMA = HAL_UART_Receive_DMA
                                    #endif
                                };


FP_State_EN FP_State = FP_IDLE; // state machine controlling variable
#define FP_BUF_SIZE         8
static uint8_t FP_Rx[FP_BUF_SIZE] = {0}; // FP RX buffer
static uint8_t FP_Tx[FP_BUF_SIZE] = {0}; // FP TX buffer 
volatile FP_Signal_State FP_Signal = FP_WAIT_SIGNAL; // FP signal state variable
uint8_t LED_ON_OFF = 0; // FP led's state 
#define FP_REGISTRATION_BLUE_DELAY_MS       500
#define FP_REGISTRATION_SEC_DELAY_MS        20
#define FP_REGISTRATION_GREEN_DELAY_MS      500

volatile uint8_t return_control = 0;
volatile uint8_t return_count = 0;

extern Smart_Lock Smart_Lock_Val;

typedef enum
{
    IDLE_FP,
    TURN_OFF_FP,
    RED_FP,
    BLUE_FP,
    GREEN_FP,
    WHITE_FP
}FP_Color;


//==============================================================================



//------------------------ STATIC FUNCTION DECLARATION -------------------------
static void Make_CMD(uint8_t CMD, uint16_t UserId, uint8_t Role);
static void Blinking_Led(uint16_t led_add);
static FP_Active_State _FP_Active_State = FP_ACTIVE;
uint8_t FP_Send_CMD1(uint8_t CMD, uint16_t User_ID, uint8_t Role, FP_Abort_Receive _abort);
static FP_Color get_blink_color();
static void set_blink_color(FP_Color color);
uint8_t wait_blinking();
//==============================================================================

volatile uint8_t return_control1 = 0;
volatile uint8_t return_control2 = 0;
volatile uint8_t return_control3 = 0;
volatile uint8_t return_control4 = 0;
volatile uint8_t return_control5 = 0;

/**
 * @brief This function is used to initialize the fingerprint sensor.
 * 
 * @return HAL_StatusTypeDef    HAL_OK       = 0x00U,
                                HAL_ERROR    = 0x01U,
                                HAL_BUSY     = 0x02U,
                                HAL_TIMEOUT  = 0x03U
 */
HAL_StatusTypeDef FP_Init(void)
{
    #if (FP_RX_TYPE == FINGER_RECEIVE_POLLING)
	
	//toplam kullanici sayisi
//				FP_Send_CMD1(CMD_GET_TOTAL_USER_NUM, REGISTER_MODE, HAND_DETECTION_NOT_LIFTING, Not_Abort_Rec);
//	HAL_Delay(50);
	for(uint8_t i =0; i < 3; i++)
	{
        FP_Send_CMD(CMD_LIGHT, USERID_TURN_OFF_LIGHT, ROLE_LIGHT, Abort_Rec); // send turn off cmd to fp
	
					return_control1 = FP_Send_CMD(CMD_CONFIG_REG_MODE, REGISTER_MODE, REGISTER_MODE_NCNR, Not_Abort_Rec); // (Select NCNR mode)

					return_control2 = FP_Send_CMD(CMD_CONFIG_REG_MODE, HOMOLOGY_LEVEL, HOMOLOGY_LEVEL_4, Not_Abort_Rec);

					return_control3 = FP_Send_CMD(CMD_CONFIG_REG_MODE, HAND_DETECTION_CONFIG, HAND_DETECTION_LIFTING, Not_Abort_Rec);

					return_control4 = FP_Send_CMD(CMD_CONFIG_REG_MODE, FINGERPRINT_DETECTION_NUMBER, FINGERPRINT_DETECTION_NUMBER_3, Not_Abort_Rec);
		
					return_control5 = FP_Send_CMD(CMD_CONFIG_REG_MODE, SELF_LEARNING_PRIORITY, FIRST_RETURN_SELF_LEARNING, Not_Abort_Rec);
		
FP_Send_CMD(CMD_CONFIG_REG_MODE, 0x0041, FIRST_RETURN_SELF_LEARNING, Not_Abort_Rec);
					
					if(return_control1 || return_control2 || return_control3 || return_control4 || return_control5)
					{
						i = i;
					}
					else 
						i = 3;
	}
	
    #elif (FP_RX_TYPE == FINGER_RECEIVE_IT)
        if(Finger1.Rx_IT(Finger1.GPIOx.Uartx, FP_Rx, FP_DATA_LEN) != FP_ACK_SUCCESS){return FP_ACK_FAIL;}
    #elif (FP_RX_TYPE == FINGER_RECEIVE_DMA)
        if(Finger1.Rx_DMA(Finger1.GPIOx.Uartx, FP_Rx, FP_DATA_LEN) != FP_ACK_SUCCESS){return FP_ACK_FAIL;}
    #endif
    LED_ON_OFF = 0;
    return FP_ACK_SUCCESS;
}


/**
 * @brief This function is used to send commands and receive data.
 * 
 * @param CMD command type base on @CMD_DEFINITION
 * @param User_ID userid value base on @USERID_Defintion
 * @param Role  role value base on @ROLE_Defintion
 * @param _abort abort receive data base on @Abort_Rec_definiton
 * @return uint8_t  0x00 : FP_ACK_SUCCESS
 *                  0x22 : FP_ACK_CHKERROR
 *                  0x01 : FP_ACK_FAIL                  
 */
uint8_t FP_Send_CMD(uint8_t CMD, uint16_t User_ID, uint8_t Role, FP_Abort_Receive _abort)
{
    Make_CMD(CMD, User_ID, Role); // create cmd frame for send to fp
    HAL_UART_Transmit(&huart1, FP_Tx, 8, 1000);
    if(_abort == Not_Abort_Rec) /*don't abort receiving data from fp*/
    {
        HAL_UART_Receive(&huart1, FP_Rx, 8, 1000);
    }else 
    {
        HAL_Delay(5);
        HAL_UART_AbortReceive(&huart1);
    }

    return FP_ACK_SUCCESS;
}


/**
 * @brief This function is used to send registration CMD.
 * 
 * @param CMD command type base on @CMD_DEFINITION
 * @param User_ID userid value base on @USERID_Defintion
 * @param Role  role value base on @ROLE_Defintion
 * @param _abort abort receive data base on @Abort_Rec_definiton
 * @return uint8_t  0x00 : FP_ACK_SUCCESS
 *                  0x22 : FP_ACK_CHKERROR
 *                  0x01 : FP_ACK_FAIL                  
 */
uint8_t FP_Send_CMD1(uint8_t CMD, uint16_t User_ID, uint8_t Role, FP_Abort_Receive _abort)
{
	  volatile uint8_t data_return_Tx = FP_ACK_SUCCESS;
		volatile uint8_t data_return_Rx = FP_ACK_SUCCESS;
	
    Make_CMD(CMD, User_ID, Role); // create cmd frame for send to fp
    data_return_Tx = HAL_UART_Transmit(&huart1, FP_Tx, 8, 1000);
    if(_abort == Not_Abort_Rec) /*don't abort receiving data from fp*/
    {
        data_return_Rx = HAL_UART_Receive(&huart1, FP_Rx, 8, 3000);
    }else 
    {
        HAL_Delay(10);
        HAL_UART_AbortReceive(&huart1);
    }

    return data_return_Rx | data_return_Tx;
}

/**
 * @brief This function is used to compare fingerprint data with the database stored within 
 * the fingerprint sensor.
 * 
 * @param id_type ID Role base on @Role_Definition
 * @param role role value base on @ROLE_Defintion
 * @return uint8_t  user id if fail returns 0xFFFF
 */
uint16_t Match_User(ID_Type id_type, uint8_t role)
{
		volatile uint8_t cmd_match_return = 0;
    uint16_t retval = 0xFFFF;
		HAL_Delay(50);//sleep mode uyanirken power aktif süresi için eklendi
    cmd_match_return = FP_Send_CMD(CMD_LIGHT, USERID_BLUE_LIGHT2, ROLE_LIGHT, Abort_Rec);  LED_ON_OFF = 1;// send turn on red led
		HAL_Delay(250);
    cmd_match_return = FP_Send_CMD1(CMD_VERIFY_ONE_TO_ALL, USERID_REGISTER, ROLE_0, Not_Abort_Rec); // send match cmd
    if(((FP_Rx[2] == 0x00) & (FP_Rx[3] == 0x00)) || (FP_Rx[0] != 0xF5) || (FP_Rx[7] != 0xF5)) // if userid is 0 then id is fail
    {
        Blinking_Led(USERID_RED_LIGHT2);
        set_blink_color(RED_FP);
        return 0xFFFF;
    }
    if(role == 0){retval = (FP_Rx[2] << 8) | FP_Rx[3];} // if role 0 don't control user access  
    else{if((FP_Rx[4] == (uint8_t)id_type)){retval =  (FP_Rx[2] << 8) | FP_Rx[3];}} // compare id's access
		Blinking_Led(USERID_GREEN_LIGHT2);
    set_blink_color(GREEN_FP);
    return retval;
    
}


/**
 * @brief This function used to add a new fingerprint
 * 
 * @param id_type 1 : normal user, 2 : admin
 * @return uint16_t user id if fail return 0x08
 */
#include "iwdg.h"
volatile uint8_t register_control_flag = 0;
uint16_t Register_User(ID_Type id_type)
{
    uint16_t retval = 0x08;
		volatile uint8_t cmd_return = FP_ACK_SUCCESS;
		volatile uint8_t fingerprint_status = 0;
	
		// Raise the hand detection configuration.
//		FP_Send_CMD(CMD_CONFIG_REG_MODE, HAND_DETECTION_CONFIG, HAND_DETECTION_LIFTING, Abort_Rec);
	FP_Init();	
		for(uint8_t i = 0; i<7; i++) // 7
    {
			__HAL_IWDG_RELOAD_COUNTER(&hiwdg);
			if(i ==0)
			{
				cmd_return = FP_Send_CMD(CMD_LIGHT, USERID_BLUE_LIGHT1, ROLE_LIGHT, Abort_Rec); // turn on blue led
				HAL_Delay(FP_REGISTRATION_BLUE_DELAY_MS);
				cmd_return = FP_Send_CMD1(CMD_REGISTER_ONE, USERID_REGISTER, (uint8_t)id_type, Not_Abort_Rec); // send first registration cmd one time
				HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
				
			}
			if(FP_Rx[4] == 0x00)
      {
				retval = (FP_Rx[2] << 8) | FP_Rx[3]; // user id 
        HAL_Delay(FP_REGISTRATION_SEC_DELAY_MS);
        cmd_return = FP_Send_CMD(CMD_LIGHT, USERID_BLUE_LIGHT1, ROLE_LIGHT, Abort_Rec); // turn on green led
				Set_Voice_Add(VOICE_READ_AGAIN);
				Play_Voice();
        HAL_Delay(FP_REGISTRATION_SEC_DELAY_MS);
				register_control_flag = 0;
      }
			else if(cmd_return != 0 && fingerprint_status < 4)
			{
				Set_Voice_Add(VOICE_FAIL_PRESSED_BOTTON);
				Play_Voice();
				fingerprint_status++;
				i = 0;
			}
			else if(FP_Rx[4] != 0x00)
      {
        HAL_Delay(FP_REGISTRATION_SEC_DELAY_MS);
        cmd_return = FP_Send_CMD(CMD_LIGHT, USERID_RED_LIGHT1, ROLE_LIGHT, Abort_Rec); // turn on red led
			  register_control_flag = 1;
        break;
      }
			
			cmd_return = FP_Send_CMD1(CMD_REGISTER_ONE, USERID_REGISTER, (uint8_t)ROLE_0, Not_Abort_Rec); // send first registration cmd one time
			
			if(FP_Rx[1] == 0x03 && FP_Rx[4] == 0x00)
			{
				register_control_flag = 0;
				cmd_return = FP_Send_CMD(CMD_LIGHT, USERID_GREEN_LIGHT1, ROLE_LIGHT, Abort_Rec); // turn on green led
				HAL_Delay(500);
				break;
			}
			else
				register_control_flag = 1;
		}
		
		if(register_control_flag)
		{
			FP_Send_CMD(CMD_LIGHT, USERID_RED_LIGHT1, ROLE_LIGHT, Abort_Rec); // turn on red led
			HAL_Delay(500);
		}

		LED_ON_OFF = 1; // turn of to turn of FP's led
    return retval;
	
}


/**
 * @brief This function is used to verify the checksum of the received data.
 * 
 * @return uint8_t  0x00 : FP_DATA_CORRECT
 *                  0x22 : FP_ACK_CHKERROR
 */
uint8_t FP_Compare_Received_Data(void)
{

    FingerPring_Data_Frame *Data = (FingerPring_Data_Frame*)(FP_Rx);

    if(Data->Sof != SOF_VALUE){return FP_ACK_CHKERROR;} // if first byte of frame is not equal 0xF5 return FP_ACK_CHKERROR 
    /* if checksum is not correct return FP_ACK_CHKERROR */
    if(((Data->CMD_ACK)^(Data->UserId1)^(Data->UserId2)^(Data->Role)^(Data->Res))){return FP_DATA_CORRECT;}
    else{return FP_ACK_CHKERROR;}
    return FP_DATA_CORRECT;
}


/**
 * @brief This function is used for waiting to receive data from the fingerprint sensor.
 * 
 * @return uint8_t  0x00 : FP_DATA_CORRECT
 *                  0x08 : FP_RX_TIMEOUT
 */
uint8_t FP_Wait_Respons(void)
{
    uint32_t tickstart = HAL_GetTick();
    /* Wait until Receive data */
    if(UART_WaitOnFlagUntilTimeout(&huart1, UART_FLAG_RXNE, RESET, tickstart, FP_RX_TIMEOUT) != HAL_OK){return FP_ACK_TIMEOUT;}
    
    return FP_DATA_CORRECT;
}


/**
 * @brief This function is used to query the operational status of the fingerprint sensor.
 * 
 * @return FP_Active_State  1 : FP_ACTIVE
 *                          2 : FP_DISACTIVE
 */
FP_Active_State Get_FP_Active_State()
{
    return _FP_Active_State;
}


/**
 * @brief This function is used to set the operational status of the fingerprint sensor.
 * 
 * @param State State of sensor operation value base on @FP_Active_State_definition
 */
void Set_FP_Active_State(FP_Active_State State)
{
    _FP_Active_State = State;
}


/**
 * @brief This function is used to determine the role associated with an ID.
 * 
 * @param UserId User id 
 * @return uint8_t  0x01~ 0x03 : id role
 *                  0x05 : Fail
 */
uint8_t Get_Role(uint16_t UserId)
{
    /* Get role of id */
    FP_Send_CMD(CMD_GET_USER_ROLE, UserId, ROLE_0, Not_Abort_Rec);
    if(FP_Rx[4] == 0x05){return 5;} // if role is equal 5 meaning the fp is not definded
    return FP_Rx[4];
}


/**
 * @brief This function is used to delete an ID from the database of the sensor.
 * 
 * @param id_type ID Role base on @Role_Definition
 */
void Delete_User(ID_Type id_type)
{
	
    FP_Send_CMD(CMD_LIGHT, USERID_RED_LIGHT2, ROLE_LIGHT, Abort_Rec); // turn on red led
		HAL_Delay(50);
    FP_Send_CMD1(CMD_VERIFY_ONE_TO_ALL, USERID_REGISTER, ROLE_0, Not_Abort_Rec); // send match command to get userid and role
		if(FP_Rx[0] == 0x55)
		{
			FP_Rx[2] = FP_Rx[3];
			FP_Rx[3] = FP_Rx[4];
			FP_Rx[4] = FP_Rx[5];
			if(((FP_Rx[2] == 0x00) && (FP_Rx[3] == 0x00)) || (FP_Rx[4] != id_type)) 
			{
					Blinking_Led(USERID_RED_LIGHT2);
					Set_Voice_Add(VOICE_WORNG_ENTRY);
			}
			else
			{
				HAL_Delay(100);
				FP_Send_CMD1(CMD_DELETE_SPECIFIC_USER, ((((uint16_t)FP_Rx[2]) << 2) + ((uint16_t)FP_Rx[3])), ROLE_0, Not_Abort_Rec); // delete detected the userid
				Blinking_Led(USERID_GREEN_LIGHT2);
				Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);
				if(id_type == ADMIN_ID)
				{
					if(Smart_Lock_Val.admin_count > 0)
						Smart_Lock_Val.admin_count--;
				}
				else
				{
					if(Smart_Lock_Val.user_count > 0)
						Smart_Lock_Val.user_count--;
				}
				Set_Flash_Write_Active(1);
			}		 

		}
		else if(FP_Rx[0] == 0xF5)
		{
		
			if(((FP_Rx[2] == 0x00) && (FP_Rx[3] == 0x00)) || (FP_Rx[4] != id_type)) 
			{
					Blinking_Led(USERID_RED_LIGHT2);
					Set_Voice_Add(VOICE_WORNG_ENTRY);
			}
			else
			{
				HAL_Delay(100);
				FP_Send_CMD1(CMD_DELETE_SPECIFIC_USER, ((((uint16_t)FP_Rx[2]) << 2) + ((uint16_t)FP_Rx[3])), ROLE_0, Not_Abort_Rec); // delete detected the userid
				Blinking_Led(USERID_GREEN_LIGHT2);
				Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);
				if(id_type == ADMIN_ID)
				{
					if(Smart_Lock_Val.admin_count > 0)
						Smart_Lock_Val.admin_count--;
				}
				else
				{
					if(Smart_Lock_Val.user_count > 0)
						Smart_Lock_Val.user_count--;
				}
				Set_Flash_Write_Active(1);
			}		

		}
   
    LED_ON_OFF = 1;
}

/**
 * @brief This function is used to delete an ID from the database of the sensor.
 * 
 * @param id_type ID Role base on @Role_Definition
 */
void Delete_All_User(void)
{
		HAL_GPIO_WritePin(FP_EN_VCC_GPIO_Port,FP_EN_VCC_Pin, GPIO_PIN_SET);
		HAL_Delay(50);
		uint8_t dell_counter = 0;
    FP_Send_CMD(CMD_LIGHT, USERID_RED_LIGHT2, ROLE_LIGHT, Abort_Rec); // turn on red led
		HAL_Delay(50);
	
		FP_Send_CMD1(CMD_DELETE_ALL_USER, (uint16_t)0, (uint8_t)0, Not_Abort_Rec); // delete detected the userid
		HAL_Delay(100);
	
		while(FP_Rx[4] != 0x00 && dell_counter < 3)
		{
			dell_counter++;
			FP_Send_CMD1(CMD_DELETE_ALL_USER, (uint16_t)0, (uint8_t)0, Not_Abort_Rec); // delete detected the userid
			HAL_Delay(250);
//			FP_Send_CMD1(CMD_DELETE_ALL_USER, (uint16_t)0, (uint8_t)2, Not_Abort_Rec); // delete detected the userid
		}
    if(dell_counter > 2)
			Set_Voice_Add(VOICE_WORNG_ENTRY);
		else 
			Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);
			
    LED_ON_OFF = 1;
		HAL_GPIO_WritePin(FP_EN_VCC_GPIO_Port,FP_EN_VCC_Pin, GPIO_PIN_RESET);
}

/**
 * @brief This function is used to activate the fingerprint sensor 
 * based on the algorithm it was built with and the machine's states.
 */
void FP_Task_F(void)
{
    FP_Active_State state = Get_FP_Active_State(); // get _FP_Active_State state
    if((state == FP_ACTIVE) && (FP_Signal == FP_RECEIVED_SIGNAL) && (LED_ON_OFF == 0)) // if signal is received and received 8 byte from fp sensor 
    {
        uint16_t cmpret = 0xFF;
        switch (FP_State)
        {
            case FP_IDLE:
                break;
            case FP_COMPARE_USER: // for open door
                cmpret = Match_User(USER_ID, 0);
                if(cmpret != 0xFFFF) { FP_Check = SUCCESS_CHECK; common_id = cmpret;}
                else { FP_Check = FAIL_CHECK; }
                FP_State = FP_IDLE;
                break;

            case FP_COMPARE_ADMIN: // for entry to admin mode
                if(Match_User(ADMIN_ID, 1) != 0xFFFF) { admin_id = ADMIN_ID_SECCESFUL;}
                else{admin_id = ADMIN_ID_FAIL;}
                break;

            case FP_REGISTERATION_ADMIN:
								if(Smart_Lock_Val.admin_count < MAX_ADMIN)
								{
									if(Match_User(ADMIN_ID, 1) == 0xFFFF)
									{
										if(((Register_User(ADMIN_ID) == 0x00)) && !register_control_flag)
										{
											Smart_Lock_Val.admin_count++; 
											Set_Flash_Write_Active(1);
											Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);
										}
										else	Set_Voice_Add(VOICE_WORNG_ENTRY);
									}
									else
										Set_Voice_Add(VOICE_REGISTERED_USER);
								}
								else
									Set_Voice_Add(VOICE_MAX_REGISTRATION); 
                break;

            case FP_REGISTERATION_USER:
								if(Smart_Lock_Val.user_count < MAX_USER_FINGERPRINT)
								{
									if(Match_User(USER_ID, 0) == 0xFFFF)
									{
										if((Register_User(USER_ID) == 0x00) && !register_control_flag)
										{
											Smart_Lock_Val.user_count++;
											Set_Flash_Write_Active(1);
											Set_Voice_Add(VOICE_OPERATION_SUCCESSFUL);/*TODO*/
										}
										else	Set_Voice_Add(VOICE_WORNG_ENTRY);
									}
									else
										Set_Voice_Add(VOICE_REGISTERED_USER);
								}
								else
									Set_Voice_Add(VOICE_MAX_REGISTRATION);
                break;

            case FP_DELETE_ADMIN_ID:
                Delete_User(ADMIN_ID);
                break;

            case FP_DELETE_USER_ID:
                Delete_User(USER_ID);
                break;
            default:
                break;
        }
        for(uint8_t i = 0; i< 8 ; i++){FP_Rx[i] = 0;}
    }else if(LED_ON_OFF == 1)
    {
        static uint32_t tickstart = 0;
        if(tickstart == 0){tickstart = HAL_GetTick();}
        if((HAL_GetTick() - tickstart) > 500)
        {
            LED_ON_OFF = 0; FP_Send_CMD(CMD_LIGHT, USERID_TURN_OFF_LIGHT, ROLE_LIGHT, Abort_Rec);
            HAL_GPIO_WritePin(FP_EN_VCC_GPIO_Port,FP_EN_VCC_Pin, GPIO_PIN_RESET);
        }
    }
    wait_blinking();
    FP_Signal = FP_WAIT_SIGNAL;
//    Set_FP_Active_State(FP_DISACTIVE);
}

//==============================================================================


//------------------------- STATIC FUNCTION DEFINITION -------------------------
/**
 * @brief This function is used to build the frame intended to be sent to the fingerprint sensor.
 * 
 * @param CMD command value base on @CMD_DEFINITION
 * @param UserId userid value base on @USERID_Defintion
 * @param Role role value base on @ROLE_Defintion
 */
static void Make_CMD(uint8_t CMD, uint16_t UserId, uint8_t Role)
{
    FingerPring_Data_Frame *Data = (FingerPring_Data_Frame*)(FP_Tx);
    Data->Sof = 0xF5;
    Data->CMD_ACK = CMD;
    Data->UserId1 = (UserId >> 8) & 0xFF;
    Data->UserId2 = UserId & 0xFF;
    Data->Role = Role;
    Data->Res = 0x00;
    Data->Chk = (Data->CMD_ACK)^(Data->UserId1)^(Data->UserId2)
                ^(Data->Role)^(Data->Res);
    Data->Eof = 0xF5;
}


/**
 * @brief This function is used to inform the user about state of operation, blinking for 300 ms.
 * 
 * @param led_add led user id
 */
static void Blinking_Led(uint16_t led_add)
{
     HAL_Delay(5);
     FP_Send_CMD(CMD_LIGHT, led_add, ROLE_LIGHT, Abort_Rec); // turn on red led
		 HAL_Delay(300);
     LED_ON_OFF = 1;
}

FP_Color cur_fp_color = IDLE_FP;


static FP_Color get_blink_color()
{
    return cur_fp_color;
}

static void set_blink_color(FP_Color color)
{
    cur_fp_color = color;
}

uint8_t wait_blinking()
{
    static uint32_t tickstart = 0;
    FP_Color _cur_color = get_blink_color();
    switch (_cur_color)
    {
        case RED_FP:
            if(tickstart == 0){tickstart = HAL_GetTick();}
            if((HAL_GetTick() - tickstart) > 500) 
            {
                FP_Send_CMD(CMD_LIGHT, USERID_RED_LIGHT2, ROLE_LIGHT, Abort_Rec);
                tickstart = 0;
                set_blink_color(TURN_OFF_FP);
            }
            break;
            
        case GREEN_FP:
            if(tickstart == 0){tickstart = HAL_GetTick();}
            if((HAL_GetTick() - tickstart) > 500) 
            {
                FP_Send_CMD(CMD_LIGHT, USERID_GREEN_LIGHT2, ROLE_LIGHT, Abort_Rec);
                tickstart = 0;
                set_blink_color(TURN_OFF_FP);
            }
            break;

        case BLUE_FP:
            if(tickstart == 0){tickstart = HAL_GetTick();}
            if((HAL_GetTick() - tickstart) > 500) 
            {
                FP_Send_CMD(CMD_LIGHT, USERID_RED_LIGHT2, ROLE_LIGHT, Abort_Rec);
                tickstart = 0;
                _cur_color = TURN_OFF_FP;
            }
            break;
        case TURN_OFF_FP:
            FP_Send_CMD(CMD_LIGHT, USERID_TURN_OFF_LIGHT, ROLE_LIGHT, Abort_Rec);
            tickstart = 0;
            set_blink_color(IDLE_FP);
            break;
        case IDLE_FP:
        default:
            break;
    }
}
//==============================================================================
